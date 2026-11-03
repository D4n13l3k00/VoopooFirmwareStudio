#pragma once
#include "app_state.h"

// Compare staged preview colors with a freshly packed and reparsed firmware.
// Optional output feeds the independent ARM call-site audit (no device access).
inline bool RunFirmwareColorAudit(const StudioState& source, std::string& error) {
    wchar_t auditDir[32768]{};
    const DWORD auditLen = GetEnvironmentVariableW(L"VOOPOO_COLOR_AUDIT_DIR", auditDir, 32768);
    const bool exportAudit = auditLen && auditLen < 32768;
    const fs::path root(auditDir);
    if (exportAudit) fs::create_directories(root);
    const auto& themes = GetPaletteThemes();
    for (size_t themeIndex = 0; themeIndex <= themes.size(); ++themeIndex) {
        StudioState staged;
        staged.firmware = source.firmware;
        staged.palette = GetDefaultPalette();
        if (themeIndex < themes.size()) ApplyPaletteTheme(staged, themes[themeIndex]);
        else {
            // Non-white digits expose the old "all font masks are white" bug.
            for (size_t i = 0; i < staged.palette.size(); ++i)
                staged.palette[i].current = Rgb8((uint8_t)(30 + i * 17), (uint8_t)(230 - i * 13), (uint8_t)(50 + i * 11));
        }
        std::vector<CodePatch> patches;
        std::vector<uint8_t> packed;
        BuildReport report;
        if (!BuildColorPatches(staged.firmware, staged.palette, {0, 0, 0}, patches, error) ||
            !staged.firmware.PackWith({}, patches, packed, report, error)) return false;
        StudioState reopened;
        if (!FirmwarePackage::Parse(packed, reopened.firmware, error)) return false;
        reopened.palette = GetDefaultPalette();
        if (!DetectFirmwareColors(reopened.firmware, reopened.palette, reopened.backgroundColor)) return false;
        std::ofstream cases;
        if (exportAudit) {
            const std::string stem = "theme_" + std::to_string(themeIndex);
            for (const auto& rec : reopened.firmware.records) {
                if (rec.device_code[0] == 0x31 && rec.device_code[1] == 0x03) {
                    std::ofstream image(root / (stem + ".bin"), std::ios::binary);
                    image.write((const char*)rec.image.data(), rec.image.size());
                }
            }
            cases.open(root / (stem + ".json"));
            cases << "[\n";
        }
        bool first = true;
        for (const auto& spec : source.catalog) {
            if (spec.format == AssetFormat::Rgb565) continue;
            const int ranks = spec.id.rfind("meter_", 0) == 0 ? 8 : 1;
            for (int mode = 0; mode < 2; ++mode) for (int rank = 0; rank < ranks; ++rank) {
                staged.previewCyanMode = reopened.previewCyanMode = mode != 0;
                staged.previewRank = reopened.previewRank = rank;
                const auto fg = staged.GetForegroundForAsset(spec).ToRgb565();
                const auto bg = staged.GetBackgroundForAsset(spec).ToRgb565();
                if (fg != reopened.GetForegroundForAsset(spec).ToRgb565() ||
                    bg != reopened.GetBackgroundForAsset(spec).ToRgb565()) {
                    error = "Packed color differs from preview: " + spec.id;
                    return false;
                }
                if (exportAudit) {
                    if (!first) cases << ",\n";
                    first = false;
                    cases << "{\"id\":\"" << spec.id << "\",\"address\":" << spec.address
                        << ",\"width\":" << spec.width << ",\"height\":" << spec.height
                        << ",\"mode\":" << mode << ",\"rank\":" << rank
                        << ",\"fg\":" << fg << ",\"bg\":" << bg << "}";
                }
            }
        }
        if (exportAudit) cases << "\n]\n";
    }
    return true;
}
