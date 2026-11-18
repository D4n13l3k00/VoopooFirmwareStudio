#include "firmware_layout_sx_pages_private.h"
#include "embedded_firmware.h"
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string_view>

namespace FirmwareLayout {
namespace {
#include "firmware_layout_sx_pages_data.h"

constexpr std::string_view PageNames[] = {
    "Main", "Logo GENE.TT & VOOPOO (boot)", "Logo VOOPOO (animated boot)", "OFF",
    "Charging", "Atomizer Short", "Check Atomizer", "Check Battery", "Low Power",
    "Max Power", "Puff Cleared", "High Temp", "Time Out", "Unlock", "Lock",
    "CHIP ID panel", "Banner ACTIVATED", "Blank screen", "Puff history", "Clock settings"
};
constexpr const char* PageIds[] = {
    "main", "boot_gene", "boot_voopoo", "off", "charging",
    "warning_atomizer_short", "warning_check_atomizer", "warning_check_battery",
    "warning_low_power", "warning_max_power", "puff_cleared", "warning_high_temp",
    "warning_time_out", "unlock", "lock", "chip_id", "activated", "blank",
    "puff_history", "clock_settings"
};
const PageRecord* FindRecord(uint16_t code, uint32_t base, uint32_t stack) {
    const auto found = std::find_if(std::begin(PageRecords), std::end(PageRecords),
        [=](const PageRecord& record) { return record.code == code && record.base == base && record.stack == stack; });
    return found == std::end(PageRecords) ? nullptr : &*found;
}
const HardwareRecord* FindHardware(const FirmwarePackage& package, uint16_t code) {
    const auto found = std::find_if(package.records.begin(), package.records.end(), [code](const HardwareRecord& record) {
        return (uint16_t(record.device_code[0]) << 8 | record.device_code[1]) == code;
    });
    return found == package.records.end() ? nullptr : &*found;
}
size_t Width(Encoding encoding) {
    return encoding == Encoding::ThumbImm8 || encoding == Encoding::ThumbMovAliasImm8 ? 2 : 4;
}
uint32_t Read(const uint8_t* bytes, size_t length) {
    uint32_t value = 0;
    for (size_t i = 0; i < length; ++i) value |= uint32_t(bytes[i]) << (i * 8);
    return value;
}
uint32_t Mask(Encoding encoding) {
    switch (encoding) {
    case Encoding::ThumbImm8: return 0xff;
    case Encoding::ThumbMovAliasImm8: return 0xffff;
    case Encoding::ThumbMovImm: case Encoding::ThumbAddImm8: return 0x00ff0000;
    default: return 0;
    }
}
bool Decode(const PageCoordinate& site, uint32_t original, uint32_t current, int& value) {
    if (site.encoding == Encoding::ThumbMovAliasImm8) {
        if ((original & 0xff80) != 0x4600) return false;
        if (current == original) { value = site.value; return true; }
        if ((current & 0xff00) != (0x2000u | ((original & 7) << 8))) return false;
        value = current & 255;
        return true;
    }
    const uint32_t mask = Mask(site.encoding);
    if (!mask || (current & ~mask) != (original & ~mask)) return false;
    value = site.encoding == Encoding::ThumbImm8 ? current & 255 : current >> 16 & 255;
    return true;
}
bool Contains(uint32_t base, size_t length, uint32_t address, size_t size) {
    return address >= base && address - base <= length && size <= length - (address - base);
}
bool MatchPages(const PageRecord& record, const std::vector<Element>& elements,
    const uint8_t* stock, const uint8_t* current, size_t length) {
    for (size_t p = record.firstPage; p < record.firstPage + record.pageCount; ++p) {
        const auto& page = Pages[p];
        for (size_t g = page.firstGuard; g < page.firstGuard + page.guardCount; ++g) {
            const auto& guard = PageGuards[g];
            if (!Contains(record.base, length, guard.address, guard.length)) return false;
            const size_t offset = guard.address - record.base;
            if (!MatchesCode(elements, record.code, guard.address, stock + offset, current + offset, guard.length)) return false;
        }
    }
    return true;
}
}

const FirmwarePackage* SxStockPackage(size_t edition) {
    static const auto packages = [] {
        std::array<FirmwarePackage, 4> result;
        for (size_t i = 0; i < result.size(); ++i) {
            std::string error;
            LoadEmbeddedFirmware(result[i], error, i >= 2 ? FirmwareModel::DragX : FirmwareModel::DragS,
                i % 2 ? FirmwareEdition::Factory : FirmwareEdition::Score);
        }
        return result;
    }();
    return edition < packages.size() && !packages[edition].records.empty() ? &packages[edition] : nullptr;
}

bool MatchesSxPageExecutable(const std::vector<Element>& elements, uint16_t code,
    uint32_t base, uint32_t stack, const uint8_t* stock, const uint8_t* current, size_t length) {
    const auto* record = FindRecord(code, base, stack);
    return record && MatchPages(*record, elements, stock, current, length);
}

bool NormalizeSxPageLayoutContext(const HardwareRecord& record, uint32_t address, uint8_t* bytes, size_t length) {
    const uint16_t code = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
    const auto* description = FindRecord(code, record.linked_base, record.sp);
    if (!description) return false;
    const auto* package = SxStockPackage(description->edition);
    const auto* stock = package ? FindHardware(*package, code) : nullptr;
    if (!stock) return false;
    for (size_t p = description->firstPage; p < description->firstPage + description->pageCount; ++p) {
        const auto& page = Pages[p];
        for (size_t s = page.firstSite; s < page.firstSite + page.siteCount; ++s) {
            const auto& site = PageCoordinates[s];
            const size_t width = Width(site.encoding);
            if (uint64_t(site.address) >= uint64_t(address) + length || uint64_t(address) >= uint64_t(site.address) + width) continue;
            if (!Contains(record.linked_base, record.image.size(), site.address, width) ||
                !Contains(record.linked_base, stock->image.size(), site.address, width)) return false;
            const size_t offset = site.address - record.linked_base;
            int value = 0;
            if (!Decode(site, Read(stock->image.data() + offset, width), Read(record.image.data() + offset, width), value)) return false;
            const uint32_t mask = Mask(site.encoding);
            for (size_t i = 0; i < width; ++i) {
                const uint64_t position = uint64_t(site.address) + i;
                if (position < address || position - address >= length) continue;
                const uint8_t field = static_cast<uint8_t>(mask >> (i * 8));
                auto& output = bytes[position - address];
                output = (output & ~field) | (stock->image[offset + i] & field);
            }
        }
    }
    return true;
}

bool SxPagePhaseArguments(size_t edition, uint16_t code, uint32_t screen,
    uint32_t base, std::span<const uint8_t> image, std::array<uint32_t, 4>& arguments) {
    const auto record = std::find_if(std::begin(PageRecords), std::end(PageRecords), [=](const PageRecord& candidate) {
        return candidate.edition == edition && candidate.code == code && candidate.base == base;
    });
    if (record == std::end(PageRecords) || (screen != 2 && screen != 3)) return false;
    const auto begin = std::begin(Pages) + record->firstPage;
    const auto end = begin + record->pageCount;
    const auto page = std::find_if(begin, end, [screen](const PageDescription& candidate) { return candidate.screen == screen; });
    if (page == end) return false;
    for (size_t i = 0; i < arguments.size(); ++i) {
        const uint32_t address = page->phase[i];
        if (!Contains(base, image.size(), address, 2)) return false;
        const uint32_t instruction = Read(image.data() + address - base, 2);
        if ((instruction & 0xff00) != (0x2000u | (uint32_t(i) << 8))) return false;
        arguments[i] = instruction & 255;
    }
    return true;
}

bool DiscoverSxPages(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error) {
    if (package.records.empty()) { error = "Empty S/X page hardware inventory"; return false; }
    const PageRecord* release = nullptr;
    for (const auto& hardware : package.records) {
        const uint16_t code = uint16_t(hardware.device_code[0]) << 8 | hardware.device_code[1];
        const auto* record = FindRecord(code, hardware.linked_base, hardware.sp);
        if (!record || (release && release->edition != record->edition)) {
            error = "Unsupported or mixed S/X page hardware inventory"; return false;
        }
        release = record;
    }
    const auto* stock = SxStockPackage(release->edition);
    if (!stock) { error = "Embedded S/X page release is unavailable"; return false; }
    std::vector<Element> discovered;
    for (const auto& hardware : package.records) {
        const uint16_t code = uint16_t(hardware.device_code[0]) << 8 | hardware.device_code[1];
        const auto& record = *FindRecord(code, hardware.linked_base, hardware.sp);
        const auto* baseline = FindHardware(*stock, code);
        if (!baseline || hardware.image.size() < baseline->image.size()) {
            error = "Missing or truncated S/X page hardware image"; return false;
        }
        for (size_t p = record.firstPage; p < record.firstPage + record.pageCount; ++p) {
            const auto& page = Pages[p];
            Element element;
            char id[48];
            std::snprintf(id, sizeof(id), "hw%04x.%s.screen%u", code, PageIds[page.screen], page.screen);
            element.id = id;
            element.title = PageNames[page.screen];
            element.hardwareCode = code;
            element.screenWidth = 80; element.screenHeight = 160;
            element.screen = page.screen;
            element.width = page.x1 - page.x0; element.height = page.y1 - page.y0;
            element.drawOrder = 0;
            Point delta;
            bool haveX = false, haveY = false;
            for (size_t s = page.firstSite; s < page.firstSite + page.siteCount; ++s) {
                const auto& site = PageCoordinates[s];
                const size_t width = Width(site.encoding);
                if (!Contains(record.base, baseline->image.size(), site.address, width)) {
                    error = "S/X page coordinate lies outside linked flash"; return false;
                }
                const size_t offset = site.address - record.base;
                int value = 0;
                if (!Decode(site, Read(baseline->image.data() + offset, width), Read(hardware.image.data() + offset, width), value) ||
                    value < site.minimum) {
                    error = "Unsupported S/X page coordinate instruction: " + element.id; return false;
                }
                const int shift = (value - site.value) * site.scale;
                int& coordinate = site.vertical ? delta.y : delta.x;
                bool& have = site.vertical ? haveY : haveX;
                if (have && coordinate != shift) {
                    error = "Inconsistent S/X page draw/erase/update coordinates: " + element.id; return false;
                }
                coordinate = shift; have = true;
                element.sites.push_back({site.address, site.encoding, site.vertical, value, site.scale});
            }
            element.position = {page.x0 + delta.x, page.y0 + delta.y};
            element.minimumOffset = {haveX ? -element.position.x : 0, haveY ? -element.position.y : 0};
            element.maximumOffset = {haveX ? 80 - element.position.x - element.width : 0,
                haveY ? 160 - element.position.y - element.height : 0};
            for (size_t s = 0; s < page.siteCount; ++s) {
                const auto& raw = PageCoordinates[page.firstSite + s];
                const auto& site = element.sites[s];
                int& minimum = site.vertical ? element.minimumOffset.y : element.minimumOffset.x;
                int& maximum = site.vertical ? element.maximumOffset.y : element.maximumOffset.x;
                minimum = std::max(minimum, (site.scale > 0 ? raw.minimum - site.original : site.original - 255));
                maximum = std::min(maximum, (site.scale > 0 ? 255 - site.original : site.original - raw.minimum));
            }
            if (element.minimumOffset.x > 0 || element.maximumOffset.x < 0 ||
                element.minimumOffset.y > 0 || element.maximumOffset.y < 0) {
                error = "S/X page lies outside its native LCD bounds: " + element.id; return false;
            }
            discovered.push_back(std::move(element));
        }
    }
    // Reconstruct only recognized palette instructions in the embedded release.
    // Resource edits live outside these executable guards and remain untouched.
    auto palette = GetDefaultPalette();
    Rgb8 background;
    if (!DetectFirmwareColors(package, palette, background)) { error = "Unsupported S/X page palette context"; return false; }
    std::vector<CodePatch> colors;
    if (!BuildColorPatches(*stock, palette, background, colors, error)) return false;
    for (const auto& hardware : package.records) {
        const uint16_t code = uint16_t(hardware.device_code[0]) << 8 | hardware.device_code[1];
        const auto& record = *FindRecord(code, hardware.linked_base, hardware.sp);
        const auto* baseline = FindHardware(*stock, code);
        const size_t index = static_cast<size_t>(baseline - stock->records.data());
        std::vector<uint8_t> colored;
        for (const auto& patch : colors) {
            if (index >= patch.locations.size()) continue;
            for (const size_t offset : patch.locations[index]) {
                if (colored.empty()) colored = baseline->image;
                if (offset > colored.size() || patch.replacement.size() > colored.size() - offset) {
                    error = "S/X page palette patch lies outside flash"; return false;
                }
                std::copy(patch.replacement.begin(), patch.replacement.end(), colored.begin() + offset);
            }
        }
        const auto& expected = colored.empty() ? baseline->image : colored;
        if (!MatchPages(record, discovered, expected.data(), hardware.image.data(), expected.size())) {
            error = "Unsupported S/X native page executable change for hardware " + std::to_string(code); return false;
        }
    }
    elements.insert(elements.end(), std::make_move_iterator(discovered.begin()), std::make_move_iterator(discovered.end()));
    return true;
}
}
