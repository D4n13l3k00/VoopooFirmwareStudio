#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <ctime>
#include <cmath>
#include <optional>
#include <string_view>
#include <deque>

#include "firmware_color.h"
#include "firmware_catalog.h"
#include "firmware_bitmap.h"
#include "zwdf_package.h"
#include "embedded_firmware.h"
#include "i18n.h"
#include "texture_cache.h"
#include "path_utils.h"

namespace fs = std::filesystem;

// -------------------------------------------------------------
// Application State
// -------------------------------------------------------------
struct StudioState {
    // Loaded firmware
    bool hasFirmware = false;
    bool isBuiltin = false;
    std::string firmwarePath;
    FirmwarePackage firmware;
    std::vector<AssetSpec> catalog;
    std::vector<std::vector<uint8_t>> originals; // Raw bytes in canonical record
    std::vector<std::vector<uint8_t>> referenceBitmaps; // Factory stock baseline from embedded firmware
    std::vector<std::vector<size_t>> locations;   // Locations across all records
    std::map<std::string, std::vector<uint8_t>> replacements; // Staged replacements
    std::vector<PaletteState> palette;
    size_t selectedPaletteIdx = 0;
    Rgb8 backgroundColor = Rgb8(0, 0, 0);

    // Filter & view
    char filterBuf[128] = "";
    int groupFilterIdx = 0; // 0: All, 1: Fonts, 2: Score, 3: Labels, 4: Controls, 5: Units, 6: Screens, 7: Animation
    bool filterOnlyCustom = false;
    std::vector<size_t> visibleAssetIndices;
    int selectedVisibleIdx = -1;
    int previewZoom = 4;
    int previewRank = 0;
    bool previewCyanMode = false;
    bool previewShowGrid = true;
    char hexEditBuf[16] = "";
    char bgHexEditBuf[16] = "";

    // Export/Import folder
    char exportDirBuf[512] = "exported-assets";

    // Status bar
    std::string statusMsg;
    bool statusIsErr = false;

    // Build report modal
    bool showBuildModal = false;
    BuildReport lastReport;
    std::string lastBuildPath;

    // Per-asset color customization
    // RGB565 brush colors; mask rendering always follows firmware draw colors.
    std::map<std::string, Rgb8> assetPaintColors;
    char assetHexEditBuf[16] = "#FFFFFF";

    // Inverse deltas capture a resource only on its first mutation in an action.
    // Undo/redo swap the stored value with the live value, without document copies.
    struct HistoryDelta {
        std::string desc;
        std::map<std::string, std::optional<std::vector<uint8_t>>> bitmaps;
        std::map<std::string, std::optional<Rgb8>> brushes;
        std::map<size_t, Rgb8> colors;
        std::optional<Rgb8> background;
        size_t accountedBytes = 0;

        size_t MemoryBytes() const {
            size_t bytes = sizeof(HistoryDelta) + desc.capacity() + 1;
            // Include map node links and allocated string/vector capacities.
            for (const auto& [id, raw] : bitmaps)
                bytes += sizeof(decltype(bitmaps)::value_type) + 4 * sizeof(void*) +
                    id.capacity() + 1 + (raw ? raw->capacity() : 0);
            for (const auto& [id, color] : brushes)
                bytes += sizeof(decltype(brushes)::value_type) + 4 * sizeof(void*) + id.capacity() + 1;
            bytes += colors.size() * (sizeof(decltype(colors)::value_type) + 4 * sizeof(void*));
            return bytes;
        }
    };
    std::deque<HistoryDelta> undoStack;
    std::deque<HistoryDelta> redoStack;
    static constexpr size_t DefaultHistoryMemoryBudget = 16 * 1024 * 1024;

private:
    size_t historyMemoryBudget = DefaultHistoryMemoryBudget;
    size_t historyMemoryBytes = 0;
    mutable bool dirtyCacheValid = false;
    mutable bool dirtyCache = false;
    std::string pendingHistoryDescription;
    bool historyActionOpen = false;
    bool historyActionRecorded = false;
    bool historyActionTooLarge = false;
    std::map<std::string, std::vector<uint8_t>> savedReplacements;
    std::vector<uint16_t> savedPalette;
    uint16_t savedBackground = 0;

    void RefreshHistoryAccounting(HistoryDelta& delta) {
        historyMemoryBytes -= delta.accountedBytes;
        delta.accountedBytes = delta.MemoryBytes();
        historyMemoryBytes += delta.accountedBytes;
    }

    HistoryDelta* CurrentHistoryDelta() {
        if (!historyActionOpen) PushUndo("Edit resource");
        if (historyActionTooLarge) return nullptr;
        if (!historyActionRecorded) {
            for (const auto& delta : redoStack) historyMemoryBytes -= delta.accountedBytes;
            redoStack.clear();
            undoStack.emplace_back();
            undoStack.back().desc = std::move(pendingHistoryDescription);
            RefreshHistoryAccounting(undoStack.back());
            historyActionRecorded = true;
        }
        return &undoStack.back();
    }

    void TrimHistory() {
        while (!undoStack.empty() && historyMemoryBytes > historyMemoryBudget) {
            historyMemoryBytes -= undoStack.front().accountedBytes;
            undoStack.pop_front();
        }
        while (!redoStack.empty() && historyMemoryBytes > historyMemoryBudget) {
            historyMemoryBytes -= redoStack.front().accountedBytes;
            redoStack.pop_front();
        }
        if (historyActionRecorded && undoStack.empty()) {
            // An action larger than the budget is not partially undoable.
            historyActionRecorded = false;
            historyActionTooLarge = true;
        }
    }

    void SwapHistoryDelta(HistoryDelta& delta) {
        if (!delta.bitmaps.empty() || !delta.colors.empty() || delta.background)
            dirtyCacheValid = false;
        for (auto& [id, stored] : delta.bitmaps) {
            auto current = replacements.find(id);
            if (stored) {
                if (current != replacements.end()) current->second.swap(*stored);
                else {
                    replacements.emplace(id, std::move(*stored));
                    stored.reset();
                }
            } else if (current != replacements.end()) {
                stored.emplace(std::move(current->second));
                replacements.erase(current);
            }
        }
        for (auto& [id, stored] : delta.brushes) {
            auto current = assetPaintColors.find(id);
            if (stored) {
                if (current != assetPaintColors.end()) std::swap(current->second, *stored);
                else {
                    assetPaintColors.emplace(id, *stored);
                    stored.reset();
                }
            } else if (current != assetPaintColors.end()) {
                stored = current->second;
                assetPaintColors.erase(current);
            }
        }
        for (auto& [index, stored] : delta.colors) std::swap(palette[index].current, stored);
        if (delta.background) std::swap(backgroundColor, *delta.background);
        RefreshHistoryAccounting(delta);
    }

    void RecordBitmapChange(const std::string& id) {
        dirtyCacheValid = false;
        if (auto* delta = CurrentHistoryDelta(); delta && !delta->bitmaps.contains(id)) {
            const auto found = replacements.find(id);
            if (found == replacements.end()) delta->bitmaps.emplace(id, std::nullopt);
            else delta->bitmaps.emplace(id, found->second);
            RefreshHistoryAccounting(*delta);
        }
    }

    void RecordBrushChange(const std::string& id) {
        if (auto* delta = CurrentHistoryDelta(); delta && !delta->brushes.contains(id)) {
            const auto found = assetPaintColors.find(id);
            if (found == assetPaintColors.end()) delta->brushes.emplace(id, std::nullopt);
            else delta->brushes.emplace(id, found->second);
            RefreshHistoryAccounting(*delta);
        }
    }

public:
    void PushUndo(const std::string& desc) {
        pendingHistoryDescription = desc;
        historyActionOpen = true;
        historyActionRecorded = false;
        historyActionTooLarge = false;
    }

    void EndUndoAction() {
        historyActionOpen = false;
        historyActionRecorded = false;
        historyActionTooLarge = false;
    }

    size_t HistoryMemoryBytes() const { return historyMemoryBytes; }

    void SetHistoryMemoryBudget(size_t bytes) {
        historyMemoryBudget = bytes;
        TrimHistory();
    }

    bool CanUndo() const { return !undoStack.empty(); }
    bool CanRedo() const { return !redoStack.empty(); }

    void SynchronizeEditorState() {
        const size_t selected = selectedVisibleIdx >= 0 &&
            selectedVisibleIdx < (int)visibleAssetIndices.size()
            ? visibleAssetIndices[selectedVisibleIdx] : catalog.size();
        UpdateVisible();
        const auto found = std::find(visibleAssetIndices.begin(), visibleAssetIndices.end(), selected);
        if (found != visibleAssetIndices.end())
            selectedVisibleIdx = (int)(found - visibleAssetIndices.begin());
        SynchronizeColorEditors();
    }

    void SynchronizeColorEditors() {
        if (selectedPaletteIdx < palette.size())
            strncpy_s(hexEditBuf, palette[selectedPaletteIdx].current.ToHex().c_str(), sizeof(hexEditBuf));
        else hexEditBuf[0] = '\0';
        strncpy_s(bgHexEditBuf, backgroundColor.ToHex().c_str(), sizeof(bgHexEditBuf));
        if (selectedVisibleIdx >= 0 && selectedVisibleIdx < (int)visibleAssetIndices.size()) {
            const auto& spec = catalog[visibleAssetIndices[selectedVisibleIdx]];
            const auto color = spec.format == AssetFormat::Rgb565 ? GetPaintColor(spec) : GetForegroundForAsset(spec);
            strncpy_s(assetHexEditBuf, color.ToHex().c_str(), sizeof(assetHexEditBuf));
        } else strncpy_s(assetHexEditBuf, "#FFFFFF", sizeof(assetHexEditBuf));
        textureNeedsUpdate = true;
    }

    bool Undo() {
        EndUndoAction();
        isPixelStrokeActive = false;
        if (undoStack.empty()) return false;
        auto delta = std::move(undoStack.back());
        undoStack.pop_back();
        SwapHistoryDelta(delta);
        redoStack.push_back(std::move(delta));
        TrimHistory();
        SynchronizeEditorState();
        return true;
    }

    bool Redo() {
        EndUndoAction();
        isPixelStrokeActive = false;
        if (redoStack.empty()) return false;
        auto delta = std::move(redoStack.back());
        redoStack.pop_back();
        SwapHistoryDelta(delta);
        undoStack.push_back(std::move(delta));
        TrimHistory();
        SynchronizeEditorState();
        return true;
    }

    void SetPaletteColor(size_t index, Rgb8 color) {
        if (index >= palette.size() || palette[index].current == color) return;
        if (auto* delta = CurrentHistoryDelta()) {
            const auto [entry, inserted] = delta->colors.try_emplace(index, palette[index].current);
            if (inserted) RefreshHistoryAccounting(*delta);
        }
        dirtyCacheValid = false;
        palette[index].current = color;
        TrimHistory();
        SynchronizeColorEditors();
    }

    void SetBackgroundColor(Rgb8 color) {
        if (backgroundColor == color) return;
        dirtyCacheValid = false;
        if (auto* delta = CurrentHistoryDelta(); delta && !delta->background)
            delta->background = backgroundColor;
        backgroundColor = color;
        TrimHistory();
        SynchronizeColorEditors();
    }

    void SetAssetPaintColor(const std::string& id, Rgb8 color) {
        const auto found = assetPaintColors.find(id);
        if (found != assetPaintColors.end() && found->second == color) return;
        RecordBrushChange(id);
        assetPaintColors[id] = color;
        TrimHistory();
        SynchronizeColorEditors();
    }

    void MarkSaved() {
        EndUndoAction();
        savedReplacements = replacements;
        savedPalette.clear();
        savedPalette.reserve(palette.size());
        for (const auto& entry : palette) savedPalette.push_back(entry.current.ToRgb565());
        savedBackground = backgroundColor.ToRgb565();
        dirtyCache = false;
        dirtyCacheValid = true;
    }

    bool HasUnsavedChanges() const {
        if (!hasFirmware) return false;
        if (!dirtyCacheValid) {
            dirtyCache = ComputeUnsavedChanges();
            dirtyCacheValid = true;
        }
        return dirtyCache;
    }

private:
    bool ComputeUnsavedChanges() const {
        if (!hasFirmware) return false;
        if (palette.size() != savedPalette.size() || backgroundColor.ToRgb565() != savedBackground)
            return true;
        for (size_t i = 0; i < palette.size(); ++i)
            if (palette[i].current.ToRgb565() != savedPalette[i]) return true;
        for (size_t i = 0; i < catalog.size(); ++i) {
            const auto live = replacements.find(catalog[i].id);
            const auto saved = savedReplacements.find(catalog[i].id);
            const auto& liveBytes = live == replacements.end() ? originals[i] : live->second;
            const auto& savedBytes = saved == savedReplacements.end() ? originals[i] : saved->second;
            if (liveBytes != savedBytes) return true;
        }
        return false;
    }

public:

    // Pixel Touch-up Tool State
    enum class PixelTool {
        Pencil = 0,
        Eraser,
        Eyedropper,
        BucketFill
    };
    PixelTool activePixelTool = PixelTool::Pencil;
    int brushSize = 1; // 1 to 4 px
    bool isPixelStrokeActive = false;

    // Hardware Textures for ImGui
    Dx11Texture activePreviewTexture;
    Dx11Texture referencePreviewTexture;
    Dx11Texture horizontalPreviewTexture;
    bool textureNeedsUpdate = true;

    void InitReferenceBitmaps(const FirmwarePackage& refPkg) {
        auto cat = GetAllAssets();
        size_t canonical = 0;
        for (size_t r = 0; r < refPkg.records.size(); ++r) {
            if (refPkg.records[r].device_code[0] == 0x31 && refPkg.records[r].device_code[1] == 0x03) {
                canonical = r;
                break;
            }
        }
        referenceBitmaps.clear();
        referenceBitmaps.reserve(cat.size());
        for (const auto& spec : cat) {
            const uint8_t* ptr = refPkg.BytesAt(canonical, spec.address, spec.byte_len());
            if (ptr) {
                referenceBitmaps.emplace_back(ptr, ptr + spec.byte_len());
            } else {
                referenceBitmaps.emplace_back();
            }
        }
    }

    bool IsActiveMatchingReference(size_t catalog_idx) const {
        if (catalog_idx >= referenceBitmaps.size() || catalog_idx >= catalog.size()) return true;
        if (referenceBitmaps[catalog_idx].empty()) return true;
        const uint8_t* cur = GetCurrentRaw(catalog_idx);
        return std::memcmp(cur, referenceBitmaps[catalog_idx].data(), catalog[catalog_idx].byte_len()) == 0;
    }

    bool IsFileMatchingReference(size_t catalog_idx) const {
        if (catalog_idx >= referenceBitmaps.size() || catalog_idx >= originals.size()) return true;
        if (referenceBitmaps[catalog_idx].empty()) return true;
        return originals[catalog_idx] == referenceBitmaps[catalog_idx];
    }

    size_t CountDifferencesFromReference(size_t catalog_idx) const {
        if (catalog_idx >= referenceBitmaps.size() || catalog_idx >= catalog.size()) return 0;
        if (referenceBitmaps[catalog_idx].empty()) return 0;
        const uint8_t* cur = GetCurrentRaw(catalog_idx);
        const uint8_t* ref = referenceBitmaps[catalog_idx].data();
        size_t len = catalog[catalog_idx].byte_len();
        size_t diff = 0;
        for (size_t i = 0; i < len; ++i) {
            if (cur[i] != ref[i]) diff++;
        }
        return diff;
    }

    size_t CountTotalNonStockAssets() const {
        size_t count = 0;
        for (size_t i = 0; i < catalog.size(); ++i) {
            if (!IsActiveMatchingReference(i)) count++;
        }
        return count;
    }

    const uint8_t* GetReferenceRaw(size_t catalog_idx) const {
        if (catalog_idx < referenceBitmaps.size() && !referenceBitmaps[catalog_idx].empty()) {
            return referenceBitmaps[catalog_idx].data();
        }
        return nullptr;
    }

    void RevertAssetToStock(size_t catalog_idx) {
        if (catalog_idx >= catalog.size() || catalog_idx >= referenceBitmaps.size()) return;
        const auto& spec = catalog[catalog_idx];
        if (referenceBitmaps[catalog_idx].empty()) return;
        PushUndo("Restore stock " + spec.id);
        SetBitmapReplacement(catalog_idx, referenceBitmaps[catalog_idx]);
    }

    void SetBitmapReplacement(size_t catalog_idx, std::vector<uint8_t> raw) {
        if (catalog_idx >= catalog.size() || catalog_idx >= originals.size() ||
            raw.size() != originals[catalog_idx].size()) return;
        const auto& id = catalog[catalog_idx].id;
        const auto found = replacements.find(id);
        const auto& current = found == replacements.end() ? originals[catalog_idx] : found->second;
        if (current == raw) return;
        RecordBitmapChange(id);
        if (raw == originals[catalog_idx]) replacements.erase(id);
        else replacements[id] = std::move(raw);
        textureNeedsUpdate = true;
        TrimHistory();
    }

    const uint8_t* GetCurrentRaw(size_t catalog_idx) const {
        const auto& spec = catalog[catalog_idx];
        auto it = replacements.find(spec.id);
        if (it != replacements.end()) {
            return it->second.data();
        }
        return originals[catalog_idx].data();
    }

    Rgb8 GetPaletteColor(const char* id) const {
        for (const auto& p : palette)
            if (std::string(p.spec.id) == id) return p.current;
        for (const auto& p : GetDefaultPalette())
            if (std::string(p.spec.id) == id) return p.current;
        return {255, 255, 255};
    }

    Rgb8 GetForegroundForAsset(const AssetSpec& spec) const {
        const char* pid = PaletteIdForAsset(spec, previewRank, previewCyanMode);
        return pid ? GetPaletteColor(pid) : FirmwareFixedForeground(firmware, spec, previewRank);
    }

    Rgb8 GetBackgroundForAsset(const AssetSpec& spec) const {
        // Only the 11x20 puff digits are drawn over the mode accent strip.
        // Global background recoloring is unsupported by the firmware patcher.
        if (spec.id.rfind("font_11x20_", 0) == 0)
            return GetPaletteColor(previewCyanMode ? "accent_cyan" : "accent_red");
        return {0, 0, 0};
    }

    Rgb8 GetPaintColor(const AssetSpec& spec) const {
        auto it = assetPaintColors.find(spec.id);
        return it == assetPaintColors.end() ? Rgb8(255, 255, 255) : it->second;
    }

    bool BuildZwdfBytes(std::vector<uint8_t>& outBytes, std::string& outErr) const {
        if (!hasFirmware) {
            outErr = "No firmware loaded in Resource Studio";
            return false;
        }
        std::vector<CodePatch> codePatches;
        if (!BuildColorPatches(firmware, palette, backgroundColor, codePatches, outErr)) {
            return false;
        }
        std::map<std::string, std::pair<std::vector<size_t>, std::vector<uint8_t>>> repMap;
        for (size_t i = 0; i < catalog.size(); ++i) {
            const auto& spec = catalog[i];
            auto it = replacements.find(spec.id);
            if (it != replacements.end()) {
                repMap[spec.id] = { locations[i], it->second };
            }
        }
        BuildReport rep;
        return firmware.PackWith(repMap, codePatches, outBytes, rep, outErr);
    }

    void UpdateVisible() {
        std::string f(filterBuf);
        std::transform(f.begin(), f.end(), f.begin(), [](unsigned char c){ return (char)::tolower(c); });

        const char* groupNames[] = { "", "Alerts", "Font", "Score", "Labels", "Controls", "Units", "Screens", "Animation" };
        std::string filterGroup = (groupFilterIdx > 0 && groupFilterIdx < (int)(sizeof(groupNames) / sizeof(groupNames[0]))) ? groupNames[groupFilterIdx] : "";

        visibleAssetIndices.clear();
        for (size_t i = 0; i < catalog.size(); ++i) {
            const auto& spec = catalog[i];
            if (filterOnlyCustom && IsActiveMatchingReference(i)) {
                continue;
            }
            if (!filterGroup.empty() && spec.group.rfind(filterGroup, 0) != 0) {
                continue;
            }
            if (!f.empty()) {
                std::string titleLower = spec.title;
                std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), [](unsigned char c){ return (char)::tolower(c); });
                std::string idLower = spec.id;
                std::transform(idLower.begin(), idLower.end(), idLower.begin(), [](unsigned char c){ return (char)::tolower(c); });
                if (titleLower.find(f) == std::string::npos && idLower.find(f) == std::string::npos) {
                    continue;
                }
            }
            visibleAssetIndices.push_back(i);
        }
        if (visibleAssetIndices.empty()) {
            selectedVisibleIdx = -1;
        } else if (selectedVisibleIdx < 0 || selectedVisibleIdx >= (int)visibleAssetIndices.size()) {
            selectedVisibleIdx = 0;
        }
        if (selectedVisibleIdx >= 0 && selectedVisibleIdx < (int)visibleAssetIndices.size()) {
            Rgb8 c = GetForegroundForAsset(catalog[visibleAssetIndices[selectedVisibleIdx]]);
            strncpy_s(assetHexEditBuf, c.ToHex().c_str(), sizeof(assetHexEditBuf));
        }
        textureNeedsUpdate = true;
    }

    bool OpenFirmware(const std::string& path, std::string& err) {
        FirmwarePackage pkg;
        if (!FirmwarePackage::Open(path, pkg, err)) {
            return false;
        }

        bool res = LoadPackage(std::move(pkg), path, err);
        if (res) {
            isBuiltin = false;
        }
        return res;
    }

    bool LoadPackage(FirmwarePackage pkg, const std::string& path, std::string& err) {
        auto cat = GetAllAssets();
        size_t canonical = 0;
        for (size_t r = 0; r < pkg.records.size(); ++r) {
            if (pkg.records[r].device_code[0] == 0x31 && pkg.records[r].device_code[1] == 0x03) {
                canonical = r;
                break;
            }
        }

        std::vector<std::vector<uint8_t>> origs;
        std::vector<std::vector<size_t>> locs;
        origs.reserve(cat.size());
        locs.reserve(cat.size());

        for (const auto& spec : cat) {
            const uint8_t* ptr = pkg.BytesAt(canonical, spec.address, spec.byte_len());
            if (!ptr) {
                err = "Asset " + spec.id + " is outside linked flash in record " + std::to_string(canonical);
                return false;
            }
            std::vector<uint8_t> orig(ptr, ptr + spec.byte_len());

            std::vector<size_t> asset_locs;
            asset_locs.reserve(pkg.records.size());
            for (size_t r = 0; r < pkg.records.size(); ++r) {
                size_t offset = 0;
                if (!pkg.FindUnique(r, orig.data(), orig.size(), offset)) {
                    err = "Cannot map unique signature for " + spec.id + " in record " + std::to_string(r);
                    return false;
                }
                asset_locs.push_back(offset);
            }

            origs.push_back(std::move(orig));
            locs.push_back(std::move(asset_locs));
        }

        hasFirmware = true;
        firmwarePath = path;
        firmware = std::move(pkg);
        catalog = std::move(cat);
        originals = std::move(origs);
        locations = std::move(locs);
        replacements.clear();
        undoStack.clear();
        redoStack.clear();
        historyMemoryBytes = 0;
        EndUndoAction();
        isPixelStrokeActive = false;
        if (referenceBitmaps.empty()) {
            FirmwarePackage refPkg;
            std::string refErr;
            if (LoadEmbeddedFirmware(refPkg, refErr)) {
                InitReferenceBitmaps(refPkg);
            }
        }
        assetPaintColors.clear();
        palette = GetDefaultPalette();
        selectedPaletteIdx = 0;
        backgroundColor = Rgb8(0, 0, 0);
        DetectFirmwareColors(firmware, palette, backgroundColor);
        SynchronizeEditorState();
        MarkSaved();
        return true;
    }

    int FindAssetIndex(const std::string& id) const {
        for (size_t i = 0; i < catalog.size(); ++i) {
            if (catalog[i].id == id) return (int)i;
        }
        return -1;
    }

    void RevertAsset(const std::string& id) {
        PushUndo("Revert " + id);
        if (replacements.contains(id)) {
            RecordBitmapChange(id);
            replacements.erase(id);
        }
        if (assetPaintColors.contains(id)) {
            RecordBrushChange(id);
            assetPaintColors.erase(id);
        }
        TrimHistory();
        SynchronizeEditorState();
    }

    void RevertAllAssets() {
        PushUndo("Revert all");
        for (const auto& [id, raw] : replacements) RecordBitmapChange(id);
        for (const auto& [id, color] : assetPaintColors) RecordBrushChange(id);
        replacements.clear();
        assetPaintColors.clear();
        TrimHistory();
        SynchronizeEditorState();
    }

    bool BuildToBuffer(std::vector<uint8_t>& outBytes, std::string& err, BuildReport* outReport = nullptr) {
        if (!hasFirmware) {
            err = "No firmware loaded.";
            return false;
        }
        std::vector<CodePatch> codePatches;
        if (!BuildColorPatches(firmware, palette, backgroundColor, codePatches, err)) {
            return false;
        }
        std::map<std::string, std::pair<std::vector<size_t>, std::vector<uint8_t>>> repMap;
        for (size_t i = 0; i < catalog.size(); ++i) {
            const auto& spec = catalog[i];
            auto it = replacements.find(spec.id);
            if (it != replacements.end()) {
                repMap[spec.id] = { locations[i], it->second };
            }
        }
        BuildReport rep;
        if (!firmware.PackWith(repMap, codePatches, outBytes, rep, err)) return false;
        if (outReport) *outReport = std::move(rep);
        return true;
    }
};

// -------------------------------------------------------------
// Color Palette Themes (Quick Presets)
// -------------------------------------------------------------
struct PaletteTheme {
    const char* name;
    const char* desc;
    Rgb8 accent_red;        // Accent / red mode
    Rgb8 accent_cyan;       // Accent / cyan mode
    Rgb8 highlight_yellow;  // Highlight / yellow
    Rgb8 rank_bronze;       // Rank / bronze
    Rgb8 rank_gold;         // Rank / gold
    Rgb8 rank_ice;          // Rank / ice
    Rgb8 rank_cyan;         // Rank / cyan
    Rgb8 rank_orange;       // Rank / orange
    Rgb8 rank_red;          // Rank / red
    Rgb8 background;        // Clear background
    Rgb8 digits_wattage;    // Large wattage readout
    Rgb8 digits_stats;      // Puff counter foreground (11x20)
};

inline const std::vector<PaletteTheme>& GetPaletteThemes() {
    static std::vector<PaletteTheme> s_themes = {
        {
            "Factory Default", "Original Voopoo Red & Yellow factory style",
            Rgb8(0xF3, 0x01, 0x33), Rgb8(0x00, 0xDB, 0xFF), Rgb8(0xF3, 0xDD, 0x01),
            Rgb8(0xB5, 0x79, 0x47), Rgb8(0xF4, 0xDC, 0x00), Rgb8(0xB2, 0xFB, 0xEC),
            Rgb8(0x01, 0xE7, 0xF2), Rgb8(0xF2, 0x53, 0x00), Rgb8(0xF2, 0x01, 0x33),
            Rgb8(0x00, 0x00, 0x00), Rgb8(0xFF, 0xFF, 0xFF), Rgb8(0xFF, 0xFF, 0xFF)
        },
        {
            "Cyberpunk Neon", "Electric Cyan, Neon Pink, and Vivid Violet",
            Rgb8(0xFF, 0x00, 0x7F), Rgb8(0x00, 0xF0, 0xFF), Rgb8(0xFF, 0xE6, 0x00),
            Rgb8(0xB3, 0x88, 0xFF), Rgb8(0xFF, 0xD6, 0x00), Rgb8(0x80, 0xD8, 0xFF),
            Rgb8(0x00, 0xE5, 0xFF), Rgb8(0xFF, 0x6D, 0x00), Rgb8(0xFF, 0x17, 0x44),
            Rgb8(0x00, 0x00, 0x00), Rgb8(0xFF, 0xFF, 0xFF), Rgb8(0xFF, 0xFF, 0xFF)
        },
        {
            "Matrix Green", "CRT Terminal Phosphor & Emerald Green",
            Rgb8(0x00, 0xFF, 0x41), Rgb8(0x00, 0xE6, 0x76), Rgb8(0x76, 0xFF, 0x03),
            Rgb8(0x2E, 0x7D, 0x32), Rgb8(0xAE, 0xEA, 0x00), Rgb8(0xB9, 0xF6, 0xCA),
            Rgb8(0x69, 0xF0, 0xAE), Rgb8(0x00, 0xC8, 0x53), Rgb8(0x00, 0xE6, 0x76),
            Rgb8(0x00, 0x00, 0x00), Rgb8(0xFF, 0xFF, 0xFF), Rgb8(0xFF, 0xFF, 0xFF)
        },
        {
            "Arctic Ice Blue", "Glacier Cyan, Deep Sky, and Azure Glow",
            Rgb8(0x00, 0xB0, 0xFF), Rgb8(0x80, 0xD8, 0xFF), Rgb8(0xE0, 0xF7, 0xFA),
            Rgb8(0x4F, 0xC3, 0xF7), Rgb8(0x29, 0xB6, 0xF6), Rgb8(0xE1, 0xF5, 0xFE),
            Rgb8(0x00, 0xE5, 0xFF), Rgb8(0x02, 0x88, 0xD1), Rgb8(0x01, 0x57, 0x9B),
            Rgb8(0x00, 0x00, 0x00), Rgb8(0xFF, 0xFF, 0xFF), Rgb8(0xFF, 0xFF, 0xFF)
        },
        {
            "Amber Fire", "Molten Lava Orange, Golden Flame, and Ember",
            Rgb8(0xFF, 0x3D, 0x00), Rgb8(0xFF, 0x91, 0x00), Rgb8(0xFF, 0xD7, 0x00),
            Rgb8(0xBF, 0x36, 0x0C), Rgb8(0xFF, 0xC1, 0x07), Rgb8(0xFF, 0xE0, 0x82),
            Rgb8(0xFF, 0xA0, 0x00), Rgb8(0xFF, 0x6D, 0x00), Rgb8(0xD5, 0x00, 0x00),
            Rgb8(0x00, 0x00, 0x00), Rgb8(0xFF, 0xFF, 0xFF), Rgb8(0xFF, 0xFF, 0xFF)
        },
        {
            "Purple Nebula", "Deep Violet, Electric Lavender, and Neon Amethyst",
            Rgb8(0xA8, 0x55, 0xF7), Rgb8(0xC0, 0x84, 0xFC), Rgb8(0xF0, 0xAB, 0xFC),
            Rgb8(0x7E, 0x22, 0xCE), Rgb8(0xE8, 0x79, 0xF9), Rgb8(0xF5, 0xD0, 0xFE),
            Rgb8(0x93, 0x33, 0xEA), Rgb8(0x6B, 0x21, 0xA8), Rgb8(0xD9, 0x46, 0xEF),
            Rgb8(0x00, 0x00, 0x00), Rgb8(0xFF, 0xFF, 0xFF), Rgb8(0xFF, 0xFF, 0xFF)
        },
        {
            "Stealth Dark Gray", "Matte Charcoal, Slate Graphite, and Gunmetal",
            Rgb8(0x94, 0xA3, 0xB8), Rgb8(0xCB, 0xD5, 0xE1), Rgb8(0xE2, 0xE8, 0xF0),
            Rgb8(0x47, 0x55, 0x69), Rgb8(0x64, 0x74, 0x8B), Rgb8(0xF1, 0xF5, 0xF9),
            Rgb8(0x33, 0x41, 0x55), Rgb8(0x64, 0x74, 0x8B), Rgb8(0x94, 0xA3, 0xB8),
            Rgb8(0x00, 0x00, 0x00), Rgb8(0xFF, 0xFF, 0xFF), Rgb8(0xFF, 0xFF, 0xFF)
        }
    };
    return s_themes;
}

inline const char* GetThemeNameTr(size_t index) {
    switch (index) {
        case 0: return Tr(i18n::Str::ThemeFactoryDefault);
        case 1: return Tr(i18n::Str::ThemeCyberpunkNeon);
        case 2: return Tr(i18n::Str::ThemeMatrixGreen);
        case 3: return Tr(i18n::Str::ThemeArcticIceBlue);
        case 4: return Tr(i18n::Str::ThemeAmberFire);
        case 5: return Tr(i18n::Str::ThemePurpleNebula);
        case 6: return Tr(i18n::Str::ThemeStealthDarkGray);
        default: return "";
    }
}

inline const char* GetThemeDescTr(size_t index) {
    switch (index) {
        case 0: return Tr(i18n::Str::ThemeFactoryDefaultDesc);
        case 1: return Tr(i18n::Str::ThemeCyberpunkNeonDesc);
        case 2: return Tr(i18n::Str::ThemeMatrixGreenDesc);
        case 3: return Tr(i18n::Str::ThemeArcticIceBlueDesc);
        case 4: return Tr(i18n::Str::ThemeAmberFireDesc);
        case 5: return Tr(i18n::Str::ThemePurpleNebulaDesc);
        case 6: return Tr(i18n::Str::ThemeStealthDarkGrayDesc);
        default: return "";
    }
}

inline const char* GetRoleLabelTr(const std::string& id) {
    if (id == "accent_red") return Tr(i18n::Str::RoleAccentRed);
    if (id == "accent_cyan") return Tr(i18n::Str::RoleAccentCyan);
    if (id == "highlight_yellow") return Tr(i18n::Str::RoleHighlightYellow);
    if (id == "rank_bronze") return Tr(i18n::Str::RoleRankBronze);
    if (id == "rank_gold") return Tr(i18n::Str::RoleRankGold);
    if (id == "rank_ice") return Tr(i18n::Str::RoleRankIce);
    if (id == "rank_cyan") return Tr(i18n::Str::RoleRankCyan);
    if (id == "rank_orange") return Tr(i18n::Str::RoleRankOrange);
    if (id == "rank_red") return Tr(i18n::Str::RoleRankRed);
    if (id == "digits_wattage") return Tr(i18n::Str::RoleDigitsWattage);
    if (id == "digits_stats") return Tr(i18n::Str::RoleDigitsStats);
    return id.c_str();
}

inline const char* GetRoleScopeTr(const std::string& id) {
    if (id == "accent_red") return Tr(i18n::Str::RoleAccentRedScope);
    if (id == "accent_cyan") return Tr(i18n::Str::RoleAccentCyanScope);
    if (id == "highlight_yellow") return Tr(i18n::Str::RoleHighlightYellowScope);
    if (id == "rank_bronze") return Tr(i18n::Str::RoleRankBronzeScope);
    if (id == "rank_gold") return Tr(i18n::Str::RoleRankGoldScope);
    if (id == "rank_ice") return Tr(i18n::Str::RoleRankIceScope);
    if (id == "rank_cyan") return Tr(i18n::Str::RoleRankCyanScope);
    if (id == "rank_orange") return Tr(i18n::Str::RoleRankOrangeScope);
    if (id == "rank_red") return Tr(i18n::Str::RoleRankRedScope);
    if (id == "digits_wattage") return Tr(i18n::Str::RoleDigitsWattageScope);
    if (id == "digits_stats") return Tr(i18n::Str::RoleDigitsStatsScope);
    return "";
}

inline void ApplyPaletteTheme(StudioState& studio, const PaletteTheme& t) {
    const auto& allThemes = GetPaletteThemes();
    size_t themeIdx = 0;
    for (size_t i = 0; i < allThemes.size(); ++i) {
        if (std::string(allThemes[i].name) == t.name) {
            themeIdx = i;
            break;
        }
    }
    const char* localizedThemeName = GetThemeNameTr(themeIdx);
    studio.PushUndo(std::string("Apply theme ") + localizedThemeName);
    for (size_t i = 0; i < studio.palette.size(); ++i) {
        const std::string_view id = studio.palette[i].spec.id;
        if (id == "accent_red") studio.SetPaletteColor(i, t.accent_red);
        else if (id == "accent_cyan") studio.SetPaletteColor(i, t.accent_cyan);
        else if (id == "highlight_yellow") studio.SetPaletteColor(i, t.highlight_yellow);
        else if (id == "rank_bronze") studio.SetPaletteColor(i, t.rank_bronze);
        else if (id == "rank_gold") studio.SetPaletteColor(i, t.rank_gold);
        else if (id == "rank_ice") studio.SetPaletteColor(i, t.rank_ice);
        else if (id == "rank_cyan") studio.SetPaletteColor(i, t.rank_cyan);
        else if (id == "rank_orange") studio.SetPaletteColor(i, t.rank_orange);
        else if (id == "rank_red") studio.SetPaletteColor(i, t.rank_red);
        else if (id == "digits_wattage") studio.SetPaletteColor(i, t.digits_wattage);
        else if (id == "digits_stats") studio.SetPaletteColor(i, t.digits_stats);
    }
    studio.SetBackgroundColor(t.background);
    char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgAppliedColorTheme), localizedThemeName); studio.statusMsg = _m;
    studio.statusIsErr = false;
    studio.textureNeedsUpdate = true;
}

inline void CopyCurrentSelectedAsset(StudioState& studio, HWND hwnd) {
    if (!studio.hasFirmware || studio.selectedVisibleIdx < 0 || studio.selectedVisibleIdx >= (int)studio.visibleAssetIndices.size())
        return;
    size_t catIdx = studio.visibleAssetIndices[studio.selectedVisibleIdx];
    const auto& s = studio.catalog[catIdx];
    const uint8_t* raw = studio.GetCurrentRaw(catIdx);
    std::vector<uint32_t> rgba;
    if (s.format == AssetFormat::Rgb565) {
        FirmwareBitmap::DecodeRgb565(raw, s.byte_len(), s.width, s.height, rgba);
    } else {
        std::vector<uint8_t> gray;
        FirmwareBitmap::DecodeMask(raw, s.byte_len(), s.width, s.height, gray);
        Rgb8 fg = studio.GetForegroundForAsset(s);
        FirmwareBitmap::ColorizeMask(gray.data(), s.width, s.height, fg, studio.GetBackgroundForAsset(s), rgba);
    }
    if (FirmwareBitmap::CopyImageToClipboard(hwnd, rgba.data(), s.width, s.height)) {
        char buf[256];
        snprintf(buf, sizeof(buf), Tr(i18n::Str::MsgCopiedToClipboard), s.id.c_str());
        studio.statusMsg = buf;
        studio.statusIsErr = false;
    }
}

inline void PasteIntoCurrentSelectedAsset(StudioState& studio, HWND hwnd) {
    if (!studio.hasFirmware || studio.selectedVisibleIdx < 0 || studio.selectedVisibleIdx >= (int)studio.visibleAssetIndices.size())
        return;
    size_t catIdx = studio.visibleAssetIndices[studio.selectedVisibleIdx];
    const auto& s = studio.catalog[catIdx];
    std::vector<uint32_t> pastedRgba;
    int pw = 0, ph = 0;
    if (!FirmwareBitmap::PasteImageFromClipboard(hwnd, pastedRgba, pw, ph)) {
        studio.statusMsg = Tr(i18n::Str::MsgClipboardNoImage);
        studio.statusIsErr = true;
        return;
    }
    studio.PushUndo("Paste image into " + s.id);
    if (s.format == AssetFormat::Rgb565) {
        std::vector<uint32_t> fittedRgba;
        FirmwareBitmap::FitRgba32((const uint8_t*)pastedRgba.data(), pw, ph, s.width, s.height, fittedRgba);
        std::vector<uint8_t> newRaw;
        if (FirmwareBitmap::EncodeRgb565(fittedRgba.data(), s.width, s.height, newRaw)) {
            studio.SetBitmapReplacement(catIdx, std::move(newRaw));
            char buf[256];
            snprintf(buf, sizeof(buf), Tr(i18n::Str::MsgPastedFromClipboard), s.id.c_str());
            studio.statusMsg = buf;
            studio.statusIsErr = false;
        }
    } else {
        std::vector<uint8_t> graySrc((size_t)pw * ph);
        for (int i = 0; i < pw * ph; ++i) {
            uint32_t px = pastedRgba[i];
            uint8_t r = (uint8_t)(px & 0xFF);
            uint8_t g = (uint8_t)((px >> 8) & 0xFF);
            uint8_t b = (uint8_t)((px >> 16) & 0xFF);
            uint8_t a = (uint8_t)((px >> 24) & 0xFF);
            float lum = (r * 0.299f + g * 0.587f + b * 0.114f) * (a / 255.0f);
            graySrc[i] = (uint8_t)std::clamp(lum, 0.0f, 255.0f);
        }
        std::vector<uint8_t> fittedGray;
        FirmwareBitmap::FitGrayscale(graySrc.data(), pw, ph, s.width, s.height, fittedGray);
        std::vector<uint8_t> newRaw;
        if (FirmwareBitmap::EncodeMaskPreserving(fittedGray.data(), s.width, s.height, studio.originals[catIdx].data(), s.byte_len(), newRaw)) {
            studio.SetBitmapReplacement(catIdx, std::move(newRaw));
            char buf[256];
            snprintf(buf, sizeof(buf), Tr(i18n::Str::MsgPastedFromClipboard), s.id.c_str());
            studio.statusMsg = buf;
            studio.statusIsErr = false;
        }
    }
    studio.textureNeedsUpdate = true;
}
