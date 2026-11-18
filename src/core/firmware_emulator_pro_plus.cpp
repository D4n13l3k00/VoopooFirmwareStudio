#include "firmware_emulator_backend.h"
#include "firmware_emulator_runtime.h"
#include "firmware_emulator_screens.h"
#include "firmware_pro_plus_native.h"
#include "firmware_layout.h"
#include "app_state.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <iterator>
#include <utility>

namespace FirmwareEmulator {
namespace {
constexpr uint32_t RamBase = 0x20000000;
constexpr size_t RamSize = 0x4000;
constexpr int Width = 80, Height = 160;
constexpr size_t Pixels = Width * Height;
constexpr uint32_t ReturnAddress = 0x0fff0000;
using namespace ProPlusNative;
const HardwareRecord* FindRecord(const FirmwarePackage& package, uint16_t code) {
    for (const auto& record : package.records)
        if ((uint16_t(record.device_code[0]) << 8 | record.device_code[1]) == code) return &record;
    return nullptr;
}
uint32_t Le32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
std::string Hex(uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", static_cast<unsigned>(value));
    return text;
}
uint32_t Rgba(uint16_t color) {
    const uint32_t r = color >> 11, g = color >> 5 & 63, b = color & 31;
    return (r << 3 | r >> 2) | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2) << 16 | 0xff000000u;
}
class ProPlusBackend final : public Backend {
    ArmRuntime runtime_;
    ResourceReadTrace trace_;
    const Profile* profile_ = nullptr;
    const StudioState* source_ = nullptr;
    const uint8_t* sourceData_ = nullptr;
    size_t sourceSize_ = 0;
    uint64_t revision_ = 0;
    std::string sourceHash_, cachedError_;
    bool cached_ = false;
    std::vector<uint8_t> image_;
    std::array<std::vector<uint8_t>, std::size(Profiles)> stock_;
    Frame* frame_ = nullptr;
    std::array<bool, Pixels> covered_{};
    size_t coverage_ = 0;
    bool landscape_ = false, dataMode_ = false, highPending_ = false;
    uint8_t high_ = 0, command_ = 0, parameterCount_ = 0;
    std::array<uint8_t, 4> parameters_{};
    std::array<uint16_t, 4> window_{};
    uint8_t windowMask_ = 0;
    uint16_t x_ = 0, y_ = 0;
    enum class Fault { None, Read, Write, Command, Window };
    Fault fault_ = Fault::None;
    uint32_t faultAddress_ = 0;

    uint32_t Spi() const { return profile_->wide ? 0x40013c0c : 0x4000380c; }
    uint32_t Gpio() const { return profile_->wide ? 0x40010800 : 0x48000000; }
    uint32_t Uid() const { return profile_->wide ? 0x1ffff7f5 : 0x1ffff7ac; }
    void Stop(Fault fault, uint32_t address) noexcept {
        if (fault_ == Fault::None) { fault_ = fault; faultAddress_ = address; }
        uc_emu_stop(runtime_.Raw());
    }
    static void HardwareRead(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t, void* user) {
        auto& self = *static_cast<ProPlusBackend*>(user);
        if ((address == self.Spi() - 4 && size == (self.profile_->wide ? 2 : 4)) ||
            (address == self.Gpio() + 0x10 && size == 4)) return;
        if (size > 0 && address >= self.Uid() && address + static_cast<unsigned>(size) <= self.Uid() + 12) return;
        self.Stop(Fault::Read, static_cast<uint32_t>(address));
    }
    static void HardwareWrite(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t value, void* user) {
        auto& self = *static_cast<ProPlusBackend*>(user);
        if (address == 0x40003000 && size == 4 && value == 0xaaaa) {
            return; // IWDG reload; no expiry occurs within a bounded drawing call.
        }
        if (size == 4 && value == 0x100 &&
            (address == self.Gpio() + 0x18 || address == self.Gpio() + 0x28)) {
            const bool data = address == self.Gpio() + 0x18;
            if (self.highPending_ && data != self.dataMode_) { self.Stop(Fault::Command, static_cast<uint32_t>(address)); return; }
            self.dataMode_ = data;
            return;
        }
        if (address != self.Spi() || size != (self.profile_->wide ? 2 : 4) || !self.frame_) {
            self.Stop(Fault::Write, static_cast<uint32_t>(address));
            return;
        }
        // Native SPI is in 8-bit mode: word writes such as 0xffff transmit 0xff.
        ++self.frame_->spiBytes;
        self.DisplayByte(static_cast<uint8_t>(value));
    }
    void DisplayByte(uint8_t value) noexcept {
        if (!dataMode_) {
            if (highPending_ || (value != 0x2a && value != 0x2b && value != 0x2c)) {
                Stop(Fault::Command, value); return;
            }
            if ((command_ == 0x2a || command_ == 0x2b) && parameterCount_ != 4) {
                Stop(Fault::Window, command_); return;
            }
            command_ = value;
            parameterCount_ = 0;
            if (value == 0x2c) {
                if (windowMask_ != 3) { Stop(Fault::Window, value); return; }
                x_ = window_[0]; y_ = window_[2];
            }
            return;
        }
        if (command_ == 0x2a || command_ == 0x2b) {
            if (parameterCount_ == 4) { Stop(Fault::Window, command_); return; }
            parameters_[parameterCount_++] = value;
            if (parameterCount_ == 4) {
                const size_t index = command_ == 0x2a ? 0 : 2;
                const uint16_t first = uint16_t(parameters_[0]) << 8 | parameters_[1];
                const uint16_t last = uint16_t(parameters_[2]) << 8 | parameters_[3];
                if (first > 255 || last > 255) { Stop(Fault::Window, command_); return; }
                window_[index] = first; window_[index + 1] = last;
                windowMask_ |= static_cast<uint8_t>(1u << (index / 2));
            }
            return;
        }
        if (command_ != 0x2c) { Stop(Fault::Command, command_); return; }
        if (!highPending_) { high_ = value; highPending_ = true; return; }
        const uint16_t color = uint16_t(high_) << 8 | value;
        highPending_ = false;
        if (window_[0] > window_[1] || window_[2] > window_[3]) { Stop(Fault::Window, command_); return; }
        // Only the 302f panel has the native two-column / one-row offset.
        // 302e and 3035 use the same logical ABI but a different LCD origin.
        const int originX = profile_->code == 0x302f ? 26 : 24;
        const int originY = profile_->code == 0x302f ? 1 : 0;
        const int x = int(x_) - originX, y = int(y_) - originY;
        if (x >= 0 && x < Width && y >= 0 && y < Height) {
            const size_t physical = size_t(y) * Width + x;
            const size_t output = landscape_ ? size_t(Width - 1 - x) * Height + y : physical;
            frame_->rgba[output] = Rgba(color);
            if (!covered_[physical]) { covered_[physical] = true; ++coverage_; }
        }
        ++frame_->pixelWrites;
        if (++x_ > window_[1]) { x_ = window_[0]; if (++y_ > window_[3]) y_ = window_[2]; }
    }
    bool HardwareResult(bool result, std::string& error) const {
        if (fault_ == Fault::None) return result;
        const char* reason = "Invalid native Pro/Plus display stream";
        switch (fault_) {
        case Fault::Read: reason = "Unsupported native Pro/Plus hardware read"; break;
        case Fault::Write: reason = "Unsupported native Pro/Plus hardware write"; break;
        case Fault::Command: reason = "Unsupported native Pro/Plus LCD command"; break;
        case Fault::Window: reason = "Invalid native Pro/Plus LCD window"; break;
        default: break;
        }
        error = std::string(reason) + " at " + Hex(faultAddress_);
        return false;
    }
    bool Call(uint32_t address, const std::array<uint32_t, 4>& arguments, std::string& error) {
        return HardwareResult(runtime_.Call(address, profile_->stack, arguments, error), error);
    }
    enum class Presentation { Frame, Animation, PowerOff };
    bool Capture(uint32_t address, uint32_t end, Presentation presentation, std::string& error) {
        // Capture power-off at its native presentation boundary, before the
        // long sleep/fade-out. Startup executes the entire real reveal animation.
        // Neither replaces raster instructions nor bypasses hardware operations.
        uc_engine* engine = runtime_.Raw();
        const std::array<int, 4> regs = { UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3 };
        const uint32_t zero = 0, link = ReturnAddress | 1;
        uc_err status = UC_ERR_OK;
        for (const int reg : regs) if (status == UC_ERR_OK) status = uc_reg_write(engine, reg, &zero);
        if (status == UC_ERR_OK) status = uc_reg_write(engine, UC_ARM_REG_SP, &profile_->stack);
        if (status == UC_ERR_OK) status = uc_reg_write(engine, UC_ARM_REG_LR, &link);
        // A moved main page executes its native full-glass clear before the
        // original panels. Its finite SPI transaction volume is larger than a
        // stock incremental update, so use a frame budget rather than the
        // runtime's short utility-call wall limit. MMIO faults still stop at
        // once, and the separate instruction bound still rejects nonreturning
        // native control flow.
        const uint64_t timeout = presentation == Presentation::Animation ? 10000000 :
            presentation == Presentation::Frame ? 3000000 : 500000;
        const size_t instructions = presentation == Presentation::Animation ? 300000000 :
            presentation == Presentation::Frame ? 60000000 : 20000000;
        if (status == UC_ERR_OK) status = uc_emu_start(engine, address | 1, end, timeout, instructions);
        uint32_t pc = 0;
        if (status == UC_ERR_OK) status = uc_reg_read(engine, UC_ARM_REG_PC, &pc);
        bool result = status == UC_ERR_OK && pc == end;
        if (!result) error = std::string(status == UC_ERR_OK ? "Native Pro/Plus presentation exceeded its bound" : uc_strerror(status)) + " at " + Hex(pc);
        return HardwareResult(result, error);
    }
    bool Guard(const FirmwarePackage& package, const HardwareRecord& record, std::string& error) {
        const size_t index = static_cast<size_t>(profile_ - Profiles);
        if (stock_[index].empty()) {
            FirmwarePackage embedded;
            if (!LoadEmbeddedFirmware(embedded, error, profile_->model, profile_->edition)) return false;
            const auto* original = FindRecord(embedded, profile_->code);
            if (!original) { error = "Embedded Pro/Plus firmware has no native hardware " + Hex(profile_->code); return false; }
            const size_t recordIndex = static_cast<size_t>(original - embedded.records.data());
            stock_[index] = std::move(embedded.records[recordIndex].image);
        }
        const auto& original = stock_[index];
        if (record.linked_base != profile_->base || record.sp != profile_->stack ||
            record.image.size() != profile_->size || original.size() != record.image.size() ||
            record.reset != profile_->reset) {
            error = "Unsupported native Pro/Plus flash or RAM layout"; return false;
        }
        FirmwareModel model; FirmwareEdition edition;
        if (!DetectFirmwareProfile(package, model, edition) || model != profile_->model || edition != profile_->edition) {
            error = "Inconsistent native Pro/Plus firmware profile"; return false;
        }
        // Permit only catalog payloads and validated palette producers. Pin
        // every other ROM byte, including complete native control flow and MMIO
        // transports. The color encoder supplies its own exact producer ranges;
        // no guessed instruction widths or broad executable exclusions are used.
        std::vector<std::pair<size_t, size_t>> editable;
        for (const auto& asset : source_->catalog) {
            uint32_t address = asset.address;
            if (!asset.recordAddresses.empty()) {
                const auto found = std::find_if(asset.recordAddresses.begin(), asset.recordAddresses.end(),
                    [&](const auto& entry) { return entry.first == profile_->code; });
                if (found == asset.recordAddresses.end()) continue;
                address = found->second;
            } else if (asset.sourceCode && asset.sourceCode != profile_->code) continue;
            const size_t length = asset.byte_len();
            if (!length || address < profile_->base || length > original.size() ||
                address - profile_->base > original.size() - length) continue;
            editable.emplace_back(address - profile_->base, address - profile_->base + length);
        }
        std::vector<PaletteState> palette;
        Rgb8 background;
        if (!DetectFirmwareColors(package, palette, background)) {
            error = "Unsupported native Pro/Plus palette producers"; return false;
        }
        const size_t recordIndex = static_cast<size_t>(&record - package.records.data());
        // XOR 1, then XOR 3: targets differ from the native value by 1 and 2.
        for (uint16_t bit : {uint16_t(1), uint16_t(3)}) {
            for (auto& entry : palette)
                entry.current = Rgb8::FromRgb565(entry.current.ToRgb565() ^ bit);
            std::vector<CodePatch> patches;
            if (!BuildColorPatches(package, palette, background, patches, error)) return false;
            for (const auto& patch : patches) {
                if (recordIndex >= patch.locations.size()) continue;
                for (size_t offset : patch.locations[recordIndex]) {
                    if (offset > original.size() || patch.replacement.size() > original.size() - offset) {
                        error = "Invalid native Pro/Plus color producer range"; return false;
                    }
                    editable.emplace_back(offset, offset + patch.replacement.size());
                }
            }
        }
        // Two distinct changed target colors cover every producer, even when
        // some per-site colors already equal one target. These probes never
        // modify the package or the image executed by Unicorn.
        std::vector<FirmwareLayout::Element> layout;
        if (!FirmwareLayout::DiscoverProPlus(package, layout, error)) return false;
        std::sort(editable.begin(), editable.end());
        auto same = [&](size_t first, size_t last) {
            if (FirmwareLayout::MatchesCode(layout, profile_->code,
                profile_->base + static_cast<uint32_t>(first), original.data() + first,
                record.image.data() + first, last - first)) return true;
            error = "Unsupported native Pro/Plus ROM change at " + Hex(profile_->base + static_cast<uint32_t>(first));
            return false;
        };
        size_t offset = 0;
        for (const auto& [first, last] : editable) {
            if (first > offset && !same(offset, first)) return false;
            offset = (std::max)(offset, last);
        }
        if (!same(offset, original.size())) return false;
        return true;
    }
    bool Open(std::string& error) {
        const std::array<uint32_t, 4> pages = profile_->wide ?
            std::array<uint32_t, 4>{0x40003000,0x40010000,0x40013000,0x1ffff000} :
            std::array<uint32_t, 4>{0x40003000,0x48000000,0x1ffff000,0x0801f000};
        // The Factory S Pro's native defaults reader consults the physical
        // settings Flash page. A fresh device has erased settings, not invented
        // application bytes. It then executes its own validation/default logic.
        const auto mappedPages = profile_->model == FirmwareModel::DragSPro && profile_->edition == FirmwareEdition::Factory ?
            std::span<const uint32_t>(pages) : std::span<const uint32_t>(pages.data(), profile_->wide ? 4 : 3);
        if (!runtime_.Open(image_, profile_->base, RamBase, RamSize,
            profile_->wide ? UC_CPU_ARM_CORTEX_M3 : UC_CPU_ARM_CORTEX_M4, mappedPages, error)) return false;
        for (uint32_t page : mappedPages) {
            if (page == 0x0801f000) continue;
            uc_hook read = 0, write = 0;
            auto status = uc_hook_add(runtime_.Raw(), &read, UC_HOOK_MEM_READ,
                reinterpret_cast<void*>(HardwareRead), this, page, page + 4095);
            if (status == UC_ERR_OK) status = uc_hook_add(runtime_.Raw(), &write, UC_HOOK_MEM_WRITE,
                reinterpret_cast<void*>(HardwareWrite), this, page, page + 4095);
            if (status != UC_ERR_OK) { error = std::string("Observing native Pro/Plus hardware: ") + uc_strerror(status); return false; }
        }
        return trace_.Attach(runtime_.Raw(), *source_, profile_->code, profile_->base, image_.size(), error);
    }
    bool Prepare(const StudioState& source, const Profile& profile, std::string& error) {
        const auto* original = FindRecord(source.firmware, profile.code);
        if (!original) { error = "Pro/Plus firmware has no hardware " + Hex(profile.code); return false; }
        const bool same = cached_ && source_ == &source && sourceData_ == original->image.data() &&
            sourceSize_ == original->image.size() && sourceHash_ == source.firmware.source_sha256 &&
            revision_ == source.ResourceRevision() && profile_ == &profile;
        if (same) { error = cachedError_; return error.empty() && runtime_.Raw(); }
        runtime_.Close();
        cached_ = true; source_ = &source; sourceData_ = original->image.data();
        sourceSize_ = original->image.size(); sourceHash_ = source.firmware.source_sha256;
        revision_ = source.ResourceRevision(); profile_ = &profile; cachedError_.clear();
        std::vector<uint8_t> built;
        FirmwarePackage package;
        if (!source.BuildZwdfBytes(built, cachedError_) || !FirmwarePackage::Parse(built, package, cachedError_)) {
            error = cachedError_; return false;
        }
        const auto* record = FindRecord(package, profile.code);
        if (!record) cachedError_ = "Built Pro/Plus firmware lost its native hardware record";
        else if (Guard(package, *record, cachedError_)) {
            const size_t recordIndex = static_cast<size_t>(record - package.records.data());
            image_ = std::move(package.records[recordIndex].image);
            if (!Open(cachedError_)) runtime_.Close();
        }
        error = cachedError_;
        return error.empty() && runtime_.Raw();
    }
    bool State(const DeviceState& device, bool temperature, std::string& error) {
        const bool tc = profile_->edition == FirmwareEdition::Tc;
        const uint32_t s = profile_->state;
        const uint16_t watts = static_cast<uint16_t>(std::clamp(device.watts, 5, profile_->maximumWatts));
        const uint16_t resistance = static_cast<uint16_t>(std::clamp(device.resistanceMilliOhms, 0, 9990));
        const uint16_t duration = static_cast<uint16_t>(std::clamp(device.timeCentiseconds, 0, 999));
        const uint32_t puffs = static_cast<uint32_t>(std::clamp(device.puffs, 0, 99999));
        const uint32_t packedPuffs = puffs << 16 | puffs >> 16;
        const uint16_t material = static_cast<uint16_t>(std::clamp(device.temperatureMaterial, 0, 2));
        const uint16_t unit = device.fahrenheit ? 0 : 1;
        const uint16_t temp = static_cast<uint16_t>(std::clamp(device.temperature,
            device.fahrenheit ? 200 : 100, device.fahrenheit ? 600 : 315));
        const uint8_t battery = static_cast<uint8_t>(std::clamp(device.battery, 0, 5));
        const uint16_t unlocked = device.locked ? 0 : 1;
        if (!runtime_.Write(profile_->flags, uint32_t(8), error) ||
            !runtime_.Write(profile_->batteries, battery, error)) return false;
        if (profile_->code == 0x310a) {
            // The early M3 image has a word-sized TC settings bank; later
            // 313x images compact only that bank to halfwords.
            for (const auto& [offset, value] : std::array<std::pair<uint32_t, uint32_t>, 12>{{
                {8, device.rba ? 1u : 0u}, {0x14, watts}, {0x18, watts},
                {0x38, uint32_t(watts) * 10}, {0x3c, material}, {0x40, unit}, {0x44, temp},
                {0x48, temperature ? 2u : landscape_ ? 1u : 0u}, {0x50, resistance},
                {0x54, unlocked}, {0x58, duration}, {0x68, puffs}}})
                if (!runtime_.Write(s + offset, value, error)) return false;
            return true;
        }
        if (profile_->wide) {
            for (const auto& [offset, value] : std::array<std::pair<uint32_t, uint32_t>, 8>{{
                {8, device.rba ? 1u : 0u}, {0x14, watts}, {0x18, watts},
                {0x3c, temperature ? 2u : landscape_ ? 1u : 0u}, {0x44, resistance},
                {0x48, unlocked}, {0x4c, duration}, {0x5c, puffs}}})
                if (!runtime_.Write(s + offset, value, error)) return false;
            return runtime_.Write(s + 0x32, uint16_t(watts * 10), error) &&
                runtime_.Write(s + 0x34, material, error) && runtime_.Write(s + 0x36, unit, error) &&
                runtime_.Write(s + 0x38, temp, error);
        }
        if (profile_->plus) {
            if (!runtime_.Write(s + 4, uint16_t(device.rba ? 1 : 0), error) ||
                !runtime_.Write(s + 0x0a, watts, error) || !runtime_.Write(s + 0x0c, watts, error) ||
                !runtime_.Write(s + (tc ? 0x24 : 0x16), uint16_t(temperature ? 2 : landscape_ ? 1 : 0), error) ||
                !runtime_.Write(s + (tc ? 0x28 : 0x1a), resistance, error) ||
                !runtime_.Write(s + (tc ? 0x2a : 0x1c), unlocked, error) ||
                !runtime_.Write(s + (tc ? 0x2c : 0x1e), duration, error) ||
                !runtime_.Write(s + (tc ? 0x34 : 0x26), packedPuffs, error)) return false;
            return !tc || (runtime_.Write(s + 0x1c, uint16_t(watts * 10), error) &&
                runtime_.Write(s + 0x1e, material, error) && runtime_.Write(s + 0x20, unit, error) &&
                runtime_.Write(s + 0x22, temp, error));
        }
        if (!runtime_.Write(s + 4, uint16_t(temperature ? 2 : device.rba ? 1 : 0), error) ||
            !runtime_.Write(s + 0x0c, watts, error) || !runtime_.Write(s + 0x0e, watts, error) ||
            !runtime_.Write(s + (tc ? 0x28 : 0x1a), resistance, error) ||
            !runtime_.Write(s + (tc ? 0x2a : 0x1c), unlocked, error) ||
            !runtime_.Write(s + (tc ? 0x2c : 0x1e), duration, error) ||
            !runtime_.Write(s + (tc ? 0x36 : 0x28), packedPuffs, error)) return false;
        return !tc || (runtime_.Write(s + 0x1e, uint16_t(watts * 10), error) &&
            runtime_.Write(s + 0x20, material, error) && runtime_.Write(s + 0x22, unit, error) &&
            runtime_.Write(s + 0x24, temp, error));
    }
    bool Draw(const DeviceState& device, Frame& output, std::string& error) {
        const bool temperature = profile_->tcMain &&
            (device.temperatureControl || device.screen == TcMaterial || device.screen == TcSettings);
        landscape_ = device.screen == Main && profile_->coreMain && device.landscape && !temperature;
        output.width = landscape_ ? Height : Width;
        output.height = landscape_ ? Width : Height;
        output.monochrome = output.emulatedUid = false;
        output.rgba.assign(Pixels, 0xff000000u);
        output.spiBytes = output.pixelWrites = 0;
        output.resources.clear(); trace_.Reset();
        frame_ = &output;
        fault_ = Fault::None; faultAddress_ = 0; coverage_ = 0; covered_.fill(false);
        dataMode_ = highPending_ = false; command_ = parameterCount_ = windowMask_ = 0;
        window_.fill(0);
        if (!runtime_.Reset(error) || !runtime_.Write(Spi() - 4, uint32_t(2), error) ||
            !runtime_.Write(Gpio() + 0x10, uint32_t(device.charging ? 0x80 : 0), error)) return false;
        // A service screen explicitly identifies its UID as emulated. Its text,
        // checksum, chip-ID digits and level code are all computed by the ROM.
        static constexpr std::array<uint8_t, 12> uid = {'S','T','U','D','I','O','0','0','0','0','0','1'};
        if (!runtime_.Write(Uid(), uid.data(), uid.size(), error)) return false;
        if (profile_->model == FirmwareModel::DragSPro && profile_->edition == FirmwareEdition::Factory) {
            std::array<uint8_t, 4096> erased;
            erased.fill(0xff);
            if (!runtime_.Write(0x0801f000, erased.data(), erased.size(), error)) return false;
        }
        for (uint32_t offset : {0u, 16u}) {
            const auto* entry = image_.data() + profile_->scatter - profile_->base + offset;
            if (!Call(Le32(entry + 12), {Le32(entry), Le32(entry + 4), Le32(entry + 8), 0}, error)) return false;
        }
        if (!Call(profile_->defaults, {}, error) || (profile_->heap && !Call(profile_->heap, {}, error)) ||
            !State(device, temperature, error)) return false;
        // Cold glass is initialized through the real native solid rasterizer.
        if (!runtime_.Write(profile_->stack, uint32_t(0), error) ||
            !Call(profile_->fill, {0,0,Width,Height}, error)) return false;
        const uint32_t address = profile_->routes[device.screen];
        const uint32_t selection = static_cast<uint32_t>(std::clamp(device.menuSelection, 0,
            device.screen == Switch || device.screen == Theme ? 1 : device.screen == Mode && !profile_->tcMain ? 1 : 2));
        bool result;
        switch (device.screen) {
        case Main: result = Capture(temperature ? profile_->tcMain : landscape_ ? profile_->coreMain : address,
            ReturnAddress, Presentation::Frame, error); break;
        case VoopooStartup: result = Capture(address, ReturnAddress, Presentation::Animation, error); break;
        case PowerOff: result = Capture(address, profile_->offCapture, Presentation::PowerOff, error); break;
        case Charging: {
            const uint32_t battery = static_cast<uint32_t>(std::clamp(device.battery, 0, 5));
            result = Call(address, {battery,battery * 20,1,0}, error); break;
        }
        case Settings: case Switch: case Mode:
            result = Call(address, {selection,device.menuConfirmed ? 1u : 0u,1,0}, error); break;
        case Theme: case TcMaterial: case TcSettings: result = Call(address, {selection,1,0,0}, error); break;
        default: result = Call(address, {}, error); break;
        }
        if (!result) return false;
        if (highPending_ || coverage_ != Pixels) {
            error = "Native Pro/Plus renderer did not produce a complete 80 x 160 LCD frame"; return false;
        }
        output.hardwareCode = profile_->code;
        output.emulatedUid = device.screen == Information;
        trace_.Collect(output);
        return true;
    }
public:
    bool Render(const StudioState& source, const DeviceState& device, Frame& output) override {
        output.error.clear();
        const auto* profile = FindProfile(source.model, source.edition, device.hardwareCode);
        if (!profile || device.screen >= ScreenCount || !profile->routes[device.screen])
            output.error = "Unsupported native Pro/Plus screen or firmware edition";
        else if (Prepare(source, *profile, output.error) && Draw(device, output, output.error)) {
            frame_ = nullptr;
            return true;
        }
        frame_ = nullptr;
        output.width = output.height = 0;
        output.monochrome = output.emulatedUid = false;
        output.rgba.clear(); output.resources.clear();
        output.spiBytes = output.pixelWrites = 0;
        output.hardwareCode = 0;
        return false;
    }
};
} // namespace

std::unique_ptr<Backend> CreateProPlusBackend() { return std::make_unique<ProPlusBackend>(); }

std::span<const ScreenInfo> ProPlusScreens(FirmwareModel model, FirmwareEdition edition, uint16_t hardwareCode) {
    static constexpr ScreenInfo proFactory[] = {
        {Main,"Main screen"},{Startup,"Device logo (boot)"},{VoopooStartup,"Logo VOOPOO (boot)"},
        {PowerOff,"OFF"},{Charging,"Charging"},{LowResistance,"Low Resistance"},
        {AtomizerShort,"Atomizer Short"},{CheckAtomizer,"Check Atomizer"},{CheckBattery,"Check Battery"},
        {LowPower,"Low Power"},{MaxPower,"Max Power"},{PuffCleared,"Puff Cleared"},
        {HighTemperature,"High Temp"},{TimeOut,"Time Out"},{Locked,"Locked"},{Unlocked,"Unlocked"},
        {Settings,"Menu SETTING",3,false,true},{Switch,"Menu SWITCH",2,false,true},
        {Mode,"Menu MODE",2,false,true},{Information,"Chip ID panel"}
    };
    static constexpr ScreenInfo proTc[] = {
        {Main,"Main screen",0,true},{Startup,"Device logo (boot)"},{VoopooStartup,"Logo VOOPOO (boot)"},
        {PowerOff,"OFF"},{Charging,"Charging"},{LowResistance,"Low Resistance"},
        {AtomizerShort,"Atomizer Short"},{CheckAtomizer,"Check Atomizer"},{CheckBattery,"Check Battery"},
        {LowPower,"Low Power"},{MaxPower,"Max Power"},{PuffCleared,"Puff Cleared"},
        {HighTemperature,"High Temp"},{TimeOut,"Time Out"},{Locked,"Locked"},{Unlocked,"Unlocked"},
        {Settings,"Menu SETTING",3,false,true},{Switch,"Menu SWITCH",2,false,true},
        {Mode,"Menu MODE",3,false,true},{Information,"Chip ID panel"},
        {TcMaterial,"TC material menu",3,false,false,1,0},{TcSettings,"TC settings",3,true,false,1,0}
    };
    static constexpr ScreenInfo plusFactory[] = {
        {Main,"Main screen"},{Startup,"Device logo (boot)"},{VoopooStartup,"Logo VOOPOO (boot)"},
        {PowerOff,"OFF"},{Charging,"Charging"},{LowResistance,"Low Resistance"},
        {AtomizerShort,"Atomizer Short"},{CheckAtomizer,"Check Atomizer"},{CheckBattery,"Check Battery"},
        {LowPower,"Low Power"},{MaxPower,"Max Power"},{PuffCleared,"Puff Cleared"},
        {HighTemperature,"High Temp"},{TimeOut,"Time Out"},{Locked,"Locked"},{Unlocked,"Unlocked"},
        {Theme,"Theme menu",2},{Information,"Chip ID panel"}
    };
    static constexpr ScreenInfo plusTc[] = {
        {Main,"Main screen",0,true},{Startup,"Device logo (boot)"},{VoopooStartup,"Logo VOOPOO (boot)"},
        {PowerOff,"OFF"},{Charging,"Charging"},{LowResistance,"Low Resistance"},
        {AtomizerShort,"Atomizer Short"},{CheckAtomizer,"Check Atomizer"},{CheckBattery,"Check Battery"},
        {LowPower,"Low Power"},{MaxPower,"Max Power"},{PuffCleared,"Puff Cleared"},
        {HighTemperature,"High Temp"},{TimeOut,"Time Out"},{Locked,"Locked"},{Unlocked,"Unlocked"},
        {Theme,"Theme menu",2},{Information,"Chip ID panel"},{TcMaterial,"TC material menu",3,false,false,1,0},
        {TcSettings,"TC settings",3,true,false,1,0}
    };
    static constexpr auto plusTcWithoutVoopoo = [] {
        std::array<ScreenInfo, std::size(plusTc) - 1> result{};
        size_t index = 0;
        for (const auto& page : plusTc)
            if (page.id != VoopooStartup) result[index++] = page;
        return result;
    }();
    const auto* profile = FindProfile(model, edition, hardwareCode);
    if (!profile) return {};
    if (profile->plus) {
        if (edition != FirmwareEdition::Tc) return plusFactory;
        return profile->routes[VoopooStartup] ? std::span<const ScreenInfo>(plusTc) :
            std::span<const ScreenInfo>(plusTcWithoutVoopoo);
    }
    return edition == FirmwareEdition::Tc ? std::span<const ScreenInfo>(proTc) : std::span<const ScreenInfo>(proFactory);
}
} // namespace FirmwareEmulator
