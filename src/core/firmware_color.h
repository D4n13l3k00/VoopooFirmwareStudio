#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Rgb8 {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    constexpr Rgb8() : r(0), g(0), b(0) {}
    constexpr Rgb8(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}

    bool operator==(const Rgb8& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const Rgb8& o) const { return !(*this == o); }

    static bool ParseHex(const std::string& str, Rgb8& out);
    std::string ToHex() const;

    uint16_t ToRgb565() const {
        return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }

    static Rgb8 FromRgb565(uint16_t val) {
        uint32_t r5 = (val >> 11) & 0x1F;
        uint32_t g6 = (val >> 5) & 0x3F;
        uint32_t b5 = val & 0x1F;
        return Rgb8(
            (uint8_t)((r5 * 255 + 15) / 31),
            (uint8_t)((g6 * 255 + 31) / 63),
            (uint8_t)((b5 * 255 + 15) / 31)
        );
    }

    Rgb8 ScreenColor() const {
        return FromRgb565(ToRgb565());
    }
};

struct PaletteSpec {
    const char* id;
    const char* label;
    const char* scope;
    Rgb8 original;
};

struct PaletteState {
    PaletteSpec spec;
    Rgb8 current;
};

std::vector<PaletteState> GetDefaultPalette();
std::vector<uint8_t> ColorInstruction(Rgb8 rgb);
bool DecodeColorInstruction(const uint8_t* code, Rgb8& out_color);
std::vector<uint8_t> MovwInstruction(int rd, uint16_t value);
bool DecodeMovwInstruction(const uint8_t* code, int expected_rd, uint16_t& out_imm16);
std::vector<uint8_t> MovwR0(uint16_t value);
