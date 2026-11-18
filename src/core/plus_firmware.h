#pragma once
#include "zwdf_package.h"
#include "firmware_catalog.h"
#include "../font/presets.h"

namespace PlusFirmware {
bool Detect(const FirmwarePackage&, FirmwareModel&, FirmwareEdition&);
std::vector<AssetSpec> Assets(FirmwareModel, FirmwareEdition);
bool DetectColors(const FirmwarePackage&, std::vector<PaletteState>&, Rgb8&);
bool BuildColorPatches(const FirmwarePackage&, const std::vector<PaletteState>&, Rgb8,
    std::vector<CodePatch>&, std::string&);
bool RecordPaletteColor(const FirmwarePackage&, uint16_t, const char*, Rgb8&);
const char* PaletteId(const AssetSpec&, bool cyanMode);
bool UsesModeColor(const AssetSpec&);
Rgb8 Foreground(const FirmwarePackage&, const AssetSpec&, const std::vector<PaletteState>&, bool cyanMode);
Rgb8 Background(const FirmwarePackage&, const AssetSpec&, const std::vector<PaletteState>&, bool cyanMode);
std::vector<Preset> Presets(FirmwareModel, FirmwareEdition);
}
