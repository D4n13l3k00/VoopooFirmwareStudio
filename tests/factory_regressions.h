#pragma once
#include "app_state.h"
#include "presets.h"
#include "firmware_color_audit.h"

inline bool RunFactoryRegressions(std::string& error) {
    StudioState state;
    for (auto model : {FirmwareModel::DragS, FirmwareModel::DragX}) {
        FirmwarePackage stock;
        if (!LoadEmbeddedFirmware(stock, error, model, FirmwareEdition::Factory)) return false;
        const auto original = stock.source_bytes;
        auto fail = [&](const char* message) { error = std::string(FirmwareModelName(model)) + " Factory: " + message; return false; };
        if (!state.LoadPackage(std::move(stock), FirmwareDefaultFilename(model, FirmwareEdition::Factory), error)) return false;
        if (state.model != model || state.edition != FirmwareEdition::Factory || state.HasUnsavedChanges())
            return fail("wrong detected profile or dirty stock document");
        const auto stockPalette = state.palette;
        std::vector<uint8_t> packed;
        if (!state.BuildToBuffer(packed, error)) return false;
        if (packed != original) return fail("unmodified editor build changed Factory bytes");
        for (size_t i = 0; i < state.catalog.size(); ++i) {
            const auto& spec = state.catalog[i];
            if (!state.IsActiveMatchingReference(i)) return fail("stock comparison uses a different edition");
            if (spec.format != AssetFormat::Rgb565) {
                std::vector<uint8_t> pixels, raw;
                if (!FirmwareBitmap::DecodeMask(state.originals[i].data(), spec.byte_len(), spec.width,
                        spec.height, pixels, spec.format) ||
                    !FirmwareBitmap::EncodeMaskPreserving(pixels.data(), spec.width, spec.height,
                        state.originals[i].data(), spec.byte_len(), raw, spec.format) || raw != state.originals[i])
                    return fail("native resource codec loses bits or padding");
            }
        }
        for (const auto& preset : Preset::all(model))
            for (const auto& glyph : preset.glyphs) {
                const auto id = fs::path(glyph.filename).stem().string();
                const int index = state.FindAssetIndex(id);
                if (index < 0 || state.catalog[index].width != static_cast<uint32_t>(preset.width) ||
                    state.catalog[index].height != static_cast<uint32_t>(preset.height))
                    return fail("font generator targets absent or wrong-size native glyph");
            }
        if (!RunFirmwareColorAudit(state, error)) return false;
        ApplyPaletteTheme(state, GetPaletteThemes()[1]);
        if (!state.BuildToBuffer(packed, error)) return false;
        FirmwarePackage themed;
        if (!FirmwarePackage::Parse(packed, themed, error)) return false;
        if (!state.LoadPackage(std::move(themed), "custom-factory.zwdf", error)) return false;
        ApplyPaletteTheme(state, GetPaletteThemes()[0]);
        for (size_t i = 0; i < stockPalette.size(); ++i)
            if (state.palette[i].spec.id != std::string(stockPalette[i].spec.id) ||
                state.palette[i].current.ToRgb565() != stockPalette[i].current.ToRgb565())
                return fail("Factory Default restores Score or customized-source colors");
        if (!LoadEmbeddedFirmware(stock, error, model, FirmwareEdition::Factory) ||
            !state.LoadPackage(std::move(stock), "factory-edit", error)) return false;
        state.PushUndo("Edit all Factory resources");
        for (size_t i = 0; i < state.catalog.size(); ++i) {
            auto raw = state.originals[i];
            raw[0] ^= 1;
            state.SetBitmapReplacement(i, std::move(raw));
        }
        if (!state.BuildToBuffer(packed, error)) return false;
        FirmwarePackage edited;
        if (!FirmwarePackage::Parse(packed, edited, error)) return false;
        for (size_t i = 0; i < state.catalog.size(); ++i)
            for (size_t r = 0; r < edited.records.size(); ++r) {
                if (state.locations[i][r] == SIZE_MAX) continue;
                const auto& expected = state.replacements.at(state.catalog[i].id);
                if (!std::equal(expected.begin(), expected.end(), edited.records[r].image.begin() + state.locations[i][r]))
                    return fail("an edited asset was not written to every hardware variant");
            }
        StudioState reopened;
        if (!reopened.LoadPackage(std::move(edited), "edited-factory.zwdf", error)) return false;
        if (reopened.edition != FirmwareEdition::Factory || reopened.model != model || reopened.HasUnsavedChanges())
            return fail("custom Factory file was classified as Score");
        for (size_t i = 0; i < state.catalog.size(); ++i)
            if (reopened.originals[i] != state.replacements.at(state.catalog[i].id) || reopened.IsActiveMatchingReference(i))
                return fail("modified Factory asset or its stock baseline was lost on reopen");
        auto reordered = reopened.firmware;
        std::reverse(reordered.records.begin(), reordered.records.end());
        if (!reopened.LoadPackage(std::move(reordered), "reordered-factory.zwdf", error)) return false;
        ApplyPaletteTheme(reopened, GetPaletteThemes()[2]);
        if (!reopened.BuildToBuffer(packed, error)) return false;
        FirmwarePackage reorderedTheme;
        if (!FirmwarePackage::Parse(packed, reorderedTheme, error)) return false;
        StudioState reorderedReopened;
        if (!reorderedReopened.LoadPackage(std::move(reorderedTheme), "reordered-theme.zwdf", error)) return false;
        for (size_t i = 0; i < reopened.palette.size(); ++i)
            if (reopened.palette[i].current.ToRgb565() != reorderedReopened.palette[i].current.ToRgb565())
                return fail("palette patches depend on hardware record order");
        FirmwarePackage score;
        if (!LoadEmbeddedFirmware(score, error, model, FirmwareEdition::Score) ||
            !state.LoadPackage(std::move(score), "score-after-factory.zwdf", error)) return false;
        if (state.edition != FirmwareEdition::Score) return fail("switching to Score retained Factory edition");
        for (size_t i = 0; i < state.catalog.size(); ++i)
            if (!state.IsActiveMatchingReference(i)) return fail("switching to Score retained Factory reference bitmaps");
    }
    return true;
}
