#pragma once
#include "firmware_layout.h"
#include <algorithm>
#include <initializer_list>

namespace FirmwareLayout::LegacyLayout {
inline void Pages(Element& element, uint32_t primary, std::initializer_list<uint32_t> shared = {}) {
    element.screen = primary;
    element.sharedScreens.assign(shared);
}
template<class Recovery>
Element& Bitmaps(Recovery& recovery, const char* id, const char* title, uint32_t screen,
    std::initializer_list<uint32_t> addresses) {
    int left = 128, top = 32, right = 0, bottom = 0;
    for (uint32_t address : addresses) {
        const auto* bytes = recovery.stock.image.data() + recovery.Address(address) - recovery.stock.linked_base;
        left = std::min(left, int(bytes[0])); top = std::min(top, int(bytes[1]));
        right = std::max(right, int(bytes[0]) + bytes[2]);
        bottom = std::max(bottom, int(bytes[1]) + bytes[3]);
    }
    auto& element = recovery.Add(id, title, {left, top}, right - left, bottom - top);
    Pages(element, screen);
    for (uint32_t address : addresses) recovery.Descriptor(element, address, 8);
    return element;
}
template<class Recovery>
void Drag1Main(Recovery& recovery) {
    auto& power = recovery.Add("power", "Power / temperature / voltage", {60, 1}, 68, 30);
    for (uint32_t address : {0x0800bc8cu, 0x0800bca8u, 0x0800bcc4u, 0x0800bd50u})
        recovery.Descriptor(power, address, 28);
    for (uint32_t address : {0x0800c66cu, 0x0800c6dcu, 0x0800c6e4u, 0x0800c6ecu, 0x0800c6f4u})
        recovery.Descriptor(power, address, 8);
    Pages(power, 0, {30, 33, 34, 35, 80, 85});
    auto& battery = recovery.Add("battery", "Battery", {1, 1}, 14, 30);
    recovery.SiteAt(battery, 0x08005760, Encoding::ThumbMovAliasImm8, false, 1, 1);
    recovery.SiteAt(battery, 0x08005758, Encoding::ThumbImm8, true);
    Pages(battery, 0, {30, 33, 34, 35, 80, 85});
    auto& resistance = recovery.Add("resistance", "Resistance", {17, 1}, 49, 30);
    for (uint32_t address : {0x0800bce0u, 0x0800bcfcu}) recovery.Descriptor(resistance, address, 28);
    for (uint32_t address : {0x0800c704u, 0x0800c714u, 0x0800cdc4u}) recovery.Descriptor(resistance, address, 8);
    Pages(resistance, 0, {30, 33, 34, 35, 80, 85});
    auto& voltage = recovery.Add("voltage", "Voltage / TC power", {18, 17}, 38, 14);
    for (uint32_t address : {0x0800bd18u, 0x0800bd34u, 0x0800bd6cu}) recovery.Descriptor(voltage, address, 28);
    for (uint32_t address : {0x0800c6fcu, 0x0800c70cu}) recovery.Descriptor(voltage, address, 8);
    Pages(voltage, 0, {30, 33, 34, 35, 80, 85});
    auto& material = Bitmaps(recovery, "mode.screen30", "TC material", 30,
        {0x0800c71c, 0x0800c724, 0x0800c72c, 0x0800c734, 0x0800c73c});
    Pages(material, 30, {0, 33, 34, 85});
    material.temperatureMode = 1;
}
template<class Recovery>
void OriginalPages(Recovery& recovery, bool drag1) {
    if (!drag1) {
        for (size_t i = 0; i < 6; ++i) Pages(recovery.elements[i], 0, {30, 33, 34, 80, 85});
        recovery.elements[1].sharedScreens.push_back(20);
        recovery.elements[4].sharedScreens.push_back(20);
        for (unsigned slot = 0; slot < 3; ++slot) {
            const char* ids[]{"fit1.screen20", "fit2.screen20", "fit3.screen20"};
            Bitmaps(recovery, ids[slot], "FIT item and selected outline", 20,
                {0x0800c8f0 + slot * 8, 0x0800c908 + slot * 8});
        }
        Bitmaps(recovery, "fit.statistics.screen20", "FIT statistics", 20, {0x0800c9b8});
        auto& material = recovery.Add("materials.screen50", "TC material list and selection", {6, 7}, 114, 18);
        Pages(material, 50);
        material.temperatureMode = 1;
        // Ordinal*43 minus these origins positions the label and outline.
        // Negative scales translate both without changing ordinal or spacing.
        for (uint32_t address : {0x080044deu, 0x08004516u, 0x08004524u, 0x0800455cu})
            recovery.SiteAt(material, address, Encoding::ThumbImm8, false, -1);
        for (uint32_t address : {0x080044e6u, 0x0800452cu, 0x08004564u})
            recovery.SiteAt(material, address, Encoding::ThumbImm8, true);
        Bitmaps(recovery, "device.logo.screen5", "Device standby logo", 5, {0x0800c378});
    }
    const uint32_t menu = drag1 ? 0x0800ca44 : 0x0800cc14;
    const auto menuPages = [&](Element& element) {
        if (drag1) Pages(element, 40, {41, 42, 45, 47, 93, 94, 95});
        else Pages(element, 40, {41, 42, 43, 44, 45, 46, 47});
    };
    auto& left = Bitmaps(recovery, "menu.previous.screen40", "Previous menu page", 40, {menu});
    menuPages(left);
    auto& right = Bitmaps(recovery, "menu.next.screen40", "Next menu page", 40, {menu + (drag1 ? 0x38 : 0x40)});
    menuPages(right);
    // Every icon alternative and its reused selection frame is one block.
    auto& first = drag1 ?
        Bitmaps(recovery, "menu.first.screen40", "First menu item and selection", 40,
            {menu + 8, menu + 16, menu + 0x40, menu + 0x48, menu + 0x70, menu + 0x78, menu + 0xc0, menu + 0xc8}) :
        Bitmaps(recovery, "menu.first.screen40", "First menu item and selection", 40,
            {menu + 8, menu + 16, menu + 24, menu + 0x48, menu + 0x50, menu + 0x78, menu + 0x80});
    menuPages(first);
    auto& second = drag1 ?
        Bitmaps(recovery, "menu.second.screen40", "Second menu item and selection", 40,
            {menu + 0x18, menu + 0x20, menu + 0x50, menu + 0x58, menu + 0x98, menu + 0xa8, menu + 0xb8}) :
        Bitmaps(recovery, "menu.second.screen40", "Second menu item and selection", 40, {menu + 0x20, menu + 0x28});
    if (drag1) Pages(second, 40, {41, 45, 47, 93, 94, 95});
    else Pages(second, 40, {41, 42, 43, 44, 45});
    auto& third = drag1 ?
        Bitmaps(recovery, "menu.third.screen40", "Third menu item and selection", 40,
            {menu + 0x28, menu + 0x30, menu + 0x60, menu + 0x68, menu + 0x80, menu + 0x88, menu + 0x90, menu + 0xa0}) :
        Bitmaps(recovery, "menu.third.screen40", "Third menu item and selection", 40,
            {menu + 0x30, menu + 0x38, menu + 0x58, menu + 0x60});
    if (drag1) Pages(third, 40, {41, 45, 47, 94, 95});
    else Pages(third, 40, {41, 42, 43, 44, 45});
    const uint32_t graphData = drag1 ? 0x0800cd84 : 0x0800ce24;
    auto& graph = Bitmaps(recovery, "curve.graph.screen60", "Curve axes, bars and cursor", 60,
        {graphData, graphData + 8, graphData + 16});
    graph.width = 109;
    // Native curve height fills the display; its y computations stay fixed.
    graph.height = 32;
    Pages(graph, 60, {61});
    recovery.Imm(graph, {0x08004046});
    recovery.SiteAt(graph, 0x0800405e, Encoding::ThumbAddImm3);
    recovery.SiteAt(graph, drag1 ? 0x0800461e : 0x0800476e, Encoding::ThumbMovImm);
    recovery.Imm(graph, {drag1 ? 0x08004668u : 0x080047b8u,
        drag1 ? 0x08004e0cu : 0x08004fa4u, drag1 ? 0x08004e62u : 0x08004ffau});
    auto& value = recovery.Add("curve.value.screen60", "Curve value and unit",
        {drag1 ? 88 : 91, 0}, drag1 ? 40 : 28, drag1 ? 14 : 8);
    Pages(value, 60, {61});
    if (drag1) {
        recovery.Descriptor(value, 0x0800bd88, 28); recovery.Descriptor(value, 0x0800bda4, 28);
        for (uint32_t address : {0x0800cdacu, 0x0800cdb4u, 0x0800cdbcu}) recovery.Descriptor(value, address, 8);
    } else {
        recovery.Descriptor(value, 0x0800c0b4, 28);
        for (uint32_t address : {0x0800ce48u, 0x0800ce50u, 0x0800ce58u}) recovery.Descriptor(value, address, 8);
        recovery.Imm(value, {0x08004738, 0x08004750, 0x08004764});
        for (uint32_t address : {0x08004736u, 0x0800474eu, 0x08004762u})
            recovery.SiteAt(value, address, Encoding::ThumbImm8, true);
    }
    if (!drag1) {
        auto& coefficient = recovery.Add("coefficient.screen70", "TC coefficient", {56, 10}, 46, 12);
        Pages(coefficient, 70);
        coefficient.temperatureMode = 1;
        recovery.Imm(coefficient, {0x080053c0, 0x080053c8, 0x080053cc, 0x080053d0});
        recovery.SiteAt(coefficient, 0x080053bc, Encoding::ThumbImm8, true);
        auto& label = recovery.Add("material.label.screen70", "TC material label", {28, 10}, 26, 12);
        Pages(label, 70);
        label.temperatureMode = 1;
        recovery.Imm(label, {0x08005438}); recovery.SiteAt(label, 0x08005436, Encoding::ThumbImm8, true);
    }
    auto& resistance = recovery.Add("resistance.value.screen75", "Resistance value, decimal and unit",
        {drag1 ? 29 : 33, drag1 ? 1 : 10}, drag1 ? 82 : 62, drag1 ? 30 : 12);
    Pages(resistance, 75);
    const int shift = drag1 ? -0x19c : 0;
    recovery.Imm(resistance, {uint32_t(0x080054c0 + shift), uint32_t(0x080054ce + shift),
        uint32_t(0x080055ba + shift)});
    for (uint32_t address : {0x080054d8u, 0x080054dcu, 0x080054e0u})
        recovery.SiteAt(resistance, address + shift, Encoding::ThumbMovImm);
    recovery.SiteAt(resistance, 0x080054c2 + shift, Encoding::ThumbMovImm, true);
    recovery.SiteAt(resistance, 0x080054d2 + shift, Encoding::ThumbImm8, true);
    recovery.SiteAt(resistance, drag1 ? 0x0800541c : 0x080055b8,
        drag1 ? Encoding::ThumbImm8 : Encoding::ThumbMovAliasImm8, true, 1, 10);
    if (!drag1) {
        auto& leftArrow = Bitmaps(recovery, "battery.previous.screen90", "Battery status left marker", 90, {0x0800ce60});
        (void)leftArrow;
        Bitmaps(recovery, "battery.next.screen90", "Battery status right marker", 90, {0x0800ce68});
        auto& batteries = recovery.Add("battery.pair.screen90", "Battery status pair", {24, 8}, 80, 16);
        Pages(batteries, 90);
        recovery.Imm(batteries, {0x080057de, 0x080057ea});
    } else {
        auto& batteries = recovery.Add("battery.pair.screen90", "Battery status pair", {4, 6}, 122, 20);
        Pages(batteries, 90);
        recovery.Imm(batteries, {0x0800563e, 0x0800564a});
        auto& coefficient = recovery.Add("coefficient.screen70", "TC coefficient", {60, 1}, 54, 30);
        Pages(coefficient, 70);
        coefficient.temperatureMode = 1;
        recovery.Imm(coefficient, {0x08005228, 0x08005230, 0x08005234, 0x08005238});
        recovery.SiteAt(coefficient, 0x08005224, Encoding::ThumbImm8, true);
        auto& label = recovery.Add("material.label.screen70", "TC material label", {25, 11}, 32, 20);
        Pages(label, 70);
        label.temperatureMode = 1;
        recovery.Imm(label, {0x0800529c});
        recovery.SiteAt(label, 0x0800529a, Encoding::ThumbImm8, true);
    }
}
template<class Recovery>
void OriginalLoaderPages(Recovery& recovery, bool drag1) {
    // These operands belong to the actual application renderers. VM loader
    // inputs are never patched into the package or replaced by host text.
    auto& version = recovery.Add("loader.version.screen91", "Version digits and separators",
        {drag1 ? 39 : 41, drag1 ? 1 : 10}, drag1 ? 72 : 58, drag1 ? 30 : 12);
    Pages(version, 91);
    version.drawOrder = 0;
    if (drag1) {
        recovery.Imm(version, {0x0800579e, 0x080057bc, 0x0800584a});
        for (uint32_t address : {0x080057b8u, 0x080057beu, 0x080057c2u, 0x080057c6u})
            recovery.SiteAt(version, address, Encoding::ThumbMovImm);
        for (uint32_t address : {0x080057a0u, 0x080057cau})
            recovery.SiteAt(version, address, Encoding::ThumbImm8, true);
    } else {
        recovery.Imm(version, {0x08005932, 0x08005950, 0x080059de});
        for (uint32_t address : {0x0800594cu, 0x08005952u, 0x08005956u, 0x0800595au})
            recovery.SiteAt(version, address, Encoding::ThumbMovImm);
        for (uint32_t address : {0x08005934u, 0x0800595eu})
            recovery.SiteAt(version, address, Encoding::ThumbImm8, true);
    }
    auto& prefix = recovery.Add("loader.prefix.screen91", "Version prefix",
        {drag1 ? 17 : 29, drag1 ? 1 : 10}, drag1 ? 20 : 10, drag1 ? 30 : 12);
    Pages(prefix, 91);
    prefix.drawOrder = 1;
    recovery.Imm(prefix, {drag1 ? 0x0800585cu : 0x080059f0u});
    recovery.SiteAt(prefix, drag1 ? 0x0800585a : 0x080059ee,
        drag1 ? Encoding::ThumbImm8 : Encoding::ThumbMovAliasImm8, true, 1, 10);
    auto& header = recovery.Add("loader.header.screen92", "CHIP-ID heading",
        {drag1 ? 12 : 8, drag1 ? 0 : 4}, drag1 ? 104 : 94, drag1 ? 16 : 12);
    Pages(header, 92);
    header.drawOrder = 0;
    recovery.Imm(header, {drag1 ? 0x080056f0u : 0x08005898u});
    recovery.SiteAt(header, drag1 ? 0x080056ee : 0x08005896, Encoding::ThumbImm8, true);
    recovery.ClipVertical(header, 0, 16);
    auto& identity = recovery.Add("loader.identity.screen92", "CHIP-ID glyph row",
        {8, drag1 ? 18 : 19}, 110, drag1 ? 10 : 8);
    Pages(identity, 92);
    identity.drawOrder = 1;
    recovery.Imm(identity, {drag1 ? 0x0800571eu : 0x080058c4u});
    recovery.SiteAt(identity, drag1 ? 0x0800571c : 0x080058c2, Encoding::ThumbImm8, true);
    // The native renderer clears the full-width bottom half between heading
    // and row. Keep both blocks inside their real paint/clear windows.
    recovery.ClipVertical(identity, 16, 32);
}
template<class Recovery>
void RefreshPages(Recovery& recovery, bool fit3) {
    const uint32_t ui = fit3 ? 0x11c : 0, page = fit3 ? 0x108 : 0, renderer = fit3 ? 0x164 : 0;
    Pages(recovery.elements[0], 0, {30});
    Pages(recovery.elements[1], 0, {20, 30, 70, 80});
    Pages(recovery.elements[2], 0, {30, 70});
    Pages(recovery.elements[3], 0, {30});
    const auto x = [&](Element& element, std::initializer_list<uint32_t> addresses, uint32_t shift) {
        for (uint32_t address : addresses) recovery.SiteAt(element, address + shift, Encoding::ThumbImm8);
    };
    const auto y = [&](Element& element, std::initializer_list<uint32_t> addresses, uint32_t shift) {
        for (uint32_t address : addresses) recovery.SiteAt(element, address + shift, Encoding::ThumbImm8, true, 8);
        element.stepY = 8;
    };
    auto& fitStatistics = recovery.Add("fit.statistics.screen20", "FIT statistics and clear bounds", {25, 0}, 35, 32);
    Pages(fitStatistics, 20);
    x(fitStatistics, {0x08005340, 0x08005344, 0x0800534c, 0x08005350}, ui);
    recovery.SiteAt(fitStatistics, 0x0800549e + ui, Encoding::ThumbMovAliasImm8, false, 1, 30);
    auto& fit = recovery.Add("fit.selection.screen20", "FIT label, ordinal and clear bounds", {58, 0}, 70, 32);
    Pages(fit, 20);
    x(fit, {0x0800683e, 0x08006842, 0x0800684a, 0x0800684e}, page);
    x(fit, {0x08005b60, 0x08005b68, 0x08005b6c, 0x08005b84, 0x08005b98,
        0x08005baa, 0x08005bb4, 0x08005bb8}, ui);
    auto& memory = recovery.Add("memory.selection.screen70", "Memory label, ordinal and clear bounds", {58, 0}, 70, 32);
    Pages(memory, 70);
    x(memory, {0x0800711e, 0x08007122}, page);
    x(memory, {0x08005adc, 0x08005ae4, 0x08005ae8, 0x08005afe,
        0x08005b0c, 0x08005b10, 0x08005b20, 0x08005b32}, ui);
    auto& materials = recovery.Add("materials.screen40", "TC material list", {6, 0}, 116, 32);
    Pages(materials, 40);
    materials.temperatureMode = 1;
    // All selected/unselected alternatives use these RAM descriptor origins.
    // Their bitmaps are 28x32, so only the horizontal axis is movable.
    x(materials, {0x08005ec2, 0x08005ec4, 0x08005ecc}, page);
    auto& tcFirst = recovery.Add("tc.first.screen43", "TC material, value and clear bounds", {0, 0}, 44, 32);
    Pages(tcFirst, 43);
    tcFirst.temperatureMode = 1;
    recovery.SiteAt(tcFirst, 0x08006014 + page, Encoding::ThumbMovAliasImm8);
    x(tcFirst, {0x08006012, 0x0800601c, 0x08006020, 0x0800607a,
        0x08006082, 0x08006086, 0x080060e6, 0x080060f4, 0x08006156,
        0x08006164, 0x08006182, 0x08006198, 0x080061a8, 0x080061c8, 0x080061da}, page);
    auto& tcSecond = recovery.Add("tc.second.screen43", "TC resistance and clear bounds", {43, 0}, 39, 32);
    Pages(tcSecond, 43);
    tcSecond.temperatureMode = 1;
    x(tcSecond, {0x0800602e, 0x08006038, 0x0800603c, 0x08006044, 0x08006048,
        0x0800608e, 0x08006092, 0x0800609a, 0x0800609e,
        0x08006100, 0x08006116, 0x08006170, 0x080061b4, 0x080061ec}, page);
    auto& tcThird = recovery.Add("tc.third.screen43", "TC power and clear bounds", {81, 0}, 47, 32);
    Pages(tcThird, 43);
    tcThird.temperatureMode = 1;
    x(tcThird, {0x08006054, 0x0800605e, 0x08006062, 0x0800606a, 0x0800606e,
        0x080060a6, 0x080060aa, 0x080060b2, 0x080060b6,
        0x0800612c, 0x0800613c, 0x080061fe}, page);
    auto& curve = recovery.Add("curve.graph.screen60", "Curve grid, bars and clear boundary", {36, 16}, 91, 16);
    Pages(curve, 60, {61, 64, 65});
    x(curve, {0x08006356, 0x0800636a}, page);
    recovery.SiteAt(curve, 0x08006502 + page, Encoding::ThumbMovImm);
    auto& curveMaterial = recovery.Add("curve.material.screen60", "Curve material label", {5, 0}, 21, 16);
    Pages(curveMaterial, 60, {61, 64, 65});
    x(curveMaterial, {0x080063c0, 0x080065b6, 0x080065c8, 0x080065f4,
        0x08006620, 0x0800665c, 0x08006688, 0x080066b6, 0x080066e4}, page);
    y(curveMaterial, {0x080063be, 0x080065b4, 0x080065c6, 0x080065f2,
        0x0800661e, 0x0800665a, 0x08006686, 0x080066b4, 0x080066e2}, page);
    auto& curveSetting = recovery.Add("curve.setting.screen60", "Curve setting and clear bounds", {26, 0}, 53, 16);
    Pages(curveSetting, 60, {61, 64, 65});
    x(curveSetting, {0x080063dc, 0x080065d0, 0x080065d4, 0x08006610,
        0x0800663c, 0x08006678, 0x080066a4, 0x080066d2, 0x08006700}, page);
    auto& curveTime = recovery.Add("curve.duration.screen60", "Curve duration", {79, 0}, 43, 16);
    Pages(curveTime, 60, {61, 64, 65});
    x(curveTime, {0x0800640e, 0x0800642a, 0x08006460, 0x080064a0, 0x080064d8, 0x080064f8}, page);
    y(curveTime, {0x0800640c, 0x08006428, 0x0800645e, 0x0800649e, 0x080064d6, 0x080064f6}, page);
    auto& curveValue = recovery.Add("curve.value.screen60", "Selected curve value and unit", {3, 16}, 31, 16);
    Pages(curveValue, 60, {61, 64, 65});
    x(curveValue, {0x08006534, 0x0800659a, 0x08006716, 0x080067a8, 0x080067f4}, page);
    // The unit page operand aliases the glyph height, so it must be split.
    y(curveValue, {0x08006532, 0x08006714}, page);
    for (uint32_t address : {0x08006598u, 0x080067a6u, 0x080067f2u})
        recovery.SiteAt(curveValue, address + page, Encoding::ThumbMovAliasImm8, true, 8, 2);
    const char* rankIds[]{"rank.level.screen80", "rank.label.screen80", "rank.score.screen80", "rank.total.screen80"};
    const Point rankPoints[]{{20, 16}, {20, 0}, {80, 16}, {74, 0}};
    const int rankWidths[]{60, 54, 48, 54};
    const uint32_t rankSites[]{0x08006a00, 0x08006d90, 0x08006f64, 0x080070e0};
    for (size_t i = 0; i < 4; ++i) {
        auto& rank = recovery.Add(rankIds[i], "Rank native block", rankPoints[i], rankWidths[i], 16);
        Pages(rank, 80);
        x(rank, {rankSites[i]}, page);
    }
    auto& statusLeft = recovery.Add("battery.left.screen90", "Battery A label and level", {0, 8}, 57, 16);
    Pages(statusLeft, 90);
    x(statusLeft, {0x080091fc, 0x0800934a, 0x0800936c}, renderer);
    for (uint32_t address : {0x08009352u, 0x08009368u})
        recovery.SiteAt(statusLeft, address + renderer, Encoding::UInt8);
    auto& statusMiddle = recovery.Add("battery.middle.screen90", "Battery B label and level", {57, 8}, 42, 16);
    Pages(statusMiddle, 90);
    x(statusMiddle, {0x0800920c, 0x080092c0, 0x080092e2}, renderer);
    for (uint32_t address : {0x080092c8u, 0x080092deu})
        recovery.SiteAt(statusMiddle, address + renderer, Encoding::UInt8);
    auto& statusRight = recovery.Add("battery.label.screen90", "Battery status label", {99, 8}, 29, 16);
    Pages(statusRight, 90);
    x(statusRight, {0x0800921c}, renderer);
    y(statusRight, {0x0800921a}, renderer);
}
}
