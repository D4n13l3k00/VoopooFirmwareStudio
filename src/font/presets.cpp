#include "presets.h"
#include "../core/firmware_extensions.h"
#include <cstdio>

Preset Preset::font_19x40() {
    Preset p;
    p.name = "19x40";
    p.description = "Digits 0-9 for 19x40 display";
    p.width = 19;
    p.height = 40;
    for (int d = 0; d <= 9; ++d) {
        char buf[32];
        snprintf(buf, sizeof(buf), "font_19x40_%02d.png", d);
        p.glyphs.push_back({ (char)('0' + d), std::string(buf) });
    }
    return p;
}

Preset Preset::font_11x20() {
    Preset p;
    p.name = "11x20";
    p.description = "Digits 0-9 for 11x20 display";
    p.width = 11;
    p.height = 20;
    for (int d = 0; d <= 9; ++d) {
        char buf[32];
        snprintf(buf, sizeof(buf), "font_11x20_%02d.png", d);
        p.glyphs.push_back({ (char)('0' + d), std::string(buf) });
    }
    return p;
}

Preset Preset::font_7x13() {
    Preset p;
    p.name = "7x13";
    p.description = "Hex glyphs 0-9, A-F for 7x13 display";
    p.width = 7;
    p.height = 13;
    const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 0; i < 16; ++i) {
        char buf[32];
        snprintf(buf, sizeof(buf), "font_7x13_%02d.png", i);
        p.glyphs.push_back({ hex_chars[i], std::string(buf) });
    }
    return p;
}

Preset Preset::font_6x10() {
    Preset p;
    p.name = "6x10";
    p.description = "Digits 0-9 for 6x10 display";
    p.width = 6;
    p.height = 10;
    for (int d = 0; d <= 9; ++d) {
        char buf[32];
        snprintf(buf, sizeof(buf), "font_6x10_%02d.png", d);
        p.glyphs.push_back({ (char)('0' + d), std::string(buf) });
    }
    return p;
}

std::vector<Preset> Preset::all(FirmwareModel model, FirmwareEdition edition) {
    if (const auto* extension = FirmwareExtensionFor(model)) return extension->presets(model, edition);
    if (model == FirmwareModel::Drag2) {
        std::vector<Preset> presets;
        auto add = [&](const char* prefix, const char* name, int width, int height, const char* chars) {
            Preset preset;
            preset.name = name;
            preset.description = name;
            preset.width = width;
            preset.height = height;
            for (size_t i = 0; chars[i]; ++i) {
                char filename[64];
                snprintf(filename, sizeof(filename), "%s_%02zu.png", prefix, i);
                preset.glyphs.push_back({chars[i], filename});
            }
            presets.push_back(std::move(preset));
        };
        add("font_10x30", "10x30", 10, 30, "0123456789");
        add("font_10x12", "10x12", 10, 12, "0123456789");
        add("font_5x8", "5x8", 5, 8, "0123456789");
        add("font_hex_5x8", "5x8 HEX", 5, 8, "0123456789ABCDEF");
        return presets;
    }
    return { font_19x40(), font_11x20(), font_7x13(), font_6x10() };
}
