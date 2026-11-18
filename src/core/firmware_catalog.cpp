#include "firmware_catalog.h"
#include "zwdf_package.h"
#include "factory_catalog.h"
#include "firmware_extensions.h"
#include "firmware_profiles.h"
#include <cstdio>
#include <algorithm>

namespace {
struct RecordProfile {
    uint16_t code;
    size_t imageSize;
    uint32_t linkedBase;
    uint32_t stackPointer;
};

constexpr RecordProfile dragSRecords[] = {
    {0x314d, 63488, 0x08005800, 0x20003820},
    {0x3014, 79400, 0x08003000, 0x200025b8},
    {0x302a, 69428, 0x08003000, 0x20002558},
    {0x3013, 79384, 0x08003000, 0x200025b8},
    {0x3029, 69412, 0x08003000, 0x20002558},
    {0x3103, 63612, 0x08005800, 0x20003820},
    {0x3105, 63936, 0x08005800, 0x20003820},
};
constexpr RecordProfile dragXRecords[] = {
    {0x314e, 63328, 0x08005800, 0x20003820},
    {0x3012, 79144, 0x08003000, 0x200025b8},
    {0x302c, 69180, 0x08003000, 0x20002558},
    {0x3011, 79128, 0x08003000, 0x200025b8},
    {0x302b, 69168, 0x08003000, 0x20002558},
    {0x3106, 63464, 0x08005800, 0x20003820},
    {0x3108, 63780, 0x08005800, 0x20003820},
};
constexpr RecordProfile dragSFactoryRecords[] = {
    {0x3148, 62592, 0x08003000, 0x20002550},
    {0x3014, 73452, 0x08003000, 0x200025b0},
    {0x3104, 64572, 0x08005800, 0x20003410},
    {0x3013, 73440, 0x08003000, 0x200025b0},
    {0x3029, 62388, 0x08003000, 0x20002550},
    {0x3103, 61340, 0x08005800, 0x20003410},
    {0x3034, 63040, 0x08003000, 0x20002550},
    {0x3105, 66176, 0x08005800, 0x20003410},
    {0x314d, 63864, 0x08005800, 0x20003410},
};
constexpr RecordProfile dragXFactoryRecords[] = {
    {0x3149, 62312, 0x08003000, 0x20002550},
    {0x3012, 73060, 0x08003000, 0x200025b0},
    {0x302c, 62196, 0x08003000, 0x20002550},
    {0x3107, 64452, 0x08005800, 0x20003410},
    {0x3011, 73052, 0x08003000, 0x200025b0},
    {0x302b, 62144, 0x08003000, 0x20002550},
    {0x3106, 64424, 0x08005800, 0x20003410},
    {0x3033, 62764, 0x08003000, 0x20002550},
    {0x3108, 66060, 0x08005800, 0x20003410},
    {0x314e, 63768, 0x08005800, 0x20003410},
};

template<size_t N>
bool MatchesProfile(const FirmwarePackage& fw, const RecordProfile (&profile)[N]) {
    if (fw.format != FirmwareFormat::Zwdf || fw.records.size() != N) return false;
    unsigned seen = 0;
    for (const auto& rec : fw.records) {
        const uint16_t code = (uint16_t)((rec.device_code[0] << 8) | rec.device_code[1]);
        size_t i = 0;
        while (i < N && profile[i].code != code) ++i;
        if (i == N || (seen & (1u << i))) return false;
        const auto& expected = profile[i];
        const uint32_t reset = expected.linkedBase +
            (expected.linkedBase == 0x08005800 ? 0x2b1 : 0x2b9);
        if (rec.image.size() != expected.imageSize || rec.linked_base != expected.linkedBase ||
            rec.sp != expected.stackPointer || rec.reset != reset) return false;
        seen |= 1u << i;
    }
    return seen == (1u << N) - 1;
}
}

bool DetectFirmwareProfile(const FirmwarePackage& fw, FirmwareModel& model, FirmwareEdition& edition) {
    // Layout metadata remains stable after bitmap and palette edits. Do not pin
    // content hashes: saved custom firmware must reopen with its own signatures.
    for (const auto family : {FirmwareModel::DragSPro, FirmwareModel::DragXPlus,
        FirmwareModel::Drag3, FirmwareModel::Drag1}) {
        if (FirmwareExtensionFor(family)->detect(fw, model, edition)) return true;
    }
    if (fw.format == FirmwareFormat::Xwdf && fw.records.size() == 1) {
        const auto& rec = fw.records.front();
        static constexpr uint8_t machine[] = {'S', 'C', '0', '0', '2', 'P', '0', '0', '3', '4', 0, 0};
        if (rec.marker.size() == sizeof(machine) &&
            std::equal(machine, machine + sizeof(machine), rec.marker.begin()) &&
            rec.device_code[0] == 0 && rec.device_code[1] == 0 &&
            rec.image.size() == 50292 && rec.linked_base == 0x08003000 &&
            rec.sp == 0x200025a8 && rec.reset == 0x08003145) {
            model = FirmwareModel::Drag2;
            edition = FirmwareEdition::Standard;
            return true;
        }
    }
    if (MatchesProfile(fw, dragSRecords)) { model = FirmwareModel::DragS; edition = FirmwareEdition::Score; return true; }
    if (MatchesProfile(fw, dragXRecords)) { model = FirmwareModel::DragX; edition = FirmwareEdition::Score; return true; }
    if (MatchesProfile(fw, dragSFactoryRecords)) { model = FirmwareModel::DragS; edition = FirmwareEdition::Factory; return true; }
    if (MatchesProfile(fw, dragXFactoryRecords)) { model = FirmwareModel::DragX; edition = FirmwareEdition::Factory; return true; }
    return false;
}

bool DetectFirmwareModel(const FirmwarePackage& fw, FirmwareModel& model) {
    FirmwareEdition edition;
    return DetectFirmwareProfile(fw, model, edition);
}

const char* FirmwareEditionName(FirmwareEdition edition) {
    switch (edition) {
    case FirmwareEdition::Factory: return "Factory";
    case FirmwareEdition::Standard: return "Standard";
    case FirmwareEdition::Tc: return "TC";
    case FirmwareEdition::V1: return "V1.0.0.1";
    case FirmwareEdition::V2: return "V1.0.0.2";
    case FirmwareEdition::Fit3: return "FIT 3";
    default: return "Score";
    }
}

const char* FirmwareModelName(FirmwareModel model) {
    switch (model) {
    case FirmwareModel::DragX: return "DRAG X";
    case FirmwareModel::Drag2: return "DRAG 2";
    case FirmwareModel::DragSPro: return "DRAG S PRO";
    case FirmwareModel::DragXPro: return "DRAG X PRO";
    case FirmwareModel::DragXPlus: return "DRAG X PLUS";
    case FirmwareModel::DragXPlusPro: return "DRAG X PLUS PRO EDITION";
    case FirmwareModel::Drag3: return "DRAG 3";
    case FirmwareModel::DragMax: return "DRAG MAX";
    case FirmwareModel::Drag1: return "DRAG";
    case FirmwareModel::DragMini: return "DRAG mini";
    case FirmwareModel::Drag2Refresh: return "DRAG 2 Refresh Edition";
    case FirmwareModel::DragMiniRefresh: return "DRAG mini Refresh Edition";
    default: return "DRAG S";
    }
}

const char* FirmwareDefaultFilename(FirmwareModel model, FirmwareEdition edition) {
    if (model == FirmwareModel::Drag2) edition = FirmwareEdition::Standard;
    const auto* profile = FindFirmwareProfile(model, edition);
    return profile ? profile->filename : "";
}
bool FirmwareHasPalette(FirmwareModel model) {
    return model != FirmwareModel::Drag2 && model != FirmwareModel::Drag1 &&
        model != FirmwareModel::DragMini && model != FirmwareModel::Drag2Refresh &&
        model != FirmwareModel::DragMiniRefresh;
}


uint16_t FirmwareCanonicalCode(FirmwareModel model) {
    switch (model) {
    case FirmwareModel::DragS: return 0x3103;
    case FirmwareModel::DragX: return 0x3106;
    case FirmwareModel::DragSPro: return 0x3119;
    case FirmwareModel::DragXPro: return 0x3110;
    case FirmwareModel::DragXPlus: return 0x302f;
    case FirmwareModel::DragXPlusPro: return 0x3139;
    case FirmwareModel::Drag3: return 0x3030;
    case FirmwareModel::DragMax: return 0x3021;
    case FirmwareModel::Drag2Refresh: return 0x1006;
    case FirmwareModel::DragMiniRefresh: return 0x1008;
    default: return 0;
    }
}

// Fixed colors are verified in both models' canonical bitmap draw call sites,
// independently of the theme's patched palette instructions.
Rgb8 FixedForegroundForAsset(const AssetSpec& spec) {
    if (spec.format == AssetFormat::Mask1BitPages) return {255, 255, 255};
    const auto& id = spec.id;
    if (id == "toggle_01") return {118, 255, 0};
    if (id == "logo_drag_s" || id == "logo_drag_x") return {220, 199, 0};
    if (id == "banner_activated") return {243, 219, 1};
    if (id == "icon_puff_cleared") return {255, 220, 0};
    if (id == "icon_unlock_large") return {90, 225, 1};
    if (id == "icon_check_atomizer_base") return {0, 223, 255};
    if (id == "icon_max_power" || id == "icon_time_out" || id == "icon_temp_high")
        return {234, 99, 0};
    if (id == "icon_lock_large" || id == "icon_low_power" || id == "icon_check_battery" ||
        id == "icon_atomizer_short" || id == "icon_check_atomizer_tank") return {235, 0, 1};
    return {255, 255, 255};
}

bool AssetUsesModeColor(const AssetSpec& spec, FirmwareModel model) {
    if (const auto* extension = FirmwareExtensionFor(model)) return extension->usesModeColor(spec);
    if (spec.format == AssetFormat::Mask1BitPages) return false;
    const auto& id = spec.id;
    if (!spec.recordAddresses.empty())
        return id.rfind("factory_panel", 0) == 0 ||
            id.rfind("factory_legacy_mode_", 0) == 0 ||
            id == "factory_legacy_unit_ohm" || id == "factory_legacy_unit_v" ||
            id == "factory_legacy_graph_top" || id == "factory_legacy_label_time";
    return id.rfind("mode_", 0) == 0 || id == "label_puff" || id == "label_time" ||
        id == "unit_ohm" || id == "unit_v" || id.rfind("font_11x20_", 0) == 0;
}

const char* PaletteIdForAsset(const AssetSpec& spec, size_t previewRank, bool cyanMode, FirmwareEdition edition, FirmwareModel model) {
    if (const auto* extension = FirmwareExtensionFor(model)) return extension->paletteId(spec, cyanMode);
    if (spec.format == AssetFormat::Mask1BitPages) return nullptr;
    const auto& id = spec.id;
    if (edition == FirmwareEdition::Factory) {
        if (id.rfind("font_19x40_", 0) == 0 || id == "factory_header" ||
            id == "factory_legacy_clock_colon") return "digits_wattage";
        if (id.rfind("font_6x10_", 0) == 0 || id.rfind("font_7x13_", 0) == 0 ||
            id == "factory_minus_6x10" || id == "unit_s" || id == "unit_ohm" ||
            id == "unit_v" || id == "decimal") return "digits_stats";
        if (id.rfind("factory_legacy_status_", 0) == 0)
            return spec.ordinal == 1 ? "accent_cyan" : "accent_red";
        if (id.rfind("mode_factory_", 0) == 0)
            return spec.ordinal == 2 ? "accent_cyan" : "accent_red";
        if (AssetUsesModeColor(spec)) return cyanMode ? "accent_cyan" : "accent_red";
        if (id.rfind("factory_graph_", 0) == 0 || id == "label_time" || id == "label_puff")
            return "accent_red";
        if (id == "logo_voopoo" || id == "panel_chip_id" || id == "panel_level_code")
            return "highlight_yellow";
        if (id == "factory_legacy_toggle_on") return "service_green";
        return nullptr;
    }
    if (id.rfind("font_19x40_", 0) == 0) return "digits_wattage";
    if (id.rfind("font_11x20_", 0) == 0) return "digits_stats";
    if (id == "status_00" || id == "toggle_00") return "accent_red";
    if (id == "status_01") return "accent_cyan";
    if (AssetUsesModeColor(spec)) return cyanMode ? "accent_cyan" : "accent_red";
    if (id.rfind("state_", 0) == 0 || id == "logo_voopoo" ||
        id == "panel_chip_id" || id == "panel_level_code") return "highlight_yellow";
    if (id.rfind("rank_icon_", 0) == 0 || id.rfind("meter_", 0) == 0) {
        // Meter ordinal selects an animation frame, never the rank color.
        const size_t rank = id.rfind("meter_", 0) == 0 ? previewRank : spec.ordinal;
        switch (rank) {
        case 1: return "rank_bronze";
        case 3: return "rank_gold";
        case 4: return "rank_ice";
        case 5: return "rank_cyan";
        case 6: return "rank_orange";
        case 7: return "rank_red";
        default: return nullptr;
        }
    }
    // No group-wide fallback: mask colors come from their actual draw call.
    return nullptr;
}

Rgb8 FirmwareFixedForeground(const FirmwarePackage& fw, const AssetSpec& spec, size_t previewRank) {
    if (spec.format == AssetFormat::Mask1BitPages) return {255, 255, 255};
    FirmwareModel model;
    FirmwareEdition edition;
    if (!DetectFirmwareProfile(fw, model, edition)) return FixedForegroundForAsset(spec);
    const uint16_t canonical = spec.sourceCode ? spec.sourceCode : FirmwareCanonicalCode(model);
    const HardwareRecord* rec = nullptr;
    for (const auto& item : fw.records)
        if (((item.device_code[0] << 8) | item.device_code[1]) == canonical)
            rec = &item;
    if (!rec) return FixedForegroundForAsset(spec);
    if (edition == FirmwareEdition::Factory) {
        if (spec.foregroundOffset && spec.foregroundOffset + 6 <= rec->image.size()) {
            Rgb8 color;
            if (DecodeColorInstruction(rec->image.data() + spec.foregroundOffset, color)) return color;
        }
        if (spec.id == "factory_legacy_label_puff") return {0, 0, 0};
        return {255, 255, 255};
    }
    const bool isX = model == FirmwareModel::DragX;
    const auto& id = spec.id;
    size_t off = 0;
    // Each pair is independently verified in 3103 / 3106. In particular the
    // boot function moves by 12 bytes, whereas the alert functions move by 36.
    if (id == "logo_drag_s" || id == "logo_drag_x") off = isX ? 0x281c : 0x2828;
    else if (id == "icon_atomizer_short") off = isX ? 0x2d44 : 0x2d68;
    else if (id == "icon_check_atomizer_tank") off = isX ? 0x30b8 : 0x30dc;
    else if (id == "icon_check_atomizer_base") off = isX ? 0x30c4 : 0x30e8;
    else if (id == "icon_check_battery") off = isX ? 0x3144 : 0x3168;
    else if (id == "icon_low_power") off = isX ? 0x31ac : 0x31d0;
    else if (id == "icon_max_power") off = isX ? 0x321c : 0x3240;
    else if (id == "icon_puff_cleared") off = isX ? 0x328c : 0x32b0;
    else if (id == "icon_temp_high") off = isX ? 0x32f4 : 0x3318;
    else if (id == "icon_time_out") off = isX ? 0x335c : 0x3380;
    else if (id == "icon_unlock_large") off = isX ? 0x33c4 : 0x33e8;
    else if (id == "banner_activated") off = isX ? 0x3466 : 0x348a;
    else if (id == "icon_lock_large") off = isX ? 0x3498 : 0x34bc;
    if (off && off + 6 <= rec->image.size()) {
        Rgb8 color;
        if (DecodeColorInstruction(rec->image.data() + off, color)) return color;
    }
    const size_t toggleOff = isX ? 0x558a : 0x55b6;
    if (id == "toggle_01" && rec->image.size() > toggleOff + 7 &&
        rec->image[toggleOff + 1] == 0x22 && rec->image[toggleOff + 5] == 0x21 &&
        rec->image[toggleOff + 7] == 0x20)
        return {rec->image[toggleOff + 6], rec->image[toggleOff + 4], rec->image[toggleOff]};
    if (id.rfind("rank_icon_", 0) == 0 || id.rfind("meter_", 0) == 0) {
        const size_t rank = id.rfind("meter_", 0) == 0 ? previewRank : spec.ordinal;
        if (rank == 0 || rank == 2) {
            const size_t colorOff = (isX ? 0xad70 : 0xae04) + rank * 12 + 8;
            if (colorOff + 2 <= rec->image.size())
                return Rgb8::FromRgb565(rec->image[colorOff] | (rec->image[colorOff + 1] << 8));
        }
    }
    return FixedForegroundForAsset(spec);
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
    const std::vector<std::string>& labels,
    AssetFormat format = AssetFormat::Mask1Bit
) {
    const uint32_t bytes_each = format == AssetFormat::Mask1BitPages ? width * ((height + 7) / 8) :
        (format == AssetFormat::Rgb565 ? width * height * 2 : ((width + 7) / 8) * height);
    char buf[16];
    for (size_t i = 0; i < count; ++i) {
        snprintf(buf, sizeof(buf), "%02zu", i);
        std::string suffix = (i < labels.size()) ? labels[i] : "";
        AssetSpec spec;
        spec.id = prefix + "_" + buf;
        const bool digit = title == "Glyph" && suffix.size() == 1 && suffix[0] >= '0' && suffix[0] <= '9';
        spec.title = suffix.empty() ? (title + " " + std::to_string(i)) :
            ((digit ? "Digit" : title) + " " + suffix);
        spec.group = group;
        spec.address = address + bytes_each * (uint32_t)i;
        spec.width = width;
        spec.height = height;
        spec.ordinal = i;
        spec.format = format;
        out.push_back(spec);
    }
}

static std::vector<AssetSpec> GetDrag2Assets() {
    std::vector<AssetSpec> out;
    out.reserve(128);
    const std::vector<std::string> digits = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "Leading-zero slot"
    };
    const std::vector<std::string> hex = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F",
        "Chip ID sentinel 16 (S)", "Chip ID sentinel 17 (S)"
    };
    constexpr auto pages = AssetFormat::Mask1BitPages;
    // Numeric descriptors at 0x0800bff0..c0b4 supply dimensions, pointer and
    // per-glyph stride to 0x0800528c. Slot 10 is used for leading-zero handling;
    // stock 10x30 has a blank, whereas stock 5x8 has a duplicate zero glyph.
    AddSeries(out, "font_10x30", "Digit", "Font 10x30", 0x0800bd10, 10, 30, 11, digits, pages);
    AddSeries(out, "font_10x12", "Digit", "Font 10x12", 0x0800bec8, 10, 12, 10, digits, pages);
    AddSeries(out, "font_5x8", "Digit", "Font 5x8", 0x0800bf90, 5, 8, 11, digits, pages);
    // 0x0800587a maps non-hex chip ID bytes to indices 16/17; retain those
    // actual S-shaped masks rather than silently omitting native glyph slots.
    AddSeries(out, "font_hex_5x8", "Glyph", "Font 5x8 Hex", 0x0800cc9c, 5, 8, 18, hex, pages);
    AddSeries(out, "fit_choice", "FIT choice", "Controls", 0x0800c7aa, 12, 22, 3, {"1", "2", "3"}, pages);
    AddSeries(out, "fit_selected", "FIT selected", "Controls", 0x0800c816, 18, 28, 3, {"1", "2", "3"}, pages);
    AddSeries(out, "tc_mode", "TC mode", "Labels", 0x0800c18a, 18, 12, 5, {"SS", "NI", "TI", "NC", "TC"}, pages);
    // 0x080044a0 loops over exactly five mode masks, index * 44 bytes.
    AddSeries(out, "tc_menu", "TC menu", "Labels", 0x0800c23e, 22, 14, 5, {"SS", "NI", "TI", "NC", "TCR"}, pages);
    // 0x0800427c selects indices 0..5, with width 11, height 30 and stride 44.
    AddSeries(out, "battery_vertical", "Vertical battery", "Animation", 0x0800c380, 11, 30, 6,
        {"(empty)", "(1 bar)", "(2 bars)", "(3 bars)", "(4 bars)", "(full)"}, pages);

    struct SingleDef {
        const char* id;
        const char* title;
        const char* group;
        uint32_t address;
        uint32_t width;
        uint32_t height;
    };
    // Each descriptor consumed by 0x08005278 is {x,y,width,height,pointer}.
    // Direct calls into 0x08008a38 provide the remaining dimensions. Shared
    // pointer/dimension pairs (selection frames, labels) appear only once.
    static constexpr SingleDef singles[] = {
        {"unit_w_large", "Unit W (large)", "Units", 0x0800c0d0, 18, 30},
        {"unit_v", "Unit V", "Units", 0x0800c118, 7, 8},
        {"unit_s", "Unit S", "Units", 0x0800c11f, 7, 8},
        {"unit_f_small", "Unit F (graph)", "Units", 0x0800c126, 7, 8},
        {"unit_c_small", "Unit C (graph)", "Units", 0x0800c12d, 7, 8},
        {"unit_f", "Unit degrees F", "Units", 0x0800c134, 18, 15},
        {"unit_c", "Unit degrees C", "Units", 0x0800c158, 18, 15},
        {"unit_w", "Unit W", "Units", 0x0800c17c, 7, 8},
        {"unit_ohm", "Unit Ohm", "Units", 0x0800c183, 7, 8},
        {"unit_v_medium", "Unit V (medium)", "Units", 0x0800c488, 10, 12},
        {"unit_ohm_medium", "Unit Ohm (medium)", "Units", 0x0800c9f8, 10, 12},
        {"decimal_small", "Decimal separator (small)", "Units", 0x0800bfea, 1, 2},
        {"decimal", "Decimal separator", "Units", 0x0800bfeb, 2, 2},
        {"decimal_large", "Decimal separator (large)", "Units", 0x0800bfed, 2, 5},
        {"logo_voopoo_small", "Logo VOOPOO (standby)", "Screens", 0x0800c31a, 46, 14},
        {"label_memory", "Memory M", "Labels", 0x0800c49c, 20, 30},
        {"menu_power", "Power menu", "Controls", 0x0800c4ec, 16, 22},
        {"menu_tc", "TC menu icon", "Controls", 0x0800c51c, 16, 22},
        {"menu_selection", "Menu selection border", "Controls", 0x0800c54c, 28, 18},
        {"menu_previous", "Previous menu arrow", "Controls", 0x0800c5a0, 8, 31},
        {"menu_next", "Next menu arrow", "Controls", 0x0800c5c0, 8, 31},
        {"menu_resistance", "Resistance SET menu", "Controls", 0x0800c5e0, 16, 22},
        {"menu_version", "Version menu", "Controls", 0x0800c610, 16, 22},
        {"menu_curve", "Temperature curve menu", "Controls", 0x0800c640, 16, 22},
        {"menu_serial", "SN menu", "Controls", 0x0800c670, 16, 22},
        {"menu_memory", "Memory 1-5 menu", "Controls", 0x0800c6a0, 16, 22},
        {"menu_focus", "Focused menu border", "Controls", 0x0800c6d0, 32, 30},
        {"logo_fit", "Logo FIT", "Labels", 0x0800c750, 30, 20},
        {"panel_chip_id", "GENE CHIP ID header", "Screens", 0x0800cb10, 94, 12},
        {"unit_w_memory", "Unit W (memory)", "Units", 0x0800cbcc, 18, 15},
        {"icon_curve", "Curve chart icon", "Controls", 0x0800cbf0, 18, 12},
        {"transition_line", "Transition line mask", "Animation", 0x0800ccf6, 128, 1},
        {"battery_frame", "Battery gauge frame", "Controls", 0x0800cd76, 38, 16},
        {"battery_segment", "Battery fill segment", "Controls", 0x0800cdc2, 6, 10},
        {"graph_top_marker", "Graph top marker", "Controls", 0x0800cdce, 5, 3},
        {"graph_right_marker", "Graph right marker", "Controls", 0x0800cdd3, 3, 5},
        {"unit_s_graph", "Unit S (graph)", "Units", 0x0800cdd6, 10, 12},
        {"profile_a", "Profile A", "Labels", 0x0800cdea, 14, 14},
        {"profile_b", "Profile B", "Labels", 0x0800ce06, 14, 14},
        {"graph_pixel", "Graph point mask", "Controls", 0x0800ce3c, 1, 1},
        {"graph_bar", "Graph bar mask", "Controls", 0x0800ce3d, 4, 1},
        {"graph_baseline", "Graph baseline marker", "Controls", 0x0800ce41, 6, 2},
        {"logo_voopoo", "Logo VOOPOO (boot)", "Screens", 0x0800d600, 128, 32},
        {"alert_new_coil", "New coil confirmation", "Alerts", 0x0800d800, 128, 32},
        {"alert_low_atomizer", "Low Atomizer", "Alerts", 0x0800da00, 128, 32},
        {"alert_high_atomizer", "High Atomizer", "Alerts", 0x0800dc00, 128, 32},
        {"alert_check_atomizer", "Check Atomizer", "Alerts", 0x0800de00, 128, 32},
        {"alert_atomizer_short", "Atomizer Short", "Alerts", 0x0800e000, 128, 32},
        {"alert_time_out", "Time Out", "Alerts", 0x0800e200, 128, 32},
        {"screen_lock", "Lock", "Screens", 0x0800e400, 128, 32},
        {"screen_unlock", "Unlock", "Screens", 0x0800e600, 128, 32},
        {"screen_off", "OFF", "Screens", 0x0800e800, 128, 32},
        {"alert_check_battery", "Check Battery", "Alerts", 0x0800ea00, 128, 32},
        {"alert_low_power", "Low Power", "Alerts", 0x0800ec00, 128, 32},
        {"alert_temp_high", "High Temp", "Alerts", 0x0800ee00, 128, 32},
    };
    for (const auto& item : singles) {
        out.push_back({item.id, item.title, item.group, item.address,
            item.width, item.height, 0, pages});
    }
    return out;
}

std::vector<AssetSpec> GetAllAssets(FirmwareModel model, FirmwareEdition edition) {
    if (const auto* extension = FirmwareExtensionFor(model)) return extension->assets(model, edition);
    if (model == FirmwareModel::Drag2) return GetDrag2Assets();
    if (edition == FirmwareEdition::Factory) return factory_catalog::Assets(model);
    std::vector<AssetSpec> out;
    std::vector<std::string> hex = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F" };
    std::vector<std::string> digits = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };

    // 1. font_6x10: 10 digits 0..9 (Address 0x080130D2, 10 bytes per glyph, total 100 bytes)
    AddSeries(out, "font_6x10", "Digit", "Font 6x10", 0x080130D2, 6, 10, 10, digits);

    // 2. font_7x13: 16 hex glyphs 0..F (Address 0x08013136, 13 bytes per glyph, total 208 bytes)
    AddSeries(out, "font_7x13", "Glyph", "Font 7x13", 0x08013136, 7, 13, 16, hex);

    // 3. font_11x20: 10 digits 0..9 (Address 0x080149F6, 40 bytes per glyph, total 400 bytes)
    AddSeries(out, "font_11x20", "Digit", "Font 11x20", 0x080149F6, 11, 20, 10, digits);

    // 4. font_19x40: 10 digits 0..9 (Address 0x08013206, 120 bytes per glyph, total 1200 bytes)
    AddSeries(out, "font_19x40", "Digit", "Font 19x40", 0x08013206, 19, 40, 10, digits);

    // 5. rank_icon: 8 items (Address 0x08014666, 24x24)
    const std::vector<std::string> ranks = { "Iron", "Bronze", "Silver", "Gold", "Platinum", "Diamond", "Master", "King" };
    AddSeries(out, "rank_icon", "Rank icon", "Score", 0x08014666, 24, 24, 8, ranks);

    // 6. rank_label: 8 items (Address 0x080148A6, 47x7)
    AddSeries(out, "rank_label", "Rank label", "Score", 0x080148A6, 47, 7, 8, ranks);

    // 7. status: SMART, RBA (Address 0x08014B86, 33x9)
    AddSeries(out, "status", "Status", "Labels", 0x08014B86, 33, 9, 2, { "SMART", "RBA" });

    // 8. toggle: off, on (Address 0x080104EF, 13x13)
    AddSeries(out, "toggle", "Toggle", "Controls", 0x080104EF, 13, 13, 2, { "off", "on" });

    // 9. mode: P&A, PRESS (Address 0x08010523, 28x11)
    AddSeries(out, "mode", "Mode", "Controls", 0x08010523, 28, 11, 2, { "P&A", "PRESS" });

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
        { "logo_voopoo", "Logo VOOPOO (header)", "Screens", 0x08010664, 80, 10 },
        { "panel_chip_id", "CHIP ID panel", "Screens", 0x080106C8, 74, 63 },
        { "panel_level_code", "LEVEL CODE panel", "Screens", 0x0801093E, 74, 46 },

        // Category 1: Alerts (dialogs & warning icons)
        { "alert_check_atomizer", "Check Atomizer", "Alerts", 0x080141D3, 56, 43 },
        { "icon_check_atomizer_tank", "Icon Check Atomizer Tank", "Alerts", 0x08014143, 20, 19 },
        { "icon_check_atomizer_base", "Icon Check Atomizer Base", "Alerts", 0x0801417C, 20, 29 },
        { "alert_atomizer_short", "Atomizer Short", "Alerts", 0x08013AAD, 56, 43 },
        { "icon_atomizer_short", "Icon Atomizer Short", "Alerts", 0x080139F9, 45, 30 },
        { "alert_time_out", "Time Out", "Alerts", 0x0801372A, 56, 43 },
        { "icon_time_out", "Icon Time Out (10s Cutoff)", "Alerts", 0x080136B6, 29, 29 },
        { "alert_temp_high", "High Temp", "Alerts", 0x08014388, 56, 43 },
        { "icon_temp_high", "Icon Temp High Waves", "Alerts", 0x08014300, 31, 34 },
        { "alert_check_battery", "Check Battery", "Alerts", 0x08013C4F, 56, 43 },
        { "icon_check_battery", "Icon Check Battery Bolt", "Alerts", 0x08013BDA, 20, 39 },
        { "alert_low_power", "Low Power", "Alerts", 0x08013DF1, 56, 43 },
        { "icon_low_power", "Icon Low Power Outline", "Alerts", 0x08013D7C, 20, 39 },
        { "alert_max_power", "Max Power", "Alerts", 0x080138CC, 56, 43 },
        { "icon_max_power", "Icon Max Power Arrow", "Alerts", 0x08013857, 23, 39 },
        { "alert_puff_cleared", "Puff Cleared", "Alerts", 0x08014539, 56, 43 },
        { "icon_puff_cleared", "Icon Puff Cleared Badge", "Alerts", 0x080144B5, 44, 22 },
        { "alert_press_fire_plus", "Press Fire+Plus to unlock", "Alerts", 0x08013F9A, 56, 43 },
        { "icon_lock_large", "Icon Lock (Closed Padlock)", "Alerts", 0x08013F1E, 29, 31 },
        { "icon_unlock_large", "Icon Unlock (Open Padlock)", "Alerts", 0x080140C7, 29, 31 },

        // Category 2: Boot & Branding
        { "boot_voopoo_animated", "Logo VOOPOO (animated boot)", "Screens", 0x08010B0A, 22, 136, AssetFormat::Rgb565 },
        { "screen_off", "OFF", "Screens", 0x0801226A, 39, 18, AssetFormat::Rgb565 },
        { "logo_drag_s", "Logo DRAG S (boot)", "Screens", 0x080127E6, 66, 18 },
        { "boot_gene_chip", "Logo GENE.TT & VOOPOO (boot)", "Screens", 0x08012888, 61, 33 },
        { "banner_activated", "Banner ACTIVATED (Rank)", "Labels", 0x0801048C, 72, 11 },

        // Category 3: Standby UI & Battery
        { "label_puff", "Label Puff", "Labels", 0x080105AA, 28, 10 },
        { "label_time", "Label Time", "Labels", 0x080105D2, 30, 10 },
        { "battery_frame", "Battery gauge frame", "Controls", 0x08012990, 47, 17 },
        { "battery_bolt", "Battery charging bolt", "Controls", 0x080130B8, 11, 13 },
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
    AddSeries(out, "meter", "Rank meter", "Score", 0x08014BE0, 24, 6, 8,
        {"(empty)", "(1 segment)", "(2 segments)", "(3 segments)", "(4 segments)", "(5 segments)", "(6 segments)", "(full)"});

    // 12. state: 6 animation frames (Address 0x08014C70, 46x5)
    AddSeries(out, "state", "Score stars", "Score", 0x08014C70, 46, 5, 6, {});

    if (model == FirmwareModel::DragX) {
        // The mode-control call site fills the P/A rectangle instead of drawing
        // masks in all seven X variants. Shared signatures map to these three
        // resource regions; the taller X wattage unit has its own bounds.
        for (auto it = out.begin(); it != out.end();) {
            if (it->id.rfind("mode_", 0) == 0) {
                it = out.erase(it);
                continue;
            }
            const uint32_t sourceAddress = it->address;
            if (sourceAddress < 0x08010523) it->address -= 80;
            else if (sourceAddress < 0x08010604) it->address -= 150;
            else it->address -= 148;
            if (it->id == "unit_w") {
                // 3106 draw at 0x0800adfe: pointer 0x080104d3, 17x15.
                // Its final nine rows also match the smaller S unit; mapping
                // only that shared suffix would silently omit six editable rows.
                it->address = 0x080104d3;
                it->height = 15;
            }
            if (it->id == "logo_drag_s") {
                it->id = "logo_drag_x";
                it->title = "Logo DRAG X (boot)";
            }
            ++it;
        }
    }
    return out;
}
