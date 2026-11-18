#include "firmware_layout.h"
#include "firmware_pro_plus_layout_data.h"
#include "embedded_firmware.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iterator>
#include <optional>
#include <utility>

namespace FirmwareLayout {
namespace {
using namespace ProPlusNative;
uint16_t Code(const HardwareRecord& record) {
    return uint16_t(record.device_code[0]) << 8 | record.device_code[1];
}
size_t CoordinateWidth(Encoding encoding) {
    return encoding == Encoding::UInt8 ? 1 :
        encoding == Encoding::ThumbMovImm || encoding == Encoding::ThumbMovw ? 4 : 2;
}
constexpr const char* PageIds[] = {
    "main", "boot", "voopoo_boot", "power_off", "charging", "low_resistance",
    "atomizer_short", "check_atomizer", "check_battery", "low_power", "max_power",
    "puff_cleared", "high_temperature", "time_out", "locked", "unlocked",
    "settings", "switch", "mode", "theme", "information", "tc_material", "tc_settings"
};
// These are recovered native component purposes, not executable addresses or
// call ordinals. Bounds identify the release's fixed panel organization before
// applying opened coordinates, so equivalent boards and reopened edits retain
// exactly the same semantic suffix.
const char* SemanticRole(const Block& block) {
    const int left = block.left, top = block.top;
    if (block.screen == Main) {
        if (block.orientation) {
            if (top == 0) return left == 51 ? "battery" : left == 0 ? "puffs" : "duration";
            if (top == 54) return left == 0 ? "resistance" : "voltage";
            if (top == 72) return "logo";
            if (top == 25) return "power";
            if (top == 26) return left < 100 ? "lock_state" : "mode_badge";
        } else if (top == 0) return left == 0 ? "logo" : "battery";
        else if (block.temperatureMode == 0) {
            switch (top) {
            case 20: return "power";
            case 66: return "resistance";
            case 67: case 144: return "puffs";
            case 83: return "voltage";
            case 85: return left == 46 ? "resistance" : "voltage";
            case 100: return "firing_status";
            case 109: case 123: return "duration";
            }
        } else if (block.temperatureMode == 1) {
            switch (top) {
            case 18: return "temperature";
            case 64: return left == 0 ? "tc_power" : "resistance";
            case 80: return "material";
            case 106: return left == 0 ? "duration" : "puffs";
            case 125: return "status_separator";
            case 131: return "firing_status";
            case 151: return "bottom_separator";
            }
        }
        return nullptr;
    }
    if (block.screen == Startup) return top == 0 ? "device_logo" : "gene_version";
    if (block.screen == VoopooStartup) return "voopoo_logo";
    if (block.screen == PowerOff) return "off_logo";
    if (block.screen == Charging) return top == 0 ? "charging_header" : top == 70 ? "battery_gauge" : "charge_percentage";
    if (block.screen >= LowResistance && block.screen <= Unlocked)
        return top == 98 ? "message" : block.screen == CheckAtomizer ?
            (top == 30 ? "atomizer_connector" : "atomizer_coil") : "icon";
    if (block.screen == Settings) return top == 0 ? "header" : top == 40 ? "switch" : top == 84 ? "mode" : "exit";
    if (block.screen == Switch) return top == 0 ? "header" : top == 55 ? "lock" : "power";
    if (block.screen == Mode) return top == 0 ? "header" : top == 27 || top == 34 ? "smart" : top == 65 || top == 78 ? "rba" : "tc";
    if (block.screen == Theme) return top == 0 ? "header" : "core";
    if (block.screen == Information) return top == 0 ? "header" : top == 33 ? "uid_panel" : "level_code";
    if (block.screen == TcMaterial) return top == 0 ? "nickel" : top == 53 ? "titanium" : "stainless_steel";
    if (block.screen == TcSettings) return top == 17 ? "field_frames" : "field_footers";
    return nullptr;
}
const uint8_t* Bytes(const HardwareRecord& record, uint32_t address, size_t length) {
    if (address < record.linked_base || address - record.linked_base > record.image.size() ||
        length > record.image.size() - (address - record.linked_base)) return nullptr;
    return record.image.data() + address - record.linked_base;
}
uint32_t Read(const uint8_t* bytes, size_t length) {
    uint32_t result = 0;
    for (size_t i = 0; i < length; ++i) result |= uint32_t(bytes[i]) << (i * 8);
    return result;
}
bool Value(const HardwareRecord& record, const Coordinate& coordinate, int& value) {
    const size_t width = CoordinateWidth(coordinate.encoding);
    const auto* bytes = Bytes(record, coordinate.address, width);
    if (!bytes) return false;
    const uint32_t opened = Read(bytes, width), stock = coordinate.instruction;
    uint32_t mask = 0;
    switch (coordinate.encoding) {
    case Encoding::ThumbMovAliasImm8:
        if (opened == stock) { value = coordinate.original; return true; }
        if ((stock & 0xff80) != 0x4600 || (opened & 0xf800) != 0x2000 ||
            ((opened >> 8) & 7) != (stock & 7)) return false;
        value = opened & 255;
        return true;
    case Encoding::ThumbImm8: case Encoding::UInt8:
        mask = 0xff; value = opened & 255; break;
    case Encoding::ThumbMovImm:
        mask = 0x00ff0000; value = opened >> 16 & 255; break;
    case Encoding::ThumbMovw:
        mask = 0x70ff040f;
        value = (opened & 15) << 12 | (opened >> 10 & 1) << 11 |
            (opened >> 28 & 7) << 8 | (opened >> 16 & 255);
        break;
    default: return false;
    }
    return ((opened ^ stock) & ~mask) == 0;
}
bool Position(const HardwareRecord& record, const Block& block, Point& delta) {
    std::optional<int> x, y;
    for (const auto& coordinate : block.coordinates) {
        int value = 0;
        if (!Value(record, coordinate, value)) return false;
        const int shift = (value - coordinate.original) * coordinate.scale;
        auto& axis = coordinate.vertical ? y : x;
        if (axis && *axis != shift) return false;
        axis = shift;
    }
    if (bool(x) != block.horizontal || bool(y) != block.vertical) return false;
    delta = {x.value_or(0), y.value_or(0)};
    const int width = block.orientation ? 160 : 80, height = block.orientation ? 80 : 160;
    return block.left + delta.x >= 0 && block.top + delta.y >= 0 &&
        block.right + delta.x <= width && block.bottom + delta.y <= height;
}
int RepaintState(const HardwareRecord& record, const LayoutRecord& layout, int view) {
    int result = -1;
    for (const auto& patch : layout.repaint) {
        if (patch.view != view) continue;
        const auto* current = Bytes(record, patch.address, patch.original.size());
        if (!current || patch.original.size() != patch.replacement.size()) return -1;
        const int state = std::equal(patch.original.begin(), patch.original.end(), current) ? 0 :
            std::equal(patch.replacement.begin(), patch.replacement.end(), current) ? 1 : -1;
        if (state < 0 || (result >= 0 && result != state)) return -1;
        result = state;
    }
    return result;
}
size_t Identity(const HardwareRecord& record) {
    for (size_t i = 0; i < std::size(Profiles); ++i) {
        const auto& profile = Profiles[i];
        if (profile.code == Code(record) && profile.size == record.image.size() &&
            profile.base == record.linked_base && profile.stack == record.sp && profile.reset == record.reset) return i;
    }
    return std::size(Profiles);
}
bool InView(const Element& element, int view) {
    return element.screen == Main && element.orientation == (view == 2 ? 1 : 0) &&
        (element.temperatureMode < 0 || element.temperatureMode == (view == 1 ? 1 : 0));
}
bool Recover(const HardwareRecord& record, size_t index, std::vector<Element>& elements, std::string& error) {
    const auto& layout = Layouts[index];
    const size_t first = elements.size();
    std::array<bool, 3> moved{};
    for (const auto& block : layout.blocks) {
        Point delta;
        if (!Position(record, block, delta)) {
            error = "Unsupported Pro/Plus coordinate instruction, bounds, or correlated update";
            return false;
        }
        Element element;
        char prefix[16];
        std::snprintf(prefix, sizeof(prefix), "hw%04X.", Code(record));
        const auto* role = SemanticRole(block);
        if (!role || block.screen >= std::size(PageIds)) {
            error = "Unidentified Pro/Plus native component purpose"; return false;
        }
        element.id = std::string(prefix) + block.id + "." + role;
        element.semanticId = std::string(PageIds[block.screen]) + "." + role + "." +
            (block.temperatureMode < 0 ? "shared" : block.temperatureMode ? "tc" : "vw") + "." +
            (block.orientation ? "landscape" : "portrait");
        if (block.screen == Information && block.top == 33) element.title = "CHIP ID panel";
        else {
            std::string label = role;
            std::replace(label.begin(), label.end(), '_', ' ');
            if (!label.empty() && label[0] >= 'a' && label[0] <= 'z') label[0] -= 'a' - 'A';
            element.title = std::move(label);
        }
        element.hardwareCode = Code(record);
        element.screen = block.screen;
        element.sharedScreens.assign(block.shared.begin(), block.shared.end());
        element.temperatureMode = block.temperatureMode;
        element.orientation = block.orientation;
        element.screenWidth = block.orientation ? 160 : 80;
        element.screenHeight = block.orientation ? 80 : 160;
        element.position = {block.left + delta.x, block.top + delta.y};
        element.width = block.right - block.left;
        element.height = block.bottom - block.top;
        element.minimumOffset = {block.horizontal ? -element.position.x : 0, block.vertical ? -element.position.y : 0};
        element.maximumOffset = {block.horizontal ? element.screenWidth - element.width - element.position.x : 0,
            block.vertical ? element.screenHeight - element.height - element.position.y : 0};
        element.sites.reserve(block.coordinates.size());
        for (const auto& coordinate : block.coordinates) {
            int value = 0;
            if (!Value(record, coordinate, value)) return false;
            element.sites.push_back({coordinate.address, coordinate.encoding, coordinate.vertical, value, coordinate.scale});
        }
        if (delta != Point{})
            for (int view = 0; view < 3; ++view) if (InView(element, view)) moved[view] = true;
        elements.push_back(std::move(element));
    }
    for (int view = 0; view < 3; ++view) {
        if (std::none_of(layout.repaint.begin(), layout.repaint.end(),
            [view](const auto& patch) { return patch.view == view; })) continue;
        if (RepaintState(record, layout, view) < 0) {
            error = "Unsupported or partially applied Pro/Plus native redraw cutover";
            return false;
        }
        auto owner = std::find_if(elements.begin() + first, elements.end(), [&](const auto& element) {
            return InView(element, view) && element.temperatureMode == (view == 1 ? 1 : 0);
        });
        if (owner == elements.end()) owner = std::find_if(elements.begin() + first, elements.end(),
            [view](const auto& element) { return InView(element, view); });
        if (owner == elements.end()) { error = "Missing Pro/Plus native redraw owner"; return false; }
        owner->repaintRequired = owner->repaintRequired || moved[view];
        for (const auto& patch : layout.repaint)
            if (patch.view == view) owner->repaint.push_back({patch.address,
                {patch.original.begin(), patch.original.end()}, {patch.replacement.begin(), patch.replacement.end()}});
    }
    return true;
}
struct Range { size_t first, last; };
bool Validate(const FirmwarePackage& package, const FirmwarePackage& stock, FirmwareModel model,
    FirmwareEdition edition, const std::vector<Element>& elements, std::string& error) {
    std::vector<std::vector<Range>> editable(package.records.size());
    const auto assets = GetAllAssets(model, edition);
    for (size_t index = 0; index < package.records.size(); ++index) {
        const auto& record = package.records[index];
        for (const auto& asset : assets) {
            uint32_t address = asset.address;
            if (!asset.recordAddresses.empty()) {
                const auto location = std::find_if(asset.recordAddresses.begin(), asset.recordAddresses.end(),
                    [&](const auto& item) { return item.first == Code(record); });
                if (location == asset.recordAddresses.end()) continue;
                address = location->second;
            } else if (asset.sourceCode && asset.sourceCode != Code(record)) continue;
            const size_t length = asset.byte_len();
            if (length && Bytes(record, address, length))
                editable[index].push_back({address - record.linked_base, address - record.linked_base + length});
        }
    }
    std::vector<PaletteState> palette;
    Rgb8 background;
    if (!DetectFirmwareColors(package, palette, background)) {
        error = "Unsupported Pro/Plus palette producers"; return false;
    }
    for (uint16_t bit : {uint16_t(1), uint16_t(3)}) {
        for (auto& role : palette) role.current = Rgb8::FromRgb565(role.current.ToRgb565() ^ bit);
        std::vector<CodePatch> colors;
        if (!BuildColorPatches(package, palette, background, colors, error)) return false;
        for (const auto& patch : colors)
            for (size_t index = 0; index < patch.locations.size(); ++index) {
                if (index >= editable.size()) { error = "Invalid Pro/Plus palette record"; return false; }
                for (const size_t offset : patch.locations[index]) {
                    if (offset > package.records[index].image.size() ||
                        patch.replacement.size() > package.records[index].image.size() - offset) {
                        error = "Invalid Pro/Plus palette instruction range"; return false;
                    }
                    editable[index].push_back({offset, offset + patch.replacement.size()});
                }
            }
    }
    for (size_t index = 0; index < package.records.size(); ++index) {
        const auto& record = package.records[index];
        const auto original = std::find_if(stock.records.begin(), stock.records.end(),
            [&](const auto& candidate) { return Code(candidate) == Code(record); });
        if (original == stock.records.end()) { error = "Missing embedded Pro/Plus hardware record"; return false; }
        auto& ranges = editable[index];
        std::sort(ranges.begin(), ranges.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        size_t offset = 0;
        const auto same = [&](size_t first, size_t last) {
            return MatchesCode(elements, Code(record), record.linked_base + static_cast<uint32_t>(first),
                original->image.data() + first, record.image.data() + first, last - first);
        };
        for (const auto& range : ranges) {
            if (range.first > offset && !same(offset, range.first)) {
                error = "Unsupported Pro/Plus native executable or descriptor change"; return false;
            }
            offset = (std::max)(offset, range.last);
        }
        if (!same(offset, record.image.size())) {
            error = "Unsupported Pro/Plus native executable or descriptor change"; return false;
        }
    }
    return true;
}
} // namespace

bool NormalizeProPlusLayoutContext(const HardwareRecord& record, uint32_t address, uint8_t* bytes, size_t length) {
    const size_t index = Identity(record);
    if (index == std::size(Profiles)) return false;
    const auto intersects = [address, length](uint32_t site, size_t size) {
        return uint64_t(site) < uint64_t(address) + length && uint64_t(address) < uint64_t(site) + size;
    };
    const auto copy = [address, length, bytes](uint32_t sourceAddress, const uint8_t* original, size_t size) {
        for (size_t i = 0; i < size; ++i)
            if (uint64_t(sourceAddress) + i >= address && uint64_t(sourceAddress) + i - address < length)
                bytes[uint64_t(sourceAddress) + i - address] = original[i];
    };
    for (const auto& block : Layouts[index].blocks) {
        if (std::none_of(block.coordinates.begin(), block.coordinates.end(), [&](const auto& coordinate) {
            return intersects(coordinate.address, CoordinateWidth(coordinate.encoding));
        })) continue;
        Point delta;
        if (!Position(record, block, delta)) return false;
        for (const auto& coordinate : block.coordinates) {
            const size_t width = CoordinateWidth(coordinate.encoding);
            if (!intersects(coordinate.address, width)) continue;
            std::array<uint8_t, 4> original{};
            for (size_t i = 0; i < width; ++i) original[i] = static_cast<uint8_t>(coordinate.instruction >> (i * 8));
            copy(coordinate.address, original.data(), width);
        }
    }
    for (const auto& patch : Layouts[index].repaint) {
        if (!intersects(patch.address, patch.original.size())) continue;
        if (RepaintState(record, Layouts[index], patch.view) < 0) return false;
        copy(patch.address, patch.original.data(), patch.original.size());
    }
    return true;
}

bool DiscoverProPlus(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error) {
    elements.clear(); error.clear();
    FirmwareModel model;
    FirmwareEdition edition;
    if (!DetectFirmwareProfile(package, model, edition) ||
        !ProPlusNative::FindProfile(model, edition)) {
        error = "Unsupported Pro/Plus firmware profile"; return false;
    }
    FirmwarePackage baseline;
    if (!LoadEmbeddedFirmware(baseline, error, model, edition)) return false;
    if (package.records.size() != baseline.records.size()) { error = "Incomplete Pro/Plus native hardware inventory"; return false; }
    for (const auto& record : package.records) {
        const auto* profile = ProPlusNative::FindProfile(model, edition, Code(record));
        const auto stock = std::find_if(baseline.records.begin(), baseline.records.end(),
            [&](const auto& original) { return Code(original) == Code(record); });
        if (!profile || stock == baseline.records.end() || record.linked_base != stock->linked_base ||
            record.sp != stock->sp || record.reset != stock->reset || record.marker != stock->marker ||
            record.image.size() != stock->image.size() || record.image.size() != profile->size) {
            error = "Unsupported Pro/Plus native hardware record"; return false;
        }
        if (!Recover(record, static_cast<size_t>(profile - Profiles), elements, error)) return false;
    }
    return Validate(package, baseline, model, edition, elements, error);
}
} // namespace FirmwareLayout
