#pragma once
#include "firmware_emulator.h"
#include "firmware_catalog.h"
#include <span>
#include <string_view>
#include <unicorn/unicorn.h>

namespace FirmwareEmulator {
struct ScreenInfo {
    uint32_t id;
    std::string_view title;
    unsigned menuItems = 0;
    bool temperatureMaterial = false;
    bool menuConfirmation = false;
    int temperatureMode = -1; // Native page forces VW/TC; -1 follows DeviceState.
    int orientation = -1; // Native page forces portrait/landscape; -1 follows DeviceState.
};
inline bool ApplyScreenMode(DeviceState& device, const ScreenInfo& screen) {
    bool changed = false;
    if (screen.temperatureMode >= 0) {
        const bool mode = screen.temperatureMode != 0;
        changed |= device.temperatureControl != mode;
        device.temperatureControl = mode;
    }
    if (screen.orientation >= 0) {
        const bool landscape = screen.orientation != 0;
        changed |= device.landscape != landscape;
        device.landscape = landscape;
    }
    return changed;
}
std::span<const ScreenInfo> AvailableScreens(const StudioState& source, uint16_t hardwareCode = 0);
std::span<const ScreenInfo> SxScreens(FirmwareModel model, FirmwareEdition edition, uint16_t hardwareCode = 0);
std::span<const ScreenInfo> LegacyScreens(FirmwareModel model, FirmwareEdition edition, uint16_t hardwareCode = 0);
std::span<const ScreenInfo> ThreeMaxScreens(FirmwareModel model, FirmwareEdition edition, uint16_t hardwareCode = 0);
std::span<const ScreenInfo> ProPlusScreens(FirmwareModel model, FirmwareEdition edition, uint16_t hardwareCode = 0);

// Observes native Flash data reads, never SRAM or raster callbacks.
class ResourceReadTrace {
public:
    bool Attach(uc_engine*, const StudioState&, uint16_t hardwareCode,
        uint32_t flashBase, size_t flashSize, std::string& error);
    void Reset();
    void Collect(Frame& frame) const;
private:
    struct Range { uint32_t first, last; size_t index; };
    std::vector<Range> ranges_;
    std::vector<bool> used_;
    static void Read(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t, void* user);
    std::vector<uint32_t> maximumEnds_;
};
}
