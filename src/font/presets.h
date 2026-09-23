#pragma once
#include <string>
#include <vector>

struct GlyphDef {
    char ch;
    std::string filename;
};

struct Preset {
    std::string name;
    std::string description;
    int width;
    int height;
    std::vector<GlyphDef> glyphs;

    static Preset font_19x40();
    static Preset font_11x20();
    static Preset font_7x13();
    static Preset font_6x10();
    static std::vector<Preset> all();
};
