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
    std::map<std::string, Rgb8> assetColorOverrides;
    char assetHexEditBuf[16] = "#FFFFFF";

    // Undo / Redo History
    struct HistorySnapshot {
        std::string desc;
        std::map<std::string, std::vector<uint8_t>> replacements;
        std::map<std::string, Rgb8> assetColorOverrides;
    };
    std::vector<HistorySnapshot> undoStack;
    std::vector<HistorySnapshot> redoStack;

    void PushUndo(const std::string& desc) {
        undoStack.push_back({ desc, replacements, assetColorOverrides });
        if (undoStack.size() > 60) {
            undoStack.erase(undoStack.begin());
        }
        redoStack.clear();
        textureNeedsUpdate = true;
    }

    bool CanUndo() const { return !undoStack.empty(); }
    bool CanRedo() const { return !redoStack.empty(); }

    bool Undo() {
        if (undoStack.empty()) return false;
        HistorySnapshot current{ "", replacements, assetColorOverrides };
        auto prev = std::move(undoStack.back());
        undoStack.pop_back();
        current.desc = prev.desc;
        redoStack.push_back(std::move(current));

        replacements = std::move(prev.replacements);
        assetColorOverrides = std::move(prev.assetColorOverrides);
        textureNeedsUpdate = true;
        return true;
    }

    bool Redo() {
        if (redoStack.empty()) return false;
        HistorySnapshot current{ "", replacements, assetColorOverrides };
        auto next = std::move(redoStack.back());
        redoStack.pop_back();
        current.desc = next.desc;
        undoStack.push_back(std::move(current));

        replacements = std::move(next.replacements);
        assetColorOverrides = std::move(next.assetColorOverrides);
        textureNeedsUpdate = true;
        return true;
    }

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
        if (IsFileMatchingReference(catalog_idx)) {
            replacements.erase(spec.id);
        } else {
            replacements[spec.id] = referenceBitmaps[catalog_idx];
        }
        textureNeedsUpdate = true;
    }

    const uint8_t* GetCurrentRaw(size_t catalog_idx) const {
        const auto& spec = catalog[catalog_idx];
        auto it = replacements.find(spec.id);
        if (it != replacements.end()) {
            return it->second.data();
        }
        return originals[catalog_idx].data();
    }

    Rgb8 GetForegroundForAsset(const AssetSpec& spec) const {
        if (spec.id.rfind("font_", 0) == 0) return Rgb8(255, 255, 255);
        auto it = assetColorOverrides.find(spec.id);
        if (it != assetColorOverrides.end()) {
            return it->second;
        }
        const char* pid = PaletteIdForAsset(spec);
        if (!pid) return Rgb8(255, 255, 255);
        for (const auto& p : palette) {
            if (p.spec.id == pid) {
                return p.current;
            }
        }
        return Rgb8(255, 255, 255);
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
        isPixelStrokeActive = false;
        if (referenceBitmaps.empty()) {
            FirmwarePackage refPkg;
            std::string refErr;
            if (LoadEmbeddedFirmware(refPkg, refErr)) {
                InitReferenceBitmaps(refPkg);
            }
        }
        assetColorOverrides.clear();
        palette = GetDefaultPalette();
        selectedPaletteIdx = 0;
        backgroundColor = Rgb8(0, 0, 0);
        DetectFirmwareColors(firmware, palette, backgroundColor);
        strncpy_s(hexEditBuf, palette[0].current.ToHex().c_str(), sizeof(hexEditBuf));
        strncpy_s(bgHexEditBuf, backgroundColor.ToHex().c_str(), sizeof(bgHexEditBuf));

        UpdateVisible();
        textureNeedsUpdate = true;
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
        replacements.erase(id);
        assetColorOverrides.erase(id);
        textureNeedsUpdate = true;
    }

    void RevertAllAssets() {
        PushUndo("Revert all");
        replacements.clear();
        assetColorOverrides.clear();
        textureNeedsUpdate = true;
    }

    bool BuildToBuffer(std::vector<uint8_t>& outBytes, std::string& err) {
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
        return firmware.PackWith(repMap, codePatches, outBytes, rep, err);
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
    Rgb8 digits_stats;      // Puff, volt, ohm small digits
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
            Rgb8(0x00, 0x00, 0x00), Rgb8(0xFF, 0xFF, 0xFF), Rgb8(0x00, 0xF0, 0xFF)
        },
        {
            "Matrix Green", "CRT Terminal Phosphor & Emerald Green",
            Rgb8(0x00, 0xFF, 0x41), Rgb8(0x00, 0xE6, 0x76), Rgb8(0x76, 0xFF, 0x03),
            Rgb8(0x2E, 0x7D, 0x32), Rgb8(0xAE, 0xEA, 0x00), Rgb8(0xB9, 0xF6, 0xCA),
            Rgb8(0x69, 0xF0, 0xAE), Rgb8(0x00, 0xC8, 0x53), Rgb8(0x00, 0xE6, 0x76),
            Rgb8(0x00, 0x00, 0x00), Rgb8(0xFF, 0xFF, 0xFF), Rgb8(0x76, 0xFF, 0x03)
        },
        {
            "Arctic Ice Blue", "Glacier Cyan, Deep Sky, and Azure Glow",
            Rgb8(0x00, 0xB0, 0xFF), Rgb8(0x80, 0xD8, 0xFF), Rgb8(0xE0, 0xF7, 0xFA),
            Rgb8(0x4F, 0xC3, 0xF7), Rgb8(0x29, 0xB6, 0xF6), Rgb8(0xE1, 0xF5, 0xFE),
            Rgb8(0x00, 0xE5, 0xFF), Rgb8(0x02, 0x88, 0xD1), Rgb8(0x01, 0x57, 0x9B),
            Rgb8(0x00, 0x00, 0x00), Rgb8(0xFF, 0xFF, 0xFF), Rgb8(0x80, 0xD8, 0xFF)
        },
        {
            "Amber Fire", "Molten Lava Orange, Golden Flame, and Ember",
            Rgb8(0xFF, 0x3D, 0x00), Rgb8(0xFF, 0x91, 0x00), Rgb8(0xFF, 0xD7, 0x00),
            Rgb8(0xBF, 0x36, 0x0C), Rgb8(0xFF, 0xC1, 0x07), Rgb8(0xFF, 0xE0, 0x82),
            Rgb8(0xFF, 0xA0, 0x00), Rgb8(0xFF, 0x6D, 0x00), Rgb8(0xD5, 0x00, 0x00),
            Rgb8(0x00, 0x00, 0x00), Rgb8(0xFF, 0xFF, 0xFF), Rgb8(0xFF, 0xD7, 0x00)
        },
        {
            "Purple Nebula", "Deep Violet, Electric Lavender, and Neon Amethyst",
            Rgb8(0xA8, 0x55, 0xF7), Rgb8(0xC0, 0x84, 0xFC), Rgb8(0xF0, 0xAB, 0xFC),
            Rgb8(0x7E, 0x22, 0xCE), Rgb8(0xE8, 0x79, 0xF9), Rgb8(0xF5, 0xD0, 0xFE),
            Rgb8(0x93, 0x33, 0xEA), Rgb8(0x6B, 0x21, 0xA8), Rgb8(0xD9, 0x46, 0xEF),
            Rgb8(0x00, 0x00, 0x00), Rgb8(0xFF, 0xFF, 0xFF), Rgb8(0xF0, 0xAB, 0xFC)
        },
        {
            "Stealth Dark Gray", "Matte Charcoal, Slate Graphite, and Gunmetal",
            Rgb8(0x94, 0xA3, 0xB8), Rgb8(0xCB, 0xD5, 0xE1), Rgb8(0xE2, 0xE8, 0xF0),
            Rgb8(0x47, 0x55, 0x69), Rgb8(0x64, 0x74, 0x8B), Rgb8(0xF1, 0xF5, 0xF9),
            Rgb8(0x33, 0x41, 0x55), Rgb8(0x64, 0x74, 0x8B), Rgb8(0x94, 0xA3, 0xB8),
            Rgb8(0x00, 0x00, 0x00), Rgb8(0xFF, 0xFF, 0xFF), Rgb8(0xCB, 0xD5, 0xE1)
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
    for (auto& p : studio.palette) {
        if (p.spec.id == std::string("accent_red")) p.current = t.accent_red;
        else if (p.spec.id == std::string("accent_cyan")) p.current = t.accent_cyan;
        else if (p.spec.id == std::string("highlight_yellow")) p.current = t.highlight_yellow;
        else if (p.spec.id == std::string("rank_bronze")) p.current = t.rank_bronze;
        else if (p.spec.id == std::string("rank_gold")) p.current = t.rank_gold;
        else if (p.spec.id == std::string("rank_ice")) p.current = t.rank_ice;
        else if (p.spec.id == std::string("rank_cyan")) p.current = t.rank_cyan;
        else if (p.spec.id == std::string("rank_orange")) p.current = t.rank_orange;
        else if (p.spec.id == std::string("rank_red")) p.current = t.rank_red;
        else if (p.spec.id == std::string("digits_wattage")) p.current = t.digits_wattage;
        else if (p.spec.id == std::string("digits_stats")) p.current = t.digits_stats;
    }
    studio.backgroundColor = t.background;
    if (studio.selectedPaletteIdx < studio.palette.size()) {
        strncpy_s(studio.hexEditBuf, studio.palette[studio.selectedPaletteIdx].current.ToHex().c_str(), sizeof(studio.hexEditBuf));
    }
    strncpy_s(studio.bgHexEditBuf, studio.backgroundColor.ToHex().c_str(), sizeof(studio.bgHexEditBuf));
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
        if (studio.assetColorOverrides.find(s.id) != studio.assetColorOverrides.end()) {
            std::vector<uint32_t> recolored;
            FirmwareBitmap::RecolorRgb565(rgba.data(), s.width, s.height, studio.GetForegroundForAsset(s), recolored);
            rgba = std::move(recolored);
        }
    } else {
        std::vector<uint8_t> gray;
        FirmwareBitmap::DecodeMask(raw, s.byte_len(), s.width, s.height, gray);
        Rgb8 fg = studio.GetForegroundForAsset(s);
        FirmwareBitmap::ColorizeMask(gray.data(), s.width, s.height, fg, studio.backgroundColor, rgba);
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
            if (newRaw == studio.originals[catIdx]) {
                studio.replacements.erase(s.id);
            } else {
                studio.replacements[s.id] = std::move(newRaw);
            }
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
            if (newRaw == studio.originals[catIdx]) {
                studio.replacements.erase(s.id);
            } else {
                studio.replacements[s.id] = std::move(newRaw);
            }
            char buf[256];
            snprintf(buf, sizeof(buf), Tr(i18n::Str::MsgPastedFromClipboard), s.id.c_str());
            studio.statusMsg = buf;
            studio.statusIsErr = false;
        }
    }
    studio.textureNeedsUpdate = true;
}
