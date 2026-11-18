#pragma once
#include "app_state.h"

inline bool RunHistoryRegressions(std::string& error) {
    error.clear();
    auto check = [&](bool ok, const char* message) {
        if (!ok) { error += message; error += "; "; }
    };
    auto initialize = [](StudioState& state, size_t bytes = 8) {
        state.hasFirmware = true;
        state.catalog = {{"rgb_history", "", "", 0, 2, 2, 0, AssetFormat::Rgb565},
                         {"other_history", "", "", 0, 2, 2, 0, AssetFormat::Rgb565}};
        state.originals = {std::vector<uint8_t>(bytes, 0), std::vector<uint8_t>(bytes, 0)};
        state.referenceBitmaps = state.originals;
        state.palette = GetDefaultPalette();
        state.UpdateVisible();
        state.MarkSaved();
    };

    StudioState customizedFile;
    initialize(customizedFile);
    customizedFile.originals[0].assign(8, 7);
    customizedFile.MarkSaved();
    check(!customizedFile.IsActiveMatchingReference(0) && !customizedFile.HasUnsavedChanges(),
          "loaded nonfactory bytes were mistaken for unsaved changes");
    customizedFile.RevertAssetToStock(0);
    check(customizedFile.HasUnsavedChanges(), "stock restore of custom file was not dirty");
    customizedFile.Undo();
    check(!customizedFile.HasUnsavedChanges() && customizedFile.GetCurrentRaw(0)[0] == 7,
          "stock restore undo failed to reach loaded custom baseline");

    StudioState state;
    initialize(state);
    const auto originalPalette = state.palette;
    const auto originalBackground = state.backgroundColor;
    auto theme = GetPaletteThemes()[1];
    theme.background = {8, 16, 24};
    ApplyPaletteTheme(state, theme);
    check(state.HasUnsavedChanges(), "theme edit was not dirty");
    check(state.undoStack.size() == 1, "theme was split into multiple history entries");
    state.textureNeedsUpdate = false;
    check(state.Undo(), "theme undo unavailable");
    for (size_t i = 0; i < originalPalette.size(); ++i)
        check(state.palette[i].current == originalPalette[i].current, "theme undo lost palette role");
    check(state.backgroundColor == originalBackground, "theme undo lost background");
    check(!state.HasUnsavedChanges(), "undo to loaded baseline stayed dirty");
    check(state.textureNeedsUpdate, "undo did not invalidate preview textures");
    check(std::string(state.hexEditBuf) == state.palette[0].current.ToHex(), "undo left stale role HEX");
    check(std::string(state.bgHexEditBuf) == originalBackground.ToHex(), "undo left stale background HEX");
    check(state.Redo() && state.backgroundColor == theme.background, "theme redo lost background");
    check(state.palette[0].current == theme.accent_red, "theme redo lost palette");
    state.MarkSaved();
    check(!state.HasUnsavedChanges(), "successful save baseline was dirty");
    check(state.Undo() && state.HasUnsavedChanges(), "undo away from saved theme was clean");
    check(state.Redo() && !state.HasUnsavedChanges(), "redo to saved theme was dirty");

    state.PushUndo("role edit");
    state.SetPaletteColor(0, {16, 32, 48});
    state.MarkSaved();
    state.PushUndo("equivalent firmware color");
    state.SetPaletteColor(0, {17, 33, 49});
    check(!state.HasUnsavedChanges(), "equivalent RGB565 palette color was dirty");
    state.PushUndo("brush only");
    state.SetAssetPaintColor("rgb_history", {255, 0, 0});
    check(!state.HasUnsavedChanges(), "nonpersisted brush color was dirty");
    check(std::string(state.assetHexEditBuf) == "#FF0000", "brush setter left stale HEX");
    state.Undo();
    check(state.assetPaintColors.empty() && std::string(state.assetHexEditBuf) == "#FFFFFF",
          "brush undo failed to restore absent brush and HEX");
    state.Redo();
    check(state.GetPaintColor(state.catalog[0]) == Rgb8(255, 0, 0), "brush redo failed");

    StudioState bitmaps;
    initialize(bitmaps);
    bitmaps.PushUndo("one stroke");
    bitmaps.SetBitmapReplacement(0, std::vector<uint8_t>(8, 1));
    bitmaps.SetBitmapReplacement(0, std::vector<uint8_t>(8, 2));
    check(bitmaps.undoStack.size() == 1 && bitmaps.undoStack.back().bitmaps.size() == 1,
          "stroke duplicated resource history");
    check(bitmaps.Undo() && bitmaps.replacements.empty(), "stroke undo did not restore original");
    check(bitmaps.Redo() && bitmaps.GetCurrentRaw(0)[0] == 2, "stroke redo lost final pixels");
    bitmaps.MarkSaved();
    check(!bitmaps.HasUnsavedChanges(), "saved bitmap replacement stayed dirty");
    bitmaps.PushUndo("manual restore");
    bitmaps.SetBitmapReplacement(0, bitmaps.originals[0]);
    check(bitmaps.replacements.empty() && bitmaps.HasUnsavedChanges(), "factory restore was mistaken for saved state");
    bitmaps.Undo();
    check(!bitmaps.HasUnsavedChanges(), "undo to saved bitmap stayed dirty");
    bitmaps.Redo();
    check(bitmaps.HasUnsavedChanges(), "redo away from saved bitmap was clean");
    bitmaps.Undo();
    bitmaps.PushUndo("no-op");
    bitmaps.SetBitmapReplacement(0, std::vector<uint8_t>(8, 2));
    check(bitmaps.CanRedo(), "no-op edit discarded redo");
    bitmaps.PushUndo("branch");
    bitmaps.SetBitmapReplacement(1, std::vector<uint8_t>(8, 3));
    check(!bitmaps.CanRedo(), "new branch retained obsolete redo");
    check(bitmaps.undoStack.back().bitmaps.size() == 1 &&
          bitmaps.undoStack.back().bitmaps.contains("other_history"),
          "editing one bitmap copied unrelated replacements");
    bitmaps.RevertAllAssets();
    check(bitmaps.replacements.empty(), "revert all did not clear replacements");
    bitmaps.Undo();
    check(bitmaps.GetCurrentRaw(0)[0] == 2 && bitmaps.GetCurrentRaw(1)[0] == 3,
          "revert-all undo lost multi-resource transaction");

    auto accountingMatches = [](const StudioState& document) {
        size_t bytes = 0;
        for (const auto& delta : document.undoStack) bytes += delta.MemoryBytes();
        for (const auto& delta : document.redoStack) bytes += delta.MemoryBytes();
        return document.HistoryMemoryBytes() == bytes;
    };
    check(accountingMatches(bitmaps), "branching or multi-resource undo corrupted byte accounting");
    StudioState longHistory;
    initialize(longHistory);
    constexpr int smallEditCount = 200;
    for (int edit = 1; edit <= smallEditCount; ++edit) {
        longHistory.PushUndo("small edit");
        longHistory.SetBitmapReplacement(0, std::vector<uint8_t>(8, (uint8_t)edit));
    }
    check(longHistory.HistoryMemoryBytes() < StudioState::DefaultHistoryMemoryBudget &&
          longHistory.undoStack.size() == smallEditCount,
          "small edits below byte budget were limited by action count");
    for (int edit = smallEditCount; edit > 0; --edit)
        check(longHistory.Undo() && longHistory.GetCurrentRaw(0)[0] == edit - 1,
              "long history lost an undo below byte budget");
    check(!longHistory.HasUnsavedChanges() && !longHistory.HasUnsavedChanges(),
          "long undo sequence left stale dirty cache");
    check(accountingMatches(longHistory), "long undo sequence corrupted byte accounting");
    for (int edit = 1; edit <= smallEditCount; ++edit)
        check(longHistory.Redo() && longHistory.GetCurrentRaw(0)[0] == edit,
              "long history lost a redo below byte budget");
    check(longHistory.HasUnsavedChanges() && longHistory.HasUnsavedChanges(),
          "long redo sequence left stale clean cache");
    check(accountingMatches(longHistory), "long redo sequence corrupted byte accounting");

    StudioState bounded;
    initialize(bounded, 4096);
    bounded.PushUndo("seed");
    bounded.SetBitmapReplacement(0, std::vector<uint8_t>(4096, 1));
    bounded.PushUndo("second");
    bounded.SetBitmapReplacement(0, std::vector<uint8_t>(4096, 2));
    const size_t oneEntryBudget = bounded.undoStack.back().MemoryBytes();
    bounded.SetHistoryMemoryBudget(oneEntryBudget);
    check(bounded.undoStack.size() == 1 && bounded.HistoryMemoryBytes() <= oneEntryBudget,
          "history did not evict oldest entries to byte budget");
    for (uint8_t value = 3; value < 20; ++value) {
        bounded.PushUndo("stroke");
        bounded.SetBitmapReplacement(0, std::vector<uint8_t>(4096, value));
        check(bounded.HistoryMemoryBytes() <= oneEntryBudget, "stroke exceeded history byte budget");
    }
    check(bounded.Undo() && bounded.GetCurrentRaw(0)[0] == 18, "bounded undo lost newest edit");
    check(bounded.HistoryMemoryBytes() <= oneEntryBudget, "undo exceeded history byte budget");
    check(bounded.Redo() && bounded.GetCurrentRaw(0)[0] == 19, "bounded redo lost newest edit");
    bounded.SetHistoryMemoryBudget(1);
    bounded.PushUndo("oversize action");
    bounded.SetBitmapReplacement(0, std::vector<uint8_t>(4096, 20));
    bounded.SetBitmapReplacement(1, std::vector<uint8_t>(4096, 20));
    check(!bounded.CanUndo() && bounded.HistoryMemoryBytes() == 0,
          "oversize action retained partial undo history");
    check(bounded.GetCurrentRaw(0)[0] == 20 && bounded.GetCurrentRaw(1)[0] == 20,
          "history budget prevented document changes");
    check(accountingMatches(bounded), "budget eviction corrupted cached byte accounting");

    StudioState filtered;
    initialize(filtered);
    filtered.filterOnlyCustom = true;
    filtered.PushUndo("custom asset");
    filtered.SetBitmapReplacement(0, std::vector<uint8_t>(8, 1));
    filtered.UpdateVisible();
    check(filtered.visibleAssetIndices.size() == 1, "custom filter fixture failed");
    filtered.Undo();
    check(filtered.visibleAssetIndices.empty() && filtered.selectedVisibleIdx == -1,
          "undo left stale custom-only asset list");
    filtered.Redo();
    check(filtered.visibleAssetIndices.size() == 1 && filtered.selectedVisibleIdx == 0,
          "redo failed to restore custom-only asset list");

    // Reloading a package starts a new clean document, even after a saved edit.
    FirmwarePackage embedded;
    std::string loadError;
    if (LoadEmbeddedFirmware(embedded, loadError)) {
        state.referenceBitmaps.clear();
        check(state.LoadPackage(std::move(embedded), "embedded", loadError), "load package failed");
        check(!state.HasUnsavedChanges() && !state.CanUndo() && !state.CanRedo(),
              "load package retained dirty baseline or history");
        const auto loadedIdentity = state.DocumentIdentity();
        const auto cleanRevision = state.ResourceRevision();
        const FirmwareMenu::Settings selectedMenu{true, true, true,
            {FirmwareMenu::Item::Exit, FirmwareMenu::Item::PuffEditor,
             FirmwareMenu::Item::Information, FirmwareMenu::Item::Voltage},
            FirmwareMenu::InformationMask | FirmwareMenu::PuffEditorMask, FirmwareMenu::InfoResistanceMask};
        state.PushUndo("Device menu and imported resource");
        state.SetMenuPatch(selectedMenu);
        auto importedPixels = state.originals.front();
        importedPixels.front() ^= 1;
        state.SetBitmapReplacement(0, std::move(importedPixels));
        state.EndUndoAction();
        check(state.menuPatch == selectedMenu &&
              state.ResourceRevision() > cleanRevision && state.HasUnsavedChanges() &&
              state.undoStack.size() == 1 && state.undoStack.back().menuPatch.has_value() &&
              state.undoStack.back().bitmaps.size() == 1,
              "menu/resource transaction lost settings, revision, dirty state, or inverse delta");
        const auto cpu = state.CreateEmulationSnapshot();
        const auto file = state.CreateFileSnapshot();
        check(cpu->menuPatch == state.menuPatch && file->menuPatch == state.menuPatch &&
              file->originalMenuPatch == state.originalMenuPatch &&
              cpu->DocumentIdentity() == loadedIdentity && cpu->ResourceRevision() == state.ResourceRevision(),
              "CPU/file snapshot omitted menu inputs or source generation");
        state.MarkSaved();
        check(state.Undo() && state.menuPatch == FirmwareMenu::Settings{} &&
              state.replacements.empty() && state.HasUnsavedChanges(),
              "menu/resource undo did not leave the saved baseline atomically");
        check(state.Redo() && state.menuPatch == selectedMenu &&
              state.GetCurrentRaw(0)[0] != state.originals[0][0] && !state.HasUnsavedChanges(),
              "menu/resource redo did not restore the saved baseline atomically");
        check(accountingMatches(state), "menu inverse delta corrupted history byte accounting");
        state.RevertAllAssets();
        check(state.menuPatch == state.originalMenuPatch && state.replacements.empty(),
              "restore original omitted device menu settings");
        check(state.Undo() && state.menuPatch == selectedMenu &&
              !state.HasUnsavedChanges(), "restore-original undo omitted menu settings");
        state.PushUndo("Menu no-op");
        state.SetMenuPatch(state.menuPatch);
        check(state.CanRedo(), "menu no-op discarded redo");
        const auto fieldRevision = state.ResourceRevision();
        auto changedFields = selectedMenu;
        changedFields.infoFields = FirmwareMenu::InfoBatteryMask | FirmwareMenu::InfoDurationMask;
        state.PushUndo("Menu INFO fields");
        state.SetMenuPatch(changedFields);
        state.EndUndoAction();
        check(state.menuPatch == changedFields && state.ResourceRevision() > fieldRevision &&
              state.HasUnsavedChanges(), "INFO field edit omitted revision or saved-state invalidation");
        check(state.Undo() && state.menuPatch == selectedMenu && !state.HasUnsavedChanges(),
              "INFO field undo did not restore saved baseline");
        check(state.Redo() && state.menuPatch == changedFields && state.HasUnsavedChanges() &&
              state.Undo() && !state.HasUnsavedChanges(),
              "INFO field redo/undo lost field choices or saved status");
        auto puffOnly = selectedMenu;
        puffOnly.enabledItems = FirmwareMenu::PuffEditorMask;
        puffOnly.infoFields = 0;
        state.PushUndo("Puff editor only");
        state.SetMenuPatch(puffOnly);
        state.EndUndoAction();
        check(state.menuPatch == puffOnly && state.HasUnsavedChanges() &&
              state.Undo() && state.menuPatch == selectedMenu && !state.HasUnsavedChanges() &&
              state.Redo() && state.menuPatch == puffOnly && state.Undo(),
              "puff-editor-only history lost settings or saved baseline");
        auto disabledEditorOrder = selectedMenu;
        disabledEditorOrder.enabledItems &= ~FirmwareMenu::PuffEditorMask;
        disabledEditorOrder.order = {FirmwareMenu::Item::PuffEditor, FirmwareMenu::Item::Exit,
            FirmwareMenu::Item::Information, FirmwareMenu::Item::Voltage};
        state.PushUndo("Disabled puff editor ordering");
        state.SetMenuPatch(disabledEditorOrder);
        state.EndUndoAction();
        state.MarkSaved();
        auto reorderedEditor = disabledEditorOrder;
        std::swap(reorderedEditor.order[0], reorderedEditor.order[1]);
        state.PushUndo("Reorder disabled puff editor");
        state.SetMenuPatch(reorderedEditor);
        state.EndUndoAction();
        check(state.HasUnsavedChanges() && state.Undo() &&
              state.menuPatch == disabledEditorOrder && !state.HasUnsavedChanges() &&
              state.Redo() && state.menuPatch == reorderedEditor && state.HasUnsavedChanges(),
              "disabled editor ordering was omitted from history or saved baseline");
        state.Undo();
        state.Undo();
        state.MarkSaved();
        state.PushUndo("Menu navigation");
        auto defaultNavigation = selectedMenu;
        defaultNavigation.reverseNavigation = false;
        state.SetMenuPatch(defaultNavigation);
        check(!state.CanRedo() && state.HasUnsavedChanges(), "menu navigation change retained stale redo or clean cache");
        StudioState replacement;
        FirmwarePackage replacementPackage;
        if (LoadEmbeddedFirmware(replacementPackage, loadError) &&
            replacement.LoadPackage(std::move(replacementPackage), "replacement", loadError)) {
            state.AdoptLoadedDocument(replacement);
            check(state.menuPatch == FirmwareMenu::Settings{} &&
                  state.originalMenuPatch == FirmwareMenu::Settings{} &&
                  state.savedMenuPatch == FirmwareMenu::Settings{} &&
                  !state.HasUnsavedChanges() && !state.CanUndo() && !state.CanRedo(),
                  "document adoption retained menu settings or history");
        } else check(false, "menu replacement document fixture failed");
    } else check(false, "embedded firmware unavailable for reload regression");
    return error.empty();
}
