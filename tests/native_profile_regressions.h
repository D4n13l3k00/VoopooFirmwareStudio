#pragma once
#include "../src/core/firmware_profiles.h"
#include "../src/font/font_renderer.h"
#include "firmware_color_audit.h"

inline bool RunNativeProfileRegressions(std::string& error) {
    FontRenderer renderer;
    if (!renderer.LoadFontFile("C:\\Windows\\Fonts\\arial.ttf")) {
        error = "Cannot load regression font";
        return false;
    }
    for (const auto& profile : FirmwareProfiles()) {
        if (!FirmwareExtensionFor(profile.model)) continue;
        const auto fail = [&](const std::string& why) {
            error = std::string(profile.filename) + ": " + why;
            return false;
        };
        FirmwarePackage package;
        StudioState state;
        if (!LoadEmbeddedFirmware(package, error, profile.model, profile.edition) ||
            !state.LoadPackage(std::move(package), profile.filename, error)) return fail(error);
        if (state.model != profile.model || state.edition != profile.edition || state.HasUnsavedChanges())
            return fail("Wrong profile or dirty initial state");
        std::vector<uint8_t> packed;
        if (!state.BuildToBuffer(packed, error) || packed != state.firmware.source_bytes)
            return fail("Stock no-op is not byte-exact: " + error);
        for (size_t i = 0; i < state.catalog.size(); ++i) {
            if (!state.IsActiveMatchingReference(i)) return fail("Wrong stock reference: " + state.catalog[i].id);
            for (size_t j = 0; j < i; ++j) {
                for (size_t r = 0; r < state.firmware.records.size(); ++r) {
                    const size_t a = state.locations[i][r], b = state.locations[j][r];
                    if (a == SIZE_MAX || b == SIZE_MAX) continue;
                    if (a < b + state.catalog[j].byte_len() && b < a + state.catalog[i].byte_len())
                        return fail("Overlapping editable resources: " + state.catalog[i].id + " / " + state.catalog[j].id);
                }
            }
        }
        if (FirmwareHasPalette(profile.model) && !RunFirmwareColorAudit(state, error)) return fail(error);
        state.PushUndo("Native resource mutation");
        for (size_t i = 0; i < state.catalog.size(); ++i) {
            auto raw = state.originals[i];
            raw[0] ^= 1;
            state.SetBitmapReplacement(i, std::move(raw));
        }
        if (!state.BuildToBuffer(packed, error)) return fail(error);
        FirmwarePackage modified;
        if (!FirmwarePackage::Parse(packed, modified, error)) return fail(error);
        for (size_t i = 0; i < state.catalog.size(); ++i) {
            const auto& expected = state.replacements.at(state.catalog[i].id);
            for (size_t r = 0; r < modified.records.size(); ++r) {
                const auto offset = state.locations[i][r];
                if (offset == SIZE_MAX) continue;
                if (!std::equal(expected.begin(), expected.end(), modified.records[r].image.begin() + offset))
                    return fail("Resource edit lost: " + state.catalog[i].id);
            }
        }
        StudioState reopened;
        if (!reopened.LoadPackage(std::move(modified), profile.filename, error)) return fail(error);
        if (reopened.model != profile.model || reopened.edition != profile.edition || reopened.HasUnsavedChanges())
            return fail("Edited profile detection failed");
        for (size_t i = 0; i < reopened.catalog.size(); ++i)
            if (reopened.IsActiveMatchingReference(i)) return fail("Edited resource mistaken for stock");
        if (!LoadEmbeddedFirmware(package, error, profile.model, profile.edition) ||
            !state.LoadPackage(std::move(package), profile.filename, error)) return fail(error);
        const auto presets = Preset::all(profile.model, profile.edition);
        if (presets.empty()) return fail("Missing native font presets");
        state.PushUndo("Generate native fonts");
        for (const auto& preset : presets) {
            RenderOptions opts;
            opts.target_width = preset.width;
            opts.target_height = preset.height;
            const auto rendered = renderer.RenderPreset(preset, opts);
            for (const auto& glyph : rendered.second) {
                const auto id = glyph.filename.substr(0, glyph.filename.size() - 4);
                const int index = state.FindAssetIndex(id);
                if (index < 0) return fail("Missing generated font target: " + id);
                const auto& spec = state.catalog[index];
                if (glyph.width != spec.width || glyph.height != spec.height)
                    return fail("Wrong generated font dimensions: " + id);
                std::vector<uint8_t> raw;
                if (!FirmwareBitmap::EncodeMaskPreserving(glyph.pixels.data(), spec.width, spec.height,
                    state.originals[index].data(), spec.byte_len(), raw, spec.format)) return fail("Font encoding failed");
                state.SetBitmapReplacement(index, std::move(raw));
            }
            if (preset.rotationQuarterTurns == 1 || preset.rotationQuarterTurns == 3) {
                Preset upright = preset;
                upright.rotationQuarterTurns = 0;
                std::swap(upright.width, upright.height);
                std::swap(opts.target_width, opts.target_height);
                const auto reference = renderer.RenderPreset(upright, opts);
                for (size_t g = 0; g < reference.second.size(); ++g) {
                    const auto& a = reference.second[g];
                    const auto& b = rendered.second[g];
                    for (int y = 0; y < a.height; ++y)
                        for (int x = 0; x < a.width; ++x)
                            if (a.pixels[y * a.width + x] != b.pixels[preset.rotationQuarterTurns == 1
                                ? x * b.width + a.height - 1 - y : (a.width - 1 - x) * b.width + y])
                                return fail("Rotated font orientation mismatch");
                }
            }
        }
        if (!state.BuildToBuffer(packed, error) || !FirmwarePackage::Parse(packed, modified, error) ||
            !reopened.LoadPackage(std::move(modified), profile.filename, error)) return fail(error);
        for (size_t i = 0; i < state.catalog.size(); ++i) {
            const auto replacement = state.replacements.find(state.catalog[i].id);
            const auto& expected = replacement == state.replacements.end() ? state.originals[i] : replacement->second;
            if (reopened.originals[i] != expected) return fail("Generated font or preserved special glyph changed");
        }
        if (FirmwareHasPalette(profile.model)) {
            const auto stockPalette = state.palette;
            ApplyPaletteTheme(state, GetPaletteThemes()[1]);
            if (!state.BuildToBuffer(packed, error) || !FirmwarePackage::Parse(packed, modified, error) ||
                !reopened.LoadPackage(std::move(modified), profile.filename, error)) return fail(error);
            ApplyPaletteTheme(reopened, GetPaletteThemes()[0]);
            if (!reopened.BuildToBuffer(packed, error) || !FirmwarePackage::Parse(packed, modified, error)) return fail(error);
            StudioState restored;
            if (!restored.LoadPackage(std::move(modified), profile.filename, error)) return fail(error);
            for (const auto& role : stockPalette)
                if (restored.GetPaletteColor(role.spec.id).ToRgb565() != role.current.ToRgb565())
                    return fail("Stock palette restoration failed");
        }
        auto reordered = state.firmware;
        std::reverse(reordered.records.begin(), reordered.records.end());
        if (!reopened.LoadPackage(std::move(reordered), profile.filename, error)) return fail(error);
    }
    return true;
}
