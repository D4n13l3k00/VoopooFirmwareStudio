#pragma once
#include "firmware_color.h"

#include <string>
#include <vector>
#include <utility>
#include <cstdint>

enum class FirmwareModel { DragS, DragX, Drag2, DragSPro, DragXPro, DragXPlus, DragXPlusPro,
    Drag3, DragMax, Drag1, DragMini, Drag2Refresh, DragMiniRefresh };
enum class FirmwareEdition { Score, Factory, Standard, Tc, V1, V2, Fit3 };

class FirmwarePackage;
bool DetectFirmwareModel(const FirmwarePackage& fw, FirmwareModel& model);
bool DetectFirmwareProfile(const FirmwarePackage& fw, FirmwareModel& model, FirmwareEdition& edition);
const char* FirmwareEditionName(FirmwareEdition edition);
const char* FirmwareModelName(FirmwareModel model);
const char* FirmwareDefaultFilename(FirmwareModel model, FirmwareEdition edition = FirmwareEdition::Score);
uint16_t FirmwareCanonicalCode(FirmwareModel model);
bool FirmwareHasPalette(FirmwareModel model);

enum class AssetFormat {
    Mask1Bit,
    Mask1BitPages,
    Rgb565
};

struct AssetSpec {
    std::string id;
    std::string title;
    std::string group;
    uint32_t address = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    size_t ordinal = 0;
    AssetFormat format = AssetFormat::Mask1Bit;
    // Nonzero for a hardware-specific resource; zero means the model's canonical record.
    uint16_t sourceCode = 0;
    // Missing codes do not contain this Factory resource.
    std::vector<std::pair<uint16_t, uint32_t>> recordAddresses;
    uint32_t foregroundOffset = 0; // RGB888 draw-color instruction in sourceCode.

    size_t stride() const {
        return format == AssetFormat::Mask1BitPages ? width :
            ((format == AssetFormat::Rgb565) ? ((size_t)width * 2) : ((width + 7) / 8));
    }
    size_t byte_len() const {
        return format == AssetFormat::Mask1BitPages ? ((size_t)width * ((height + 7) / 8)) :
            ((format == AssetFormat::Rgb565) ? ((size_t)width * height * 2) : (stride() * (size_t)height));
    }
};

const char* PaletteIdForAsset(const AssetSpec& spec, size_t previewRank = 0, bool cyanMode = false,
    FirmwareEdition edition = FirmwareEdition::Score, FirmwareModel model = FirmwareModel::DragS);
Rgb8 FixedForegroundForAsset(const AssetSpec& spec);
bool AssetUsesModeColor(const AssetSpec& spec, FirmwareModel model = FirmwareModel::DragS);
Rgb8 FirmwareFixedForeground(const FirmwarePackage& fw, const AssetSpec& spec, size_t previewRank);
std::vector<AssetSpec> GetAllAssets(FirmwareModel model = FirmwareModel::DragS,
    FirmwareEdition edition = FirmwareEdition::Score);
