#include "firmware_emulator_backend.h"
#include "firmware_emulator_runtime.h"
#include "firmware_emulator_screens.h"
#include "app_state.h"
#include "firmware_layout.h"
#include "firmware_three_max_profiles.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>

namespace FirmwareEmulator {
namespace {
constexpr uint32_t RamBase = 0x20000000;
constexpr size_t RamSize = 0x5000;
constexpr size_t PixelCount = 128 * 220;
constexpr int MinimumWatts = 5;
constexpr int MaximumWatts = 177;
using Profile = ThreeMaxNative::Profile;
constexpr auto CommonScreens = [] {
    std::array<ScreenInfo, 16> result{{
        {0, "Main"}, {1, "Theme menu", 2}, {2, "Device logo (boot)"}, {3, "OFF"},
        {4, "Charging"}, {5, "Atomizer Short"}, {6, "Check Atomizer"},
        {7, "Check Battery"}, {8, "Low Power"}, {9, "Max Power"},
        {10, "Puff Cleared"}, {11, "High Temp"}, {12, "Time Out"},
        {13, "Locked"}, {14, "Unlocked"}, {15, "Chip ID panel"}
    }};
    for (auto& screen : result) if (screen.id) screen.orientation = 0;
    return result;
}();
constexpr auto FactoryScreens = [] {
    std::array<ScreenInfo, 18> result{};
    std::copy(std::begin(CommonScreens), std::end(CommonScreens), result.begin());
    result[16] = {16, "Logo VOOPOO (animated boot)"};
    result[17] = {17, "Blank screen"};
    result[16].orientation = result[17].orientation = 0;
    return result;
}();
constexpr auto TcScreens = [] {
    std::array<ScreenInfo, 20> result{};
    std::copy(FactoryScreens.begin(), FactoryScreens.end(), result.begin());
    result[0].temperatureMaterial = true;
    result[18] = {18, "TC material menu", 0, true};
    result[18].temperatureMode = 1;
    result[18].orientation = 0;
    result[19] = {19, "TC settings", 3, true};
    result[19].temperatureMode = 1;
    result[19].orientation = 0;
    return result;
}();
constexpr auto MaxScreens = [] {
    std::array<ScreenInfo, 17> result{};
    std::copy(std::begin(CommonScreens), std::end(CommonScreens), result.begin());
    result[16] = {17, "Blank screen"};
    result[16].orientation = 0;
    return result;
}();

uint32_t Read32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
const HardwareRecord* Canonical(const FirmwarePackage& package, uint16_t code) {
    for (const auto& record : package.records)
        if ((uint16_t(record.device_code[0]) << 8 | record.device_code[1]) == code) return &record;
    return nullptr;
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
std::span<const ScreenInfo> ScreensFor(FirmwareModel model, FirmwareEdition edition) {
    if (model == FirmwareModel::DragMax && edition == FirmwareEdition::Standard) return MaxScreens;
    if (model == FirmwareModel::Drag3) {
        if (edition == FirmwareEdition::Factory) return FactoryScreens;
        if (edition == FirmwareEdition::Tc) return TcScreens;
    }
    return {};
}

class Drag3Backend final : public Backend {
    ArmRuntime runtime_;
    ResourceReadTrace trace_;
    const StudioState* source_ = nullptr;
    const uint8_t* sourceData_ = nullptr;
    size_t sourceSize_ = 0;
    uint64_t revision_ = 0;
    std::string sourceHash_, cachedError_;
    FirmwareEdition edition_ = FirmwareEdition::Factory;
    FirmwareModel model_ = FirmwareModel::Drag3;
    const Profile* profile_ = nullptr;
    bool cached_ = false;
    std::vector<uint8_t> image_;
    Frame* frame_ = nullptr;
    std::array<bool, PixelCount> covered_{};
    size_t coverage_ = 0;
    bool landscape_ = false, dataMode_ = false, highPending_ = false;
    uint8_t high_ = 0;
    uint16_t command_ = 0;
    std::array<uint16_t, 4> window_{};
    uint8_t windowWords_ = 0;
    uint8_t windowMask_ = 0;
    uint16_t x_ = 0, y_ = 0;
    enum class Fault { None, Read, Write, Command, Window };
    Fault fault_ = Fault::None;
    uint32_t faultAddress_ = 0;
    uint32_t boundaryAddress_ = 0;
    bool boundaryReached_ = false;

    void Stop(Fault fault, uint32_t address) noexcept {
        if (fault_ == Fault::None) { fault_ = fault; faultAddress_ = address; }
        uc_emu_stop(runtime_.Raw());
    }
    static void ReadHardware(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t, void* user) {
        auto& self = *static_cast<Drag3Backend*>(user);
        if ((size == (self.profile_->WideSettings() ? 2 : 4) && address == self.profile_->Spi() + 8) ||
            (size == 4 && address == self.profile_->Gpio() + 0x10)) return;
        self.Stop(Fault::Read, static_cast<uint32_t>(address));
    }
    static void ReadIdentity(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t, void* user) {
        auto& self = *static_cast<Drag3Backend*>(user);
        if (size <= 0 || address < self.profile_->Identity() || address + size > uint64_t(self.profile_->Identity()) + 12)
            self.Stop(Fault::Read, static_cast<uint32_t>(address));
    }
    static void PresentationBoundary(uc_engine* engine, uint64_t address, uint32_t, void* user) {
        auto& self = *static_cast<Drag3Backend*>(user);
        if (address == self.boundaryAddress_) {
            self.boundaryReached_ = true;
            uc_emu_stop(engine);
        }
    }
    static void WriteHardware(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t value, void* user) {
        auto& self = *static_cast<Drag3Backend*>(user);
        if (size == 4 && value == 0x100 &&
            (address == self.profile_->Gpio() + 0x18 || address == self.profile_->Gpio() + 0x28)) {
            const bool data = address == self.profile_->Gpio() + 0x18;
            if (self.highPending_ && data != self.dataMode_) { self.Stop(Fault::Command, static_cast<uint32_t>(address)); return; }
            self.dataMode_ = data;
            return;
        }
        if (address == 0x40003000 && size == 4 && value == 0xaaaa) return;
        if (address != self.profile_->Spi() + 0x0c || size != (self.profile_->WideSettings() ? 2 : 4) || !self.frame_) {
            self.Stop(Fault::Write, static_cast<uint32_t>(address));
            return;
        }
        ++self.frame_->spiBytes;
        self.DisplayByte(static_cast<uint8_t>(value));
    }
    void DisplayByte(uint8_t value) noexcept {
        if (profile_->ByteCommands() && !dataMode_) {
            if (highPending_ || (value != 0x2a && value != 0x2b && value != 0x2c)) {
                Stop(Fault::Command, value);
                return;
            }
            command_ = value;
            windowWords_ = 0;
            if (value == 0x2c) {
                if (windowMask_ != 15) { Stop(Fault::Window, value); return; }
                x_ = window_[0]; y_ = window_[2];
            }
            return;
        }
        if (!highPending_) { high_ = value; highPending_ = true; return; }
        const uint16_t word = uint16_t(high_) << 8 | value;
        highPending_ = false;
        if (profile_->ByteCommands() && command_ != 0x2c) {
            if ((command_ != 0x2a && command_ != 0x2b) || windowWords_ >= 2 || word > 255) {
                Stop(Fault::Window, command_); return;
            }
            const size_t index = (command_ == 0x2a ? 0 : 2) + windowWords_++;
            window_[index] = word;
            windowMask_ |= uint8_t(1u << index);
            return;
        }
        if (!profile_->ByteCommands() && !dataMode_) {
            if (word != 0x37 && word != 0x36 && word != 0x39 && word != 0x38 && word != 0x22) {
                Stop(Fault::Command, word);
                return;
            }
            command_ = word;
            if (word == 0x22) {
                if (windowMask_ != 15) {
                    Stop(Fault::Window, word);
                    return;
                }
                x_ = window_[0]; y_ = window_[2];
            }
            return;
        }
        if (!profile_->ByteCommands() && command_ != 0x22) {
            size_t index;
            switch (command_) {
            case 0x37: index = 0; break;
            case 0x36: index = 1; break;
            case 0x39: index = 2; break;
            case 0x38: index = 3; break;
            default: Stop(Fault::Command, command_); return;
            }
            // This controller has eight-bit GRAM address registers. Native
            // landscape glyphs also clear a padding row (220) outside the glass.
            if (word > 255) { Stop(Fault::Window, word); return; }
            window_[index] = word;
            windowMask_ |= static_cast<uint8_t>(1u << index);
            return;
        }
        // The native TC graph emits zero-height rectangles: it programs an
        // inverted window and RAM-write command, then sends no pixel data.
        // Address setup alone is legal; reject an actual write to that window.
        if (window_[0] > window_[1] || window_[2] > window_[3]) {
            Stop(Fault::Window, command_);
            return;
        }
        const int origin = profile_->ByteCommands() ? 16 : 24;
        if (x_ >= origin && x_ < origin + profile_->width && y_ < profile_->height) {
            const size_t physical = size_t(y_) * profile_->width + x_ - origin;
            const size_t pixel = landscape_ ? size_t(profile_->width - 1 - (x_ - origin)) * profile_->height + y_ : physical;
            frame_->rgba[pixel] = Rgba(word);
            if (!covered_[physical]) { covered_[physical] = true; ++coverage_; }
        }
        ++frame_->pixelWrites;
        if (++x_ > window_[1]) { x_ = window_[0]; if (++y_ > window_[3]) y_ = window_[2]; }
    }
    bool Call(uint32_t address, const std::array<uint32_t, 4>& args, std::string& error) {
        const bool result = runtime_.Call(address, profile_->stack, args, error);
        if (fault_ == Fault::None) return result;
        const char* reason = "Invalid LCD pixel stream";
        switch (fault_) {
        case Fault::Read: reason = "Unsupported DRAG display hardware read"; break;
        case Fault::Write: reason = "Unsupported DRAG display hardware write"; break;
        case Fault::Command: reason = "Unsupported DRAG LCD command"; break;
        case Fault::Window: reason = "Invalid DRAG LCD address window"; break;
        default: break;
        }
        error = std::string(reason) + " at " + Hex(faultAddress_);
        return false;
    }
    bool CallUntil(uint32_t address, uint32_t boundary, std::string& error) {
        // Snapshot an explicit native presentation boundary, not an instruction
        // timeout or unsupported peripheral. No firmware instructions are patched.
        boundaryAddress_ = boundary;
        boundaryReached_ = false;
        Call(address, {}, error);
        boundaryAddress_ = 0;
        uint32_t pc = 0;
        const auto result = uc_reg_read(runtime_.Raw(), UC_ARM_REG_PC, &pc);
        if (result == UC_ERR_OK && boundaryReached_ && pc == boundary && fault_ == Fault::None) {
            error.clear();
            return true;
        }
        if (error.empty()) error = "Native DRAG display did not reach its presentation boundary";
        return false;
    }
    bool BootLogo(std::string& error) {
        if (!CallUntil(profile_->screens[16], profile_->animation, error)) return false;
        std::array<uint32_t, 4> args{};
        const std::array<int, 4> registers{UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3};
        uint32_t stack = 0;
        uc_err result = uc_reg_read(runtime_.Raw(), UC_ARM_REG_SP, &stack);
        for (size_t i = 0; i < args.size() && result == UC_ERR_OK; ++i)
            result = uc_reg_read(runtime_.Raw(), registers[i], &args[i]);
        std::array<uint32_t, 3> colorsAndResource{};
        if (result == UC_ERR_OK)
            result = uc_mem_read(runtime_.Raw(), stack, colorsAndResource.data(), sizeof(colorsAndResource));
        if (result != UC_ERR_OK) {
            error = std::string("Reading native boot presentation arguments: ") + uc_strerror(result);
            return false;
        }
        // The caller supplies geometry, colors and resource pointer. Render its
        // completed visible phase with the firmware's static mask rasterizer;
        // the slow center-out animation and its delay loops are not this frame.
        return runtime_.Write(profile_->stack, colorsAndResource.data(), sizeof(colorsAndResource), error) &&
            Call(profile_->mask, args, error);
    }
    bool Guard(const FirmwarePackage& package, const HardwareRecord& record, std::string& error) {
        if (record.linked_base != profile_->base || record.sp != profile_->stack || record.image.size() != profile_->size) {
            error = "Unsupported DRAG record-local flash or RAM layout";
            return false;
        }
        std::vector<FirmwareLayout::Element> elements;
        return FirmwareLayout::DiscoverThreeMax(package, elements, error);
    }
    bool Open(std::string& error) {
        std::array<uint32_t, 3> peripherals = {0x40003000, profile_->Spi() & ~4095u, profile_->Gpio() & ~4095u};
        std::sort(peripherals.begin(), peripherals.end());
        const auto last = std::unique(peripherals.begin(), peripherals.end());
        const std::span<const uint32_t> pages(peripherals.data(), size_t(last - peripherals.begin()));
        if (!runtime_.Open(image_, profile_->base, RamBase, RamSize, UC_CPU_ARM_CORTEX_M4, pages, error)) return false;
        for (uint32_t page : pages) {
            uc_hook read = 0, write = 0;
            // Never hook SRAM: Unicorn 2.1.4 memory hooks perturb Thumb IT state.
            uc_err result = uc_hook_add(runtime_.Raw(), &read, UC_HOOK_MEM_READ,
                reinterpret_cast<void*>(ReadHardware), this, page, page + 4095);
            if (result == UC_ERR_OK) result = uc_hook_add(runtime_.Raw(), &write, UC_HOOK_MEM_WRITE,
                reinterpret_cast<void*>(WriteHardware), this, page, page + 4095);
            if (result != UC_ERR_OK) { error = std::string("Observing DRAG 3 LCD hardware: ") + uc_strerror(result); return false; }
        }
        // The chip-ID screen is explicitly labeled as emulated identity in the
        // frame contract. It executes the ROM's own ID/level-code formatting.
        constexpr std::array<uint8_t, 12> identity = {
            0xcf, 0xe2, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe, 0x01, 0x23, 0x45, 0x67
        };
        uc_err result = uc_mem_map(runtime_.Raw(), 0x1ffff000, 4096, UC_PROT_READ);
        if (result == UC_ERR_OK && !runtime_.Write(profile_->Identity(), identity.data(), identity.size(), error)) return false;
        uc_hook identityRead = 0;
        if (result == UC_ERR_OK) result = uc_hook_add(runtime_.Raw(), &identityRead, UC_HOOK_MEM_READ,
            reinterpret_cast<void*>(ReadIdentity), this, 0x1ffff000, 0x1fffffff);
        for (uint32_t address : {profile_->animation, profile_->delay}) {
            uc_hook boundary = 0;
            if (result == UC_ERR_OK) result = uc_hook_add(runtime_.Raw(), &boundary, UC_HOOK_CODE,
                reinterpret_cast<void*>(PresentationBoundary), this, address, address);
        }
        if (result != UC_ERR_OK) { error = std::string("Mapping DRAG display identity/boundaries: ") + uc_strerror(result); return false; }
        return trace_.Attach(runtime_.Raw(), *source_, profile_->code, profile_->base, image_.size(), error);
    }
    bool Prepare(const StudioState& state, uint16_t requestedCode, std::string& error) {
        const auto* selected = ThreeMaxNative::Find(state.model, state.edition, requestedCode);
        if (!selected) {
            error = "Unknown DRAG 3 / MAX hardware record or firmware edition";
            return false;
        }
        const auto* original = Canonical(state.firmware, selected->code);
        if (!original) { error = "Requested DRAG hardware record is absent"; return false; }
        const bool same = cached_ && source_ == &state && sourceData_ == original->image.data() &&
            sourceSize_ == original->image.size() && sourceHash_ == state.firmware.source_sha256 &&
            revision_ == state.ResourceRevision() && edition_ == state.edition && model_ == state.model && profile_ == selected;
        if (same) { error = cachedError_; return error.empty() && runtime_.Raw(); }
        runtime_.Close();
        cached_ = true;
        source_ = &state; sourceData_ = original->image.data(); sourceSize_ = original->image.size();
        sourceHash_ = state.firmware.source_sha256; revision_ = state.ResourceRevision(); edition_ = state.edition;
        model_ = state.model;
        profile_ = selected;
        cachedError_.clear();
        std::vector<uint8_t> built;
        FirmwarePackage package;
        if (!state.BuildZwdfBytes(built, cachedError_) || !FirmwarePackage::Parse(built, package, cachedError_)) {
            error = cachedError_;
            return false;
        }
        const auto* record = Canonical(package, profile_->code);
        if (!record) cachedError_ = "Built DRAG firmware has no requested hardware record";
        else if (Guard(package, *record, cachedError_)) {
            image_ = std::move(package.records[size_t(record - package.records.data())].image);
            if (!Open(cachedError_)) runtime_.Close();
        }
        error = cachedError_;
        return error.empty() && runtime_.Raw();
    }
    bool DrawScreen(const DeviceState& device, std::string& error) {
        switch (device.screen) {
        case 1:
            return Call(profile_->screens[1], {static_cast<uint32_t>(std::clamp(device.menuSelection, 0, 1)), 1, 0, 0}, error);
        case 3:
            // Preserve the native OFF frame before its five-second wait and
            // subsequent erase animation. The wait itself is not emulated away.
            return CallUntil(profile_->screens[3], profile_->delay, error);
        case 4: {
            const uint32_t battery = static_cast<uint32_t>(std::clamp(device.battery, 0, 5));
            return runtime_.Write(profile_->stack, uint32_t(1), error) &&
                Call(profile_->screens[4], {battery, battery * 20, battery, battery * 20}, error);
        }
        case 16: return BootLogo(error);
        case 17: return true; // Native GRAM clear in Draw is the powered-off screen.
        case 18:
            return Call(profile_->screens[18], {static_cast<uint32_t>(std::clamp(device.temperatureMaterial, 0, 2)), 1, 0, 0}, error);
        case 19:
            return Call(profile_->screens[19], {static_cast<uint32_t>(std::clamp(device.menuSelection, 0, 2)), 1, 0, 0}, error);
        default: return Call(profile_->screens[device.screen], {}, error);
        }
    }
    bool Draw(const DeviceState& device, Frame& output, std::string& error) {
        const auto screens = ScreensFor(model_, edition_);
        if (std::none_of(screens.begin(), screens.end(),
                [&](const ScreenInfo& screen) { return screen.id == device.screen; })) {
            error = "Unknown native DRAG 3 / MAX screen";
            return false;
        }
        const bool tc = edition_ == FirmwareEdition::Tc;
        const bool temperature = tc && device.temperatureControl;
        landscape_ = device.screen == 0 && device.landscape && !temperature;
        output.width = landscape_ ? profile_->height : profile_->width;
        output.height = landscape_ ? profile_->width : profile_->height;
        output.monochrome = false;
        output.emulatedUid = false;
        trace_.Reset();
        output.resources.clear();
        output.rgba.assign(size_t(profile_->width) * profile_->height, 0xff000000u);
        output.spiBytes = output.pixelWrites = 0;
        frame_ = &output;
        fault_ = Fault::None; faultAddress_ = 0;
        dataMode_ = highPending_ = false; command_ = 0; windowMask_ = 0;
        window_.fill(0); covered_.fill(false); coverage_ = 0;
        if (!runtime_.Reset(error) || !runtime_.Write(profile_->Spi() + 8, uint32_t(2), error) ||
            !runtime_.Write(profile_->Gpio() + 0x10, uint32_t(device.charging ? 0x80 : 0), error)) return false;
        // Execute the firmware's own compressed-data and zero-init routines.
        for (uint32_t offset : { 0u, 16u }) {
            const uint8_t* entry = image_.data() + profile_->scatter - profile_->base + offset;
            if (!Call(Read32(entry + 12), { Read32(entry), Read32(entry + 4), Read32(entry + 8), 0 }, error)) return false;
        }
        if (!Call(profile_->defaults, {}, error)) return false;
        const uint32_t settings = profile_->settings;
        const uint16_t watts = static_cast<uint16_t>(std::clamp(device.watts, MinimumWatts,
            temperature ? 80 : MaximumWatts));
        const uint16_t resistance = static_cast<uint16_t>(std::clamp(device.resistanceMilliOhms, 0, 9990));
        const uint16_t duration = static_cast<uint16_t>(std::clamp(device.timeCentiseconds, 0, 999));
        const uint32_t puffs = static_cast<uint32_t>(std::clamp(device.puffs, 0, 99999));
        // The packed puff counter is stored with its two halfwords swapped.
        const uint32_t packedPuffs = puffs << 16 | puffs >> 16;
        const uint8_t battery = static_cast<uint8_t>(std::clamp(device.battery, 0, 5));
        const std::array<uint8_t, 4> batteries = { battery, 0, battery, 0 };
        if (!runtime_.Write(profile_->Flags(), uint32_t(8), error) ||
            !runtime_.Write(profile_->Batteries(), batteries.data(), batteries.size(), error)) return false;
        const bool wide = profile_->WideSettings();
        const auto setting = [&](uint32_t narrow, uint32_t wideOffset, uint32_t value) {
            return wide ? runtime_.Write(settings + wideOffset, value, error) :
                runtime_.Write(settings + narrow, uint16_t(value), error);
        };
        if (!setting(4, 8, device.rba ? 1 : 0) ||
            !(wide ? runtime_.Write(settings + 0x10, puffs, error) : runtime_.Write(settings + 8, packedPuffs, error)) ||
            !setting(0x0e, 0x18, watts) || !setting(0x10, 0x1c, watts) ||
            !setting(tc ? 0x2c : 0x1e, tc ? 0x54 : 0x38, resistance) ||
            !setting(tc ? 0x2e : 0x20, tc ? 0x58 : 0x3c, device.locked ? 0 : 1) ||
            !setting(tc ? 0x30 : 0x22, tc ? 0x5c : 0x40, duration) ||
            !setting(tc ? 0x28 : 0x1a, tc ? 0x4c : 0x30, temperature ? 2 : landscape_ ? 1 : 0)) return false;
        if (tc) {
            const uint16_t value = static_cast<uint16_t>(std::clamp(device.temperature,
                device.fahrenheit ? 200 : 100, device.fahrenheit ? 600 : 315));
            if (!setting(0x20, 0x3c, std::min<int>(watts, 80) * 10) ||
                !setting(0x22, 0x40, std::clamp(device.temperatureMaterial, 0, 2)) ||
                !setting(0x24, 0x44, device.fahrenheit ? 0 : 1) ||
                !setting(0x26, 0x48, value)) return false;
        }
        // Cold LCD GRAM is cleared through the actual native solid rasterizer;
        // its fifth AAPCS argument lives at the incoming stack pointer.
        if (!runtime_.Write(profile_->stack, uint32_t(0), error) ||
            !Call(profile_->fill, { 0, 0, uint32_t(profile_->width), uint32_t(profile_->height) }, error) ||
            !DrawScreen(device, error)) return false;
        if (highPending_ || coverage_ != size_t(profile_->width) * profile_->height) {
            error = "Native DRAG renderer did not produce a complete record-local LCD frame";
            return false;
        }
        output.hardwareCode = profile_->code;
        output.emulatedUid = device.screen == 15;
        trace_.Collect(output);
        return true;
    }
public:
    bool Render(const StudioState& source, const DeviceState& device, Frame& output) override {
        output.error.clear();
        if (Prepare(source, device.hardwareCode, output.error) && Draw(device, output, output.error)) {
            frame_ = nullptr;
            return true;
        }
        frame_ = nullptr;
        output.rgba.clear();
        output.width = output.height = 0;
        output.spiBytes = output.pixelWrites = 0;
        output.resources.clear();
        output.hardwareCode = 0;
        output.monochrome = output.emulatedUid = false;
        return false;
    }
};
} // namespace

std::unique_ptr<Backend> CreateDrag3Backend() { return std::make_unique<Drag3Backend>(); }
std::span<const ScreenInfo> ThreeMaxScreens(FirmwareModel model, FirmwareEdition edition, uint16_t hardwareCode) {
    if (!ThreeMaxNative::Find(model, edition, hardwareCode)) return {};
    return ScreensFor(model, edition);
}
} // namespace FirmwareEmulator
