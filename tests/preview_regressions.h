#pragma once
#include "app_state.h"
#include "firmware_preview.h"
#include "firmware_emulator_screens.h"
#include "firmware_profiles.h"

inline bool RunPreviewRegressions(std::string& error) {
    using namespace FirmwareEmulator;
    error.clear();
    auto check = [&](bool ok, const char* message) {
        if (!ok) { error += message; error += "; "; }
        return ok;
    };
    auto sameFrame = [](const Frame& a, const Frame& b) {
        return a.width == b.width && a.height == b.height && a.monochrome == b.monochrome &&
            a.hardwareCode == b.hardwareCode && a.rgba == b.rgba && a.resources == b.resources &&
            a.spiBytes == b.spiBytes && a.pixelWrites == b.pixelWrites && a.error == b.error;
    };
    for (const auto& profile : FirmwareProfiles()) {
        StudioState editor;
        FirmwarePackage package;
        if (!LoadEmbeddedFirmware(package, error, profile.model, profile.edition) ||
            !editor.LoadPackage(std::move(package), "native preview regression", error)) return false;
        FirmwarePreview::Cache preview;
        DeviceState device;
        auto render = [&] {
            if (preview.Render(editor, device)) return true;
            error += std::string(FirmwareModelName(profile.model)) + " / " + FirmwareEditionName(profile.edition) +
                ": before: " + preview.Before().error + "; after: " + preview.After().error;
            return false;
        };
        if (!render()) return false;
        const Frame opened = preview.Before();
        check(sameFrame(opened, preview.After()), "unedited before/after native frames differ");
        const auto beforeGeneration = preview.BeforeGeneration();
        const auto afterGeneration = preview.AfterGeneration();
        if (!render()) return false;
        check(beforeGeneration == preview.BeforeGeneration() && afterGeneration == preview.AfterGeneration(),
            "unchanged preview reran native firmware");
        editor.PushUndo("Before/after native bitmap regression");
        for (size_t i = 0; i < editor.catalog.size(); ++i)
            editor.SetBitmapReplacement(i, std::vector<uint8_t>(editor.catalog[i].byte_len(), 0));
        editor.EndUndoAction();
        const auto revision = editor.ResourceRevision();
        const auto undoCount = editor.undoStack.size();
        const auto redoCount = editor.redoStack.size();
        const auto replacements = editor.replacements;
        const bool dirty = editor.HasUnsavedChanges();
        if (!render()) return false;
        check(sameFrame(opened, preview.Before()) && beforeGeneration == preview.BeforeGeneration(),
            "staged edit changed or reran the opened-firmware native frame");
        check(opened.rgba != preview.After().rgba, "staged bitmap edit absent from after native frame");
        check(editor.ResourceRevision() == revision && editor.undoStack.size() == undoCount &&
            editor.redoStack.size() == redoCount && editor.replacements == replacements &&
            editor.HasUnsavedChanges() == dirty, "before/after preview changed editor history or dirty state");
        editor.Undo();
        if (!render()) return false;
        check(sameFrame(opened, preview.After()) && !editor.HasUnsavedChanges(),
            "after preview failed to follow atomic Undo");
        device.watts = 42;
        device.puffs = 98765;
        device.battery = 1;
        device.locked = true;
        if (!render()) return false;
        check(sameFrame(preview.Before(), preview.After()) && preview.Before().rgba != opened.rgba,
            "before/after did not use the same changed DeviceState");
        device.screen = AvailableScreens(editor).back().id;
        if (!render()) return false;
        Renderer independent;
        Frame expected;
        if (!independent.Render(editor, device, expected)) { error = expected.error; return false; }
        check(sameFrame(expected, preview.Before()) && sameFrame(expected, preview.After()),
            "before/after selected screen differs from independent native execution");
        if (profile.model == FirmwareModel::DragS || profile.model == FirmwareModel::DragX) {
            for (const auto& record : editor.firmware.records) {
                auto revisionDevice = device;
                revisionDevice.hardwareCode = static_cast<uint16_t>(
                    (record.device_code[0] << 8) | record.device_code[1]);
                const auto screens = AvailableScreens(editor, revisionDevice.hardwareCode);
                if (!check(!screens.empty() && screens.front().id == 0,
                    "revision screen catalog lost the native Main screen")) return false;
                const auto hasScreen = [&](uint32_t id) {
                    return std::any_of(screens.begin(), screens.end(),
                        [id](const ScreenInfo& screen) { return screen.id == id; });
                };
                const bool stm = record.linked_base == 0x08003000;
                const bool rtos = record.sp == 0x200025b8 || record.sp == 0x200025b0;
                check(hasScreen(16) == (profile.edition == FirmwareEdition::Score || stm) &&
                    hasScreen(18) == rtos && hasScreen(19) == rtos,
                    "native revision catalog lost a real page or advertised an absent page");
                std::vector<uint32_t> mainPixels;
                for (const auto& screen : screens) {
                    revisionDevice.screen = screen.id;
                    if (!preview.Render(editor, revisionDevice)) {
                        error += std::string(FirmwareModelName(profile.model)) + " / " +
                            FirmwareEditionName(profile.edition) + " / HW " +
                            std::to_string(revisionDevice.hardwareCode) + " / " + std::string(screen.title) +
                            ": revision catalog advertised an unsupported native screen: " +
                            preview.Before().error + preview.After().error;
                        return false;
                    }
                    check(preview.Before().hardwareCode == revisionDevice.hardwareCode &&
                        sameFrame(preview.Before(), preview.After()),
                        "revision screen preview used another hardware record or stale frame");
                    if (screen.id == 0) mainPixels = preview.Before().rgba;
                    else if (screen.id != 17) {
                        check(preview.Before().rgba != mainPixels,
                            "native secondary page was replaced by the Main screen");
                        check(std::any_of(preview.Before().rgba.begin(), preview.Before().rgba.end(),
                            [](uint32_t pixel) { return pixel != 0xff000000u; }),
                            "native secondary page produced only a cleared LCD");
                    }
                    if (screen.id == 18 || screen.id == 19) {
                        const auto pixels = preview.Before().rgba;
                        auto changedDevice = revisionDevice;
                        if (screen.id == 18) {
                            changedDevice.puffHistory.fill(0);
                            changedDevice.historySelection = 0;
                        } else {
                            changedDevice.clockHour = 23;
                            changedDevice.clockMinute = 59;
                            changedDevice.menuSelection = 1;
                        }
                        if (!preview.Render(editor, changedDevice)) {
                            error += "native history/clock state change failed: " +
                                preview.Before().error + preview.After().error;
                            return false;
                        }
                        check(preview.Before().rgba != pixels && sameFrame(preview.Before(), preview.After()),
                            "native history/clock ignored its shared preview state");
                    }
                }
                revisionDevice.screen = 0;
                if (!preview.Render(editor, revisionDevice)) {
                    error += "main redraw after another native page failed: " +
                        preview.Before().error + preview.After().error;
                    return false;
                }
                check(preview.Before().rgba == mainPixels,
                    "native secondary page leaked state into the next Main redraw");
                auto changedPuffs = revisionDevice;
                changedPuffs.puffs = 9876;
                if (!preview.Render(editor, changedPuffs)) {
                    error += "native revision puff update failed: " +
                        preview.Before().error + preview.After().error;
                    return false;
                }
                check(preview.Before().rgba != mainPixels && sameFrame(preview.Before(), preview.After()),
                    "native revision ignored its puff counter or read the wrong counter width");
            }
        }
        device.screen = UINT32_MAX;
        check(!preview.Render(editor, device) && !preview.Before().error.empty() &&
            !preview.After().error.empty() && preview.Before().rgba.empty() && preview.After().rgba.empty(),
            "failed native preview retained a stale or fallback framebuffer");
    }

    // A customized opened package must be the baseline, never embedded factory.
    FirmwarePackage stock;
    if (!LoadEmbeddedFirmware(stock, error, FirmwareModel::DragS, FirmwareEdition::Score)) return false;
    const auto stockHash = stock.source_sha256;
    const auto stockBytes = stock.source_bytes;
    std::optional<StudioState> slot;
    slot.emplace();
    if (!slot->LoadPackage(stock, "same path", error)) return false;
    auto* const address = &*slot;
    FirmwarePreview::Cache preview;
    DeviceState device;
    if (!preview.Render(*slot, device)) { error = preview.Before().error + preview.After().error; return false; }
    const Frame factory = preview.Before();
    const auto initialRevision = slot->ResourceRevision();
    const auto accent = std::find_if(slot->palette.begin(), slot->palette.end(),
        [](const PaletteState& color) { return std::string_view(color.spec.id) == "accent_red"; });
    if (!check(accent != slot->palette.end(), "preview fixture lacks accent")) return false;
    slot->PushUndo("Custom opened firmware");
    slot->SetPaletteColor(static_cast<size_t>(accent - slot->palette.begin()), {0, 255, 0});
    slot->EndUndoAction();
    std::vector<uint8_t> customBytes;
    if (!slot->BuildToBuffer(customBytes, error)) return false;
    FirmwarePackage custom;
    if (!FirmwarePackage::Parse(customBytes, custom, error)) return false;
    // Colliding metadata deliberately proves the parsed record bytes are checked.
    custom.source_sha256 = stockHash;
    custom.source_bytes = stockBytes;
    slot.emplace();
    if (!slot->LoadPackage(std::move(custom), "same path", error)) return false;
    check(&*slot == address && slot->ResourceRevision() == initialRevision,
        "preview reload fixture did not reuse document address and revision");
    if (!preview.Render(*slot, device)) { error = preview.Before().error + preview.After().error; return false; }
    const Frame customOpened = preview.Before();
    check(factory.rgba != customOpened.rgba && sameFrame(customOpened, preview.After()),
        "preview reused same-address/hash/revision cache or substituted factory baseline");
    slot->PushUndo("Stage color on custom original");
    slot->SetPaletteColor(static_cast<size_t>(std::find_if(slot->palette.begin(), slot->palette.end(),
        [](const PaletteState& color) { return std::string_view(color.spec.id) == "accent_red"; }) - slot->palette.begin()),
        {0, 0, 255});
    slot->EndUndoAction();
    if (!preview.Render(*slot, device)) { error = preview.Before().error + preview.After().error; return false; }
    check(sameFrame(customOpened, preview.Before()) && customOpened.rgba != preview.After().rgba,
        "custom opened firmware baseline changed with staged palette edit");
    slot->firmware.records.clear();
    check(!preview.Render(*slot, device, true) && !preview.Before().error.empty() &&
        preview.Before().rgba.empty() && !preview.Original(), "original-load failure was hidden or reused stale original");
    return error.empty();
}
