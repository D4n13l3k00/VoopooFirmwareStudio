#pragma once
#include "app_state.h"
#include "firmware_emulator.h"
#include "firmware_emulator_screens.h"
#include "firmware_profiles.h"
#include <ostream>

inline bool RunNativeCoverageRegressions(std::string& error, std::ostream* proof = nullptr) {
    using namespace FirmwareEmulator;
    error.clear();
    Renderer renderer;
    size_t records = 0, pages = 0;
    for (const auto& profile : FirmwareProfiles()) {
        FirmwarePackage package;
        StudioState studio;
        if (!LoadEmbeddedFirmware(package, error, profile.model, profile.edition) ||
            !studio.LoadPackage(std::move(package), "native coverage", error)) return false;
        if (!studio.layoutError.empty()) { error = studio.layoutError; return false; }
        for (const auto& record : studio.firmware.records) {
            const auto code = static_cast<uint16_t>(uint16_t(record.device_code[0]) << 8 | record.device_code[1]);
            auto fail = [&](const std::string& reason) {
                error = std::string(FirmwareModelName(profile.model)) + " / " + FirmwareEditionName(profile.edition) +
                    " / HW " + std::to_string(code) + ": " + reason;
                return false;
            };
            const auto screens = AvailableScreens(studio, code);
            if (screens.empty()) return fail("hardware record has no native screens");
            const bool legacy = profile.model == FirmwareModel::Drag1 || profile.model == FirmwareModel::Drag2 ||
                profile.model == FirmwareModel::DragMini || profile.model == FirmwareModel::Drag2Refresh ||
                profile.model == FirmwareModel::DragMiniRefresh;
            const bool landscape = profile.model == FirmwareModel::Drag3 || profile.model == FirmwareModel::DragMax ||
                profile.model == FirmwareModel::DragXPlus || profile.model == FirmwareModel::DragXPlusPro;
            const bool temperature = legacy || profile.edition == FirmwareEdition::Tc;
            auto render = [&](DeviceState device, Frame& frame) {
                device.hardwareCode = code;
                const auto screen = std::find_if(screens.begin(), screens.end(),
                    [&](const ScreenInfo& item) { return item.id == device.screen; });
                if (screen != screens.end()) ApplyScreenMode(device, *screen);
                if (!renderer.Render(studio, device, frame))
                    return fail("screen " + std::to_string(device.screen) + ": " + frame.error);
                if (frame.hardwareCode != code || frame.width <= 0 || frame.height <= 0 ||
                    frame.rgba.size() != size_t(frame.width) * frame.height || !frame.pixelWrites)
                    return fail("native screen used another record, geometry or an empty LCD stream");
                for (size_t resource : frame.resources) {
                    if (resource >= studio.catalog.size()) return fail("native resource trace escaped its document");
                }
                return true;
            };
            for (const auto& screen : screens) {
                DeviceState device;
                device.screen = screen.id;
                Frame first, repeated;
                if (!render(device, first) || !render(device, repeated)) return false;
                if (first.rgba != repeated.rgba || first.width != repeated.width || first.height != repeated.height)
                    return fail("native page depends on previous CPU/RAM state: " + std::string(screen.title));
                for (unsigned row = 1; row < screen.menuItems; ++row) {
                    device.menuSelection = static_cast<int>(row);
                    Frame selected;
                    if (!render(device, selected)) return false;
                    // Native curve-bank selection can show identical initialized
                    // curves; this is not a visual menu highlight.
                    const bool identicalCurveBanks = temperature && !landscape &&
                        (screen.id == 60 || screen.id == 61);
                    if (!identicalCurveBanks && selected.rgba == first.rgba)
                        return fail("native menu selection has no effect: " + std::string(screen.title));
                }
                if (screen.menuConfirmation) {
                    device.menuSelection = 0;
                    device.menuConfirmed = true;
                    Frame confirmed;
                    if (!render(device, confirmed)) return false;
                    if (confirmed.rgba == first.rgba)
                        return fail("native confirmation has no effect: " + std::string(screen.title));
                }
                if (screen.id == 0) {
                    for (const auto [tc, wide] : {std::pair{false, false}, std::pair{true, false}, std::pair{false, true}}) {
                        if ((tc && !temperature) || (wide && !landscape)) continue;
                        device = {};
                        device.temperatureControl = tc;
                        device.landscape = wide;
                        Frame view, changed;
                        if (!render(device, view)) return false;
                        device.watts = 42;
                        device.temperature = 290;
                        if (!render(device, changed)) return false;
                        if (view.rgba == changed.rgba) return fail("native main view ignored its power/temperature inputs");
                    }
                }
                ++pages;
                if (proof) *proof << "NATIVE PAGE " << FirmwareModelName(profile.model) << '/' << FirmwareEditionName(profile.edition)
                    << " hw=" << code << " screen=" << screen.id << " " << first.width << 'x' << first.height << '\n' << std::flush;
            }
            DeviceState invalid;
            invalid.hardwareCode = code;
            invalid.screen = UINT32_MAX;
            Frame rejected;
            if (renderer.Render(studio, invalid, rejected) || rejected.error.empty() || !rejected.rgba.empty())
                return fail("unknown page silently fell back to another page");
            if (studio.HasUnsavedChanges()) return fail("native rendering changed the document");
            ++records;
        }
    }
    if (proof) *proof << "NATIVE COVERAGE OK profiles=" << FirmwareProfiles().size() << " records=" << records
        << " pages=" << pages << '\n' << std::flush;
    return true;
}
