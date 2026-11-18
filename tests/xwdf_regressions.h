#pragma once
#include "app_state.h"
#include "firmware_save.h"
#include "presets.h"

inline bool RunXwdfRegressions(std::string& error) {
    FirmwarePackage package;
    if (!LoadEmbeddedFirmware(package, error, FirmwareModel::Drag2)) return false;
    auto fail = [&](const char* message) { error = message; return false; };
    if (package.format != FirmwareFormat::Xwdf || package.records.size() != 1 ||
        package.records[0].image.size() != 50292 || package.records[0].sp != 0x200025a8 ||
        package.records[0].reset != 0x08003145 || package.records[0].linked_base != 0x08003000)
        return fail("DRAG 2 decoded image metadata does not match actual vectors");
    const auto source = package.source_bytes;
    std::vector<uint8_t> packed;
    BuildReport report;
    if (!package.PackWith({}, {}, packed, report, error)) return false;
    if (packed != source) return fail("Untouched XWDF did not roundtrip byte-for-byte");
    for (size_t offset : {size_t(48), size_t(64), size_t(96), size_t(228), size_t(360), source.size() - 1}) {
        auto damaged = source;
        damaged[offset] ^= 1;
        FirmwarePackage rejected;
        std::string expectedError;
        if (FirmwarePackage::Parse(damaged, rejected, expectedError))
            return fail("XWDF accepted corrupted block, segment or package checksum");
    }
    for (size_t size : {size_t(4), size_t(99), size_t(231), source.size() - 4}) {
        FirmwarePackage rejected;
        std::string expectedError;
        if (FirmwarePackage::Parse(std::vector<uint8_t>(source.begin(), source.begin() + size), rejected, expectedError))
            return fail("XWDF accepted a truncated container");
    }
    StudioState state;
    if (!state.LoadPackage(std::move(package), "DRAG2_V1.0.0.1.xwdf", error)) return false;
    state.isBuiltin = true;
    if (state.model != FirmwareModel::Drag2 || FirmwareHasPalette(state.model) || !state.palette.empty())
        return fail("DRAG 2 acquired a color palette or incorrect model");
    if (!state.BuildToBuffer(packed, error)) return false;
    if (packed != source) return fail("DRAG 2 default editor build changed stock bytes");
    const auto fontPresets = Preset::all(FirmwareModel::Drag2);
    for (const auto& preset : fontPresets)
        for (const auto& glyph : preset.glyphs) {
            const auto id = fs::path(glyph.filename).stem().string();
            if (glyph.ch == ' ' || (id.rfind("font_hex_5x8_", 0) == 0 ? id.ends_with("_16") || id.ends_with("_17") : id.ends_with("_10")))
                return fail("DRAG 2 font generation overwrites a special firmware glyph");
            const int index = state.FindAssetIndex(id);
            if (index < 0 || state.catalog[index].width != static_cast<uint32_t>(preset.width) ||
                state.catalog[index].height != static_cast<uint32_t>(preset.height))
                return fail("DRAG 2 font generator targets absent or wrong-size native glyph");
        }
    state.PushUndo("Edit all DRAG 2 resources");
    for (size_t i = 0; i < state.catalog.size(); ++i) {
        const auto& spec = state.catalog[i];
        std::vector<uint8_t> pixels, raw;
        if (!FirmwareBitmap::DecodeMask(state.originals[i].data(), spec.byte_len(), spec.width,
                spec.height, pixels, spec.format) ||
            !FirmwareBitmap::EncodeMaskPreserving(pixels.data(), spec.width, spec.height,
                state.originals[i].data(), spec.byte_len(), raw, spec.format) || raw != state.originals[i])
            return fail("DRAG 2 native bitmap decode/re-encode lost pixels or padding");
        pixels[0] ^= 255;
        if (!FirmwareBitmap::EncodeMaskPreserving(pixels.data(), spec.width, spec.height,
                state.originals[i].data(), spec.byte_len(), raw, spec.format)) return fail("DRAG 2 pixel edit failed");
        state.SetBitmapReplacement(i, std::move(raw));
    }
    if (!state.BuildToBuffer(packed, error)) return false;
    FirmwarePackage edited;
    if (!FirmwarePackage::Parse(packed, edited, error)) return false;
    for (size_t i = 0; i < state.catalog.size(); ++i) {
        const auto& expected = state.replacements.at(state.catalog[i].id);
        if (!std::equal(expected.begin(), expected.end(), edited.records[0].image.begin() + state.locations[i][0]))
            return fail("XWDF rebuilt an edited resource incorrectly");
    }
    StudioState reopened;
    if (!reopened.LoadPackage(std::move(edited), "modified-drag2.xwdf", error)) return false;
    for (size_t i = 0; i < state.catalog.size(); ++i)
        if (reopened.originals[i] != state.replacements.at(state.catalog[i].id) || reopened.IsActiveMatchingReference(i))
            return fail("Modified DRAG 2 resources or stock baseline failed on reopen");
    auto root = fs::temp_directory_path() / (L"voopoo_xwdf_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64()));
    fs::create_directory(root);
    struct Cleanup { fs::path root; ~Cleanup() { std::error_code ignored; fs::remove_all(root, ignored); } } cleanup{root};
    const auto destination = root / L"DRAG2.xwdf";
    const auto prepared = FirmwareSave::Prepare(state.CreateFileSnapshot());
    auto saved = FirmwareSave::Write(*prepared, state, FirmwareSave::Approval::Confirmed,
        PathUtils::WideToUtf8(destination.wstring()));
    if (saved.FirmwareSaved()) state.MarkSaved();
    if (saved.outcome != FirmwareSave::Outcome::Complete || state.HasUnsavedChanges())
        return fail("DRAG 2 save/report did not complete");
    FirmwarePackage disk;
    if (!FirmwarePackage::Open(PathUtils::WideToUtf8(destination.wstring()), disk, error)) return false;
    if (disk.source_bytes != packed || disk.format != FirmwareFormat::Xwdf)
        return fail("DRAG 2 saved output is not the rebuilt XWDF");
    return true;
}
