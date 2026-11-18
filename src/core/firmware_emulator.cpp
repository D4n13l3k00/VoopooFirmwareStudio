#include "firmware_emulator.h"
#include "app_state.h"
#include "firmware_emulator_backend.h"
#include "firmware_emulator_screens.h"
#include "firmware_layout.h"
#include "firmware_emulator_sx_score.h"
#include "firmware_emulator_sx_factory.h"
#include "firmware_layout_sx_pages_private.h"
#include "firmware_logo_patch.h"
#include "firmware_menu_patch.h"

#include <unicorn/unicorn.h>
#include <unicorn/arm.h>
#include <array>
#include <cstdio>
#include <utility>

namespace FirmwareEmulator {
namespace {
constexpr uint32_t FlashBase = 0x08005800;
constexpr uint32_t RamBase = 0x20000000;
constexpr size_t RamSize = 0x4000;
constexpr uint32_t ReturnAddress = 0x0fff0000;
constexpr uint32_t GpioPage = 0x40010000;
constexpr uint32_t SpiPage = 0x40013000;
constexpr uint32_t WatchdogPage = 0x40003000;
constexpr int PanelWidth = 80;
constexpr int PanelHeight = 160;
constexpr size_t PixelCount = PanelWidth * PanelHeight;

struct Anchor { uint32_t address; size_t length; };
struct Profile {
    uint32_t stack, scatter, defaults, redraw, settings;
    uint16_t code = 0x3103;
    int maxWatts = 60;
    bool dragX = false;
    size_t stockIndex = 0;
    std::array<Anchor, 8> anchors;
    uint32_t flashBase = FlashBase, fill = 0;
    int lcdX = 24, lcdY = 0;
    bool halfSettings = false, canonical = true;
};
constexpr Profile ScoreProfile = {
    0x20003820, 0x08014fcc, 0x08007d1c, 0x08007e94, 0x200018a8, 0x3103, 60, false, 0,
    {{{0x08005c10, 16}, {0x0800d2d2, 14}, {0x08007d1c, 16},
      {0x08007e94, 16}, {0x08009ede, 16}, {0x0800a148, 16},
      {0x0800a214, 12}, {0x08014fcc, 32}}}
};
constexpr Profile FactoryProfile = {
    0x20003410, 0x080146e8, 0x08007d60, 0x08007edc, 0x20001498, 0x3103, 60, false, 1,
    {{{0x08005c10, 16}, {0x0800cf16, 14}, {0x08007d60, 16},
      {0x08007edc, 16}, {0x0800a504, 16}, {0x0800a76c, 16},
      {0x0800a838, 12}, {0x080146e8, 32}}}
};
constexpr Profile XScoreProfile = {
    0x20003820, 0x08014f38, 0x08007d18, 0x08007e90, 0x200018a8, 0x3106, 80, true, 2,
    {{{0x08005c10, 16}, {0x0800d276, 14}, {0x08007d18, 16},
      {0x08007e90, 16}, {0x08009eba, 16}, {0x0800a124, 16},
      {0x0800a1f0, 12}, {0x08014f38, 32}}}
};
constexpr Profile XFactoryProfile = {
    0x20003410, 0x080152f4, 0x08007b6c, 0x08007d08, 0x20001498, 0x3106, 80, true, 3,
    {{{0x08005c10, 16}, {0x0800d0f2, 14}, {0x08007b6c, 16},
      {0x08007d08, 16}, {0x0800a30c, 16}, {0x0800a504, 16},
      {0x0800a7a0, 12}, {0x080152f4, 32}}}
};

// Each entry executes its own linked code and compressed-data initialization.
// Low-base builds use GD32F3x0 GPIO/SPI and halfword settings; high-base builds
// use the N32-style GPIO/SPI ABI. Each linked entrypoint is record-specific.
struct NativeRecordProfile {
    size_t editionIndex;
    uint16_t code;
    uint32_t base, stack, scatter, defaults, main, settings, raster, fill;
};
constexpr NativeRecordProfile NativeRecords[] = {
    {0, 0x314d, 0x8005800, 0x20003820, 0x8014f50, 0x8007d1c, 0x8007e94, 0x200018a8, 0x8009ef6, 0x800a090},
    {0, 0x3014, 0x8003000, 0x200025b8, 0x8016558, 0x80054b8, 0x8005740, 0x20001530, 0x8007680, 0x8007870},
    {0, 0x302a, 0x8003000, 0x20002558, 0x8013e68, 0x80054f4, 0x80056e4, 0x20001520, 0x8007058, 0x8007248},
    {0, 0x3013, 0x8003000, 0x200025b8, 0x8016548, 0x80054b8, 0x8005740, 0x20001530, 0x8007680, 0x8007870},
    {0, 0x3029, 0x8003000, 0x20002558, 0x8013e58, 0x80054f4, 0x80056e4, 0x20001520, 0x8007058, 0x8007248},
    {0, 0x3103, 0x8005800, 0x20003820, 0x8014fcc, 0x8007d1c, 0x8007e94, 0x200018a8, 0x8009ede, 0x800a0d8},
    {0, 0x3105, 0x8005800, 0x20003820, 0x8015110, 0x8007d1c, 0x8007e94, 0x200018a8, 0x8009ede, 0x800a0d8},
    {1, 0x3148, 0x8003000, 0x20002550, 0x80123b0, 0x8004e08, 0x8004fb0, 0x20001518, 0x8006f5c, 0x80070f0},
    {1, 0x3014, 0x8003000, 0x200025b0, 0x8014e20, 0x8004da0, 0x8005038, 0x20001528, 0x8006f0c, 0x80070fc},
    {1, 0x3104, 0x8005800, 0x20003410, 0x8015388, 0x8007b90, 0x8007d2c, 0x20001498, 0x800a354, 0x800a54c},
    {1, 0x3013, 0x8003000, 0x200025b0, 0x8014e10, 0x8004da0, 0x8005038, 0x20001528, 0x8006f0c, 0x80070fc},
    {1, 0x3029, 0x8003000, 0x20002550, 0x80122e4, 0x8004e08, 0x8004fb0, 0x20001518, 0x8006f20, 0x8007110},
    {1, 0x3103, 0x8005800, 0x20003410, 0x80146e8, 0x8007d60, 0x8007edc, 0x20001498, 0x800a504, 0x800a6fe},
    {1, 0x3034, 0x8003000, 0x20002550, 0x8012570, 0x8004e08, 0x8004fb0, 0x20001518, 0x8006f20, 0x8007110},
    {1, 0x3105, 0x8005800, 0x20003410, 0x80159cc, 0x8007b90, 0x8007d2c, 0x20001498, 0x800a354, 0x800a54c},
    {1, 0x314d, 0x8005800, 0x20003410, 0x80150c4, 0x8007bbc, 0x8007d58, 0x20001498, 0x800a3d0, 0x800a568},
    {2, 0x314e, 0x8005800, 0x20003820, 0x8014eb0, 0x8007d18, 0x8007e90, 0x200018a8, 0x8009ed2, 0x800a06c},
    {2, 0x3012, 0x8003000, 0x200025b8, 0x8016458, 0x8005498, 0x800571c, 0x20001530, 0x8007640, 0x8007830},
    {2, 0x302c, 0x8003000, 0x20002558, 0x8013d70, 0x80054d4, 0x80056c0, 0x20001520, 0x8007018, 0x8007208},
    {2, 0x3011, 0x8003000, 0x200025b8, 0x8016448, 0x8005498, 0x800571c, 0x20001530, 0x8007640, 0x8007830},
    {2, 0x302b, 0x8003000, 0x20002558, 0x8013d60, 0x80054d4, 0x80056c0, 0x20001520, 0x8007018, 0x8007208},
    {2, 0x3106, 0x8005800, 0x20003820, 0x8014f38, 0x8007d18, 0x8007e90, 0x200018a8, 0x8009eba, 0x800a0b4},
    {2, 0x3108, 0x8005800, 0x20003820, 0x8015074, 0x8007d18, 0x8007e90, 0x200018a8, 0x8009eba, 0x800a0b4},
    {3, 0x3149, 0x8003000, 0x20002550, 0x8012298, 0x8004d70, 0x8004f18, 0x20001518, 0x8006ea0, 0x8007034},
    {3, 0x3012, 0x8003000, 0x200025b0, 0x8014c98, 0x8004d08, 0x8004f9c, 0x20001528, 0x8006e54, 0x8007044},
    {3, 0x302c, 0x8003000, 0x20002550, 0x8012224, 0x8004d70, 0x8004f18, 0x20001518, 0x8006e64, 0x8007054},
    {3, 0x3107, 0x8005800, 0x20003410, 0x8015310, 0x8007b6c, 0x8007d08, 0x20001498, 0x800a30c, 0x800a504},
    {3, 0x3011, 0x8003000, 0x200025b0, 0x8014c90, 0x8004d08, 0x8004f9c, 0x20001528, 0x8006e54, 0x8007044},
    {3, 0x302b, 0x8003000, 0x20002550, 0x80121f0, 0x8004d70, 0x8004f18, 0x20001518, 0x8006e64, 0x8007054},
    {3, 0x3106, 0x8005800, 0x20003410, 0x80152f4, 0x8007b6c, 0x8007d08, 0x20001498, 0x800a30c, 0x800a504},
    {3, 0x3033, 0x8003000, 0x20002550, 0x801245c, 0x8004d70, 0x8004f18, 0x20001518, 0x8006e64, 0x8007054},
    {3, 0x3108, 0x8005800, 0x20003410, 0x8015958, 0x8007b6c, 0x8007d08, 0x20001498, 0x800a30c, 0x800a504},
    {3, 0x314e, 0x8005800, 0x20003410, 0x8015064, 0x8007b84, 0x8007d20, 0x20001498, 0x800a364, 0x800a4fc},
};

const SxScreenProfile* FindScreens(size_t index, uint16_t code) {
    const std::span<const SxScreenProfile> screens = index % 2 ?
        std::span<const SxScreenProfile>(SxFactoryScreenProfiles) :
        std::span<const SxScreenProfile>(SxScoreScreenProfiles);
    const auto found = std::find_if(screens.begin(), screens.end(),
        [index, code](const SxScreenProfile& item) { return item.editionIndex == index && item.code == code; });
    return found == screens.end() ? nullptr : &*found;
}

constexpr ScreenInfo SxScreenCatalog[] = {
    {0, "Main"}, {1, "Logo GENE.TT & VOOPOO (boot)"},
    {2, "Logo VOOPOO (animated boot)"}, {3, "OFF"},
    {4, "Charging"}, {5, "Atomizer Short"}, {6, "Check Atomizer"},
    {7, "Check Battery"}, {8, "Low Power"}, {9, "Max Power"},
    {10, "Puff Cleared"}, {11, "High Temp"}, {12, "Time Out"},
    {13, "Unlock"}, {14, "Lock"}, {15, "CHIP ID panel"}, {17, "Blank screen"},
    {16, "Banner ACTIVATED"},
    {18, "Puff history"}, {19, "Clock settings"},
};

uint32_t Read32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
const HardwareRecord* Canonical(const FirmwarePackage& package, uint16_t code) {
    for (const auto& record : package.records)
        if (((record.device_code[0] << 8) | record.device_code[1]) == code) return &record;
    return nullptr;
}
std::string Hex(uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", static_cast<unsigned>(value));
    return text;
}
uint32_t Rgba(uint16_t color) {
    const uint32_t r = color >> 11;
    const uint32_t g = (color >> 5) & 63;
    const uint32_t b = color & 31;
    return (r << 3 | r >> 2) | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2) << 16 | 0xff000000u;
}
}

std::span<const ScreenInfo> SxScreens(FirmwareModel model, FirmwareEdition edition, uint16_t hardwareCode) {
    if ((model != FirmwareModel::DragS && model != FirmwareModel::DragX) ||
        (edition != FirmwareEdition::Score && edition != FirmwareEdition::Factory)) return {};
    const size_t index = (model == FirmwareModel::DragX ? 2 : 0) +
        (edition == FirmwareEdition::Factory ? 1 : 0);
    const auto* profile = FindScreens(index, hardwareCode ? hardwareCode : FirmwareCanonicalCode(model));
    if (!profile) return {};
    // RTOS history/clock pages follow ACTIVATED; bare STM32 Factory has
    // ACTIVATED only, while GD32 Factory has neither page.
    return {SxScreenCatalog, profile->clock ? 20u : profile->history ? 19u : profile->draw[16] ? 18u : 17u};
}

struct Renderer::Impl {
    uc_engine* engine = nullptr;
    uc_context* initialCpu = nullptr;
    const StudioState* source = nullptr;
    const uint8_t* sourceData = nullptr;
    size_t sourceSize = 0;
    uint64_t revision = 0;
    std::string sourceHash;
    FirmwareEdition edition = FirmwareEdition::Score;
    const Profile* profile = nullptr;
    Profile selectedProfile{};
    const SxScreenProfile* screenProfile = nullptr;
    FirmwareModel model = FirmwareModel::DragS;
    std::unique_ptr<Backend> legacy;
    std::unique_ptr<Backend> drag3;
    std::unique_ptr<Backend> proPlus;
    bool cached = false;
    bool nativeRepaint = false;
    bool logoNative = false;
    bool logoRunning = false;
    bool logoCaptured = false;
    int logoRequestedPhase = -1;
    size_t logoTarget = 0;
    uint32_t logoBoundary = 0, logoInfoExitEntry = 0;
    uint32_t previewCycles = 0, previewDwtControl = 0, previewDemcr = 0;
    std::string cachedError;
    std::vector<uint8_t> image;
    std::vector<uint8_t> stock;
    std::array<FirmwarePackage, 4> stockPackages;
    std::array<uint8_t, RamSize> zeroRam{};
    std::array<uint8_t, 4096> zeroPage{};
    Frame* frame = nullptr;
    FrameObserver frameObserver = nullptr;
    void* observerContext = nullptr;
    std::array<bool, PixelCount> covered{};
    size_t coverage = 0;
    ResourceReadTrace resourceTrace;
    enum class Fault { None, HardwareRead, HardwareWrite, Command, Window, Pixel };
    Fault fault = Fault::None;
    uint32_t faultAddress = 0;
    bool dataMode = false;
    uint8_t command = 0;
    std::array<uint8_t, 4> parameters{};
    size_t parameterCount = 0;
    uint16_t x0 = 0, x1 = 0, y0 = 0, y1 = 0, x = 0, y = 0;
    bool haveX = false, haveY = false, haveHigh = false;
    uint8_t high = 0;

    ~Impl() { Close(); }
    void Close() {
        if (initialCpu) uc_context_free(initialCpu);
        initialCpu = nullptr;
        if (engine) uc_close(engine);
        engine = nullptr;
    }
    bool Check(uc_err result, const char* operation, std::string& error) {
        if (result == UC_ERR_OK) return true;
        error = std::string(operation) + ": " + uc_strerror(result);
        return false;
    }
    void Stop(Fault reason, uint32_t address) noexcept {
        if (fault == Fault::None) { fault = reason; faultAddress = address; }
        uc_emu_stop(engine);
    }
    static void LogoInstruction(uc_engine* engine, uint64_t address, uint32_t, void* user) {
        auto& self = *static_cast<Impl*>(user);
        if (!self.logoRunning || (self.logoRequestedPhase < 0 && !self.frameObserver)) return;
        uint32_t phase = 0;
        if (uc_reg_read(engine, UC_ARM_REG_R5, &phase) != UC_ERR_OK) {
            self.Stop(Fault::Pixel, static_cast<uint32_t>(address)); return;
        }
        const uint32_t frameCount = FirmwareLogo::FrameCount(self.source->screenPatches[self.logoTarget]);
        if (self.frameObserver && phase > 0 && phase <= frameCount &&
            self.fault == Fault::None && !self.haveHigh && self.coverage == PixelCount)
            self.frameObserver(phase - 1, *self.frame, self.observerContext);
        if (self.logoRequestedPhase >= 0 &&
            (phase == static_cast<uint32_t>(self.logoRequestedPhase + 1) || frameCount == 1)) {
            self.logoCaptured = true;
            uc_emu_stop(engine);
        }
    }
    // Timing-only preview MMIO. Guest raster/window/SPI code still executes in
    // full. One read advances this isolated preview clock by one millisecond;
    // the whole-device machine instead uses its actual virtual HCLK timeline.
    static uint64_t PreviewDwtRead(uc_engine*, uint64_t offset, unsigned size, void* user) {
        auto& self = *static_cast<Impl*>(user);
        if (size != 4 || (offset != 0 && offset != 4)) {
            self.Stop(Fault::HardwareRead, 0xe0001000 + static_cast<uint32_t>(offset)); return 0;
        }
        if (!offset) return self.previewDwtControl;
        if ((self.previewDwtControl & 1) && (self.previewDemcr & 0x1000000))
            self.previewCycles += self.profile->halfSettings ? 96000 : 72000;
        return self.previewCycles;
    }
    static void PreviewDwtWrite(uc_engine*, uint64_t offset, unsigned size, uint64_t value, void* user) {
        auto& self = *static_cast<Impl*>(user);
        if (size != 4 || offset != 0) { self.Stop(Fault::HardwareWrite, 0xe0001000 + static_cast<uint32_t>(offset)); return; }
        self.previewDwtControl = static_cast<uint32_t>(value);
    }
    static uint64_t PreviewDebugRead(uc_engine*, uint64_t offset, unsigned size, void* user) {
        auto& self = *static_cast<Impl*>(user);
        if (size != 4 || offset != 0xdfc) { self.Stop(Fault::HardwareRead, 0xe000e000 + static_cast<uint32_t>(offset)); return 0; }
        return self.previewDemcr;
    }
    static void PreviewDebugWrite(uc_engine*, uint64_t offset, unsigned size, uint64_t value, void* user) {
        auto& self = *static_cast<Impl*>(user);
        if (size != 4 || offset != 0xdfc) { self.Stop(Fault::HardwareWrite, 0xe000e000 + static_cast<uint32_t>(offset)); return; }
        self.previewDemcr = static_cast<uint32_t>(value);
    }
    static void ReadHardware(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t, void* user) {
        auto& self = *static_cast<Impl*>(user);
        const bool stm = self.profile->halfSettings;
        if ((address == (stm ? 0x40003808u : 0x40013c08u) && size == (stm ? 4 : 2)) ||
            (address == (stm ? 0x48000010u : 0x40010810u) && size == 4)) return;
        self.Stop(Fault::HardwareRead, static_cast<uint32_t>(address));
    }
    static void ReadIdentity(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t, void* user) {
        auto& self = *static_cast<Impl*>(user);
        const uint32_t first = self.profile->halfSettings ? 0x1ffff7ac : 0x1ffff7f5;
        if (address < first || size <= 0 || address + size > first + 12)
            self.Stop(Fault::HardwareRead, static_cast<uint32_t>(address));
    }
    static void WriteHardware(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t value, void* user) {
        auto& self = *static_cast<Impl*>(user);
        if (address == WatchdogPage && size == 4 && value == 0xaaaa) return;
        const uint32_t gpio = self.profile->halfSettings ? 0x48000000 : 0x40010800;
        if ((address == gpio + 0x18 || address == gpio + 0x28) && size == 4 && value == 0x100) {
            self.dataMode = address == gpio + 0x18;
            return;
        }
        if (address != (self.profile->halfSettings ? 0x4000380cu : 0x40013c0cu) ||
            size != (self.profile->halfSettings ? 4 : 2) || !self.frame) {
            self.Stop(Fault::HardwareWrite, static_cast<uint32_t>(address));
            return;
        }
        ++self.frame->spiBytes;
        self.DisplayByte(static_cast<uint8_t>(value));
    }
    void DisplayByte(uint8_t value) noexcept {
        if (!dataMode) {
            if (haveHigh || ((command == 0x2a || command == 0x2b) && parameterCount != 4)) {
                Stop(Fault::Command, value);
                return;
            }
            command = value;
            parameterCount = 0;
            if (command == 0x2c) {
                if (!haveX || !haveY) { Stop(Fault::Window, command); return; }
                x = x0; y = y0;
            } else if (command != 0x2a && command != 0x2b) {
                Stop(Fault::Command, command);
            }
            return;
        }
        if (command == 0x2a || command == 0x2b) {
            if (parameterCount == parameters.size()) { Stop(Fault::Command, command); return; }
            parameters[parameterCount++] = value;
            if (parameterCount != parameters.size()) return;
            const auto first = static_cast<uint16_t>(uint16_t(parameters[0]) << 8 | parameters[1]);
            const auto last = static_cast<uint16_t>(uint16_t(parameters[2]) << 8 | parameters[3]);
            if (first > last || (command == 0x2a ?
                first < profile->lcdX || last >= profile->lcdX + PanelWidth :
                first < profile->lcdY || last >= profile->lcdY + PanelHeight)) {
                Stop(Fault::Window, command);
                return;
            }
            if (command == 0x2a) { x0 = first; x1 = last; haveX = true; }
            else { y0 = first; y1 = last; haveY = true; }
            return;
        }
        if (command != 0x2c) { Stop(Fault::Pixel, command); return; }
        if (!haveHigh) { high = value; haveHigh = true; return; }
        const size_t index = size_t(y - profile->lcdY) * PanelWidth + x - profile->lcdX;
        frame->rgba[index] = Rgba(static_cast<uint16_t>(uint16_t(high) << 8 | value));
        ++frame->pixelWrites;
        if (!covered[index]) { covered[index] = true; ++coverage; }
        haveHigh = false;
        if (++x > x1) { x = x0; if (++y > y1) y = y0; }
    }
    bool Guard(const HardwareRecord& originalCandidate, HardwareRecord& menuNormalized, std::string& error) {
        auto& embedded = stockPackages[profile->stockIndex];
        if (embedded.records.empty() && !LoadEmbeddedFirmware(embedded, error, model, edition)) return false;
        const auto* record = Canonical(embedded, profile->code);
        if (!record) { error = "Embedded firmware has no selected hardware record"; return false; }
        const HardwareRecord* normalized = &originalCandidate;
        if (source->menuPatch.enabled) {
            menuNormalized = originalCandidate;
            if (!FirmwareMenu::NormalizeForGuard(*record, menuNormalized, error)) return false;
            normalized = &menuNormalized;
        }
        const auto& candidate = *normalized;
        const size_t recordIndex = static_cast<size_t>(record - embedded.records.data());
        std::vector<CodePatch> colors;
        if (!BuildColorPatches(embedded, source->palette, source->backgroundColor, colors, error)) return false;
        const std::vector<uint8_t>* expected = &record->image;
        for (const auto& patch : colors) {
            if (recordIndex >= patch.locations.size() || patch.locations[recordIndex].empty()) continue;
            if (expected != &stock) { stock = record->image; expected = &stock; }
            for (const size_t offset : patch.locations[recordIndex]) {
                if (offset > stock.size() || patch.replacement.size() > stock.size() - offset) {
                    error = "Palette patch lies outside native firmware"; return false;
                }
                std::copy(patch.replacement.begin(), patch.replacement.end(), stock.begin() + offset);
            }
        }
        if (expected != &stock) { stock = record->image; expected = &stock; }
        if (!FirmwareLogo::NormalizeForGuard(stock, candidate, error)) return false;
        const auto& reference = *expected;
        if (candidate.linked_base != profile->flashBase || candidate.sp != profile->stack ||
            candidate.image.size() != reference.size() || candidate.linked_base + candidate.image.size() >= 0x0801f000) {
            error = "Unsupported hardware flash or RAM layout";
            return false;
        }
        for (const auto& anchor : profile->anchors) {
            if (!anchor.length) continue;
            const size_t offset = anchor.address - profile->flashBase;
            if (offset > reference.size() || anchor.length > reference.size() - offset ||
                !FirmwareLayout::MatchesCode(source->layoutElements, profile->code, anchor.address,
                    reference.data() + offset, candidate.image.data() + offset, anchor.length)) {
                error = "Unsupported native renderer layout at " + Hex(anchor.address);
                return false;
            }
        }
        if (!FirmwareLayout::MatchesSxExecutable(source->layoutElements, profile->code,
            profile->flashBase, profile->stack, reference.data(), candidate.image.data(), reference.size())) {
            error = "Unsupported native executable change for selected hardware record";
            return false;
        }
        return true;
    }
    bool Open(std::string& error) {
        Close();
        if (!Check(uc_open(UC_ARCH_ARM, static_cast<uc_mode>(UC_MODE_THUMB | UC_MODE_MCLASS), &engine), "Opening Unicorn", error)) return false;
        if (!Check(uc_ctl_set_cpu_model(engine, UC_CPU_ARM_CORTEX_M4), "Selecting Cortex-M4", error)) return false;
        const uint32_t mappedBase = profile->flashBase & ~0xfffu;
        const size_t flashSize = (profile->flashBase - mappedBase + image.size() + 4095) & ~size_t(4095);
        if (!Check(uc_mem_map(engine, mappedBase, flashSize, UC_PROT_READ | UC_PROT_EXEC), "Mapping firmware flash", error) ||
            !Check(uc_mem_write(engine, profile->flashBase, image.data(), image.size()), "Loading linked firmware", error) ||
            !Check(uc_mem_map(engine, RamBase, RamSize, UC_PROT_READ | UC_PROT_WRITE), "Mapping device RAM", error) ||
            !Check(uc_mem_map(engine, ReturnAddress, 4096, UC_PROT_READ | UC_PROT_EXEC), "Mapping native return", error)) return false;
        const uint8_t breakpoint[] = { 0, 0xbe };
        if (!Check(uc_mem_write(engine, ReturnAddress, breakpoint, sizeof(breakpoint)), "Writing native return", error)) return false;
        for (uint32_t page : { WatchdogPage, GpioPage, SpiPage, 0x48000000u }) {
            if (!Check(uc_mem_map(engine, page, 4096, UC_PROT_READ | UC_PROT_WRITE), "Mapping display hardware", error)) return false;
            uc_hook readHook = 0, writeHook = 0;
            // SRAM read hooks perturb Thumb IT state in Unicorn 2.1.4. Only the
            // physical peripheral pages are observed; all raster code runs intact.
            if (!Check(uc_hook_add(engine, &readHook, UC_HOOK_MEM_READ, reinterpret_cast<void*>(ReadHardware), this, page, page + 4095), "Observing hardware reads", error) ||
                !Check(uc_hook_add(engine, &writeHook, UC_HOOK_MEM_WRITE, reinterpret_cast<void*>(WriteHardware), this, page, page + 4095), "Observing LCD writes", error)) return false;
        }
        // The virtual MCU has an explicitly all-zero unique ID. Only the native
        // identity screen reads it; the UI labels it as emulated, not a device ID.
        if (!Check(uc_mem_map(engine, 0x1ffff000, 4096, UC_PROT_READ), "Mapping virtual MCU identity", error)) return false;
        uc_hook identityReadHook = 0;
        if (!Check(uc_hook_add(engine, &identityReadHook, UC_HOOK_MEM_READ,
            reinterpret_cast<void*>(ReadIdentity), this, 0x1ffff000, 0x1fffffff), "Observing MCU identity", error) ||
            !resourceTrace.Attach(engine, *source, profile->code, profile->flashBase, image.size(), error)) return false;
        if (logoNative) {
            uc_hook logoHook = 0;
            if (!Check(uc_hook_add(engine, &logoHook, UC_HOOK_CODE,
                reinterpret_cast<void*>(LogoInstruction), this, logoBoundary, logoBoundary),
                "Observing native logo phases", error) ||
                !Check(uc_mmio_map(engine, 0xe0001000, 4096, PreviewDwtRead, this, PreviewDwtWrite, this),
                    "Mapping preview cycle counter", error) ||
                !Check(uc_mmio_map(engine, 0xe000e000, 4096, PreviewDebugRead, this, PreviewDebugWrite, this),
                    "Mapping preview debug control", error)) return false;
        }
        uint32_t xpsr = 0x01000000;
        if (!Check(uc_reg_write(engine, UC_ARM_REG_XPSR, &xpsr), "Initializing Thumb state", error) ||
            !Check(uc_context_alloc(engine, &initialCpu), "Allocating CPU state", error) ||
            !Check(uc_context_save(engine, initialCpu), "Saving initial CPU state", error)) return false;
        return true;
    }
    bool Prepare(const StudioState& state, uint16_t selectedCode, std::string& error) {
        const uint16_t canonicalCode = state.model == FirmwareModel::DragX ? 0x3106 : 0x3103;
        const uint16_t code = selectedCode ? selectedCode : canonicalCode;
        const auto* original = Canonical(state.firmware, code);
        if (!original) { error = "Firmware has no selected hardware record"; return false; }
        const bool same = cached && profile && profile->code == code &&
            source == &state && sourceData == original->image.data() &&
            sourceSize == original->image.size() && sourceHash == state.firmware.source_sha256 &&
            revision == state.ResourceRevision() && edition == state.edition && model == state.model;
        if (same) { error = cachedError; return error.empty() && engine; }
        Close();
        cached = true;
        source = &state; sourceData = original->image.data(); sourceSize = original->image.size();
        sourceHash = state.firmware.source_sha256; revision = state.ResourceRevision(); edition = state.edition;
        model = state.model;
        profile = model == FirmwareModel::DragX ?
            (edition == FirmwareEdition::Factory ? &XFactoryProfile : &XScoreProfile) :
            (edition == FirmwareEdition::Factory ? &FactoryProfile : &ScoreProfile);
        const auto index = profile->stockIndex;
        const auto found = std::find_if(std::begin(NativeRecords), std::end(NativeRecords),
            [index, code](const NativeRecordProfile& record) { return record.editionIndex == index && record.code == code; });
        if (found == std::end(NativeRecords)) {
            cached = false;
            error = "No native ABI for selected hardware record";
            return false;
        }
        screenProfile = FindScreens(index, code);
        if (!screenProfile) {
            cached = false;
            error = "No native screen ABI for selected hardware record";
            return false;
        }
        selectedProfile = *profile;
        selectedProfile.code = code;
        selectedProfile.flashBase = found->base;
        selectedProfile.stack = found->stack;
        selectedProfile.scatter = found->scatter;
        selectedProfile.defaults = found->defaults;
        selectedProfile.redraw = found->main;
        selectedProfile.settings = found->settings;
        selectedProfile.fill = found->fill;
        selectedProfile.halfSettings = found->base == 0x08003000;
        selectedProfile.canonical = code == canonicalCode;
        const bool shiftedPanel = code == 0x3014 || code == 0x302a || code == 0x3012 ||
            code == 0x302c || code == 0x3104 || code == 0x3107;
        selectedProfile.lcdX = shiftedPanel ? 26 : 24;
        selectedProfile.lcdY = shiftedPanel ? 1 : 0;
        if (!selectedProfile.canonical) {
            selectedProfile.anchors = {{{found->base + 0x2b0, 16}, {found->defaults, 16},
                {found->main, 16}, {found->raster, 16}, {found->fill, 16}, {found->scatter, 32}, {}, {}}};
        }
        profile = &selectedProfile;
        logoNative = model == FirmwareModel::DragS && std::any_of(source->screenPatches.begin(),
            source->screenPatches.end(), [](const FirmwareLogo::Settings& settings) { return settings.enabled; });
        cachedError.clear();
        std::vector<uint8_t> built;
        FirmwarePackage package;
        if (!state.BuildZwdfBytes(built, cachedError) || !FirmwarePackage::Parse(built, package, cachedError)) {
            error = cachedError;
            return false;
        }
        const auto* record = Canonical(package, profile->code);
        HardwareRecord menuNormalized;
        if (!record) cachedError = "Built firmware has no selected hardware record";
        else if (Guard(*record, menuNormalized, cachedError)) {
            nativeRepaint = std::any_of(source->layoutElements.begin(), source->layoutElements.end(),
                [&](const FirmwareLayout::Element& element) {
                    if (element.hardwareCode != profile->code || element.repaint.empty()) return false;
                    const auto& patch = element.repaint.front();
                    const size_t offset = patch.address - profile->flashBase;
                    return offset <= record->image.size() && patch.replacement.size() <= record->image.size() - offset &&
                        std::equal(patch.replacement.begin(), patch.replacement.end(), record->image.data() + offset);
                });
            logoBoundary = logoInfoExitEntry = 0;
            if (logoNative) {
                const auto& logoRecord = source->menuPatch.enabled ? menuNormalized : *record;
                for (size_t target = 0; target < FirmwareLogo::TargetCount; ++target) {
                    if (!source->screenPatches[target].enabled) continue;
                    const uint32_t boundary = FirmwareLogo::FrameBoundary(logoRecord, static_cast<FirmwareLogo::Target>(target));
                    if (!boundary || (logoBoundary && boundary != logoBoundary)) {
                        cachedError = "Unrecognized native screen envelope"; error = cachedError; return false;
                    }
                    logoBoundary = boundary;
                    const auto& settings = source->screenPatches[target];
                    if (target == size_t(FirmwareLogo::Target::DeviceInfo) &&
                        settings.exitAnimation != FirmwareLogo::Animation::None) {
                        const uint32_t exitPhase = settings.animation == FirmwareLogo::Animation::None ?
                            1u : FirmwareLogo::PhaseCount;
                        logoInfoExitEntry = FirmwareLogo::FrameEntry(logoRecord, FirmwareLogo::Target::DeviceInfo, exitPhase);
                        if (!logoInfoExitEntry) {
                            cachedError = "Unrecognized native service exit entry"; error = cachedError; return false;
                        }
                    }
                }
            }
            image = record->image;
            if (!Open(cachedError)) Close();
        }
        error = cachedError;
        return error.empty() && engine;
    }
    bool Call(uint32_t address, const std::array<uint32_t, 4>& arguments, std::string& error) {
        const std::array<int, 4> registers = { UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3 };
        for (size_t i = 0; i < registers.size(); ++i)
            if (!Check(uc_reg_write(engine, registers[i], &arguments[i]), "Seeding native arguments", error)) return false;
        uint32_t stack = profile->stack, link = ReturnAddress | 1;
        if (!Check(uc_reg_write(engine, UC_ARM_REG_SP, &stack), "Seeding native stack", error) ||
            !Check(uc_reg_write(engine, UC_ARM_REG_LR, &link), "Seeding native return", error)) return false;
        // Bulk observation includes a complete timeline and host-side work at
        // every boundary. Budget wall time per frame; retain the guest instruction cap.
        const uint64_t timeout = logoRunning ? 10000000ull *
            (frameObserver ? FirmwareLogo::FrameCount(source->screenPatches[logoTarget]) : 1u) : 500000ull;
        const uc_err result = uc_emu_start(engine, address | 1, ReturnAddress, timeout, logoRunning ? 200000000 : 20000000);
        uint32_t pc = 0;
        if (!Check(uc_reg_read(engine, UC_ARM_REG_PC, &pc), "Reading stopped CPU", error)) return false;
        if (fault != Fault::None) {
            const char* reason = "Invalid LCD pixel stream";
            switch (fault) {
            case Fault::HardwareRead: reason = "Unsupported hardware read"; break;
            case Fault::HardwareWrite: reason = "Unsupported hardware write"; break;
            case Fault::Command: reason = "Unsupported or incomplete LCD command"; break;
            case Fault::Window: reason = "Invalid LCD address window"; break;
            default: break;
            }
            error = std::string(reason) + " at " + Hex(faultAddress) + " (PC " + Hex(pc) + ")";
            return false;
        }
        if (result != UC_ERR_OK) {
            error = "Native execution failed at " + Hex(pc) + ": " + uc_strerror(result);
            return false;
        }
        if (logoRunning && logoCaptured && pc == logoBoundary) return true;
        if (pc != ReturnAddress) {
            size_t timedOut = 0;
            uc_query(engine, UC_QUERY_TIMEOUT, &timedOut);
            error = std::string(timedOut ? "Native renderer timed out at " : "Native instruction limit reached at ") + Hex(pc);
            return false;
        }
        return true;
    }
    bool Word(uint32_t address, uint32_t value, std::string& error) {
        const std::array<uint8_t, 4> bytes = { uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16), uint8_t(value >> 24) };
        return Check(uc_mem_write(engine, address, bytes.data(), bytes.size()), "Seeding device state", error);
    }
    bool HalfWord(uint32_t address, uint32_t value, std::string& error) {
        const std::array<uint8_t, 2> bytes = { uint8_t(value), uint8_t(value >> 8) };
        return Check(uc_mem_write(engine, address, bytes.data(), bytes.size()), "Seeding device duration", error);
    }
    bool ClearDisplay(const SxScreenProfile& screen, std::string& error) {
        return Word(profile->stack, 0, error) &&
            Call(screen.fill, {0, 0, PanelWidth, PanelHeight}, error);
    }
    bool DrawScreen(const DeviceState& device, std::string& error) {
        const auto& screen = *screenProfile;
        if (logoNative && device.screen == 2 && source->screenPatches[0].enabled &&
            source->screenPatches[0].bootStages == FirmwareLogo::BootStages::Both)
            return ClearDisplay(screen, error) && Call(screen.draw[1] + 0xb0, {}, error);
        const size_t target = (device.screen == 1 || device.screen == 2) ? static_cast<size_t>(FirmwareLogo::Target::Boot) :
            device.screen == 3 ? static_cast<size_t>(FirmwareLogo::Target::Shutdown) :
            device.screen == 15 ? static_cast<size_t>(FirmwareLogo::Target::DeviceInfo) : FirmwareLogo::TargetCount;
        const bool selectedBootStage = target != 0 ||
            (static_cast<uint32_t>(source->screenPatches[0].bootStages) & (device.screen == 2 ? 1u : 2u));
        if (logoNative && target < FirmwareLogo::TargetCount && source->screenPatches[target].enabled && selectedBootStage) {
            if (device.logoPhase < -1 || device.logoPhase >= static_cast<int>(FirmwareLogo::FrameCount(source->screenPatches[target]))) {
                error = "Unsupported native screen animation phase"; return false;
            }
            logoTarget = target;
            logoRequestedPhase = device.logoPhase;
            logoCaptured = false;
            logoRunning = true;
            uint32_t entry = device.screen == 3 ? screen.draw[1] + 0x60 :
                device.screen == 2 ? screen.draw[1] + 0xb0 : screen.draw[device.screen];
            const uint32_t entranceFrames = source->screenPatches[target].animation == FirmwareLogo::Animation::None ?
                1u : FirmwareLogo::PhaseCount;
            if (target == size_t(FirmwareLogo::Target::DeviceInfo) &&
                device.logoPhase >= int(entranceFrames)) entry = logoInfoExitEntry;
            const bool rendered = Call(entry, {}, error);
            logoRunning = false;
            return rendered;
        }
        switch (device.screen) {
        case 1:
            return Call(screen.draw[1], {}, error);
        case 0:
            // Edited firmware performs its own full clear before all native
            // draws. Stock MAIN only clears separators, so seed its physical LCD.
            if (nativeRepaint) return Call(profile->redraw, {}, error);
            return Word(profile->stack, 0, error) &&
                Call(profile->fill, {0, 0, PanelWidth, PanelHeight}, error) &&
                Call(profile->redraw, {}, error);
        case 2:
        case 3: {
            // Capture the completed visible phase through the actual native RGB
            // rasterizer. Coordinates and dimensions come from the staged caller,
            // including after export/reopen; only delays/erase timing are omitted.
            std::array<uint32_t, 4> arguments;
            if (!FirmwareLayout::SxPagePhaseArguments(profile->stockIndex, profile->code,
                device.screen, profile->flashBase, image, arguments)) {
                error = "Unsupported staged native boot/OFF arguments";
                return false;
            }
            return ClearDisplay(screen, error) &&
                Word(profile->stack, device.screen == 2 ? screen.animatedLogo : screen.offLogo, error) &&
                Call(screen.rgb, arguments, error);
        }
        case 4: {
            const auto battery = static_cast<uint32_t>(std::clamp(device.battery, 0, 5));
            return Call(screen.draw[4], {battery, battery * 20, 1, 0}, error);
        }
        case 15:
            // Factory startup initializes the pool separately; Score's identity
            // routine initializes it again harmlessly. Keep its allocator native.
            return Call(screen.pool, {}, error) && Call(screen.draw[15], {}, error);
        case 16:
            return (profile->halfSettings ? HalfWord(profile->settings + 2, 2, error) :
                Word(profile->settings + 4, 2, error)) && Call(screen.draw[16], {}, error);
        case 17: return ClearDisplay(screen, error);
        case 18: {
            for (size_t day = 0; day < device.puffHistory.size(); ++day)
                if (!HalfWord(profile->settings + screen.historyOffset + static_cast<uint32_t>(day) * 2,
                    static_cast<uint32_t>(std::clamp(device.puffHistory[day], 0, 65535)), error)) return false;
            return Call(screen.history, {}, error) &&
                Call(screen.historyUpdate, {static_cast<uint32_t>(std::clamp(device.historySelection, 0, 13)), 0, 0, 0}, error);
        }
        case 19: {
            const uint32_t hour = static_cast<uint32_t>(std::clamp(device.clockHour, 0, 23));
            const uint32_t minute = static_cast<uint32_t>(std::clamp(device.clockMinute, 0, 59));
            const auto bcd = [](uint32_t value) { return (value / 10) << 4 | value % 10; };
            return HalfWord(screen.clockState + 4, bcd(hour) | bcd(minute) << 8, error) &&
                Call(screen.clock, {}, error) &&
                Call(screen.clockUpdate, {hour, minute, static_cast<uint32_t>(std::clamp(device.menuSelection, 0, 1)), 0}, error);
        }
        default: return Call(screen.draw[device.screen], {}, error);
        }
    }
    bool Draw(const DeviceState& device, Frame& output, std::string& error) {
        output.resources.clear();
        output.emulatedUid = false;
        resourceTrace.Reset();
        const auto screens = SxScreens(model, edition, profile->code);
        if (std::none_of(screens.begin(), screens.end(),
            [&device](const ScreenInfo& screen) { return screen.id == device.screen; })) {
            error = "Unsupported native screen selection";
            return false;
        }
        frame = &output;
        fault = Fault::None; faultAddress = 0;
        dataMode = false; command = 0; parameterCount = 0;
        haveX = haveY = haveHigh = false;
        covered.fill(false); coverage = 0;
        output.width = PanelWidth;
        output.height = PanelHeight;
        output.hardwareCode = profile->code;
        output.monochrome = false;
        output.rgba.assign(PixelCount, 0xff000000u);
        previewCycles = previewDwtControl = previewDemcr = 0;
        output.spiBytes = output.pixelWrites = 0;
        if (!Check(uc_context_restore(engine, initialCpu), "Resetting native CPU", error) ||
            !Check(uc_mem_write(engine, RamBase, zeroRam.data(), zeroRam.size()), "Resetting device RAM", error)) return false;
        for (uint32_t page : { WatchdogPage, GpioPage, SpiPage, 0x48000000u })
            if (!Check(uc_mem_write(engine, page, zeroPage.data(), zeroPage.size()), "Resetting display hardware", error)) return false;
        if (!Word(profile->halfSettings ? 0x40003808 : 0x40013c08, 2, error) ||
            !Word(profile->halfSettings ? 0x48000010 : 0x40010810, device.charging ? 0x80 : 0, error)) return false;
        // Execute the firmware's compressed-data and zero-initialization routines,
        // including its own ROM tables. Do not substitute a host-side RAM image.
        for (uint32_t offset : { 0u, 16u }) {
            const uint8_t* entry = image.data() + profile->scatter - profile->flashBase + offset;
            if (!Call(Read32(entry + 12), { Read32(entry), Read32(entry + 4), Read32(entry + 8), 0 }, error)) return false;
        }
        if (!Call(profile->defaults, {}, error)) return false;
        const uint32_t settings = profile->settings;
        const uint32_t watts = static_cast<uint32_t>(std::clamp(device.watts, 5, profile->maxWatts));
        const uint32_t resistance = static_cast<uint32_t>(std::clamp(device.resistanceMilliOhms, 0, 9990));
        const uint32_t puffs = static_cast<uint32_t>(std::clamp(device.puffs, 0, 99999));
        const uint32_t duration = static_cast<uint32_t>(std::clamp(device.timeCentiseconds, 0, 999));
        const uint32_t battery = static_cast<uint32_t>(std::clamp(device.battery, 0, 5));
        if (profile->halfSettings) {
            // STM32F3 settings use halfwords, with RTOS builds carrying extra
            // history entries. Score RTOS uses a single 16-bit puff counter;
            // the other STM32 counters are two big-endian halfwords.
            const bool rtos = profile->stack == 0x200025b8 || profile->stack == 0x200025b0;
            const bool score = edition == FirmwareEdition::Score;
            const uint32_t powerOffset = score ? 0x12 : (rtos ? 0x0e : 0x0a);
            const uint32_t resistanceOffset = score ? (rtos ? 0x24 : 0x20) : (rtos ? 0x20 : 0x18);
            const uint32_t durationOffset = score ? (rtos ? 0x28 : 0x24) : (rtos ? 0x24 : 0x1c);
            const uint32_t puffOffset = score ? (rtos ? 0x50 : 0x2e) : (rtos ? 8 : 0x28);
            const uint32_t mode = score ? (device.rba ? (profile->dragX ? 1u : 2u) : 0u) :
                (profile->dragX ? (device.rba ? 1u : 0u) :
                    (device.rba ? 2u : 0u) + (device.activation ? 0u : 1u));
            if (!Word(0x20000054, 8, error) || !HalfWord(0x2000003a, battery, error) ||
                !HalfWord(settings + 4, mode, error) ||
                !HalfWord(settings + 2, device.activation ? (profile->dragX ? 0 : 2) : (profile->dragX ? 3 : 0), error) ||
                !HalfWord(settings + powerOffset, watts, error) || !HalfWord(settings + powerOffset + 2, watts, error) ||
                !HalfWord(settings + resistanceOffset, resistance, error) ||
                !HalfWord(settings + resistanceOffset + 2, device.locked ? 0 : 1, error) ||
                !HalfWord(settings + durationOffset, duration, error)) return false;
            if (score && rtos) {
                if (!HalfWord(settings + puffOffset, (std::min)(puffs, 9999u), error)) return false;
            } else if (!HalfWord(settings + puffOffset, puffs >> 16, error) ||
                !HalfWord(settings + puffOffset + 2, puffs & 0xffff, error)) return false;
            if (score) {
                const uint32_t accumulated = static_cast<uint32_t>(std::clamp(device.score, 0, 99999)) * 1000;
                if (!HalfWord(settings + 0x0a, static_cast<uint32_t>(std::clamp(device.scoreDisplay, 0, 9999)), error) ||
                    !HalfWord(settings + 0x0c, accumulated >> 16, error) ||
                    !HalfWord(settings + 0x0e, accumulated & 0xffff, error)) return false;
            }
        } else if (edition == FirmwareEdition::Score) {
            if (!Word(0x20000058, 8, error) || !Word(0x2000003c, battery, error)) return false;
            if (!Word(settings + 8, device.rba ? (profile->dragX ? 1 : 2) : 0, error) ||
                !Word(settings + 4, device.activation ? (profile->dragX ? 0 : 2) : (profile->dragX ? 3 : 0), error) ||
                !Word(settings + 0x20, watts, error) || !Word(settings + 0x24, watts, error) ||
                !Word(settings + 0x3c, resistance, error) || !Word(settings + 0x40, device.locked ? 0 : 1, error) ||
                !Word(settings + 0x14, static_cast<uint32_t>(std::clamp(device.scoreDisplay, 0, 9999)), error) ||
                !Word(settings + 0x18, static_cast<uint32_t>(std::clamp(device.score, 0, 99999)) * 1000, error) ||
                !HalfWord(settings + 0x44, duration, error) || !Word(settings + 0x50, puffs, error)) return false;
        } else {
            if (!Word(0x20000058, 8, error) || !Word(0x2000003c, battery, error)) return false;
            const uint32_t mode = profile->dragX ? (device.rba ? 1u : 0u) :
                (device.rba ? 2u : 0u) + (device.activation ? 0u : 1u);
            if (!Word(settings + 8, mode, error) ||
                !Word(settings + 0x14, watts, error) || !Word(settings + 0x18, watts, error) ||
                !Word(settings + 0x34, resistance, error) || !Word(settings + 0x38, device.locked ? 0 : 1, error) ||
                !HalfWord(settings + 0x3c, duration, error) || !Word(settings + 0x4c, puffs, error)) return false;
        }
        if (!DrawScreen(device, error)) return false;
        if (haveHigh || coverage != PixelCount) {
            error = "Native renderer did not produce a complete 80 x 160 LCD frame";
            return false;
        }
        output.emulatedUid = device.screen == 15 &&
            !(logoNative && source->screenPatches[static_cast<size_t>(FirmwareLogo::Target::DeviceInfo)].enabled);
        resourceTrace.Collect(output);
        return true;
    }
};

bool Supports(const StudioState& source) {
    return !AvailableScreens(source).empty();
}
Renderer::Renderer() : impl_(std::make_unique<Impl>()) {}
Renderer::~Renderer() = default;
Renderer::Renderer(Renderer&&) noexcept = default;
Renderer& Renderer::operator=(Renderer&&) noexcept = default;

bool Renderer::Render(const StudioState& source, const DeviceState& device, Frame& output,
    FrameObserver observer, void* context) {
    output.error.clear();
    output.emulatedLoaderMetadata = false;
    auto fail = [&output](std::string error) {
        output.rgba.clear(); output.width = output.height = 0;
        output.spiBytes = output.pixelWrites = 0; output.hardwareCode = 0;
        output.resources.clear(); output.emulatedUid = false;
        output.error = std::move(error);
        return false;
    };
    if (!source.hasFirmware) return fail("No firmware loaded");
    if (!impl_) impl_ = std::make_unique<Impl>();
    if (source.model == FirmwareModel::Drag1 || source.model == FirmwareModel::Drag2 ||
        source.model == FirmwareModel::DragMini || source.model == FirmwareModel::Drag2Refresh ||
        source.model == FirmwareModel::DragMiniRefresh) {
        if (!impl_->legacy) impl_->legacy = CreateLegacyBackend();
        return impl_->legacy->Render(source, device, output);
    }
    if (source.model == FirmwareModel::Drag3 || source.model == FirmwareModel::DragMax) {
        if (!impl_->drag3) impl_->drag3 = CreateDrag3Backend();
        return impl_->drag3->Render(source, device, output);
    }
    if (source.model == FirmwareModel::DragSPro || source.model == FirmwareModel::DragXPro ||
        source.model == FirmwareModel::DragXPlus || source.model == FirmwareModel::DragXPlusPro) {
        if (!impl_->proPlus) impl_->proPlus = CreateProPlusBackend();
        return impl_->proPlus->Render(source, device, output);
    }
    if ((source.model != FirmwareModel::DragS && source.model != FirmwareModel::DragX) ||
        (source.edition != FirmwareEdition::Score && source.edition != FirmwareEdition::Factory))
        return fail("Native screen emulation does not support this firmware profile");
    std::string error;
    if (!impl_->Prepare(source, device.hardwareCode, error)) return fail(std::move(error));
    impl_->frameObserver = observer;
    impl_->observerContext = context;
    const bool success = impl_->Draw(device, output, error);
    impl_->frameObserver = nullptr;
    impl_->observerContext = nullptr;
    impl_->frame = nullptr;
    if (!success) {
        impl_->Close();
        impl_->cached = false;
        return fail(std::move(error));
    }
    return true;
}
}
