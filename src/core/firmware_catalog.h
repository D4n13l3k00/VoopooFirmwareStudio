#pragma once
#include "firmware_color.h"

#include <string>
#include <vector>
#include <cstdint>

enum class AssetFormat {
    Mask1Bit,
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

    size_t stride() const {
        return (format == AssetFormat::Rgb565) ? ((size_t)width * 2) : ((width + 7) / 8);
    }
    size_t byte_len() const {
        return (format == AssetFormat::Rgb565) ? ((size_t)width * height * 2) : (stride() * (size_t)height);
    }
};

const char* PaletteIdForAsset(const AssetSpec& spec, size_t previewRank = 0, bool cyanMode = false);
Rgb8 FixedForegroundForAsset(const AssetSpec& spec);
bool AssetUsesModeColor(const AssetSpec& spec);
class FirmwarePackage;
Rgb8 FirmwareFixedForeground(const FirmwarePackage& fw, const AssetSpec& spec, size_t previewRank);
std::vector<AssetSpec> GetAllAssets();
