#include "firmware_emulator_backend.h"
#include "firmware_emulator_runtime.h"
#include "firmware_emulator_screens.h"
#include "app_state.h"
#include "embedded_firmware.h"
#include "firmware_layout.h"
#include "firmware_legacy_loader_data.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <utility>

namespace FirmwareEmulator {
namespace {
constexpr uint32_t Ram = 0x20000000, Stack = 0x2000e000;
constexpr uint32_t Settings = 0x20005000, Object = 0x20005100;
constexpr uint32_t SvcReturn = 0x0ffe0000;
struct Profile {
    FirmwareModel model;
    FirmwareEdition edition;
    uint32_t base, stack, main, clear, flush, descriptor, full, lock, enter, leave;
    size_t size;
    std::array<uint32_t, 8> anchors;
    uint32_t scatter;
    bool Refresh() const {
        return model == FirmwareModel::Drag2Refresh || model == FirmwareModel::DragMiniRefresh;
    }
    bool Mini() const { return model == FirmwareModel::DragMini || model == FirmwareModel::DragMiniRefresh; }
    uint16_t Code() const { return Refresh() ? (Mini() ? 0x1008 : 0x1006) : 0; }
    uint32_t OriginalAddress(uint32_t drag2, uint32_t mini, uint32_t drag1 = 0) const {
        return model == FirmwareModel::Drag1 ? drag1 : Mini() ? mini : drag2;
    }
    uint32_t Globals() const {
        if (model == FirmwareModel::Drag1) return 0x20000d98;
        if (model == FirmwareModel::DragMini && edition == FirmwareEdition::V1) return 0x20000dc8;
        return 0x20000dcc;
    }
};
constexpr Profile Profiles[] = {
    {FirmwareModel::Drag1, FirmwareEdition::Standard, 0x08003000, 0x20002570,
        0x08004414, 0x080086dc, 0x0800872c, 0x080050e0, 0, 0x0800e400, 0, 0, 50280,
        {0x08004414, 0x080050e0, 0x080050f4, 0x08005740, 0x0800872c, 0x0800884c, 0x0800b538, 0x0800b680}, 0x0800cfc0},
    {FirmwareModel::Drag2, FirmwareEdition::Standard, 0x08003000, 0x200025a8,
        0x080045a0, 0x080088c8, 0x08008918, 0x08005278, 0, 0x0800e400, 0, 0, 50292,
        {0x080045a0, 0x08005278, 0x0800528c, 0x080041c8, 0x08008918, 0x08008a38, 0x0800b858, 0x0800b9a0}, 0x0800cfd4},
    {FirmwareModel::Drag2Refresh, FirmwareEdition::Standard, 0x08002c00, 0x200035a8,
        0x08006874, 0x0800592c, 0, 0, 0x08009658, 0x08012baa, 0x08010f5d, 0x08010fad, 77124,
        {0x08006874, 0x0800592c, 0x08007dac, 0x08009658, 0x08009244, 0x08011c3c, 0x08010f5c, 0x08010fac}, 0x08015490},
    {FirmwareModel::Drag2Refresh, FirmwareEdition::Fit3, 0x08002c00, 0x200035a8,
        0x0800697c, 0x08005a48, 0, 0, 0x080097bc, 0x08012ece, 0x08011219, 0x08011269, 77924,
        {0x0800697c, 0x08005a48, 0x08007f10, 0x080097bc, 0x080093a8, 0x08011f60, 0x08011218, 0x08011268}, 0x080157b0},
    {FirmwareModel::DragMini, FirmwareEdition::V1, 0x08003000, 0x200025a8,
        0x08004550, 0x08008814, 0x08008864, 0x08005228, 0, 0x0800e400, 0, 0, 50288,
        {0x08004550, 0x08005228, 0x0800523c, 0x080041c8, 0x08008864, 0x08008984, 0x0800b7b4, 0x0800b8fc}, 0x0800cf30},
    {FirmwareModel::DragMini, FirmwareEdition::V2, 0x08003000, 0x200025a8,
        0x08004550, 0x08008814, 0x08008864, 0x08005228, 0, 0x0800e400, 0, 0, 50288,
        {0x08004550, 0x08005228, 0x0800523c, 0x080041c8, 0x08008864, 0x08008984, 0x0800b7c8, 0x0800b910}, 0x0800cf44},
    {FirmwareModel::DragMiniRefresh, FirmwareEdition::Standard, 0x08002c00, 0x200035a8,
        0x08006874, 0x0800592c, 0, 0, 0x08009658, 0x08012b9a, 0x08010f5d, 0x08010fad, 77108,
        {0x08006874, 0x0800592c, 0x08007dac, 0x08009658, 0x08009244, 0x08011c3c, 0x08010f5c, 0x08010fac}, 0x08015480},
    {FirmwareModel::DragMiniRefresh, FirmwareEdition::Fit3, 0x08002c00, 0x200035a8,
        0x0800697c, 0x08005a48, 0, 0, 0x080097bc, 0x08012ebe, 0x08011219, 0x08011269, 77908,
        {0x0800697c, 0x08005a48, 0x08007f10, 0x080097bc, 0x080093a8, 0x08011f60, 0x08011218, 0x08011268}, 0x080157a0},
};

// Stable native UI states, independent of resource catalog indices.
// Version/CHIP-ID execute application renderers using explicitly emulated
// loader data. Only their four/eight recovered input bytes are mapped, read-only
// and non-executable; no bootloader implementation or device identity is invented.
constexpr ScreenInfo OriginalScreens[] = {
    {0, "Main", 0, true}, {1, "Logo VOOPOO (boot)"}, {2, "Lock"}, {3, "Unlock"}, {4, "OFF"},
    {5, "Logo DRAG (standby)"},
    {10, "Check Atomizer"}, {11, "Low Atomizer"}, {12, "High Atomizer"},
    {13, "Atomizer Short"}, {14, "Time Out"}, {15, "Check Battery"},
    {16, "Low Power"}, {17, "High Temp"}, {19, "New coil confirmation"},
    {30, "Temperature control", 0, true, false, 1}, {33, "TC mode NC", 0, false, false, 1},
    {34, "TC mode TC", 0, false, false, 1}, {35, "Voltage mode", 0, false, false, 0},
    {40, "Power menu", 0, false, false, 0}, {41, "Memory 1-5 menu", 0, false, false, 0}, {42, "SN menu"},
    {45, "Resistance SET menu", 0, false, false, 1}, {47, "Temperature curve menu", 0, false, false, 1},
    {60, "Power curve", 5, false, false, 0}, {61, "Temperature curve", 5, false, false, 1},
    {70, "TC material setting", 5, false, false, 1}, {75, "Resistance setting"},
    {80, "Power memory", 5, false, false, 0}, {85, "TC memory", 5, true, false, 1}, {90, "Battery status"},
    {91, "Version (emulated loader data)", 0, false, false, -1, 0},
    {92, "CHIP-ID (emulated loader data)", 0, false, false, -1, 0},
    {93, "Save menu"}, {94, "Power-to-voltage menu"}, {95, "Voltage-to-power menu"},
};
constexpr ScreenInfo Drag2Screens[] = {
    {0, "Main", 0, true}, {1, "Logo VOOPOO (boot)"}, {2, "Lock"}, {3, "Unlock"}, {4, "OFF"},
    {5, "Device logo (standby)"},
    {10, "Check Atomizer"}, {11, "Low Atomizer"}, {12, "High Atomizer"},
    {13, "Atomizer Short"}, {14, "Time Out"}, {15, "Check Battery"},
    {16, "Low Power"}, {17, "High Temp"}, {19, "New coil confirmation"},
    {20, "FIT selection", 3, false, false, 0},
    {30, "Temperature control", 0, true, false, 1}, {33, "TC mode NC", 0, false, false, 1},
    {34, "TC mode TC", 0, false, false, 1},
    {40, "Power menu", 0, false, false, 0}, {41, "Memory 1-5 menu", 0, false, false, 0}, {42, "SN menu"},
    {43, "TC menu icon", 0, false, false, 1}, {44, "TC memory menu", 0, false, false, 1},
    {45, "Resistance SET menu", 0, false, false, 1},
    {46, "Version menu"}, {47, "Temperature curve menu", 0, false, false, 1},
    {50, "TC material menu", 5, false, false, 1},
    {60, "Power curve", 5, false, false, 0}, {61, "Temperature curve", 5, false, false, 1},
    {70, "TC material setting", 5, false, false, 1}, {75, "Resistance setting"},
    {80, "Power memory", 5, false, false, 0}, {85, "TC memory", 5, true, false, 1}, {90, "Battery status"},
    {91, "Version (emulated loader data)", 0, false, false, -1, 0},
    {92, "CHIP-ID (emulated loader data)", 0, false, false, -1, 0},
};
constexpr ScreenInfo RefreshScreens[] = {
    {0, "Main", 0, true}, {1, "Device logo (boot)"}, {2, "Lock"}, {3, "Unlock"}, {4, "OFF"},
    {5, "Logo VOOPOO (boot)"}, {10, "Check Atomizer"}, {12, "High Atomizer"},
    {13, "Atomizer Short"}, {14, "Time Out"}, {15, "Check Battery"},
    {16, "Low Power"}, {17, "High Temp"}, {18, "Max Power"},
    {20, "FIT selection", 3, false, false, 0}, {30, "Temperature control", 0, true, false, 1},
    {40, "TC material menu", 3, false, false, 1}, {43, "TC settings", 3, true, false, 1},
    {60, "Power curve", 0, false, false, 0}, {61, "Temperature curve", 0, true, false, 1},
    {64, "Power curve B", 0, false, false, 0}, {65, "Temperature curve B", 0, true, false, 1},
    {70, "Memory selection", 3}, {80, "Rank"}, {90, "Battery status"},
};
std::string Hex(uint32_t value) {
    char text[16];
    snprintf(text, sizeof(text), "0x%08X", value);
    return text;
}
const HardwareRecord* NativeRecord(const FirmwarePackage& package, const Profile& profile, uint16_t requestedCode = 0) {
    const uint16_t selectedCode = requestedCode ? requestedCode : profile.Code();
    if (selectedCode != profile.Code()) return nullptr;
    const HardwareRecord* selected = nullptr;
    for (const auto& record : package.records) {
        const uint16_t code = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
        if (code != selectedCode) continue;
        if (selected) return nullptr;
        selected = &record;
    }
    return selected;
}

// The OLED is a 128x32, four-page, vertical-LSB display. All rasterization,
// numeric layout, glyph reads and I2C bit banging run in the original ROM.
class LegacyBackend final : public Backend {
    ArmRuntime runtime;
    LegacyLoaderData loaderData;
    ResourceReadTrace resourceTrace;
    const Profile* profile = nullptr;
    const StudioState* source = nullptr;
    const uint8_t* sourceData = nullptr;
    size_t sourceSize = 0;
    uint64_t revision = 0;
    std::string sourceHash, cachedError, fault;
    bool cached = false;
    std::vector<uint8_t> image;
    std::array<std::vector<uint8_t>, std::size(Profiles)> stock;
    Frame* frame = nullptr;
    std::array<bool, 512> covered{};
    size_t coverage = 0;
    bool sda = true, scl = true, active = false, displayEnabled = true;
    unsigned bits = 0, byte = 0, transactionBytes = 0, control = 0;
    unsigned page = 0, column = 0;

    void Stop(const char* reason, uint32_t address) {
        if (fault.empty()) fault = std::string(reason) + " at " + Hex(address);
        uc_emu_stop(runtime.Raw());
    }
    void Display(uint8_t value) {
        if (!control) {
            if ((value & 0xf0) == 0xb0 && (value & 15) < 4) page = value & 15;
            else if ((value & 0xf0) == 0x10) column = (column & 15) | ((value & 15) << 4);
            else if (value < 16) column = (column & 0xf0) | value;
            else if (value == 0xae || value == 0xaf) displayEnabled = value == 0xaf;
            else Stop("Unsupported OLED command", value);
            return;
        }
        if (page >= 4 || column >= 128) { Stop("Invalid OLED address window", column); return; }
        const size_t index = size_t(page) * 128 + column;
        if (!covered[index]) { covered[index] = true; ++coverage; }
        for (unsigned bit = 0; bit < 8; ++bit) {
            frame->rgba[size_t(page * 8 + bit) * 128 + column] = value & (1u << bit) ? 0xffffffffu : 0xff000000u;
            ++frame->pixelWrites;
        }
        ++column;
    }
    void Byte(uint8_t value) {
        ++frame->spiBytes;
        if (transactionBytes == 0) {
            if (value != 0x78) { Stop("Unsupported I2C device", value); return; }
        } else if (transactionBytes == 1) {
            if (value != 0 && value != 0x40) { Stop("Unsupported OLED control byte", value); return; }
            control = value;
        } else if (transactionBytes == 2) Display(value);
        else { Stop("Invalid OLED I2C transaction", transactionBytes); return; }
        ++transactionBytes;
    }
    void Pins(bool data, bool clock) {
        if (sda != data && scl && clock) {
            if (!data) {
                if (active && transactionBytes != 3) { Stop("Incomplete OLED I2C transaction", transactionBytes); return; }
                active = true; bits = byte = transactionBytes = 0;
            } else active = false;
        } else if (!scl && clock && active) {
            if (bits < 8) byte = (byte << 1) | unsigned(data);
            if (++bits == 9) { Byte(static_cast<uint8_t>(byte)); bits = byte = 0; }
        }
        sda = data; scl = clock;
    }
    static void WriteHardware(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t value, void* user) {
        auto& self = *static_cast<LegacyBackend*>(user);
        const uint32_t a = static_cast<uint32_t>(address), v = static_cast<uint32_t>(value);
        if (size != 4) { self.Stop("Unsupported OLED hardware write width", a); return; }
        if (!self.profile->Refresh()) {
            if ((a != 0x42210198 && a != 0x4221019c) || v > 1) self.Stop("Unsupported OLED bit-band write", a);
            else self.Pins(a == 0x42210198 ? v != 0 : self.sda, a == 0x4221019c ? v != 0 : self.scl);
            return;
        }
        if (a == 0x40010810 || a == 0x40010814) {
            if (v != 0x40 && v != 0x80) { self.Stop("Unsupported OLED GPIO pin", a); return; }
            self.Pins(v == 0x40 ? a == 0x40010810 : self.sda, v == 0x80 ? a == 0x40010810 : self.scl);
        } else if (a == 0x40010800) {
            // GPIOA6 alternates between push-pull output and ACK input.
            if ((v & ~0x0f000000u) || ((v >> 24) != 2 && (v >> 24) != 4))
                self.Stop("Unsupported OLED GPIO configuration", a);
        } else if (a == 0xe000e180 || a == 0xe000e100) {
            if (v != 1) self.Stop("Unsupported RTOS interrupt mask", a);
        } else if (a == 0xe000ed04) {
            if (v != 0) self.Stop("Unsupported RTOS exception request", a);
        } else self.Stop("Unsupported native hardware write", a);
    }
    static void ReadHardware(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t, void* user) {
        auto& self = *static_cast<LegacyBackend*>(user);
        if (!self.profile->Refresh() || size != 4 || (address != 0x40010800 && address != 0xe000ed04))
            self.Stop("Unsupported native hardware read", static_cast<uint32_t>(address));
    }
    static void Interrupt(uc_engine* engine, uint32_t number, void* user) {
        auto& self = *static_cast<LegacyBackend*>(user);
        uint32_t pc = 0, target = 0, sp = 0, apsr = 0;
        uc_reg_read(engine, UC_ARM_REG_PC, &pc);
        uc_reg_read(engine, UC_ARM_REG_R12, &target);
        uc_reg_read(engine, UC_ARM_REG_SP, &sp);
        uc_reg_read(engine, UC_ARM_REG_XPSR, &apsr);
        std::array<uint16_t, 4> site{};
        const bool readableSite = pc >= self.profile->base + sizeof(site) &&
            pc <= self.profile->base + self.image.size() &&
            uc_mem_read(engine, pc - sizeof(site), site.data(), sizeof(site)) == UC_ERR_OK;
        // Curve number formatting loads its fifth argument into r5 between
        // LDR.W r12,[pc,#imm] and SVC#0. It still calls the same native critical
        // helper; the intervening LDR r5,[sp,#0x38] does not modify r12 or APSR.
        const bool adjacentLoad = site[1] == 0xf8df && (site[2] & 0xf000) == 0xc000;
        const bool curveArgumentLoad = site[0] == 0xf8df &&
            (site[1] & 0xf000) == 0xc000 && site[2] == 0x9d0e;
        if (!self.profile->Refresh() || number != 2 || !readableSite ||
            site[3] != 0xdf00 || (!adjacentLoad && !curveArgumentLoad) ||
            (target != self.profile->enter && target != self.profile->leave) ||
            sp < Ram + 32 || sp > Stack) {
            self.Stop("Unsupported native interrupt or RTOS dispatch", pc); return;
        }
        // SVC#0's stock handler calls r12, returns r0, and exception-restores the
        // other volatile registers/APSR. Only its non-scheduling critical-section
        // helpers are admitted. No drawing function is intercepted.
        const std::array<int, 5> registers{UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_R12, UC_ARM_REG_LR};
        std::array<uint32_t, 8> saved{apsr, 0};
        for (size_t i = 0; i < registers.size(); ++i) uc_reg_read(engine, registers[i], &saved[i + 2]);
        saved[7] = pc | 1;
        sp -= sizeof(saved);
        const uint32_t link = SvcReturn | 1;
        if (uc_mem_write(engine, sp, saved.data(), sizeof(saved)) != UC_ERR_OK ||
            uc_reg_write(engine, UC_ARM_REG_SP, &sp) != UC_ERR_OK ||
            uc_reg_write(engine, UC_ARM_REG_LR, &link) != UC_ERR_OK ||
            uc_reg_write(engine, UC_ARM_REG_PC, &target) != UC_ERR_OK)
            self.Stop("Unable to enter native RTOS helper", pc);
    }
    bool Hook(std::string& error) {
        const std::array<uint32_t, 2> refreshPages{0x40010000, 0xe000e000};
        const std::array<uint32_t, 1> originalPages{0x42210000};
        const auto pages = profile->Refresh() ? std::span<const uint32_t>(refreshPages) : std::span<const uint32_t>(originalPages);
        if (!runtime.Open(image, profile->base, Ram, 0x10000, UC_CPU_ARM_CORTEX_M3, pages, error)) return false;
        if (!profile->Refresh() && !loaderData.Open(runtime.Raw(), error)) return false;
        for (uint32_t pageAddress : pages) {
            uc_hook write = 0, read = 0;
            if (uc_hook_add(runtime.Raw(), &write, UC_HOOK_MEM_WRITE, reinterpret_cast<void*>(WriteHardware), this, pageAddress, pageAddress + 4095) != UC_ERR_OK ||
                uc_hook_add(runtime.Raw(), &read, UC_HOOK_MEM_READ, reinterpret_cast<void*>(ReadHardware), this, pageAddress, pageAddress + 4095) != UC_ERR_OK) {
                error = "Unable to observe native OLED hardware"; return false;
            }
        }
        if (profile->Refresh()) {
            // A CPU exception-return shim, not replacement firmware/raster code.
            constexpr uint8_t restore[] = {0xbd,0xe8,0x00,0x50, 0x8c,0xf3,0x00,0x88, 0xbd,0xe8,0x0e,0x50, 0x00,0xbd};
            uc_hook interrupt = 0;
            if (uc_mem_map(runtime.Raw(), SvcReturn, 4096, UC_PROT_READ | UC_PROT_EXEC) != UC_ERR_OK ||
                !runtime.Write(SvcReturn, restore, sizeof(restore), error) ||
                uc_hook_add(runtime.Raw(), &interrupt, UC_HOOK_INTR, reinterpret_cast<void*>(Interrupt), this, 1, 0) != UC_ERR_OK) {
                if (error.empty()) error = "Unable to initialize native RTOS exception return";
                return false;
            }
        }
        return true;
    }
    bool Guard(const HardwareRecord& record, std::string& error) {
        const size_t index = size_t(profile - Profiles);
        if (stock[index].empty()) {
            FirmwarePackage embedded;
            if (!LoadEmbeddedFirmware(embedded, error, profile->model, profile->edition)) return false;
            const auto* original = NativeRecord(embedded, *profile);
            if (!original) { error = "Embedded legacy OLED record is unavailable"; return false; }
            stock[index] = original->image;
        }
        if (record.linked_base != profile->base || record.sp != profile->stack || record.image.size() != profile->size ||
            stock[index].size() != profile->size) { error = "Unsupported legacy OLED flash/RAM layout"; return false; }
        // Validate the executable callgraph, not just the entrypoint. Editable
        // OLED resources begin after this boundary in each specific release.
        const uint32_t firstAsset = profile->Refresh() ? 0x08012164 +
            (profile->edition == FirmwareEdition::Fit3 ? 0x324 : 0) - (profile->Mini() ? 16 : 0) :
            profile->OriginalAddress(0x0800bd10, profile->edition == FirmwareEdition::V2 ? 0x0800bc80 : 0x0800bc6c, 0x0800b9f0);
        if (!FirmwareLayout::MatchesCode(source->layoutElements, profile->Code(), profile->base,
            stock[index].data(), record.image.data(), firstAsset - profile->base)) {
            error = "Unsupported modified legacy OLED executable code"; return false;
        }
        const size_t scatterOffset = profile->scatter - profile->base;
        if (!std::equal(stock[index].begin() + scatterOffset, stock[index].begin() + scatterOffset + 16,
            record.image.begin() + scatterOffset)) {
            error = "Unsupported native OLED scatter layout"; return false;
        }
        uint32_t scatterSource = 0;
        std::memcpy(&scatterSource, record.image.data() + scatterOffset, sizeof(scatterSource));
        const size_t initializedData = scatterSource - profile->base;
        if (initializedData >= record.image.size() ||
            !std::equal(stock[index].begin() + initializedData, stock[index].end(),
                record.image.begin() + initializedData)) {
            error = "Unsupported modified native OLED initialization data"; return false;
        }
        for (uint32_t address : profile->anchors) {
            const size_t offset = address - profile->base;
            if (offset + 16 > record.image.size() ||
                !FirmwareLayout::MatchesCode(source->layoutElements, profile->Code(), address,
                    stock[index].data() + offset, record.image.data() + offset, 16)) {
                error = "Unsupported native OLED renderer at " + Hex(address); return false;
            }
        }
        return true;
    }
    bool Prepare(const StudioState& state, const Profile& selected, uint16_t hardwareCode, std::string& error) {
        const auto* original = NativeRecord(state.firmware, selected, hardwareCode);
        if (!original) { error = "Firmware has no matching native legacy OLED hardware record"; return false; }
        if (cached && source == &state && profile == &selected && sourceData == original->image.data() &&
            sourceSize == original->image.size() && sourceHash == state.firmware.source_sha256 && revision == state.ResourceRevision()) {
            error = cachedError; return error.empty() && runtime.Raw();
        }
        runtime.Close();
        cached = true; source = &state; profile = &selected; sourceData = original->image.data();
        sourceSize = original->image.size(); sourceHash = state.firmware.source_sha256; revision = state.ResourceRevision();
        cachedError.clear();
        std::vector<uint8_t> bytes;
        FirmwarePackage built;
        if (state.BuildZwdfBytes(bytes, cachedError) && FirmwarePackage::Parse(bytes, built, cachedError)) {
            const auto* record = NativeRecord(built, selected, hardwareCode);
            if (!record) cachedError = "Built firmware has no matching native legacy OLED hardware record";
            else if (Guard(*record, cachedError)) {
                image = record->image;
                if (!Hook(cachedError) || !resourceTrace.Attach(runtime.Raw(), state,
                    selected.Code(), selected.base, image.size(), cachedError))
                    runtime.Close();
            }
        }
        error = cachedError;
        return error.empty() && runtime.Raw();
    }
    bool Call(uint32_t address, std::array<uint32_t, 4> arguments, std::string& error) {
        const bool ok = runtime.Call(address, Stack, arguments, error);
        if (!fault.empty()) { error = fault; return false; }
        if (!loaderData.Error().empty()) { error = loaderData.Error(); return false; }
        return ok;
    }
    template<class T> bool Seed(uint32_t address, T value, std::string& error) { return runtime.Write(address, value, error); }
    bool Initialize(std::string& error) {
        // Execute the image's actual scatter decompressor. The second scatter
        // entry is zero-initialized RAM, already provided by ArmRuntime::Reset.
        std::array<uint32_t, 4> scatter{};
        std::memcpy(scatter.data(), image.data() + profile->scatter - profile->base, sizeof(scatter));
        if (!Call(scatter[3], {scatter[0], scatter[1], scatter[2], 0}, error)) return false;
        // Mode 2 is the firmware's non-hardware settings initialization path.
        // It validates erased persistent slots, constructs all five power/TC
        // curves and supplies native TCR defaults without running device boot.
        return profile->Refresh() ||
            Call(profile->OriginalAddress(0x08008334, 0x0800827c, 0x08008180), {2, 0, 0, 0}, error);
    }
    bool FullScreen(uint32_t pixels, std::string& error) {
        if (profile->Refresh()) return Call(profile->full, {pixels, 0, 0, 0}, error);
        const std::array<uint32_t, 2> descriptor{0x20800000, pixels};
        return runtime.Write(Object + 32, descriptor.data(), sizeof(descriptor), error) &&
            Call(profile->descriptor, {Object + 32, 0, 0, 0}, error);
    }
    bool DrawRefresh(uint32_t id, const DeviceState& device, std::string& error) {
        const uint32_t code = profile->edition == FirmwareEdition::Fit3 ? 0x108 : 0;
        const uint32_t pixels = profile->edition == FirmwareEdition::Fit3 ? 0x324 : 0;
        uint32_t bitmap = 0;
        switch (id) {
        case 0:
            return device.locked ? FullScreen(profile->lock, error) : Call(profile->main, {}, error);
        case 1: bitmap = 0x08014a40; break;
        case 2: bitmap = 0x08012baa; break;
        case 3: bitmap = 0x08012daa; break;
        case 4: return Call(code ? 0x0800992c : 0x080097c8, {}, error);
        case 5: bitmap = 0x080121aa; break;
        case 10: bitmap = 0x080123aa; break;
        case 12: bitmap = 0x080125aa; break;
        case 13: bitmap = 0x080127aa; break;
        case 14: bitmap = 0x080129aa; break;
        case 15: bitmap = 0x080131aa; break;
        case 16: bitmap = 0x080133aa; break;
        case 17: bitmap = 0x080135aa; break;
        case 18: bitmap = 0x08015164; break;
        default: break;
        }
        if (bitmap) return FullScreen(bitmap + pixels - (profile->Mini() ? 16 : 0), error);
        constexpr uint16_t materials[]{2, 3, 1}; // Shared NI, TI, SS -> native selectors.
        const uint16_t material = materials[std::clamp(device.temperatureMaterial, 0, 2)];
        if (id == 20)
            return Seed(0x20002bf6, uint16_t(std::clamp(device.menuSelection, 0, 2)), error) &&
                Call(0x08006804 + code, {}, error);
        if (id == 30) {
            return Seed(0x20002bf2, uint16_t(2), error) &&
                Seed(0x20002bfa, material, error) &&
                Seed(0x20002cb8 + material * 2,
                    uint16_t(std::clamp(device.temperature, device.fahrenheit ? 200 : 100, device.fahrenheit ? 600 : 315)), error) &&
                Call(profile->main, {}, error);
        }
        if (id == 40)
            return Seed(0x20002bfa, uint16_t(std::clamp(device.menuSelection, 0, 2) + 1), error) &&
                Call(0x08005ea4 + code, {}, error);
        if (id == 43) {
            return Seed(0x20002bfa, material, error) &&
                Seed(0x200000b9, uint8_t(12), error) &&
                Seed(0x200000ba, uint8_t(2), error) &&
                Seed(0x200000b5, uint8_t(std::clamp(device.menuSelection, 0, 2)), error) &&
                Call(0x08005fec + code, {}, error);
        }
        if (id == 60 || id == 61 || id == 64 || id == 65) {
            const uint32_t bank = id >= 64 ? 1 : 0;
            const uint16_t curve = id % 4 == 0 ? 0 : uint16_t(std::clamp(device.temperatureMaterial, 0, 2) + 1);
            return Seed(0x20002bfc, uint16_t(bank), error) &&
                Seed(0x20002ca2 + bank * 10, curve, error) &&
                Call(0x08006338 + code, {}, error);
        }
        if (id == 70)
            return Seed(0x20002bfc, uint16_t(std::clamp(device.menuSelection, 0, 2)), error) &&
                Call(0x08007100 + code, {}, error);
        if (id == 80) {
            const auto score = static_cast<uint32_t>(std::clamp(device.score, 0, 99999));
            return Seed(0x20002d02, score * 1000, error) && Call(0x08006908 + code, {}, error);
        }
        if (id == 90) {
            const uint32_t battery = uint32_t(std::clamp(device.battery, 0, 5));
            return Call(0x080091d4 + (code ? 0x164 : 0), {battery, battery, 0, 0}, error);
        }
        error = "Unmapped native Refresh screen";
        return false;
    }
    bool DrawOriginal(uint32_t id, const DeviceState& device, std::string& error) {
        const bool drag1 = profile->model == FirmwareModel::Drag1;
        const auto address = [&](uint32_t drag2, uint32_t mini, uint32_t original = 0) {
            return profile->OriginalAddress(drag2, mini, original);
        };
        const uint32_t tc = address(0x08004d00, 0x08004cb0, 0x08004b8c);
        if (id == 91 || id == 92) {
            const uint32_t renderer = id == 91 ? address(0x08005904, 0x08005850, 0x08005770) :
                address(0x08005804, 0x08005750, 0x0800565c);
            return loaderData.Seed(runtime.Raw(), device.loaderVersion, device.loaderId, error) &&
                Call(renderer, {}, error);
        }
        uint32_t bitmap = 0;
        switch (id) {
        case 0:
            return device.locked ? FullScreen(profile->lock, error) : Call(profile->main, {Object, 0, 0, 0}, error);
        case 1: bitmap = 0x0800d600; break;
        case 2: bitmap = 0x0800e400; break;
        case 3: bitmap = 0x0800e600; break;
        case 4: bitmap = 0x0800e800; break;
        case 10: bitmap = 0x0800de00; break;
        case 11: bitmap = 0x0800da00; break;
        case 12: bitmap = 0x0800dc00; break;
        case 13: bitmap = 0x0800e000; break;
        case 14: bitmap = 0x0800e200; break;
        case 15: bitmap = 0x0800ea00; break;
        case 16: bitmap = 0x0800ec00; break;
        case 17: bitmap = 0x0800ee00; break;
        case 19: bitmap = 0x0800d800; break;
        default: break;
        }
        if (bitmap) return FullScreen(bitmap, error);
        if (id == 5)
            return Seed(Object, uint8_t(1), error) &&
                Seed(address(0x20000384, 0x20000380, 0x20000381), uint8_t(1), error) &&
                Call(address(0x08004890, 0x08004840, 0x08004740), {Object, 0, 0, 0}, error);
        if (id == 20)
            return Seed(profile->Globals() + 0x3c, uint8_t(std::clamp(device.menuSelection, 0, 2) + 1), error) &&
                Call(0x080040ac, {Object, 0, 0, 0}, error);
        constexpr uint8_t materials[]{1, 2, 0}; // NI, TI, SS.
        const uint8_t material = materials[std::clamp(device.temperatureMaterial, 0, 2)];
        if (id == 30 || id == 33 || id == 34)
            return Seed(Settings + 0x10, id == 30 ? material : uint8_t(id - 30), error) &&
                Seed(Settings + 4,
                    uint16_t(std::clamp(device.temperature, device.fahrenheit ? 200 : 100, device.fahrenheit ? 600 : 315)), error) &&
                Seed(Settings + 6, uint8_t(device.fahrenheit ? 0 : 1), error) &&
                Call(tc, {Object, 0, 0, 0}, error);
        if (id == 35) {
            uint32_t voltage = 0;
            if (uc_mem_read(runtime.Raw(), profile->Globals() + 0x34, &voltage, sizeof(voltage)) != UC_ERR_OK) {
                error = "Unable to read native voltage setting"; return false;
            }
            return Seed(0x20000dd0, uint8_t(1), error) &&
                Seed(Settings + 0x14, voltage, error) &&
                Call(profile->main, {Object, 0, 0, 0}, error);
        }
        if (drag1 && ((id >= 40 && id <= 47) || (id >= 93 && id <= 95))) {
            uint8_t mode = 1, selection = 1, pageNumber = 1;
            if (id == 41) selection = 2;
            if (id == 42) { selection = 4; pageNumber = 2; }
            if (id == 45) { mode = 2; selection = 3; }
            if (id == 47) { mode = 2; pageNumber = 2; }
            if (id == 93) { selection = 5; pageNumber = 2; }
            if (id == 94) selection = 3;
            if (id == 95) selection = 3;
            return Seed(Settings + 0x34, mode, error) && Seed(Settings + 0x35, uint8_t(1), error) &&
                Seed(Settings + 0x36, pageNumber, error) && Seed(Settings + 0x37, selection, error) &&
                Seed(0x20000dd0, uint8_t(id == 95), error) &&
                Call(0x08004130, {Object, 0, 0, 0}, error);
        }
        if (id >= 40 && id <= 47) {
            uint8_t mode = id >= 43 && id <= 45 ? 2 : 1;
            uint8_t selection = id <= 42 ? uint8_t(id - 39) : uint8_t(id - 42);
            uint8_t pageNumber = 1;
            if (id == 46) { selection = 4; pageNumber = 2; }
            if (id == 47) { mode = 2; selection = 1; pageNumber = 2; }
            return Seed(Settings + 0x38, mode, error) && Seed(Settings + 0x39, uint8_t(1), error) &&
                Seed(Settings + 0x3a, pageNumber, error) && Seed(Settings + 0x3b, selection, error) &&
                Call(address(0x080042dc, 0x0800428c), {Object, 0, 0, 0}, error);
        }
        if (id == 50) {
            // Native enabled-material list and one-based menu ordinal.
            constexpr std::array<uint8_t, 5> enabled{1, 1, 1, 1, 1};
            return runtime.Write(profile->Mini() ? 0x200003dc : 0x200003e0, enabled.data(), enabled.size(), error) &&
                Seed(profile->Globals() + 0x6b, uint8_t(std::clamp(device.menuSelection, 0, 4) + 1), error) &&
                Call(address(0x080044a0, 0x08004450), {Object, 0, 0, 0}, error);
        }
        if (id == 60 || id == 61)
            return Seed(Settings + (drag1 ? 0x2d : 0x31), uint8_t(std::clamp(device.menuSelection, 0, 4) + 1), error) &&
                Call(id == 60 ? address(0x080046d8, 0x08004688, 0x080045e0) :
                    address(0x08004f38, 0x08004ee8, 0x08004da4), {Object, 0, 0, 0}, error);
        if (id == 70) {
            const uint32_t selection = uint32_t(std::clamp(device.menuSelection, 0, 4));
            uint32_t coefficient = 0;
            if (uc_mem_read(runtime.Raw(), 0x2000015c + selection * 4, &coefficient, sizeof(coefficient)) != UC_ERR_OK) {
                error = "Unable to read native material setting"; return false;
            }
            return Seed(Settings + 0x10, uint8_t(selection), error) &&
                Seed(Settings + (drag1 ? 0x38 : 0x3c), coefficient, error) &&
                Call(address(0x080053b4, 0x08005364, 0x0800521c), {Object, 0, 0, 0}, error);
        }
        if (id == 75)
            return Seed(Settings + (drag1 ? 0x3c : 0x40), uint32_t(std::clamp(device.resistanceMilliOhms, 0, 9990)), error) &&
                Call(address(0x080054b8, 0x08005468, 0x0800531c), {Object, 0, 0, 0}, error);
        if (id == 80 || id == 85)
            return Seed(Settings + (drag1 ? 0x2e : 0x32), uint8_t(1), error) &&
                Seed(Settings + (drag1 ? 0x2d : 0x31), uint8_t(std::clamp(device.menuSelection, 0, 4) + 1), error) &&
                Seed(Settings + 0x10, material, error) &&
                Seed(Settings + 4, uint16_t(std::clamp(device.temperature,
                    device.fahrenheit ? 200 : 100, device.fahrenheit ? 600 : 315)), error) &&
                Call(id < 85 ? profile->main : tc, {Object, 0, 0, 0}, error);
        if (id == 90) {
            constexpr std::array<uint32_t, 6> voltages{3200, 3300, 3500, 3600, 3750, 3900};
            const uint32_t voltage = voltages[std::clamp(device.battery, 0, 5)];
            return Call(address(0x08005704, 0x080056b4, 0x08005570), {voltage, voltage, 0, 0}, error);
        }
        error = "Unmapped native original OLED screen";
        return false;
    }
    bool Draw(const DeviceState& device, Frame& output, std::string& error) {
        frame = &output; fault.clear(); coverage = 0; covered.fill(false);
        sda = scl = true; active = false; bits = byte = transactionBytes = control = page = column = 0;
        displayEnabled = true;
        output.width = 128; output.height = 32; output.monochrome = true;
        output.emulatedUid = false;
        output.emulatedLoaderMetadata = !profile->Refresh() && (device.screen == 91 || device.screen == 92);
        loaderData.ResetError();
        resourceTrace.Reset();
        output.rgba.assign(128 * 32, 0xff000000u); output.spiBytes = output.pixelWrites = 0;
        if (!runtime.Reset(error) || !Initialize(error)) return false;
        const uint32_t battery = static_cast<uint32_t>(std::clamp(device.battery, 0, 5));
        if (profile->Refresh()) {
            // Native fixed-block allocator control and free chain. Allocator,
            // formatting and compositing routines themselves execute unchanged.
            if (!Seed(0x20001c58, uint32_t(0x20002000), error)) return false;
            for (uint32_t i = 0; i < 8; ++i)
                if (!Seed(0x20002000 + i * 120, i == 7 ? 0u : 0x20002000 + (i + 1) * 120, error)) return false;
            // The original initializer supplies valid TC/curve/memory defaults.
            if (!Call(profile->edition == FirmwareEdition::Fit3 ? 0x08005168 : 0x08005044, {}, error)) return false;
            const uint16_t watts = static_cast<uint16_t>(std::clamp(device.watts, 5, profile->Mini() ? 117 : 177) * 10);
            if (!Seed(0x20000068, uint32_t(8), error) ||
                !Seed(0x20000039, uint8_t(battery), error) || !Seed(0x2000003a, uint8_t(battery), error) ||
                !Seed(0x20002bf2, uint16_t(device.rba ? 1 : 5), error) ||
                !Seed(0x20002c00, uint16_t(device.fahrenheit ? 1 : 0), error) ||
                !Seed(0x20002cb8, watts, error) || !Seed(0x20002cc4, watts, error) ||
                !Seed(0x20002ce0, uint16_t(std::clamp(device.resistanceMilliOhms, 0, 9999)), error) ||
                !Seed(0x20002ce8, uint16_t(std::clamp(device.timeCentiseconds / 10, 0, 999)), error) ||
                !Seed(0x20002cee, uint16_t(999), error)) return false;
            const uint32_t screen = device.screen == 0 && device.temperatureControl && !device.locked ? 30 : device.screen;
            if (!Call(profile->clear, {0, 0, 127, 3}, error) || !DrawRefresh(screen, device, error)) return false;
        } else {
            const bool drag1 = profile->model == FirmwareModel::Drag1;
            constexpr std::array<uint32_t, 6> voltages{3200, 3300, 3500, 3600, 3750, 3900};
            if (!Seed(Object + 12, Settings, error) || !Seed(0x2000036e, uint8_t(1), error) ||
                !Seed(Settings, uint32_t(std::clamp(device.watts, 5, drag1 ? 157 : profile->Mini() ? 117 : 177) * 1000), error) ||
                !Seed(Settings + 8, uint32_t(std::clamp(device.watts, 5, drag1 ? 157 : profile->Mini() ? 117 : 177) * 1000), error) ||
                !Seed(profile->Globals() + 0x1c, uint8_t(drag1 ? (device.fahrenheit ? 1 : 0) :
                    (device.fahrenheit ? 2 : 1)), error) ||
                !Seed(Settings + 12, uint32_t(std::clamp(device.resistanceMilliOhms, 0, 9990)), error) ||
                !Seed(Settings + 0x18, voltages[battery], error) || !Seed(Settings + 0x1c, voltages[battery], error) ||
                !Seed(Settings + 6, uint8_t(device.fahrenheit ? 0 : 1), error) ||
                !Seed(Settings + 0x48, uint32_t(std::clamp(device.timeCentiseconds, 0, 999) * 10), error)) return false;
            const uint32_t screen = device.screen == 0 && device.temperatureControl && !device.locked ? 30 : device.screen;
            if (!Call(profile->clear, {}, error) || !DrawOriginal(screen, device, error)) return false;
            if (!Call(profile->flush, {}, error)) return false;
        }
        if (coverage != covered.size() || (active && transactionBytes != 3)) {
            error = "Native OLED renderer did not produce a complete 128x32 frame"; return false;
        }
        if (!displayEnabled) std::fill(output.rgba.begin(), output.rgba.end(), 0xff000000u);
        resourceTrace.Collect(output);
        return true;
    }
public:
    bool Render(const StudioState& state, const DeviceState& device, Frame& output) override {
        auto fail = [&](std::string error) { output = {}; output.error = std::move(error); frame = nullptr; return false; };
        const Profile* selected = nullptr;
        for (const auto& candidate : Profiles)
            if (state.model == candidate.model && state.edition == candidate.edition) selected = &candidate;
        if (!selected) return fail("Unsupported legacy OLED profile");
        const auto screens = LegacyScreens(state.model, state.edition, device.hardwareCode);
        if (std::none_of(screens.begin(), screens.end(),
            [&](const ScreenInfo& screen) { return screen.id == device.screen; }))
            return fail("Unsupported native legacy screen ID " + Hex(device.screen));
        if (device.landscape) return fail("Native OLED rotation is unavailable");
        std::string error;
        if (!Prepare(state, *selected, device.hardwareCode, error) || !Draw(device, output, error)) return fail(std::move(error));
        output.hardwareCode = selected->Code();
        output.error.clear(); frame = nullptr;
        return true;
    }
};
}
std::span<const ScreenInfo> LegacyScreens(FirmwareModel model, FirmwareEdition edition, uint16_t hardwareCode) {
    const bool refresh = model == FirmwareModel::Drag2Refresh || model == FirmwareModel::DragMiniRefresh;
    const uint16_t expectedCode = refresh ? (model == FirmwareModel::DragMiniRefresh ? 0x1008 : 0x1006) : 0;
    if (hardwareCode && hardwareCode != expectedCode) return {};
    if (model == FirmwareModel::Drag1 && edition == FirmwareEdition::Standard) return OriginalScreens;
    if (model == FirmwareModel::Drag2 && edition == FirmwareEdition::Standard) return Drag2Screens;
    if (model == FirmwareModel::DragMini &&
        (edition == FirmwareEdition::V1 || edition == FirmwareEdition::V2)) return Drag2Screens;
    if ((model == FirmwareModel::Drag2Refresh || model == FirmwareModel::DragMiniRefresh) &&
        (edition == FirmwareEdition::Standard || edition == FirmwareEdition::Fit3)) return RefreshScreens;
    return {};
}
std::unique_ptr<Backend> CreateLegacyBackend() { return std::make_unique<LegacyBackend>(); }
}
