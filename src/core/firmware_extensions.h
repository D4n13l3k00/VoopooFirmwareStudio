#pragma once
#include "pro_firmware.h"
#include "plus_firmware.h"
#include "three_max_firmware.h"
#include "legacy_firmware.h"

struct FirmwareExtension {
    bool (*detect)(const FirmwarePackage&, FirmwareModel&, FirmwareEdition&);
    std::vector<AssetSpec> (*assets)(FirmwareModel, FirmwareEdition);
    bool (*detectColors)(const FirmwarePackage&, std::vector<PaletteState>&, Rgb8&);
    bool (*buildColors)(const FirmwarePackage&, const std::vector<PaletteState>&, Rgb8, std::vector<CodePatch>&, std::string&);
    bool (*recordColor)(const FirmwarePackage&, uint16_t, const char*, Rgb8&);
    const char* (*paletteId)(const AssetSpec&, bool);
    bool (*usesModeColor)(const AssetSpec&);
    Rgb8 (*foreground)(const FirmwarePackage&, const AssetSpec&, const std::vector<PaletteState>&, bool);
    Rgb8 (*background)(const FirmwarePackage&, const AssetSpec&, const std::vector<PaletteState>&, bool);
    std::vector<Preset> (*presets)(FirmwareModel, FirmwareEdition);
};

inline const FirmwareExtension* FirmwareExtensionFor(FirmwareModel model) {
    static const FirmwareExtension pro = {ProFirmware::Detect, ProFirmware::Assets, ProFirmware::DetectColors,
        ProFirmware::BuildColorPatches, ProFirmware::RecordPaletteColor, ProFirmware::PaletteId,
        ProFirmware::UsesModeColor, ProFirmware::Foreground, ProFirmware::Background, ProFirmware::Presets};
    static const FirmwareExtension plus = {PlusFirmware::Detect, PlusFirmware::Assets, PlusFirmware::DetectColors,
        PlusFirmware::BuildColorPatches, PlusFirmware::RecordPaletteColor, PlusFirmware::PaletteId,
        PlusFirmware::UsesModeColor, PlusFirmware::Foreground, PlusFirmware::Background, PlusFirmware::Presets};
    static const FirmwareExtension threeMax = {ThreeMaxFirmware::Detect, ThreeMaxFirmware::Assets, ThreeMaxFirmware::DetectColors,
        ThreeMaxFirmware::BuildColorPatches, ThreeMaxFirmware::RecordPaletteColor, ThreeMaxFirmware::PaletteId,
        ThreeMaxFirmware::UsesModeColor, ThreeMaxFirmware::Foreground, ThreeMaxFirmware::Background, ThreeMaxFirmware::Presets};
    static const FirmwareExtension legacy = {LegacyFirmware::Detect, LegacyFirmware::Assets, LegacyFirmware::DetectColors,
        LegacyFirmware::BuildColorPatches, LegacyFirmware::RecordPaletteColor, LegacyFirmware::PaletteId,
        LegacyFirmware::UsesModeColor, LegacyFirmware::Foreground, LegacyFirmware::Background, LegacyFirmware::Presets};
    switch (model) {
    case FirmwareModel::DragSPro: case FirmwareModel::DragXPro: return &pro;
    case FirmwareModel::DragXPlus: case FirmwareModel::DragXPlusPro: return &plus;
    case FirmwareModel::Drag3: case FirmwareModel::DragMax: return &threeMax;
    case FirmwareModel::Drag1: case FirmwareModel::DragMini:
    case FirmwareModel::Drag2Refresh: case FirmwareModel::DragMiniRefresh: return &legacy;
    default: return nullptr;
    }
}
