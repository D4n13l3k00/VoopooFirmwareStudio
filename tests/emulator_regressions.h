#pragma once
#include "app_state.h"
#include "firmware_emulator.h"
#include "firmware_emulator_screens.h"
#include "firmware_profiles.h"

inline bool RunEmulatorRegressions(std::string& error) {
    using namespace FirmwareEmulator;
    error.clear();
    auto check = [&](bool ok, const char* message) {
        if (!ok) { error += message; error += "; "; }
    };
    Renderer renderer;
    DeviceState device;
    for (const auto [model, edition] : {
        std::pair{FirmwareModel::DragS, FirmwareEdition::Score},
        std::pair{FirmwareModel::DragS, FirmwareEdition::Factory},
        std::pair{FirmwareModel::DragX, FirmwareEdition::Score},
        std::pair{FirmwareModel::DragX, FirmwareEdition::Factory}}) {
        StudioState editor;
        FirmwarePackage package;
        if (!LoadEmbeddedFirmware(package, error, model, edition) ||
            !editor.LoadPackage(std::move(package), "emulator regression", error)) return false;
        Frame stock, repeated, changed;
        auto render = [&](const DeviceState& state, Frame& frame, const char* stage) {
            if (renderer.Render(editor, state, frame)) return true;
            error = std::string(FirmwareModelName(model)) + " / " + FirmwareEditionName(edition) +
                " / " + stage + ": " + frame.error;
            return false;
        };
        if (!render(device, stock, "main")) return false;
        check(stock.rgba.size() == 80 * 160 && stock.width == 80 && stock.height == 160 &&
            stock.hardwareCode == FirmwareCanonicalCode(model) &&
            stock.spiBytes > stock.rgba.size() * 2 && stock.pixelWrites >= stock.rgba.size(),
            "native renderer did not deliver a full LCD stream");
        if (!render(device, repeated, "repeated main")) return false;
        check(stock.rgba == repeated.rgba, "native frame depends on previous CPU or RAM state");
        auto puffState = device;
        puffState.puffs = 98765;
        if (!render(puffState, changed, "puff count")) return false;
        check(stock.rgba != changed.rgba, "puff count did not update native Score/Factory display");
        auto alternate = device;
        alternate.rba = true;
        alternate.activation = false;
        alternate.locked = true;
        alternate.watts = 42;
        alternate.puffs = 98765;
        alternate.battery = 1;
        alternate.score = 46550;
        if (!render(alternate, changed, "alternate controls")) return false;
        check(stock.rgba != changed.rgba, "device controls did not change native screen");
        const auto accent = std::find_if(editor.palette.begin(), editor.palette.end(),
            [](const PaletteState& color) { return std::string_view(color.spec.id) == "accent_red"; });
        if (accent == editor.palette.end()) { error = "emulator fixture missing accent"; return false; }
        editor.PushUndo("Native screen color regression");
        editor.SetPaletteColor(static_cast<size_t>(accent - editor.palette.begin()), {0, 255, 0});
        editor.EndUndoAction();
        if (!render(device, changed, "palette edit")) return false;
        check(stock.rgba != changed.rgba, "staged palette edits absent from executed firmware");
        editor.Undo();
        if (!render(device, repeated, "palette Undo")) return false;
        check(stock.rgba == repeated.rgba, "native emulator retained palette after Undo");
        // Replace every large-digit mask: a visible wattage digit must change,
        // independently of each edition's resource naming and linked address.
        editor.PushUndo("Native screen bitmap regression");
        bool replaced = false;
        for (size_t i = 0; i < editor.catalog.size(); ++i) {
            const auto& spec = editor.catalog[i];
            if (spec.width != 19 || spec.height != 40) continue;
            editor.SetBitmapReplacement(i, std::vector<uint8_t>(spec.byte_len(), 0));
            replaced = true;
        }
        editor.EndUndoAction();
        check(replaced, "emulator fixture has no wattage masks");
        if (!render(device, changed, "blank wattage masks")) return false;
        check(stock.rgba != changed.rgba, "staged bitmap edits absent from executed firmware");
        editor.Undo();
        if (!render(device, repeated, "bitmap Undo")) return false;
        check(stock.rgba == repeated.rgba && !editor.HasUnsavedChanges(),
            "emulation mutated document or failed bitmap Undo");
    }
    for (const auto& profile : FirmwareProfiles()) {
        const auto model = profile.model;
        const auto edition = profile.edition;
        if (model == FirmwareModel::DragS || model == FirmwareModel::DragX) continue;
        StudioState editor;
        FirmwarePackage package;
        if (!LoadEmbeddedFirmware(package, error, model, edition) ||
            !editor.LoadPackage(std::move(package), "additional emulator regression", error)) return false;
        Frame stock, changed, repeated;
        auto render = [&](const DeviceState& state, Frame& frame, const char* stage) {
            if (renderer.Render(editor, state, frame)) return true;
            error = std::string(FirmwareModelName(model)) + " / " + FirmwareEditionName(edition) +
                " / " + stage + ": " + frame.error;
            return false;
        };
        if (!renderer.Render(editor, device, stock)) {
            error = std::string(FirmwareModelName(model)) + " / " + FirmwareEditionName(edition) + ": " + stock.error;
            return false;
        }
        const bool monochrome = model == FirmwareModel::Drag1 || model == FirmwareModel::Drag2 ||
            model == FirmwareModel::DragMini || model == FirmwareModel::Drag2Refresh || model == FirmwareModel::DragMiniRefresh;
        const bool largePanel = model == FirmwareModel::Drag3 || model == FirmwareModel::DragMax;
        check(stock.width == (monochrome || largePanel ? 128 : 80) &&
            stock.height == (monochrome ? 32 : largePanel ? 220 : 160) &&
            stock.rgba.size() == static_cast<size_t>(stock.width * stock.height) &&
            stock.monochrome == monochrome, "native framebuffer geometry or format incorrect");
        check(std::any_of(stock.rgba.begin(), stock.rgba.end(), [](uint32_t pixel) {
            return pixel != 0xff000000u;
        }), "native screen unexpectedly blank");
        auto alternate = device;
        alternate.watts = 42;
        if (!render(alternate, changed, "power")) return false;
        check(stock.rgba != changed.rgba, "native wattage control has no effect");
        if (largePanel || model == FirmwareModel::DragXPlus || model == FirmwareModel::DragXPlusPro) {
            alternate = device;
            alternate.landscape = true;
            if (!render(alternate, changed, "landscape")) return false;
            check(changed.width == (largePanel ? 220 : 160) && changed.height == (largePanel ? 128 : 80),
                "native landscape screen dimensions incorrect");
            if (edition == FirmwareEdition::Tc) {
                alternate.landscape = false;
                alternate.temperatureControl = true;
                alternate.temperature = 230;
                if (!render(alternate, changed, "temperature")) return false;
                check(changed.rgba != stock.rgba && changed.width == (largePanel ? 128 : 80) &&
                    changed.height == (largePanel ? 220 : 160),
                    "native temperature screen did not appear");
            }
        }
        editor.PushUndo("Additional native bitmap regression");
        for (size_t i = 0; i < editor.catalog.size(); ++i)
            editor.SetBitmapReplacement(i, std::vector<uint8_t>(editor.catalog[i].byte_len(), 0));
        editor.EndUndoAction();
        if (!render(device, changed, "edited resources")) return false;
        check(stock.rgba != changed.rgba, "native additional profile ignored edited resources");
        editor.Undo();
        if (!render(device, repeated, "Undo")) return false;
        check(stock.rgba == repeated.rgba && !editor.HasUnsavedChanges(),
            "native additional profile retained edits after Undo");
    }
    for (const auto& profile : FirmwareProfiles()) {
        const auto model = profile.model;
        const auto edition = profile.edition;
        StudioState editor;
        FirmwarePackage package;
        if (!LoadEmbeddedFirmware(package, error, model, edition) ||
            !editor.LoadPackage(std::move(package), "native screen regression", error)) return false;
        Frame main;
        if (!renderer.Render(editor, device, main)) { error = main.error; return false; }
        if (main.resources.empty()) {
            error = std::string(FirmwareModelName(model)) + " / " + FirmwareEditionName(edition) +
                ": native main screen did not report resource reads";
            return false;
        }
        bool differentScreen = false;
        for (const auto& screen : AvailableScreens(editor)) {
            auto state = device;
            state.screen = screen.id;
            Frame frame;
            if (!renderer.Render(editor, state, frame)) {
                error = std::string(FirmwareModelName(model)) + " / " + FirmwareEditionName(edition) +
                    " / " + std::string(screen.title) + ": " + frame.error;
                return false;
            }
            check(frame.width == main.width && frame.height == main.height &&
                frame.rgba.size() == main.rgba.size() && frame.pixelWrites > 0,
                "native selected screen did not deliver its LCD frame");
            differentScreen |= frame.rgba != main.rgba;
            check(std::all_of(frame.resources.begin(), frame.resources.end(),
                [&](size_t index) { return index < editor.catalog.size(); }),
                "native screen resource report belongs to another firmware");
            for (unsigned row = 1; row < screen.menuItems; ++row) {
                state.menuSelection = static_cast<int>(row);
                Frame selected;
                if (!renderer.Render(editor, state, selected) || selected.width != frame.width ||
                    selected.height != frame.height || selected.pixelWrites == 0 ||
                    (!main.monochrome && selected.rgba == frame.rgba)) {
                    error = std::string(FirmwareModelName(model)) + " / " + FirmwareEditionName(edition) +
                        " / " + std::string(screen.title) + " / row " + std::to_string(row + 1) +
                        ": native selection frame incorrect: " + selected.error;
                    return false;
                }
            }
            if (screen.menuItems) {
                state.menuSelection = 0;
                Frame restored;
                if (!renderer.Render(editor, state, restored) || restored.rgba != frame.rgba) {
                    error = std::string(FirmwareModelName(model)) + " / " + std::string(screen.title) +
                        ": selection leaked into a subsequent redraw: " + restored.error;
                    return false;
                }
            }
            if (screen.menuConfirmation) {
                state.menuSelection = 0;
                state.menuConfirmed = true;
                Frame confirmed;
                if (!renderer.Render(editor, state, confirmed) || confirmed.rgba == frame.rgba) {
                    error = std::string(FirmwareModelName(model)) + " / " + std::string(screen.title) +
                        ": native confirmation did not change: " + confirmed.error;
                    return false;
                }
            }
        }
        check(differentScreen, "screen selector always rendered the main screen");
        auto invalid = device;
        invalid.screen = UINT32_MAX;
        Frame rejected;
        check(!renderer.Render(editor, invalid, rejected) && rejected.rgba.empty() && !rejected.error.empty(),
            "unknown native screen silently rendered a fallback");
        check(!editor.HasUnsavedChanges(), "native screen selection changed the document");
    }
    StudioState empty;
    Frame rejected;
    rejected.rgba.assign(10, 0xffffffff);
    check(!renderer.Render(empty, device, rejected) && rejected.rgba.empty() && !rejected.error.empty(),
        "unsupported document displayed a stale frame");
    return error.empty();
}
