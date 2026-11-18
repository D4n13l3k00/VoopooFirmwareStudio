#pragma once
#include "app_state.h"
#include "firmware_color_audit.h"

inline bool RunModelRegressions(std::string& error) {
    const std::vector<uint8_t> pageMask{0x81, 0x02, 0xfd, 0xfe};
    std::vector<uint8_t> pixels, encoded;
    if (!FirmwareBitmap::DecodeMask(pageMask.data(), pageMask.size(), 2, 10, pixels, AssetFormat::Mask1BitPages)) {
        error = "OLED page-major mask decoding failed";
        return false;
    }
    for (size_t y = 0; y < 10; ++y)
        for (size_t x = 0; x < 2; ++x) {
            const bool lit = x == 0 ? (y == 0 || y == 7 || y == 8) : (y == 1 || y == 9);
            if (pixels[y * 2 + x] != (lit ? 255 : 0)) {
                error = "OLED mask pixels cross columns/pages incorrectly";
                return false;
            }
        }
    pixels[0] = 0;
    pixels[9 * 2] = 255;
    if (!FirmwareBitmap::EncodeMaskPreserving(pixels.data(), 2, 10, pageMask.data(), pageMask.size(),
            encoded, AssetFormat::Mask1BitPages) || encoded != std::vector<uint8_t>({0x80, 0x02, 0xff, 0xfe})) {
        error = "OLED edit changed another pixel or discarded page padding";
        return false;
    }
    StudioState state;
    for (auto model : {FirmwareModel::DragS, FirmwareModel::DragX, FirmwareModel::DragS}) {
        FirmwarePackage stock;
        if (!LoadEmbeddedFirmware(stock, error, model)) return false;
        const auto original = stock.source_bytes;
        if (!state.LoadPackage(std::move(stock), "model-regression", error)) return false;
        auto fail = [&](const char* text) { error = std::string(FirmwareModelName(model)) + ": " + text; return false; };
        if (state.model != model || state.HasUnsavedChanges()) return fail("wrong model or dirty loaded document");
        for (size_t i = 0; i < state.catalog.size(); ++i)
            if (!state.IsActiveMatchingReference(i)) return fail("stock baseline belongs to another model");
        std::vector<uint8_t> packed;
        if (!state.BuildToBuffer(packed, error)) return false;
        if (packed != original) return fail("unmodified build is not byte-identical");
        const std::string logoId = model == FirmwareModel::DragX ? "logo_drag_x" : "logo_drag_s";
        const int logo = state.FindAssetIndex(logoId);
        if (logo < 0 || state.FindAssetIndex(model == FirmwareModel::DragX ? "logo_drag_s" : "logo_drag_x") >= 0)
            return fail("catalog contains the wrong model logo");
        state.PushUndo("edit all model resources");
        if (model == FirmwareModel::DragX) {
            const int wattUnit = state.FindAssetIndex("unit_w");
            if (wattUnit < 0 || state.catalog[wattUnit].width != 17 || state.catalog[wattUnit].height != 15 ||
                state.catalog[wattUnit].address != 0x080104d3)
                return fail("W mask is cropped to the DRAG S suffix");
        }
        for (size_t i = 0; i < state.catalog.size(); ++i) {
            auto bytes = state.originals[i];
            bytes[0] ^= 0x80;
            state.SetBitmapReplacement(i, std::move(bytes));
        }
        if (!state.BuildToBuffer(packed, error)) return false;
        FirmwarePackage edited;
        if (!FirmwarePackage::Parse(packed, edited, error)) return false;
        for (size_t i = 0; i < state.catalog.size(); ++i)
            for (size_t r = 0; r < edited.records.size(); ++r) {
                const auto offset = state.locations[i][r];
                const auto& expected = state.replacements.at(state.catalog[i].id);
                if (!std::equal(expected.begin(), expected.end(), edited.records[r].image.begin() + offset))
                    return fail("edited resource was not propagated to every hardware record");
            }
        StudioState reopened;
        if (!reopened.LoadPackage(std::move(edited), "modified-model", error)) return false;
        if (reopened.model != model || reopened.HasUnsavedChanges()) return fail("modified file failed model detection");
        for (size_t i = 0; i < state.catalog.size(); ++i)
            if (reopened.originals[i] != state.replacements.at(state.catalog[i].id) || reopened.IsActiveMatchingReference(i))
                return fail("reopened resource or stock difference is incorrect");
        if (!RunFirmwareColorAudit(state, error)) return false;
        auto reordered = state.firmware;
        std::reverse(reordered.records.begin(), reordered.records.end());
        if (!reopened.LoadPackage(std::move(reordered), "reordered-model", error)) return false;
        ApplyPaletteTheme(reopened, GetPaletteThemes()[1]);
        if (!reopened.BuildToBuffer(packed, error)) return false;
        FirmwarePackage themed;
        if (!FirmwarePackage::Parse(packed, themed, error)) return false;
        if (!reopened.LoadPackage(std::move(themed), "reordered-theme", error)) return false;
        if (reopened.palette[0].current.ToRgb565() != GetPaletteThemes()[1].accent_red.ToRgb565())
            return fail("palette patches depend on hardware record order");
    }
    FirmwarePackage invalid;
    if (!LoadEmbeddedFirmware(invalid, error, FirmwareModel::DragX)) return false;
    invalid.records[0].device_code[1] = 0x4d;
    if (state.LoadPackage(std::move(invalid), "mixed-model", error)) {
        error = "Mixed DRAG S / DRAG X package was accepted";
        return false;
    }
    error.clear();
    return true;
}
