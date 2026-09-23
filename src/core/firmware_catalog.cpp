#include "firmware_catalog.h"
#include <cstdio>

const char* PaletteIdForAsset(const AssetSpec& spec) {
    // Glyphs are one-bit masks; do not assign them the fallback accent color.
    if (spec.id.rfind("font_", 0) == 0) return nullptr;

    if (spec.id.rfind("rank_icon_", 0) == 0 || spec.id.rfind("rank_label_", 0) == 0) {
        switch (spec.ordinal) {
        case 1: return "rank_bronze";
        case 3: return "rank_gold";
        case 4: return "rank_ice";
        case 5: return "rank_cyan";
        case 6: return "rank_orange";
        case 7: return "rank_red";
        default: return nullptr;
        }
    }

    // All alert dialog boxes (frames and text) are rendered in pure white (0xFFFF) in firmware
    if (spec.id.rfind("alert_", 0) == 0) return nullptr;

    // Boot screens: DRAG S logo is yellow in firmware, GENE.TT is white, RGB565 boot logos are native color
    if (spec.id == "logo_drag_s") return "highlight_yellow";
    if (spec.id == "boot_gene_chip") return nullptr;
    if (spec.id == "boot_voopoo_animated" || spec.id == "screen_off") return nullptr;

    // Specific alert & UI component color mappings
    if (spec.id == "icon_puff_cleared") return "highlight_yellow";
    if (spec.id == "icon_unlock_large") return "accent_cyan";
    if (spec.id.rfind("icon_", 0) == 0) return "accent_red";

    if (spec.id == "battery_bolt") return "highlight_yellow";
    if (spec.id == "battery_frame") return "accent_cyan";
    if (spec.id == "logo_voopoo") return "highlight_yellow";

    if (spec.group == "Alerts") return "accent_red";
    if (spec.group == "Screens") return "highlight_yellow";
    if (spec.group == "Labels") return "accent_cyan";
    if (spec.group == "Controls") return "accent_red";
    return "accent_red";
}

static void AddSeries(
    std::vector<AssetSpec>& out,
    const std::string& prefix,
    const std::string& title,
    const std::string& group,
    uint32_t address,
    uint32_t width,
    uint32_t height,
    size_t count,
    const std::vector<std::string>& labels
) {
    uint32_t bytes_each = (uint32_t)(((width + 7) / 8) * height);
    char buf[16];
    for (size_t i = 0; i < count; ++i) {
        snprintf(buf, sizeof(buf), "%02zu", i);
        std::string suffix = (i < labels.size()) ? labels[i] : "";
        AssetSpec spec;
        spec.id = prefix + "_" + buf;
        spec.title = suffix.empty() ? (title + " " + std::to_string(i)) : (title + " " + suffix);
        spec.group = group;
        spec.address = address + bytes_each * (uint32_t)i;
        spec.width = width;
        spec.height = height;
        spec.ordinal = i;
        out.push_back(spec);
    }
}

std::vector<AssetSpec> GetAllAssets() {
    std::vector<AssetSpec> out;
    std::vector<std::string> hex = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F" };
    std::vector<std::string> digits = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };

    // 1. font_6x10: 10 digits 0..9 (Address 0x080130D2, 10 bytes per glyph, total 100 bytes)
    AddSeries(out, "font_6x10", "Glyph", "Font 6x10", 0x080130D2, 6, 10, 10, digits);

    // 2. font_7x13: 16 hex glyphs 0..F (Address 0x08013136, 13 bytes per glyph, total 208 bytes)
    AddSeries(out, "font_7x13", "Glyph", "Font 7x13", 0x08013136, 7, 13, 16, hex);

    // 3. font_11x20: 10 digits 0..9 (Address 0x080149F6, 40 bytes per glyph, total 400 bytes)
    AddSeries(out, "font_11x20", "Digit", "Font 11x20", 0x080149F6, 11, 20, 10, digits);

    // 4. font_19x40: 10 digits 0..9 (Address 0x08013206, 120 bytes per glyph, total 1200 bytes)
    AddSeries(out, "font_19x40", "Digit", "Font 19x40", 0x08013206, 19, 40, 10, digits);

    // 5. rank_icon: 8 items (Address 0x08014666, 24x24)
    AddSeries(out, "rank_icon", "Rank icon", "Score", 0x08014666, 24, 24, 8, {});

    // 6. rank_label: 8 items (Address 0x080148A6, 47x7)
    AddSeries(out, "rank_label", "Rank label", "Score", 0x080148A6, 47, 7, 8, {});

    // 7. status: SMART, RBA (Address 0x08014B86, 33x9)
    AddSeries(out, "status", "Status", "Labels", 0x08014B86, 33, 9, 2, { "SMART", "RBA" });

    // 8. toggle: off, on (Address 0x080104EF, 13x13)
    AddSeries(out, "toggle", "Toggle", "Controls", 0x080104EF, 13, 13, 2, { "off", "on" });

    // 9. mode: P, A (Address 0x08010523, 28x11)
    AddSeries(out, "mode", "Mode", "Controls", 0x08010523, 28, 11, 2, { "P", "A" });

    // 10. Individual items
    struct SingleDef {
        const char* id;
        const char* title;
        const char* group;
        uint32_t address;
        uint32_t width;
        uint32_t height;
        AssetFormat format = AssetFormat::Mask1Bit;
    };
    SingleDef singles[] = {
        { "unit_w", "Unit W", "Units", 0x0801057B, 17, 9 },
        { "decimal", "Decimal separator", "Units", 0x080103C0, 2, 10 },
        { "unit_ohm", "Unit Ohm", "Units", 0x08010596, 7, 10 },
        { "unit_v", "Unit V", "Units", 0x080105A0, 7, 10 },
        { "unit_s", "Unit S", "Units", 0x080105FA, 4, 10 },
        { "logo_voopoo", "Logo VOOPOO (Header Banner)", "Screens", 0x08010664, 80, 10 },
        { "panel_chip_id", "CHIP ID panel", "Screens", 0x080106C8, 74, 63 },
        { "panel_level_code", "LEVEL CODE panel", "Screens", 0x0801093E, 74, 46 },

        // Category 1: Alerts (dialogs & warning icons)
        { "alert_check_atomizer", "Dialog Check Atomizer", "Alerts", 0x080141D3, 56, 43 },
        { "icon_check_atomizer_tank", "Icon Check Atomizer Tank", "Alerts", 0x08014143, 20, 19 },
        { "icon_check_atomizer_base", "Icon Check Atomizer Base", "Alerts", 0x0801417C, 20, 29 },
        { "alert_atomizer_short", "Dialog Atomizer Short", "Alerts", 0x08013AAD, 56, 43 },
        { "icon_atomizer_short", "Icon Atomizer Short", "Alerts", 0x080139F9, 45, 30 },
        { "alert_time_out", "Dialog Time Out (10s Cutoff)", "Alerts", 0x0801372A, 56, 43 },
        { "icon_time_out", "Icon Time Out (10s Cutoff)", "Alerts", 0x080136B6, 29, 29 },
        { "alert_temp_high", "Dialog Temp High (>85C)", "Alerts", 0x08014388, 56, 43 },
        { "icon_temp_high", "Icon Temp High Waves", "Alerts", 0x08014300, 31, 34 },
        { "alert_check_battery", "Dialog Check Battery", "Alerts", 0x08013C4F, 56, 43 },
        { "icon_check_battery", "Icon Check Battery Bolt", "Alerts", 0x08013BDA, 20, 39 },
        { "alert_low_power", "Dialog Low Power", "Alerts", 0x08013DF1, 56, 43 },
        { "icon_low_power", "Icon Low Power Outline", "Alerts", 0x08013D7C, 20, 39 },
        { "alert_max_power", "Dialog Max Power", "Alerts", 0x080138CC, 56, 43 },
        { "icon_max_power", "Icon Max Power Arrow", "Alerts", 0x08013857, 23, 39 },
        { "alert_puff_cleared", "Dialog Puff Cleared", "Alerts", 0x08014539, 56, 43 },
        { "icon_puff_cleared", "Icon Puff Cleared Badge", "Alerts", 0x080144B5, 44, 22 },
        { "alert_press_fire_plus", "Dialog Press Fire+Plus to Unlock", "Alerts", 0x08013F9A, 56, 43 },
        { "icon_lock_large", "Icon Lock (Closed Padlock)", "Alerts", 0x08013F1E, 29, 31 },
        { "icon_unlock_large", "Icon Unlock (Open Padlock)", "Alerts", 0x080140C7, 29, 31 },

        // Category 2: Boot & Branding
        { "boot_voopoo_animated", "Logo VOOPOO (Animated Boot)", "Screens", 0x08010B0A, 22, 136, AssetFormat::Rgb565 },
        { "screen_off", "Screen OFF (Shutdown)", "Screens", 0x0801226A, 39, 18, AssetFormat::Rgb565 },
        { "logo_drag_s", "Logo DRAG S (Boot)", "Screens", 0x080127E6, 66, 18 },
        { "boot_gene_chip", "Screen GENE.TT & VOOPOO (Boot)", "Screens", 0x08012888, 61, 33 },
        { "banner_activated", "Banner ACTIVATED (Rank)", "Labels", 0x0801048C, 72, 11 },

        // Category 3: Standby UI & Battery
        { "label_puff", "Label Puff", "Labels", 0x080105AA, 32, 10 },
        { "label_time", "Label Time", "Labels", 0x080105D2, 32, 10 },
        { "battery_frame", "Battery Gauge Frame", "Controls", 0x08012990, 47, 17 },
        { "battery_bolt", "Battery Charging Bolt", "Controls", 0x080130B8, 11, 13 },
    };
    for (const auto& s : singles) {
        AssetSpec spec;
        spec.id = s.id;
        spec.title = s.title;
        spec.group = s.group;
        spec.address = s.address;
        spec.width = s.width;
        spec.height = s.height;
        spec.ordinal = 0;
        spec.format = s.format;
        out.push_back(spec);
    }

    // 11. meter: 8 animation frames (Address 0x08014BE0, 24x6)
    AddSeries(out, "meter", "Meter frame", "Animation", 0x08014BE0, 24, 6, 8, {});

    // 12. state: 6 animation frames (Address 0x08014C70, 46x5)
    AddSeries(out, "state", "State frame", "Animation", 0x08014C70, 46, 5, 6, {});

    return out;
}
