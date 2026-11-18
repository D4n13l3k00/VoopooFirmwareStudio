#pragma once
#include "zwdf_package.h"
#include "firmware_catalog.h"
#include "../font/presets.h"

namespace ThreeMaxFirmware {
bool Detect(const FirmwarePackage& fw, FirmwareModel& model, FirmwareEdition& edition);
std::vector<AssetSpec> Assets(FirmwareModel model, FirmwareEdition edition);
bool DetectColors(const FirmwarePackage& fw, std::vector<PaletteState>& palette, Rgb8& background);
bool BuildColorPatches(const FirmwarePackage& fw, const std::vector<PaletteState>& palette,
    Rgb8 background, std::vector<CodePatch>& patches, std::string& error);
bool RecordPaletteColor(const FirmwarePackage& fw, uint16_t code, const char* role, Rgb8& color);
const char* PaletteId(const AssetSpec& asset, bool cyanMode);
bool UsesModeColor(const AssetSpec& asset);
Rgb8 Foreground(const FirmwarePackage& fw, const AssetSpec& asset,
    const std::vector<PaletteState>& palette, bool cyanMode);
Rgb8 Background(const FirmwarePackage& fw, const AssetSpec& asset,
    const std::vector<PaletteState>& palette, bool cyanMode);
std::vector<Preset> Presets(FirmwareModel model, FirmwareEdition edition);
}
