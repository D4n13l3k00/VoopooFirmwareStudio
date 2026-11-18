#include "firmware_layout.h"
#include "firmware_three_max_profiles.h"
#include "firmware_three_max_layout_profiles.h"
#include "firmware_three_max_semantic_profiles.h"
#include "embedded_firmware.h"
#include "three_max_firmware.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <optional>
#include <utility>

namespace FirmwareLayout {
namespace {
uint16_t Code(const HardwareRecord& record) {
    return uint16_t(record.device_code[0]) << 8 | record.device_code[1];
}
uint32_t ReadWord(const HardwareRecord& record, uint32_t address, unsigned width) {
    uint32_t value = 0;
    const size_t offset = address - record.linked_base;
    for (unsigned i = 0; i < width; ++i) value |= uint32_t(record.image[offset + i]) << (8 * i);
    return value;
}
const ThreeMaxNative::Profile* ProfileFor(const HardwareRecord& record) {
    for (const auto& profile : ThreeMaxNative::Profiles)
        if (profile.code == Code(record) && profile.base == record.linked_base &&
            profile.stack == record.sp && profile.size == record.image.size()) return &profile;
    return nullptr;
}
bool Decode(const HardwareRecord& record, const ThreeMaxNative::LayoutSite& site, int& value) {
    if (site.address < record.linked_base || site.address - record.linked_base > record.image.size() ||
        site.width > record.image.size() - (site.address - record.linked_base)) return false;
    const uint32_t current = ReadWord(record, site.address, site.width);
    uint32_t mask = 0;
    switch (site.encoding) {
    case Encoding::ThumbImm8: mask = 0xff; value = current & 255; break;
    case Encoding::ThumbMovImm: mask = 0x00ff0000; value = current >> 16 & 255; break;
    case Encoding::ThumbMovw:
        mask = 0x70ff040f;
        value = (current & 15) << 12 | (current >> 10 & 1) << 11 |
            (current >> 28 & 7) << 8 | (current >> 16 & 255);
        break;
    case Encoding::ThumbMovAliasImm8: {
        if (current == site.instruction) { value = site.value; return true; }
        const unsigned destination = (site.instruction & 0xf800) == 0x9800 ?
            (site.instruction >> 8 & 7) : (site.instruction & 7);
        if ((current & 0xf800) != 0x2000 || (current >> 8 & 7) != destination) return false;
        value = current & 255;
        return true;
    }
    default: return false;
    }
    return (current & ~mask) == (site.instruction & ~mask);
}
bool RestorePalette(FirmwarePackage& package, const std::vector<PaletteState>& palette, std::string& error) {
    std::vector<CodePatch> patches;
    if (!ThreeMaxFirmware::BuildColorPatches(package, palette, {0, 0, 0}, patches, error)) return false;
    for (const auto& patch : patches)
        for (size_t r = 0; r < patch.locations.size(); ++r)
            for (size_t offset : patch.locations[r])
                std::copy(patch.replacement.begin(), patch.replacement.end(), package.records[r].image.begin() + offset);
    return true;
}
bool Recover(const HardwareRecord& record, const ThreeMaxNative::Profile& profile,
    std::vector<Element>& elements, std::string& error) {
    static constexpr const char* screenNames[] = {
        "main", "theme_menu", "device_logo", "off", "charging", "atomizer_short",
        "check_atomizer", "check_battery", "low_power", "max_power", "puff_cleared",
        "high_temperature", "time_out", "locked", "unlocked", "chip_id", "boot_logo",
        "blank", "tc_material_menu", "tc_settings"
    };
    const unsigned profileIndex = static_cast<unsigned>(&profile - ThreeMaxNative::Profiles);
    for (const auto& block : ThreeMaxNative::LayoutBlocks) {
        if (block.profile != profileIndex) continue;
        Element element;
        char id[128];
        std::snprintf(id, sizeof(id), "hw%04X.%s.block_%08X.mode%d.orientation%d",
            profile.code, screenNames[block.screen], block.call, block.mode, block.orientation);
        element.id = id;
        static_assert(std::size(ThreeMaxNative::SemanticBlocks) == std::size(ThreeMaxNative::LayoutBlocks));
        const auto& semantic = ThreeMaxNative::SemanticBlocks[&block - ThreeMaxNative::LayoutBlocks];
        if (semantic.profile != profileIndex || semantic.call != block.call) {
            error = "Inconsistent native DRAG semantic provenance";
            return false;
        }
        element.semanticId = semantic.semanticId;
        element.title = semantic.title;
        element.hardwareCode = profile.code;
        element.screen = block.screen;
        for (unsigned screen = 0; screen < 32; ++screen)
            if (block.sharedScreens & (1u << screen)) element.sharedScreens.push_back(screen);
        element.temperatureMode = block.mode;
        element.orientation = block.orientation;
        const bool landscape = block.orientation == 1;
        element.screenWidth = landscape ? profile.height : profile.width;
        element.screenHeight = landscape ? profile.width : profile.height;
        element.position = landscape ? Point{block.y, profile.width - block.x - block.width} : Point{block.x, block.y};
        element.width = landscape ? block.height : block.width;
        element.height = landscape ? block.width : block.height;
        std::optional<int> dx, dy;
        bool haveX = false, haveY = false;
        for (size_t i = 0; i < block.siteCount; ++i) {
            const auto& source = ThreeMaxNative::LayoutSites[block.firstSite + i];
            int value = 0;
            if (!Decode(record, source, value)) { error = "Unsupported DRAG native coordinate instruction"; return false; }
            const bool vertical = landscape ? !source.vertical : source.vertical;
            const int scale = landscape && !source.vertical ? -1 : 1;
            const int delta = (value - source.value) * scale;
            auto& axis = vertical ? dy : dx;
            if (axis && *axis != delta) { error = "Incoherent DRAG native block coordinates"; return false; }
            axis = delta;
            (vertical ? haveY : haveX) = true;
            element.sites.push_back({source.address, source.encoding, vertical, value, scale});
        }
        for (size_t i = 0; i < block.repaintCount; ++i) {
            const auto& source = ThreeMaxNative::RepaintSites[block.firstRepaint + i];
            const uint32_t current = ReadWord(record, source.address, 2);
            if (current != source.original && current != source.replacement) {
                error = "Unsupported DRAG native repaint instruction";
                return false;
            }
            NativePatch patch;
            patch.address = source.address;
            patch.original = {uint8_t(source.original), uint8_t(source.original >> 8)};
            patch.replacement = {uint8_t(source.replacement), uint8_t(source.replacement >> 8)};
            element.repaint.push_back(std::move(patch));
            element.repaintRequired |= current == source.replacement;
        }
        element.position.x += dx.value_or(0);
        element.position.y += dy.value_or(0);
        element.minimumOffset = {haveX ? -element.position.x : 0, haveY ? -element.position.y : 0};
        element.maximumOffset = {haveX ? element.screenWidth - element.width - element.position.x : 0,
            haveY ? element.screenHeight - element.height - element.position.y : 0};
        if (element.minimumOffset.x > 0 || element.maximumOffset.x < 0 ||
            element.minimumOffset.y > 0 || element.maximumOffset.y < 0) {
            error = "Native DRAG block is outside its LCD coordinate range";
            return false;
        }
        elements.push_back(std::move(element));
    }
    return true;
}
} // namespace

bool NormalizeThreeMaxLayoutContext(const HardwareRecord& record, uint32_t address, uint8_t* bytes, size_t length) {
    const auto* profile = ProfileFor(record);
    if (!profile) return false;
    const unsigned index = static_cast<unsigned>(profile - ThreeMaxNative::Profiles);
    for (const auto& block : ThreeMaxNative::LayoutBlocks) {
        if (block.profile != index) continue;
        for (size_t i = 0; i < block.siteCount; ++i) {
            const auto& site = ThreeMaxNative::LayoutSites[block.firstSite + i];
            if (uint64_t(site.address) + site.width <= address || site.address >= uint64_t(address) + length) continue;
            int value;
            if (!Decode(record, site, value)) return false;
            for (unsigned b = 0; b < site.width; ++b) {
                const uint64_t at = uint64_t(site.address) + b;
                if (at >= address && at - address < length) bytes[at - address] = uint8_t(site.instruction >> (8 * b));
            }
        }
        for (size_t i = 0; i < block.repaintCount; ++i) {
            const auto& source = ThreeMaxNative::RepaintSites[block.firstRepaint + i];
            if (uint64_t(source.address) + 2 <= address || source.address >= uint64_t(address) + length) continue;
            const uint32_t current = ReadWord(record, source.address, 2);
            if (current != source.original && current != source.replacement) return false;
            for (unsigned b = 0; b < 2; ++b) {
                const uint64_t at = uint64_t(source.address) + b;
                if (at >= address && at - address < length) bytes[at - address] = uint8_t(source.original >> (8 * b));
            }
        }
    }
    return true;
}

bool DiscoverThreeMax(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error) {
    elements.clear();
    error.clear();
    FirmwareModel model;
    FirmwareEdition edition;
    if (!ThreeMaxFirmware::Detect(package, model, edition)) {
        error = "Unsupported DRAG 3 / MAX native layout profile";
        return false;
    }
    FirmwarePackage stock;
    if (!LoadEmbeddedFirmware(stock, error, model, edition)) return false;
    if (package.records.size() != stock.records.size()) { error = "Incomplete DRAG native hardware inventory"; return false; }
    FirmwarePackage normalized;
    normalized.records = package.records;
    std::vector<PaletteState> palette;
    Rgb8 background;
    if (!ThreeMaxFirmware::DetectColors(stock, palette, background)) return false;
    auto normalizedStock = std::move(stock);
    if (!RestorePalette(normalizedStock, palette, error) || !RestorePalette(normalized, palette, error)) return false;
    const auto assets = GetAllAssets(model, edition);
    for (size_t r = 0; r < package.records.size(); ++r) {
        const auto& record = package.records[r];
        const auto* profile = ThreeMaxNative::Find(model, edition, Code(record));
        const auto original = std::find_if(normalizedStock.records.begin(), normalizedStock.records.end(),
            [&](const HardwareRecord& candidate) { return Code(candidate) == Code(record); });
        if (!profile || original == normalizedStock.records.end() || record.linked_base != profile->base ||
            record.sp != profile->stack || record.reset != original->reset || record.image.size() != profile->size ||
            record.marker != original->marker) { error = "Unsupported DRAG native hardware record"; return false; }
        if (!Recover(record, *profile, elements, error)) return false;
        std::vector<std::pair<size_t, size_t>> resources;
        for (const auto& asset : assets) {
            const auto location = std::find_if(asset.recordAddresses.begin(), asset.recordAddresses.end(),
                [&](const auto& item) { return item.first == profile->code; });
            if (location == asset.recordAddresses.end()) continue;
            const size_t offset = location->second - profile->base;
            if (location->second < profile->base || offset > record.image.size() || asset.byte_len() > record.image.size() - offset) {
                error = "DRAG native resource is outside its hardware record";
                return false;
            }
            resources.emplace_back(offset, offset + asset.byte_len());
        }
        std::sort(resources.begin(), resources.end());
        size_t cursor = 0;
        const auto matches = [&](size_t end) {
            return MatchesCode(elements, profile->code, profile->base + static_cast<uint32_t>(cursor),
                original->image.data() + cursor, normalized.records[r].image.data() + cursor, end - cursor);
        };
        for (const auto& [begin, end] : resources) {
            if (begin > cursor && !matches(begin)) { error = "Unsupported DRAG executable mutation"; return false; }
            cursor = std::max(cursor, end);
        }
        if (!matches(record.image.size())) { error = "Unsupported DRAG executable mutation"; return false; }
    }
    return true;
}
} // namespace FirmwareLayout
