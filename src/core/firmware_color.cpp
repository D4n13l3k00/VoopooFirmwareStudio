#include "firmware_color.h"
#include <cstdio>
#include <cctype>

bool Rgb8::ParseHex(const std::string& str, Rgb8& out) {
    size_t start = 0;
    while (start < str.size() && (std::isspace((unsigned char)str[start]) || str[start] == '#')) {
        start++;
    }
    size_t end = str.size();
    while (end > start && std::isspace((unsigned char)str[end - 1])) {
        end--;
    }
    if (end - start != 6) return false;

    uint32_t val = 0;
    for (size_t i = start; i < end; ++i) {
        char c = str[i];
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        else return false;
    }
    out.r = (uint8_t)((val >> 16) & 0xFF);
    out.g = (uint8_t)((val >> 8) & 0xFF);
    out.b = (uint8_t)(val & 0xFF);
    return true;
}

std::string Rgb8::ToHex() const {
    char buf[10];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    return std::string(buf);
}

std::vector<PaletteState> GetDefaultPalette() {
    return {
        { { "accent_red", "Accent / red mode", "Main UI accent in standard modes - 8 sites", Rgb8(0xF3, 0x01, 0x33) }, Rgb8(0xF3, 0x01, 0x33) },
        { { "accent_cyan", "Accent / cyan mode", "Main UI accent in SMART/RBA states - 7 sites", Rgb8(0x00, 0xDB, 0xFF) }, Rgb8(0x00, 0xDB, 0xFF) },
        { { "highlight_yellow", "Highlight / yellow", "Score strip and service panels - 4 sites", Rgb8(0xF3, 0xDD, 0x01) }, Rgb8(0xF3, 0xDD, 0x01) },
        { { "rank_bronze", "Rank / bronze", "Score rank bronze palette", Rgb8(0xB5, 0x79, 0x47) }, Rgb8(0xB5, 0x79, 0x47) },
        { { "rank_gold", "Rank / gold", "Score rank gold palette", Rgb8(0xF4, 0xDC, 0x00) }, Rgb8(0xF4, 0xDC, 0x00) },
        { { "rank_ice", "Rank / ice", "Score rank ice palette", Rgb8(0xB2, 0xFB, 0xEC) }, Rgb8(0xB2, 0xFB, 0xEC) },
        { { "rank_cyan", "Rank / cyan", "Score rank cyan palette", Rgb8(0x01, 0xE7, 0xF2) }, Rgb8(0x01, 0xE7, 0xF2) },
        { { "rank_orange", "Rank / orange", "Score rank orange palette", Rgb8(0xF2, 0x53, 0x00) }, Rgb8(0xF2, 0x53, 0x00) },
        { { "rank_red", "Rank / red", "Score rank red palette", Rgb8(0xF2, 0x01, 0x33) }, Rgb8(0xF2, 0x01, 0x33) },
        { { "digits_wattage", "Wattage digits", "Large wattage readout on main screen (20x37)", Rgb8(0xFF, 0xFF, 0xFF) }, Rgb8(0xFF, 0xFF, 0xFF) },
        { { "digits_stats", "Stats digits", "Puff counter, voltage, and resistance digits (11x21)", Rgb8(0xFF, 0xFF, 0xFF) }, Rgb8(0xFF, 0xFF, 0xFF) },
    };
}

std::vector<uint8_t> ColorInstruction(Rgb8 rgb) {
    return { rgb.b, 0x22, rgb.g, 0x21, rgb.r, 0x20 };
}

bool DecodeColorInstruction(const uint8_t* code, Rgb8& out_color) {
    if (!code) return false;
    if (code[1] != 0x22 || code[3] != 0x21 || code[5] != 0x20) {
        return false;
    }
    out_color = Rgb8(code[4], code[2], code[0]);
    return true;
}

std::vector<uint8_t> MovwInstruction(int rd, uint16_t value) {
    uint32_t imm4 = (value >> 12) & 0xF;
    uint32_t i = (value >> 11) & 1;
    uint32_t imm3 = (value >> 8) & 0x7;
    uint32_t imm8 = value & 0xFF;
    uint32_t first = 0xF240 | (i << 10) | imm4;
    uint32_t second = (imm3 << 12) | ((rd & 0xF) << 8) | imm8;
    return {
        (uint8_t)first,
        (uint8_t)(first >> 8),
        (uint8_t)second,
        (uint8_t)(second >> 8)
    };
}

bool DecodeMovwInstruction(const uint8_t* code, int expected_rd, uint16_t& out_imm16) {
    if (!code) return false;
    uint16_t first = (uint16_t)code[0] | ((uint16_t)code[1] << 8);
    uint16_t second = (uint16_t)code[2] | ((uint16_t)code[3] << 8);
    if ((first & 0xFBF0) != 0xF240) return false;
    int rd = (second >> 8) & 0xF;
    if (rd != expected_rd) return false;
    uint32_t imm4 = first & 0xF;
    uint32_t i = (first >> 10) & 1;
    uint32_t imm3 = (second >> 12) & 0x7;
    uint32_t imm8 = second & 0xFF;
    out_imm16 = (uint16_t)((imm4 << 12) | (i << 11) | (imm3 << 8) | imm8);
    return true;
}

std::vector<uint8_t> MovwR0(uint16_t value) {
    return MovwInstruction(0, value);
}
