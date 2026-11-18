#include "firmware_layout.h"
#include "firmware_layout_legacy_private.h"
#include "firmware_layout_legacy_pages.h"
#include "embedded_firmware.h"
#include "firmware_catalog.h"
#include <array>
#include <algorithm>
#include <cstdio>
#include <optional>
#include <utility>

namespace FirmwareLayout {
namespace {
uint16_t Code(const HardwareRecord& record) {
    return uint16_t(record.device_code[0]) << 8 | record.device_code[1];
}
uint16_t Word(const HardwareRecord& record, uint32_t address) {
    const auto offset = address - record.linked_base;
    return uint16_t(record.image[offset]) | uint16_t(record.image[offset + 1]) << 8;
}
struct Recovery {
    const HardwareRecord& stock;
    const HardwareRecord& current;
    std::vector<Element> elements;
    std::vector<std::pair<uint32_t, size_t>> descriptors;
    std::map<uint32_t, int> aliasOriginals;
    bool valid = true;
    int codeShift = 0, dataShift = 0;
    struct VerticalWindow { size_t element = SIZE_MAX; int top = 0, bottom = 32; };
    std::array<VerticalWindow, 2> verticalWindows{};
    size_t windowCount = 0;
    void ClipVertical(Element& element, int top, int bottom) {
        if (windowCount == verticalWindows.size()) { valid = false; return; }
        verticalWindows[windowCount++] = {size_t(&element - elements.data()), top, bottom};
    }
    uint32_t Address(uint32_t address) const {
        // Mini keeps the battery entrypoint but folds the two-cell state
        // machine. Its draw tail and charge-page tail relocate independently.
        if (codeShift && address >= 0x0800428c && address <= 0x080042b0) return address - 0x52;
        if (codeShift && (address == 0x080057de || address == 0x080057ea)) return address - 0xb6;
        if (codeShift && address >= 0x08005804 && address < 0x080059f8) return address - 0xb4;
        return address + (address >= 0x0800bd10 ? dataShift : address >= 0x0800428c ? codeShift : 0);
    }

    Element& Add(const char* id, const char* title, Point position, int width, int height) {
        char prefix[16];
        snprintf(prefix, sizeof(prefix), "hw%04X.", Code(current));
        Element element;
        element.id = std::string(prefix) + id;
        element.title = title;
        element.hardwareCode = Code(current);
        element.screenWidth = 128;
        element.screenHeight = 32;
        element.position = position;
        element.orientation = 0;
        element.width = width;
        element.height = height;
        elements.push_back(std::move(element));
        return elements.back();
    }
    // Addresses are recovered call arguments/descriptors in the embedded release,
    // not byte signatures. Every surrounding executable byte is checked below.
    void SiteAt(Element& element, uint32_t address, Encoding encoding, bool vertical = false,
        int scale = 1, int aliasValue = 0) {
        address = Address(address);
        const size_t width = encoding == Encoding::UInt8 ? 1 :
            (encoding == Encoding::ThumbMovImm || encoding == Encoding::ThumbAddImm8) ? 4 : 2;
        if (address < current.linked_base || address - current.linked_base > current.image.size() ||
            width > current.image.size() - (address - current.linked_base)) { valid = false; return; }
        int value = 0;
        if (encoding == Encoding::UInt8) value = current.image[address - current.linked_base];
        else if (encoding == Encoding::ThumbMovImm || encoding == Encoding::ThumbAddImm8)
            value = Word(current, address + 2) & 255;
        else if (encoding == Encoding::ThumbAddImm3) value = (Word(current, address) >> 6) & 7;
        else if (encoding == Encoding::ThumbMovAliasImm8) {
            const uint16_t original = Word(stock, address), opened = Word(current, address);
            if ((original & 0xff80) != 0x4600 ||
                (opened != original && ((opened & 0xf800) != 0x2000 ||
                    ((opened >> 8) & 7) != (original & 7)))) { valid = false; return; }
            value = opened == original ? aliasValue : opened & 255;
            aliasOriginals.emplace(address, aliasValue);
        } else value = Word(current, address) & 255;
        element.sites.push_back({address, encoding, vertical, value, scale});
    }
    void Imm(Element& element, std::initializer_list<uint32_t> addresses) {
        for (const auto address : addresses) SiteAt(element, address, Encoding::ThumbImm8);
    }
    void Descriptor(Element& element, uint32_t address, size_t size) {
        descriptors.emplace_back(Address(address), size);
        SiteAt(element, address, Encoding::UInt8);
        SiteAt(element, address + 1, Encoding::UInt8, true);
    }
    bool Finish(uint32_t executableEnd, std::string& error) {
        for (auto& element : elements) {
            std::optional<int> dx, dy;
            for (const auto& site : element.sites) {
                int original;
                if (site.encoding == Encoding::UInt8) original = stock.image[site.address - stock.linked_base];
                else if (site.encoding == Encoding::ThumbMovImm || site.encoding == Encoding::ThumbAddImm8)
                    original = Word(stock, site.address + 2) & 255;
                else if (site.encoding == Encoding::ThumbAddImm3) original = (Word(stock, site.address) >> 6) & 7;
                else if (site.encoding == Encoding::ThumbMovAliasImm8) {
                    original = aliasOriginals.at(site.address);
                } else original = Word(stock, site.address) & 255;
                const int delta = (site.original - original) * site.scale;
                auto& axis = site.vertical ? dy : dx;
                if (axis && *axis != delta) valid = false;
                axis = delta;
            }
            element.position.x += dx.value_or(0);
            element.position.y += dy.value_or(0);
            element.minimumOffset = {dx ? -element.position.x : 0, dy ? -element.position.y : 0};
            element.maximumOffset = {dx ? 128 - element.width - element.position.x : 0,
                dy ? 32 - element.height - element.position.y : 0};
            for (size_t i = 0; i < windowCount; ++i) {
                const auto& window = verticalWindows[i];
                if (window.element != size_t(&element - elements.data())) continue;
                element.minimumOffset.y = std::max(element.minimumOffset.y, window.top - element.position.y);
                element.maximumOffset.y = std::min(element.maximumOffset.y,
                    window.bottom - element.height - element.position.y);
            }
            for (const auto& site : element.sites) {
                auto& minimum = site.vertical ? element.minimumOffset.y : element.minimumOffset.x;
                auto& maximum = site.vertical ? element.maximumOffset.y : element.maximumOffset.x;
                const int limit = site.encoding == Encoding::ThumbAddImm3 ? 7 : 255;
                minimum = std::max(minimum, site.scale > 0 ? -site.original * site.scale :
                    (limit - site.original) * site.scale);
                maximum = std::min(maximum, site.scale > 0 ? (limit - site.original) * site.scale :
                    -site.original * site.scale);
            }
            if (element.minimumOffset.x > 0 || element.maximumOffset.x < 0 ||
                element.minimumOffset.y > 0 || element.maximumOffset.y < 0) valid = false;
        }
        if (!valid || !MatchesCode(elements, Code(current), stock.linked_base, stock.image.data(),
            current.image.data(), executableEnd - stock.linked_base)) {
            error = "Unsupported legacy OLED native layout instructions or incoherent coordinates";
            return false;
        }
        for (const auto& [address, size] : descriptors) {
            const size_t offset = address - stock.linked_base;
            if (offset > stock.image.size() || size > stock.image.size() - offset ||
                !MatchesCode(elements, Code(current), address, stock.image.data() + offset,
                    current.image.data() + offset, size)) {
                error = "Unsupported legacy OLED native layout descriptor";
                return false;
            }
        }
        return true;
    }
};
void Original(Recovery& recovery) {
    // 0x0800528c clears and redraws numbers from the SAME descriptor. Its
    // decimal, leading-zero and glyph paths all derive x/y from bytes 0 and 1.
    auto& power = recovery.Add("power", "Power", {70, 1}, 58, 30);
    for (const uint32_t address : {0x0800bff0u, 0x0800c00cu, 0x0800c07cu})
        recovery.Descriptor(power, address, 28);
    for (const uint32_t address : {0x0800c920u, 0x0800c990u, 0x0800c998u, 0x0800c9a0u, 0x0800cc7cu})
        recovery.Descriptor(power, address, 8);
    auto& battery = recovery.Add("battery", "Battery", {1, 1}, 24, 30);
    // 0x080041c8 is shared by normal, low-battery animation and charge updates.
    // MOV r0,r1 aliases x=1 to y=1. A same-width MOVS r0,#x separates the
    // operands; flags are dead across BL 0x08008a38 (and its own comparisons).
    recovery.SiteAt(battery, 0x08004294, Encoding::ThumbMovAliasImm8, false, 1, 1);
    recovery.Imm(battery, {0x080042b0});
    recovery.SiteAt(battery, 0x0800428c, Encoding::ThumbImm8, true);
    recovery.SiteAt(battery, 0x080042ae, Encoding::ThumbImm8, true);
    auto& resistance = recovery.Add("resistance", "Resistance", {30, 1}, 28, 8);
    recovery.Descriptor(resistance, 0x0800c028, 28);
    recovery.Descriptor(resistance, 0x0800c9c8, 8);
    auto& voltage = recovery.Add("voltage", "Voltage", {30, 12}, 28, 8);
    recovery.Descriptor(voltage, 0x0800c044, 28);
    recovery.Descriptor(voltage, 0x0800c098, 28); // TC uses this row for watts.
    recovery.Descriptor(voltage, 0x0800c9a8, 8);
    recovery.Descriptor(voltage, 0x0800c9c0, 8);
    auto& duration = recovery.Add("duration", "Duration", {30, 23}, 28, 8);
    recovery.Descriptor(duration, 0x0800c060, 28);
    recovery.Descriptor(duration, 0x0800c9b0, 8);
    auto& mode = recovery.Add("mode", "Mode", {109, 1}, 18, 12);
    for (uint32_t address = 0x0800c9d0; address <= 0x0800c9f0; address += 8)
        recovery.Descriptor(mode, address, 8);
    recovery.Descriptor(mode, 0x0800cc84, 8); // Native curve label alternative.
}
void Refresh(Recovery& recovery, bool fit3) {
    // FIT3's functions below the renderer move by 0x11c, main by 0x108,
    // while battery/native font helpers move by 0x164. These are exact release
    // relocations, not a guessed global shift or instruction search.
    const uint32_t ui = fit3 ? 0x11c : 0;
    const uint32_t main = fit3 ? 0x108 : 0;
    const uint32_t renderer = fit3 ? 0x164 : 0;
    auto& power = recovery.Add("power", "Power", {60, 0}, 44, 32);
    for (const auto address : {0x080059aau, 0x08005a48u, 0x08005a8cu,
        0x08005be4u, 0x08005becu, 0x08005c04u, 0x08005c0cu})
        recovery.Imm(power, {address + ui});
    recovery.Imm(power, {0x080068b0 + main, 0x080068b4 + main});
    auto& battery = recovery.Add("battery", "Battery", {0, 0}, 25, 32);
    recovery.Imm(battery, {0x08009252 + renderer, 0x08009268 + renderer,
        0x0800926c + renderer, 0x080094ec + renderer, 0x080094ee + renderer});
    recovery.SiteAt(battery, 0x08009258 + renderer, Encoding::ThumbMovAliasImm8);
    // Keep the unchanged page operand independent from the moved x endpoint.
    recovery.SiteAt(battery, 0x08009256 + renderer, Encoding::ThumbMovAliasImm8, true, 8);
    recovery.SiteAt(battery, 0x0800926a + renderer, Encoding::ThumbImm8, true, 8);
    recovery.SiteAt(battery, 0x080094e4 + renderer, Encoding::ThumbMovImm, true, 8);
    battery.stepY = 8;
    auto& statistics = recovery.Add("statistics", "Statistics", {20, 0}, 40, 32);
    // Resistance/voltage/time are one native 30x32 allocated bitmap. Move the
    // final draw and its left clear paths, never its allocator or buffer strides.
    recovery.Imm(statistics, {0x080054f8 + ui, 0x080054fc + ui,
        0x08005712 + ui, 0x08005716 + ui, 0x08006892 + main, 0x08006896 + main});
    recovery.SiteAt(statistics, 0x080056b2 + ui, Encoding::ThumbMovAliasImm8, false, 1, 30);
    recovery.SiteAt(statistics, 0x080058de + ui, Encoding::ThumbMovAliasImm8, false, 1, 30);
    statistics.stepY = 8;
    auto& mode = recovery.Add("mode", "Mode", {104, 0}, 24, 32);
    for (const auto address : {0x080059b2u, 0x080059b6u, 0x080059cau, 0x080059eau,
        0x080059fau, 0x08005a0eu, 0x08005a20u, 0x08005a50u, 0x08005a54u,
        0x08005a64u, 0x08005a6au, 0x08005a94u, 0x08005a98u, 0x08005aa8u, 0x08005aaeu})
        recovery.Imm(mode, {address + ui});
    mode.stepY = 8;
}
} // namespace

bool LegacyLayout::DiscoverRelease(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error) {
    elements.clear();
    error.clear();
    FirmwareModel model;
    FirmwareEdition edition;
    if (!DetectFirmwareProfile(package, model, edition) ||
        (model != FirmwareModel::Drag1 && model != FirmwareModel::Drag2 && model != FirmwareModel::DragMini &&
            model != FirmwareModel::Drag2Refresh && model != FirmwareModel::DragMiniRefresh)) {
        error = "Unsupported legacy OLED native layout profile";
        return false;
    }
    FirmwarePackage baseline;
    if (!LoadEmbeddedFirmware(baseline, error, model, edition)) return false;
    const bool mini = model == FirmwareModel::DragMini || model == FirmwareModel::DragMiniRefresh;
    const bool refresh = model == FirmwareModel::Drag2Refresh || model == FirmwareModel::DragMiniRefresh;
    const bool drag1 = model == FirmwareModel::Drag1;
    const bool fit3 = edition == FirmwareEdition::Fit3;
    const uint16_t expectedCode = refresh ? (mini ? 0x1008 : 0x1006) : 0;
    // Inventory: XWDF originals and SWDF Refresh releases each have one actual
    // hardware record. Reject duplicates/missing records, never select a proxy.
    if (package.records.size() != 1 || baseline.records.size() != 1) {
        error = "Legacy OLED release must contain its one native hardware record";
        return false;
    }
    for (const auto& record : package.records) {
        const auto original = std::find_if(baseline.records.begin(), baseline.records.end(),
            [&](const HardwareRecord& item) { return Code(item) == Code(record); });
        if (Code(record) != expectedCode || original == baseline.records.end() ||
            record.linked_base != original->linked_base || record.sp != original->sp ||
            record.reset != original->reset || record.image.size() != original->image.size() ||
            record.marker != original->marker) {
            error = "Unsupported legacy OLED native hardware record";
            return false;
        }
        Recovery recovery{*original, record};
        if (!refresh && mini) {
            recovery.codeShift = -0x50;
            recovery.dataShift = edition == FirmwareEdition::V2 ? -0x90 : -0xa4;
        }
        if (refresh) {
            Refresh(recovery, fit3);
            LegacyLayout::RefreshPages(recovery, fit3);
        } else {
            if (drag1) LegacyLayout::Drag1Main(recovery); else Original(recovery);
            LegacyLayout::OriginalPages(recovery, drag1);
            LegacyLayout::OriginalLoaderPages(recovery, drag1);
        }
        const uint32_t executableEnd = refresh ? 0x08012164 + (fit3 ? 0x324 : 0) - (mini ? 16 : 0) :
            drag1 ? 0x0800b9f0 : mini ? (edition == FirmwareEdition::V2 ? 0x0800bc80 : 0x0800bc6c) : 0x0800bd10;
        if (!recovery.Finish(executableEnd, error)) return false;
        auto& sequence = recovery.elements.front();
        if (refresh) {
            // MAIN 0x08006874: 68aa Statistics, 68c0 Power, 5a66 Mode,
            // 68d0 Battery. FIT3 has the same calls at its release offsets.
            sequence.drawSequence = {2, 0, 3, 1};
            sequence.tcDrawSequence = sequence.drawSequence;
            for (size_t order = 0; order < sequence.drawSequence.size(); ++order)
                recovery.elements[sequence.drawSequence[order]].drawOrder = static_cast<int>(order);
        } else if (!drag1) {
            // MAIN 45be Power digits; 45d0 Duration; 45e0 Voltage; 45f0
            // Resistance; 4608 Power unit; 462e Battery. TC 4d00 interleaves
            // units and numbers before its live-update tail and battery.
            sequence.drawSequence = {0, 4, 3, 2, 0, 1};
            sequence.tcDrawSequence = {0, 3, 2, 3, 2, 4, 5, 3, 0, 5, 3, 2, 1};
            sequence.drawOrderInterleaved = true;
        }
        for (auto& element : recovery.elements) elements.push_back(std::move(element));
    }
    return !elements.empty();
}
bool DiscoverDrag2(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error) {
    FirmwareModel model;
    FirmwareEdition edition;
    if (!DetectFirmwareProfile(package, model, edition) ||
        (model != FirmwareModel::Drag2 && model != FirmwareModel::Drag2Refresh)) {
        elements.clear();
        error = "Unsupported DRAG2 native layout profile";
        return false;
    }
    return LegacyLayout::DiscoverRelease(package, elements, error);
}
} // namespace FirmwareLayout
