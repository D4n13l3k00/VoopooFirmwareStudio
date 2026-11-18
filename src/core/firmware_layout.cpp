#include "firmware_layout.h"
#include "firmware_catalog.h"
#include <algorithm>
#include <limits>

namespace FirmwareLayout {
namespace {
size_t Width(Encoding encoding) {
    switch (encoding) {
    case Encoding::UInt8: return 1;
    case Encoding::ThumbImm8: case Encoding::ThumbMovAliasImm8:
    case Encoding::ThumbAddImm3: case Encoding::UInt16: return 2;
    default: return 4;
    }
}
uint32_t Mask(Encoding encoding) {
    switch (encoding) {
    case Encoding::ThumbImm8: case Encoding::UInt8: return 0xff;
    case Encoding::ThumbMovImm: case Encoding::ThumbAddImm8: return 0x00ff0000;
    case Encoding::ThumbMovAliasImm8: return 0xffff;
    case Encoding::ThumbAddImm3: return 0x01c0;
    case Encoding::ThumbMovw: return 0x70ff040f;
    case Encoding::UInt16: return 0xffff;
    default: return 0xffffffff;
    }
}
uint32_t Read(const uint8_t* bytes, size_t width) {
    uint32_t result = 0;
    for (size_t i = 0; i < width; ++i) result |= uint32_t(bytes[i]) << (i * 8);
    return result;
}
bool AliasCompatible(uint16_t stock, uint16_t current) {
    unsigned destination;
    if ((stock & 0xff80) == 0x4600) destination = stock & 7;
    else if ((stock & 0xf800) == 0x2000 || (stock & 0xf800) == 0x9800) destination = stock >> 8 & 7;
    else return false;
    return (current & 0xf800) == 0x2000 && (current >> 8 & 7) == destination;
}
bool Encode(const Site& site, uint32_t source, int64_t value, uint32_t& result) {
    if (value < 0) return false;
    uint32_t decoded = source;
    uint32_t encoded = static_cast<uint32_t>(value);
    switch (site.encoding) {
    case Encoding::ThumbImm8: {
        const auto opcode = source & 0xf800;
        if (value > 255 || (opcode != 0x2000 && opcode != 0x3000 && opcode != 0x3800)) return false;
        decoded = source & 255;
        break;
    }
    case Encoding::ThumbMovAliasImm8: {
        if (value > 255) return false;
        unsigned destination;
        if ((source & 0xff80) == 0x4600) {
            destination = source & 7;
            decoded = static_cast<uint32_t>(site.original);
        } else if ((source & 0xf800) == 0x9800) {
            // Cut over the proven draw argument, not its stack producer:
            // the same producer may also initialize a native control flag.
            destination = source >> 8 & 7;
            decoded = static_cast<uint32_t>(site.original);
        } else if ((source & 0xf800) == 0x2000) {
            destination = source >> 8 & 7;
            decoded = source & 255;
        } else return false;
        encoded = 0x2000 | destination << 8 | static_cast<uint32_t>(value);
        break;
    }
    case Encoding::ThumbAddImm3:
        if (value > 7 || (source & 0xfe00) != 0x1c00) return false;
        decoded = source >> 6 & 7;
        encoded <<= 6;
        break;
    case Encoding::ThumbAddImm8:
        if (value > 255 || (source & 0xfbf0) != 0xf100 || (source & 0xf0000400)) return false;
        decoded = source >> 16 & 255;
        encoded <<= 16;
        break;
    case Encoding::ThumbMovImm:
        if (value > 255 || (source & 0xffff) != 0xf04f || (source & 0xf0000000)) return false;
        decoded = source >> 16 & 255;
        encoded <<= 16;
        break;
    case Encoding::ThumbMovw:
        if (value > 65535 || (source & 0x8000fbf0) != 0x0000f240) return false;
        decoded = (source & 15) << 12 | (source >> 10 & 1) << 11 |
            (source >> 28 & 7) << 8 | (source >> 16 & 255);
        encoded = (encoded >> 12 & 15) | (encoded >> 11 & 1) << 10 |
            (encoded >> 8 & 7) << 28 | (encoded & 255) << 16;
        break;
    case Encoding::UInt8: if (value > 255) return false; break;
    case Encoding::UInt16: if (value > 65535) return false; break;
    case Encoding::UInt32: if (value > std::numeric_limits<uint32_t>::max()) return false; break;
    }
    if (site.original < 0 || decoded != static_cast<uint32_t>(site.original)) return false;
    const uint32_t mask = Mask(site.encoding);
    result = (source & ~mask) | encoded;
    return true;
}
struct Pending {
    size_t record, offset, width;
    uint32_t original, replacement;
    const std::string* id;
};
}

bool Discover(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error) {
    elements.clear();
    error.clear();
    FirmwareModel model;
    FirmwareEdition edition;
    if (!DetectFirmwareProfile(package, model, edition)) {
        error = "Cannot identify firmware for native layout editing";
        return false;
    }
    if (model == FirmwareModel::DragS || model == FirmwareModel::DragX) {
        if (!DiscoverSx(package, elements, error)) return false;
        return DiscoverSxPages(package, elements, error);
    }
    if (model == FirmwareModel::Drag2 || model == FirmwareModel::Drag2Refresh)
        return DiscoverDrag2(package, elements, error);
    if (model == FirmwareModel::Drag1 || model == FirmwareModel::DragMini ||
        model == FirmwareModel::DragMiniRefresh) return DiscoverLegacy(package, elements, error);
    if (model == FirmwareModel::Drag3 || model == FirmwareModel::DragMax)
        return DiscoverThreeMax(package, elements, error);
    if (model == FirmwareModel::DragSPro || model == FirmwareModel::DragXPro ||
        model == FirmwareModel::DragXPlus || model == FirmwareModel::DragXPlusPro)
        return DiscoverProPlus(package, elements, error);
    return true;
}

bool BuildPatches(const FirmwarePackage& package, const std::vector<Element>& elements,
    const Offsets& offsets, std::vector<CodePatch>& patches, std::string& error) {
    error.clear();
    std::vector<Pending> pending;
    for (const auto& [id, delta] : offsets) {
        const auto element = std::find_if(elements.begin(), elements.end(),
            [&](const Element& item) { return item.id == id; });
        if (element == elements.end()) { error = "Unknown layout element: " + id; return false; }
        if (element->stepX <= 0 || element->stepY <= 0 ||
            delta.x < element->minimumOffset.x || delta.x > element->maximumOffset.x ||
            delta.y < element->minimumOffset.y || delta.y > element->maximumOffset.y ||
            delta.x % element->stepX || delta.y % element->stepY) {
            error = "Layout position is outside its native coordinate range: " + id;
            return false;
        }
        if (delta == Point{}) continue;
        const auto record = std::find_if(package.records.begin(), package.records.end(), [&](const HardwareRecord& item) {
            return (uint16_t(item.device_code[0]) << 8 | item.device_code[1]) == element->hardwareCode;
        });
        if (record == package.records.end()) { error = "Layout hardware record is missing: " + id; return false; }
        const size_t recordIndex = static_cast<size_t>(record - package.records.begin());
        if (element->sites.empty()) { error = "Layout element has no native coordinate sites: " + id; return false; }
        bool haveX = delta.x == 0, haveY = delta.y == 0;
        for (const auto& site : element->sites) {
            const int shift = site.vertical ? delta.y : delta.x;
            if (!site.scale || shift % site.scale) { error = "Layout coordinate is not aligned: " + id; return false; }
            if (site.vertical) haveY = true; else haveX = true;
            const size_t width = Width(site.encoding);
            const auto* bytes = package.BytesAt(recordIndex, site.address, width);
            if (!bytes) { error = "Layout coordinate is outside the linked image: " + id; return false; }
            const uint32_t original = Read(bytes, width);
            uint32_t replacement;
            if (!Encode(site, original, int64_t(site.original) + shift / site.scale, replacement)) {
                error = "Native layout coordinate instruction mismatch: " + id;
                return false;
            }
            if (replacement != original) pending.push_back({recordIndex,
                static_cast<size_t>(site.address - record->linked_base), width, original, replacement, &element->id});
        }
        if (!haveX || !haveY) { error = "Layout axis has no native coordinate sites: " + id; return false; }
    }
    std::sort(pending.begin(), pending.end(), [](const Pending& a, const Pending& b) {
        if (a.record != b.record) return a.record < b.record;
        if (a.offset != b.offset) return a.offset < b.offset;
        return a.width < b.width;
    });
    for (size_t i = 1; i < pending.size(); ++i) {
        const auto& a = pending[i - 1];
        const auto& b = pending[i];
        if (a.record != b.record || a.offset + a.width <= b.offset) continue;
        if (a.offset != b.offset || a.width != b.width || a.replacement != b.replacement) {
            error = "Layout elements have conflicting coordinate sites: " + *a.id + " / " + *b.id;
            return false;
        }
    }
    size_t previousRecord = SIZE_MAX, previousOffset = SIZE_MAX;
    for (const auto& change : pending) {
        if (change.record == previousRecord && change.offset == previousOffset) continue;
        previousRecord = change.record; previousOffset = change.offset;
        CodePatch patch;
        patch.id = "layout:" + *change.id;
        patch.original.resize(change.width);
        patch.replacement.resize(change.width);
        for (size_t i = 0; i < change.width; ++i) {
            patch.original[i] = static_cast<uint8_t>(change.original >> (i * 8));
            patch.replacement[i] = static_cast<uint8_t>(change.replacement >> (i * 8));
        }
        patch.locations.resize(package.records.size());
        patch.locations[change.record].push_back(change.offset);
        patches.push_back(std::move(patch));
    }
    for (const auto& owner : elements) {
        if (owner.repaint.empty()) continue;
        const bool moved = owner.repaintRequired || std::any_of(elements.begin(), elements.end(),
            [&](const Element& element) {
                const auto offset = offsets.find(element.id);
                return element.hardwareCode == owner.hardwareCode && ViewsOverlap(element, owner) &&
                    offset != offsets.end() && offset->second != Point{};
            });
        if (!moved) continue;
        const auto record = std::find_if(package.records.begin(), package.records.end(),
            [&](const HardwareRecord& item) {
                return (uint16_t(item.device_code[0]) << 8 | item.device_code[1]) == owner.hardwareCode;
            });
        if (record == package.records.end()) { error = "Missing native redraw hardware record"; return false; }
        const size_t index = static_cast<size_t>(record - package.records.begin());
        for (const auto& native : owner.repaint) {
            const auto* current = package.BytesAt(index, native.address, native.original.size());
            if (!current) { error = "Native redraw patch lies outside linked flash"; return false; }
            if (std::equal(native.replacement.begin(), native.replacement.end(), current)) continue;
            if (!std::equal(native.original.begin(), native.original.end(), current)) {
                error = "Unsupported native redraw instructions"; return false;
            }
            CodePatch patch;
            patch.id = "native-redraw:" + owner.id;
            patch.original = native.original;
            patch.replacement = native.replacement;
            patch.locations.resize(package.records.size());
            patch.locations[index].push_back(native.address - record->linked_base);
            patches.push_back(std::move(patch));
        }
    }
    return true;
}

bool MatchesCode(const std::vector<Element>& elements, uint16_t code, uint32_t address,
    const uint8_t* stock, const uint8_t* current, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        const uint8_t changed = stock[i] ^ current[i];
        if (!changed) continue;
        uint8_t allowed = 0;
        const uint64_t position = uint64_t(address) + i;
        for (const auto& element : elements) {
            if (element.hardwareCode != code) continue;
            for (const auto& patch : element.repaint) {
                if (patch.address < address || uint64_t(patch.address) + patch.original.size() > uint64_t(address) + length ||
                    position < patch.address || position - patch.address >= patch.original.size()) continue;
                const size_t offset = patch.address - address;
                if (std::equal(patch.original.begin(), patch.original.end(), stock + offset) &&
                    std::equal(patch.replacement.begin(), patch.replacement.end(), current + offset)) allowed = 0xff;
            }
            for (const auto& site : element.sites) {
                if (position < site.address || position - site.address >= Width(site.encoding)) continue;
                if (site.encoding == Encoding::ThumbMovAliasImm8) {
                    if (site.address < address || uint64_t(site.address) + 2 > uint64_t(address) + length) continue;
                    const size_t offset = site.address - address;
                    if (!AliasCompatible(static_cast<uint16_t>(Read(stock + offset, 2)),
                        static_cast<uint16_t>(Read(current + offset, 2)))) continue;
                }
                allowed |= static_cast<uint8_t>(Mask(site.encoding) >> ((position - site.address) * 8));
            }
        }
        if (changed & ~allowed) return false;
    }
    return true;
}
}
