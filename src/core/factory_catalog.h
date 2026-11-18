#pragma once
#include "firmware_catalog.h"

// Native Factory resources. Addresses and dimensions come from each hardware
// renderer; 301x has a different UI, not a relocated Score or modern Factory UI.
// Zero addresses mean that hardware does not render this resource.
namespace factory_catalog {
struct Resource {
    const char* id;
    const char* title;
    const char* group;
    uint16_t width, height, count;
    AssetFormat format;
    uint16_t sourceCode;
    uint32_t foregroundOffset;
    uint32_t addresses[10];
};
constexpr uint16_t sCodes[] = { 0x3148, 0x3014, 0x3104, 0x3013, 0x3029, 0x3103, 0x3034, 0x3105, 0x314d };
constexpr uint16_t xCodes[] = { 0x3149, 0x3012, 0x302c, 0x3107, 0x3011, 0x302b, 0x3106, 0x3033, 0x3108, 0x314e };
constexpr Resource sResources[] = {
    { "font_6x10", "Glyph", "Font 6x10", 6, 10, 10, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x8010a10, 0x8013388, 0x80139b8, 0x8013378, 0x8010944, 0x8012d14, 0x8010bd0, 0x8013ffc, 0x80136f4 } },
    { "font_7x13", "Glyph", "Font 7x13", 7, 13, 16, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x8010a7e, 0x80133ec, 0x8013a26, 0x80133dc, 0x80109b2, 0x8012d82, 0x8010c3e, 0x801406a, 0x8013762 } },
    { "font_11x20", "Digit", "Font 11x20", 11, 20, 10, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x8011fae, 0x80149c1, 0x8014f56, 0x80149b1, 0x8011ee2, 0x80142b2, 0x801216e, 0x801559a, 0x8014c92 } },
    { "font_19x40", "Digit", "Font 19x40", 19, 40, 10, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x8010b4e, 0x80134bc, 0x8013af6, 0x80134ac, 0x8010a82, 0x8012e52, 0x8010d0e, 0x801413a, 0x8013832 } },
    { "factory_panel", "Mode panel", "Controls / Modern hardware", 40, 30, 2, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800dc4c, 0x0, 0x8010bf4, 0x0, 0x800db80, 0x800ff50, 0x800de0c, 0x8011238, 0x8010930 } },
    { "mode_factory", "Mode", "Controls / Modern hardware", 13, 14, 3, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800ddaa, 0x0, 0x8010d52, 0x0, 0x800dcde, 0x80100ae, 0x800df6a, 0x8011396, 0x8010a8e } },
    { "factory_header", "Header DRAG", "Labels / Modern hardware", 29, 9, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800dc28, 0x0, 0x8010bd0, 0x0, 0x800db5c, 0x800ff2c, 0x800dde8, 0x8011214, 0x801090c } },
    { "factory_panel_cap", "Mode panel right edge", "Controls / Modern hardware", 8, 30, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800dd78, 0x0, 0x8010d20, 0x0, 0x800dcac, 0x801007c, 0x800df38, 0x8011364, 0x8010a5c } },
    { "factory_graph_top", "Puff graph top edge", "Controls / Modern hardware", 80, 6, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800de2b, 0x0, 0x8010dd3, 0x0, 0x800dd5f, 0x801012f, 0x800dfeb, 0x8011417, 0x8010b0f } },
    { "factory_graph_bottom", "Puff graph bottom edge", "Controls / Modern hardware", 80, 8, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800de67, 0x0, 0x8010e0f, 0x0, 0x800dd9b, 0x801016b, 0x800e027, 0x8011453, 0x8010b4b } },
    { "label_time", "Time panel", "Labels / Modern hardware", 37, 10, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800deb7, 0x0, 0x8010e5f, 0x0, 0x800ddeb, 0x80101bb, 0x800e077, 0x80114a3, 0x8010b9b } },
    { "unit_s", "Unit S", "Units / Modern hardware", 6, 10, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800dee9, 0x0, 0x8010e91, 0x0, 0x800de1d, 0x80101ed, 0x800e0a9, 0x80114d5, 0x8010bcd } },
    { "factory_graph_side", "Puff graph side edge", "Controls / Modern hardware", 5, 30, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800def3, 0x0, 0x8010e9b, 0x0, 0x800de27, 0x80101f7, 0x800e0b3, 0x80114df, 0x8010bd7 } },
    { "factory_graph_divider", "Puff graph divider", "Controls / Modern hardware", 75, 5, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800df11, 0x0, 0x8010eb9, 0x0, 0x800de45, 0x8010215, 0x800e0d1, 0x80114fd, 0x8010bf5 } },
    { "label_puff", "Puff panel", "Labels / Modern hardware", 37, 15, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800df43, 0x0, 0x8010eeb, 0x0, 0x800de77, 0x8010247, 0x800e103, 0x801152f, 0x8010c27 } },
    { "unit_w", "Unit W", "Units / Modern hardware", 17, 15, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800ddfe, 0x0, 0x8010da6, 0x0, 0x800dd32, 0x8010102, 0x800dfbe, 0x80113ea, 0x8010ae2 } },
    { "unit_ohm", "Unit Ohm", "Units / Modern hardware", 7, 10, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800dd96, 0x0, 0x8010d3e, 0x0, 0x800dcca, 0x801009a, 0x800df56, 0x8011382, 0x8010a7a } },
    { "unit_v", "Unit V", "Units / Modern hardware", 7, 10, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800dda0, 0x0, 0x8010d48, 0x0, 0x800dcd4, 0x80100a4, 0x800df60, 0x801138c, 0x8010a84 } },
    { "decimal", "Decimal separator", "Units", 2, 10, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800dbb2, 0x800f062, 0x8010b5a, 0x800f052, 0x800dae6, 0x800feb6, 0x800dd72, 0x801119e, 0x8010896 } },
    { "battery_small_cap", "Small battery right edge", "Controls / Modern hardware", 2, 9, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800dc1f, 0x0, 0x8010bc7, 0x0, 0x800db53, 0x800ff23, 0x800dddf, 0x801120b, 0x8010903 } },
    { "logo_voopoo", "Logo VOOPOO (header)", "Screens", 80, 10, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800dfa2, 0x800f2de, 0x8010f4a, 0x800f2ce, 0x800ded6, 0x80102a6, 0x800e162, 0x801158e, 0x8010c86 } },
    { "panel_chip_id", "CHIP ID panel", "Screens", 74, 63, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800e006, 0x800f342, 0x8010fae, 0x800f332, 0x800df3a, 0x801030a, 0x800e1c6, 0x80115f2, 0x8010cea } },
    { "panel_level_code", "LEVEL CODE panel", "Screens", 74, 46, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x800e27c, 0x800f5b8, 0x8011224, 0x800f5a8, 0x800e1b0, 0x8010580, 0x800e43c, 0x8011868, 0x8010f60 } },
    { "alert_check_atomizer", "Check Atomizer", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x8011b1b, 0x8014489, 0x8014ac3, 0x8014479, 0x8011a4f, 0x8013e1f, 0x8011cdb, 0x8015107, 0x80147ff } },
    { "icon_check_atomizer_tank", "Icon Check Atomizer Tank", "Alerts", 20, 19, 1, AssetFormat::Mask1Bit, 0x3103, 0x37a0, { 0x8011a8b, 0x80143f9, 0x8014a33, 0x80143e9, 0x80119bf, 0x8013d8f, 0x8011c4b, 0x8015077, 0x801476f } },
    { "icon_check_atomizer_base", "Icon Check Atomizer Base", "Alerts", 20, 29, 1, AssetFormat::Mask1Bit, 0x3103, 0x37be, { 0x8011ac4, 0x8014432, 0x8014a6c, 0x8014422, 0x80119f8, 0x8013dc8, 0x8011c84, 0x80150b0, 0x80147a8 } },
    { "alert_atomizer_short", "Atomizer Short", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x80113f5, 0x8013d63, 0x801439d, 0x8013d53, 0x8011329, 0x80136f9, 0x80115b5, 0x80149e1, 0x80140d9 } },
    { "icon_atomizer_short", "Icon Atomizer Short", "Alerts", 45, 30, 1, AssetFormat::Mask1Bit, 0x3103, 0x3450, { 0x8011341, 0x8013caf, 0x80142e9, 0x8013c9f, 0x8011275, 0x8013645, 0x8011501, 0x801492d, 0x8014025 } },
    { "alert_time_out", "Time Out", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x8011072, 0x80139e0, 0x801401a, 0x80139d0, 0x8010fa6, 0x8013376, 0x8011232, 0x801465e, 0x8013d56 } },
    { "icon_time_out", "Icon Time Out (10s Cutoff)", "Alerts", 29, 29, 1, AssetFormat::Mask1Bit, 0x3103, 0x3a4e, { 0x8010ffe, 0x801396c, 0x8013fa6, 0x801395c, 0x8010f32, 0x8013302, 0x80111be, 0x80145ea, 0x8013ce2 } },
    { "alert_temp_high", "High Temp", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x8011cd0, 0x801463e, 0x8014c78, 0x801462e, 0x8011c04, 0x8013fd4, 0x8011e90, 0x80152bc, 0x80149b4 } },
    { "icon_temp_high", "Icon Temp High Waves", "Alerts", 31, 34, 1, AssetFormat::Mask1Bit, 0x3103, 0x39e0, { 0x8011c48, 0x80145b6, 0x8014bf0, 0x80145a6, 0x8011b7c, 0x8013f4c, 0x8011e08, 0x8015234, 0x801492c } },
    { "alert_check_battery", "Check Battery", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x8011597, 0x8013f05, 0x801453f, 0x8013ef5, 0x80114cb, 0x801389b, 0x8011757, 0x8014b83, 0x801427b } },
    { "icon_check_battery", "Icon Check Battery Bolt", "Alerts", 20, 39, 1, AssetFormat::Mask1Bit, 0x3103, 0x382e, { 0x8011522, 0x8013e90, 0x80144ca, 0x8013e80, 0x8011456, 0x8013826, 0x80116e2, 0x8014b0e, 0x8014206 } },
    { "alert_low_power", "Low Power", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x8011739, 0x80140a7, 0x80146e1, 0x8014097, 0x801166d, 0x8013a3d, 0x80118f9, 0x8014d25, 0x801441d } },
    { "icon_low_power", "Icon Low Power Outline", "Alerts", 20, 39, 1, AssetFormat::Mask1Bit, 0x3103, 0x3878, { 0x80116c4, 0x8014032, 0x801466c, 0x8014022, 0x80115f8, 0x80139c8, 0x8011884, 0x8014cb0, 0x80143a8 } },
    { "alert_max_power", "Max Power", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x8011214, 0x8013b82, 0x80141bc, 0x8013b72, 0x8011148, 0x8013518, 0x80113d4, 0x8014800, 0x8013ef8 } },
    { "icon_max_power", "Icon Max Power Arrow", "Alerts", 23, 39, 1, AssetFormat::Mask1Bit, 0x3103, 0x390a, { 0x801119f, 0x8013b0d, 0x8014147, 0x8013afd, 0x80110d3, 0x80134a3, 0x801135f, 0x801478b, 0x8013e83 } },
    { "alert_puff_cleared", "Puff Cleared", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x8011e81, 0x80147ef, 0x8014e29, 0x80147df, 0x8011db5, 0x8014185, 0x8012041, 0x801546d, 0x8014b65 } },
    { "icon_puff_cleared", "Icon Puff Cleared Badge", "Alerts", 44, 22, 1, AssetFormat::Mask1Bit, 0x3103, 0x3976, { 0x8011dfd, 0x801476b, 0x8014da5, 0x801475b, 0x8011d31, 0x8014101, 0x8011fbd, 0x80153e9, 0x8014ae1 } },
    { "alert_press_fire_plus", "Press Fire+Plus to unlock", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x80118e2, 0x8014250, 0x801488a, 0x8014240, 0x8011816, 0x8013be6, 0x8011aa2, 0x8014ece, 0x80145c6 } },
    { "icon_lock_large", "Icon Lock (Closed Padlock)", "Alerts", 29, 31, 1, AssetFormat::Mask1Bit, 0x3103, 0x3b08, { 0x8011866, 0x80141d4, 0x801480e, 0x80141c4, 0x801179a, 0x8013b6a, 0x8011a26, 0x8014e52, 0x801454a } },
    { "icon_unlock_large", "Icon Unlock (Open Padlock)", "Alerts", 29, 31, 1, AssetFormat::Mask1Bit, 0x3103, 0x3a98, { 0x8011a0f, 0x801437d, 0x80149b7, 0x801436d, 0x8011943, 0x8013d13, 0x8011bcf, 0x8014ffb, 0x80146f3 } },
    { "boot_voopoo_animated", "Logo VOOPOO (animated boot)", "Screens", 22, 136, 1, AssetFormat::Rgb565, 0x3103, 0x0, { 0x800e448, 0x800f784, 0x80113f0, 0x800f774, 0x800e37c, 0x801074c, 0x800e608, 0x8011a34, 0x801112c } },
    { "screen_off", "OFF", "Screens", 39, 18, 1, AssetFormat::Rgb565, 0x3103, 0x0, { 0x800fba8, 0x8010ee4, 0x8012b50, 0x8010ed4, 0x800fadc, 0x8011eac, 0x800fd68, 0x8013194, 0x801288c } },
    { "logo_drag_s", "Logo DRAG S (boot)", "Screens", 66, 18, 1, AssetFormat::Mask1Bit, 0x3103, 0x2f80, { 0x8010124, 0x8011460, 0x80130cc, 0x8011450, 0x8010058, 0x8012428, 0x80102e4, 0x8013710, 0x8012e08 } },
    { "boot_gene_chip", "Logo GENE.TT & VOOPOO (boot)", "Screens", 61, 33, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x80101c6, 0x8011502, 0x801316e, 0x80114f2, 0x80100fa, 0x80124ca, 0x8010386, 0x80137b2, 0x8012eaa } },
    { "battery_frame", "Battery gauge frame", "Controls", 47, 17, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x80102e2, 0x801161e, 0x801328a, 0x801160e, 0x8010216, 0x80125e6, 0x80104a2, 0x80138ce, 0x8012fc6 } },
    { "battery_bolt", "Battery charging bolt", "Controls", 11, 13, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x80109f6, 0x8011d32, 0x801399e, 0x8011d22, 0x801092a, 0x8012cfa, 0x8010bb6, 0x8013fe2, 0x80136da } },
    { "factory_legacy_status", "Status", "Labels / Legacy hardware", 33, 9, 2, AssetFormat::Mask1Bit, 0x3014, 0x0, { 0x0, 0x8014b51, 0x0, 0x8014b41, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_toggle_on", "Toggle on", "Controls / Legacy hardware", 13, 13, 1, AssetFormat::Mask1Bit, 0x3014, 0x50d8, { 0x0, 0x800f0e9, 0x0, 0x800f0d9, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_mode", "Mode", "Controls / Legacy hardware", 28, 11, 2, AssetFormat::Mask1Bit, 0x3014, 0x509c, { 0x0, 0x800f103, 0x0, 0x800f0f3, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_unit_w", "Unit W", "Units / Legacy hardware", 17, 9, 1, AssetFormat::Mask1Bit, 0x3014, 0x0, { 0x0, 0x800f15b, 0x0, 0x800f14b, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_unit_ohm", "Unit Ohm", "Units / Legacy hardware", 7, 10, 1, AssetFormat::Mask1Bit, 0x3014, 0x0, { 0x0, 0x800f176, 0x0, 0x800f166, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_unit_v", "Unit V", "Units / Legacy hardware", 7, 10, 1, AssetFormat::Mask1Bit, 0x3014, 0x0, { 0x0, 0x800f180, 0x0, 0x800f170, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_graph_top", "Puff graph top edge", "Controls / Legacy hardware", 80, 4, 1, AssetFormat::Mask1Bit, 0x3014, 0x4b1c, { 0x0, 0x800f18a, 0x0, 0x800f17a, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_label_puff", "Puff panel", "Labels / Legacy hardware", 80, 17, 1, AssetFormat::Mask1Bit, 0x3014, 0x0, { 0x0, 0x800f1b2, 0x0, 0x800f1a2, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_label_time", "Time panel", "Labels / Legacy hardware", 76, 13, 1, AssetFormat::Mask1Bit, 0x3014, 0x5242, { 0x0, 0x800f25c, 0x0, 0x800f24c, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_battery_charging", "Charging battery", "Controls / Legacy hardware", 49, 20, 1, AssetFormat::Mask1Bit, 0x3014, 0x2d52, { 0x0, 0x8011d4c, 0x0, 0x8011d3c, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_clock_h", "Clock hours label", "Labels / Legacy hardware", 13, 11, 1, AssetFormat::Mask1Bit, 0x3014, 0x313a, { 0x0, 0x801491c, 0x0, 0x801490c, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_clock_m", "Clock minutes label", "Labels / Legacy hardware", 13, 11, 1, AssetFormat::Mask1Bit, 0x3014, 0x313a, { 0x0, 0x8014932, 0x0, 0x8014922, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_clock_colon", "Clock separator", "Units / Legacy hardware", 6, 11, 1, AssetFormat::Mask1Bit, 0x3014, 0x0, { 0x0, 0x8014948, 0x0, 0x8014938, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_clock_prompt", "Clock setting instructions", "Labels / Legacy hardware", 80, 11, 1, AssetFormat::Mask1Bit, 0x3014, 0x313a, { 0x0, 0x8014953, 0x0, 0x8014943, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_banner_activated", "Banner ACTIVATED", "Labels / Legacy hardware", 72, 11, 1, AssetFormat::Mask1Bit, 0x3014, 0x3356, { 0x0, 0x800f06c, 0x0, 0x800f05c, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_minus_6x10", "Glyph - (unavailable value)", "Font 6x10 / Modern hardware", 6, 10, 1, AssetFormat::Mask1Bit, 0x3103, 0x0, { 0x8010a74, 0x0, 0x8013a1c, 0x0, 0x80109a8, 0x8012d78, 0x8010c34, 0x8014060, 0x8013758 } },
};
constexpr Resource xResources[] = {
    { "font_6x10", "Glyph", "Font 6x10", 6, 10, 10, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x80108f8, 0x8013202, 0x8010884, 0x8013940, 0x80131fa, 0x8010850, 0x8013924, 0x8010abc, 0x8013f88, 0x8013694 } },
    { "font_7x13", "Glyph", "Font 7x13", 7, 13, 16, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x8010966, 0x8013266, 0x80108f2, 0x80139ae, 0x801325e, 0x80108be, 0x8013992, 0x8010b2a, 0x8013ff6, 0x8013702 } },
    { "font_11x20", "Digit", "Font 11x20", 11, 20, 10, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x8011e96, 0x801483b, 0x8011e22, 0x8014ede, 0x8014833, 0x8011dee, 0x8014ec2, 0x801205a, 0x8015526, 0x8014c32 } },
    { "font_19x40", "Digit", "Font 19x40", 19, 40, 10, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x8010a36, 0x8013336, 0x80109c2, 0x8013a7e, 0x801332e, 0x801098e, 0x8013a62, 0x8010bfa, 0x80140c6, 0x80137d2 } },
    { "factory_panel", "Mode panel", "Controls / Modern hardware", 40, 30, 2, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800db34, 0x0, 0x800dac0, 0x8010b7c, 0x0, 0x800da8c, 0x8010b60, 0x800dcf8, 0x80111c4, 0x80108d0 } },
    { "mode_factory", "Mode", "Controls / Modern hardware", 13, 14, 3, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800dc92, 0x0, 0x800dc1e, 0x8010cda, 0x0, 0x800dbea, 0x8010cbe, 0x800de56, 0x8011322, 0x8010a2e } },
    { "factory_header", "Header DRAG", "Labels / Modern hardware", 29, 9, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800db10, 0x0, 0x800da9c, 0x8010b58, 0x0, 0x800da68, 0x8010b3c, 0x800dcd4, 0x80111a0, 0x80108ac } },
    { "factory_panel_cap", "Mode panel right edge", "Controls / Modern hardware", 8, 30, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800dc60, 0x0, 0x800dbec, 0x8010ca8, 0x0, 0x800dbb8, 0x8010c8c, 0x800de24, 0x80112f0, 0x80109fc } },
    { "factory_graph_top", "Puff graph top edge", "Controls / Modern hardware", 80, 6, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800dd13, 0x0, 0x800dc9f, 0x8010d5b, 0x0, 0x800dc6b, 0x8010d3f, 0x800ded7, 0x80113a3, 0x8010aaf } },
    { "factory_graph_bottom", "Puff graph bottom edge", "Controls / Modern hardware", 80, 8, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800dd4f, 0x0, 0x800dcdb, 0x8010d97, 0x0, 0x800dca7, 0x8010d7b, 0x800df13, 0x80113df, 0x8010aeb } },
    { "label_time", "Time panel", "Labels / Modern hardware", 37, 10, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800dd9f, 0x0, 0x800dd2b, 0x8010de7, 0x0, 0x800dcf7, 0x8010dcb, 0x800df63, 0x801142f, 0x8010b3b } },
    { "unit_s", "Unit S", "Units / Modern hardware", 6, 10, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800ddd1, 0x0, 0x800dd5d, 0x8010e19, 0x0, 0x800dd29, 0x8010dfd, 0x800df95, 0x8011461, 0x8010b6d } },
    { "factory_graph_side", "Puff graph side edge", "Controls / Modern hardware", 5, 30, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800dddb, 0x0, 0x800dd67, 0x8010e23, 0x0, 0x800dd33, 0x8010e07, 0x800df9f, 0x801146b, 0x8010b77 } },
    { "factory_graph_divider", "Puff graph divider", "Controls / Modern hardware", 75, 5, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800ddf9, 0x0, 0x800dd85, 0x8010e41, 0x0, 0x800dd51, 0x8010e25, 0x800dfbd, 0x8011489, 0x8010b95 } },
    { "label_puff", "Puff panel", "Labels / Modern hardware", 37, 15, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800de2b, 0x0, 0x800ddb7, 0x8010e73, 0x0, 0x800dd83, 0x8010e57, 0x800dfef, 0x80114bb, 0x8010bc7 } },
    { "unit_w", "Unit W", "Units", 17, 15, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800dce6, 0x800efc3, 0x800dc72, 0x8010d2e, 0x800efbb, 0x800dc3e, 0x8010d12, 0x800deaa, 0x8011376, 0x8010a82 } },
    { "unit_ohm", "Unit Ohm", "Units / Modern hardware", 7, 10, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800dc7e, 0x0, 0x800dc0a, 0x8010cc6, 0x0, 0x800dbd6, 0x8010caa, 0x800de42, 0x801130e, 0x8010a1a } },
    { "unit_v", "Unit V", "Units / Modern hardware", 7, 10, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800dc88, 0x0, 0x800dc14, 0x8010cd0, 0x0, 0x800dbe0, 0x8010cb4, 0x800de4c, 0x8011318, 0x8010a24 } },
    { "decimal", "Decimal separator", "Units", 2, 10, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800da9a, 0x800ef22, 0x800da26, 0x8010ae2, 0x800ef1a, 0x800d9f2, 0x8010ac6, 0x800dc5e, 0x801112a, 0x8010836 } },
    { "battery_small_cap", "Small battery right edge", "Controls / Modern hardware", 2, 9, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800db07, 0x0, 0x800da93, 0x8010b4f, 0x0, 0x800da5f, 0x8010b33, 0x800dccb, 0x8011197, 0x80108a3 } },
    { "logo_voopoo", "Logo VOOPOO (header)", "Screens", 80, 10, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800de8a, 0x800f158, 0x800de16, 0x8010ed2, 0x800f150, 0x800dde2, 0x8010eb6, 0x800e04e, 0x801151a, 0x8010c26 } },
    { "panel_chip_id", "CHIP ID panel", "Screens", 74, 63, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800deee, 0x800f1bc, 0x800de7a, 0x8010f36, 0x800f1b4, 0x800de46, 0x8010f1a, 0x800e0b2, 0x801157e, 0x8010c8a } },
    { "panel_level_code", "LEVEL CODE panel", "Screens", 74, 46, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x800e164, 0x800f432, 0x800e0f0, 0x80111ac, 0x800f42a, 0x800e0bc, 0x8011190, 0x800e328, 0x80117f4, 0x8010f00 } },
    { "alert_check_atomizer", "Check Atomizer", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x8011a03, 0x8014303, 0x801198f, 0x8014a4b, 0x80142fb, 0x801195b, 0x8014a2f, 0x8011bc7, 0x8015093, 0x801479f } },
    { "icon_check_atomizer_tank", "Icon Check Atomizer Tank", "Alerts", 20, 19, 1, AssetFormat::Mask1Bit, 0x3106, 0x3600, { 0x8011973, 0x8014273, 0x80118ff, 0x80149bb, 0x801426b, 0x80118cb, 0x801499f, 0x8011b37, 0x8015003, 0x801470f } },
    { "icon_check_atomizer_base", "Icon Check Atomizer Base", "Alerts", 20, 29, 1, AssetFormat::Mask1Bit, 0x3106, 0x361e, { 0x80119ac, 0x80142ac, 0x8011938, 0x80149f4, 0x80142a4, 0x8011904, 0x80149d8, 0x8011b70, 0x801503c, 0x8014748 } },
    { "alert_atomizer_short", "Atomizer Short", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x80112dd, 0x8013bdd, 0x8011269, 0x8014325, 0x8013bd5, 0x8011235, 0x8014309, 0x80114a1, 0x801496d, 0x8014079 } },
    { "icon_atomizer_short", "Icon Atomizer Short", "Alerts", 45, 30, 1, AssetFormat::Mask1Bit, 0x3106, 0x32ac, { 0x8011229, 0x8013b29, 0x80111b5, 0x8014271, 0x8013b21, 0x8011181, 0x8014255, 0x80113ed, 0x80148b9, 0x8013fc5 } },
    { "alert_time_out", "Time Out", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x8010f5a, 0x801385a, 0x8010ee6, 0x8013fa2, 0x8013852, 0x8010eb2, 0x8013f86, 0x801111e, 0x80145ea, 0x8013cf6 } },
    { "icon_time_out", "Icon Time Out (10s Cutoff)", "Alerts", 29, 29, 1, AssetFormat::Mask1Bit, 0x3106, 0x38ae, { 0x8010ee6, 0x80137e6, 0x8010e72, 0x8013f2e, 0x80137de, 0x8010e3e, 0x8013f12, 0x80110aa, 0x8014576, 0x8013c82 } },
    { "alert_temp_high", "High Temp", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x8011bb8, 0x80144b8, 0x8011b44, 0x8014c00, 0x80144b0, 0x8011b10, 0x8014be4, 0x8011d7c, 0x8015248, 0x8014954 } },
    { "icon_temp_high", "Icon Temp High Waves", "Alerts", 31, 34, 1, AssetFormat::Mask1Bit, 0x3106, 0x3840, { 0x8011b30, 0x8014430, 0x8011abc, 0x8014b78, 0x8014428, 0x8011a88, 0x8014b5c, 0x8011cf4, 0x80151c0, 0x80148cc } },
    { "alert_check_battery", "Check Battery", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x801147f, 0x8013d7f, 0x801140b, 0x80144c7, 0x8013d77, 0x80113d7, 0x80144ab, 0x8011643, 0x8014b0f, 0x801421b } },
    { "icon_check_battery", "Icon Check Battery Bolt", "Alerts", 20, 39, 1, AssetFormat::Mask1Bit, 0x3106, 0x368e, { 0x801140a, 0x8013d0a, 0x8011396, 0x8014452, 0x8013d02, 0x8011362, 0x8014436, 0x80115ce, 0x8014a9a, 0x80141a6 } },
    { "alert_low_power", "Low Power", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x8011621, 0x8013f21, 0x80115ad, 0x8014669, 0x8013f19, 0x8011579, 0x801464d, 0x80117e5, 0x8014cb1, 0x80143bd } },
    { "icon_low_power", "Icon Low Power Outline", "Alerts", 20, 39, 1, AssetFormat::Mask1Bit, 0x3106, 0x36d8, { 0x80115ac, 0x8013eac, 0x8011538, 0x80145f4, 0x8013ea4, 0x8011504, 0x80145d8, 0x8011770, 0x8014c3c, 0x8014348 } },
    { "alert_max_power", "Max Power", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x80110fc, 0x80139fc, 0x8011088, 0x8014144, 0x80139f4, 0x8011054, 0x8014128, 0x80112c0, 0x801478c, 0x8013e98 } },
    { "icon_max_power", "Icon Max Power Arrow", "Alerts", 23, 39, 1, AssetFormat::Mask1Bit, 0x3106, 0x376a, { 0x8011087, 0x8013987, 0x8011013, 0x80140cf, 0x801397f, 0x8010fdf, 0x80140b3, 0x801124b, 0x8014717, 0x8013e23 } },
    { "alert_puff_cleared", "Puff Cleared", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x8011d69, 0x8014669, 0x8011cf5, 0x8014db1, 0x8014661, 0x8011cc1, 0x8014d95, 0x8011f2d, 0x80153f9, 0x8014b05 } },
    { "icon_puff_cleared", "Icon Puff Cleared Badge", "Alerts", 44, 22, 1, AssetFormat::Mask1Bit, 0x3106, 0x37d6, { 0x8011ce5, 0x80145e5, 0x8011c71, 0x8014d2d, 0x80145dd, 0x8011c3d, 0x8014d11, 0x8011ea9, 0x8015375, 0x8014a81 } },
    { "alert_press_fire_plus", "Press Fire+Plus to unlock", "Alerts", 56, 43, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x80117ca, 0x80140ca, 0x8011756, 0x8014812, 0x80140c2, 0x8011722, 0x80147f6, 0x801198e, 0x8014e5a, 0x8014566 } },
    { "icon_lock_large", "Icon Lock (Closed Padlock)", "Alerts", 29, 31, 1, AssetFormat::Mask1Bit, 0x3106, 0x3968, { 0x801174e, 0x801404e, 0x80116da, 0x8014796, 0x8014046, 0x80116a6, 0x801477a, 0x8011912, 0x8014dde, 0x80144ea } },
    { "icon_unlock_large", "Icon Unlock (Open Padlock)", "Alerts", 29, 31, 1, AssetFormat::Mask1Bit, 0x3106, 0x38f8, { 0x80118f7, 0x80141f7, 0x8011883, 0x801493f, 0x80141ef, 0x801184f, 0x8014923, 0x8011abb, 0x8014f87, 0x8014693 } },
    { "boot_voopoo_animated", "Logo VOOPOO (animated boot)", "Screens", 22, 136, 1, AssetFormat::Rgb565, 0x3106, 0x0, { 0x800e330, 0x800f5fe, 0x800e2bc, 0x8011378, 0x800f5f6, 0x800e288, 0x801135c, 0x800e4f4, 0x80119c0, 0x80110cc } },
    { "screen_off", "OFF", "Screens", 39, 18, 1, AssetFormat::Rgb565, 0x3106, 0x0, { 0x800fa90, 0x8010d5e, 0x800fa1c, 0x8012ad8, 0x8010d56, 0x800f9e8, 0x8012abc, 0x800fc54, 0x8013120, 0x801282c } },
    { "logo_drag_x", "Logo DRAG X (boot)", "Screens", 66, 18, 1, AssetFormat::Mask1Bit, 0x3106, 0x2d8c, { 0x801000c, 0x80112da, 0x800ff98, 0x8013054, 0x80112d2, 0x800ff64, 0x8013038, 0x80101d0, 0x801369c, 0x8012da8 } },
    { "boot_gene_chip", "Logo GENE.TT & VOOPOO (boot)", "Screens", 61, 33, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x80100ae, 0x801137c, 0x801003a, 0x80130f6, 0x8011374, 0x8010006, 0x80130da, 0x8010272, 0x801373e, 0x8012e4a } },
    { "battery_frame", "Battery gauge frame", "Controls", 47, 17, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x80101ca, 0x8011498, 0x8010156, 0x8013212, 0x8011490, 0x8010122, 0x80131f6, 0x801038e, 0x801385a, 0x8012f66 } },
    { "battery_bolt", "Battery charging bolt", "Controls", 11, 13, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x80108de, 0x8011bac, 0x801086a, 0x8013926, 0x8011ba4, 0x8010836, 0x801390a, 0x8010aa2, 0x8013f6e, 0x801367a } },
    { "factory_legacy_status", "Status", "Labels / Legacy hardware", 33, 9, 2, AssetFormat::Mask1Bit, 0x3012, 0x0, { 0x0, 0x80149cb, 0x0, 0x0, 0x80149c3, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_toggle_on", "Toggle on", "Controls / Legacy hardware", 13, 13, 1, AssetFormat::Mask1Bit, 0x3012, 0x4ff8, { 0x0, 0x800efa9, 0x0, 0x0, 0x800efa1, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_unit_ohm", "Unit Ohm", "Units / Legacy hardware", 7, 10, 1, AssetFormat::Mask1Bit, 0x3012, 0x0, { 0x0, 0x800eff0, 0x0, 0x0, 0x800efe8, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_unit_v", "Unit V", "Units / Legacy hardware", 7, 10, 1, AssetFormat::Mask1Bit, 0x3012, 0x0, { 0x0, 0x800effa, 0x0, 0x0, 0x800eff2, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_graph_top", "Puff graph top edge", "Controls / Legacy hardware", 80, 4, 1, AssetFormat::Mask1Bit, 0x3012, 0x4a40, { 0x0, 0x800f004, 0x0, 0x0, 0x800effc, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_label_puff", "Puff panel", "Labels / Legacy hardware", 80, 17, 1, AssetFormat::Mask1Bit, 0x3012, 0x0, { 0x0, 0x800f02c, 0x0, 0x0, 0x800f024, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_label_time", "Time panel", "Labels / Legacy hardware", 76, 13, 1, AssetFormat::Mask1Bit, 0x3012, 0x512e, { 0x0, 0x800f0d6, 0x0, 0x0, 0x800f0ce, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_battery_charging", "Charging battery", "Controls / Legacy hardware", 49, 20, 1, AssetFormat::Mask1Bit, 0x3012, 0x2c9a, { 0x0, 0x8011bc6, 0x0, 0x0, 0x8011bbe, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_clock_h", "Clock hours label", "Labels / Legacy hardware", 13, 11, 1, AssetFormat::Mask1Bit, 0x3012, 0x3082, { 0x0, 0x8014796, 0x0, 0x0, 0x801478e, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_clock_m", "Clock minutes label", "Labels / Legacy hardware", 13, 11, 1, AssetFormat::Mask1Bit, 0x3012, 0x3082, { 0x0, 0x80147ac, 0x0, 0x0, 0x80147a4, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_clock_colon", "Clock separator", "Units / Legacy hardware", 6, 11, 1, AssetFormat::Mask1Bit, 0x3012, 0x0, { 0x0, 0x80147c2, 0x0, 0x0, 0x80147ba, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_clock_prompt", "Clock setting instructions", "Labels / Legacy hardware", 80, 11, 1, AssetFormat::Mask1Bit, 0x3012, 0x3082, { 0x0, 0x80147cd, 0x0, 0x0, 0x80147c5, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_legacy_banner_activated", "Banner ACTIVATED", "Labels / Legacy hardware", 72, 11, 1, AssetFormat::Mask1Bit, 0x3012, 0x329e, { 0x0, 0x800ef2c, 0x0, 0x0, 0x800ef24, 0x0, 0x0, 0x0, 0x0, 0x0 } },
    { "factory_minus_6x10", "Glyph - (unavailable value)", "Font 6x10 / Modern hardware", 6, 10, 1, AssetFormat::Mask1Bit, 0x3106, 0x0, { 0x801095c, 0x0, 0x80108e8, 0x80139a4, 0x0, 0x80108b4, 0x8013988, 0x8010b20, 0x8013fec, 0x80136f8 } },
};

inline std::vector<AssetSpec> Assets(FirmwareModel model) {
    const bool isX = model == FirmwareModel::DragX;
    const auto* resources = isX ? xResources : sResources;
    const size_t count = isX ? sizeof(xResources) / sizeof(*xResources) : sizeof(sResources) / sizeof(*sResources);
    const auto* codes = isX ? xCodes : sCodes;
    const size_t records = isX ? sizeof(xCodes) / sizeof(*xCodes) : sizeof(sCodes) / sizeof(*sCodes);
    std::vector<AssetSpec> out;
    out.reserve(isX ? 109 : 112);
    for (size_t r = 0; r < count; ++r) {
        const auto& resource = resources[r];
        for (size_t ordinal = 0; ordinal < resource.count; ++ordinal) {
            AssetSpec spec;
            spec.id = resource.id;
            spec.title = resource.title;
            if (spec.id.rfind("font_", 0) == 0) {
                spec.title = ordinal < 10 ? "Digit " : "Glyph ";
                spec.title += "0123456789ABCDEF"[ordinal];
            } else if (spec.id == "factory_panel") {
                spec.title = ordinal == 0 ? "Mode panel A&P" : "Mode panel P";
            } else if (spec.id == "mode_factory") {
                constexpr const char* labels[] = { "Lock", "Mode SMART", "Mode RBA" };
                spec.title = labels[ordinal];
            } else if (spec.id == "factory_legacy_status") {
                spec.title = ordinal == 0 ? "Status SMART" : "Status RBA";
            } else if (spec.id == "factory_legacy_mode") {
                spec.title = ordinal == 0 ? "Mode P&A" : "Mode PRESS";
            }
            if (resource.count > 1) {
                spec.id += ordinal < 10 ? "_0" : "_";
                spec.id += std::to_string(ordinal);
            }
            spec.group = resource.group;
            spec.width = resource.width;
            spec.height = resource.height;
            spec.ordinal = ordinal;
            spec.format = resource.format;
            spec.sourceCode = resource.sourceCode;
            spec.foregroundOffset = resource.foregroundOffset;
            for (size_t record = 0; record < records; ++record) {
                if (!resource.addresses[record]) continue;
                const uint32_t address = resource.addresses[record] + static_cast<uint32_t>(ordinal * spec.byte_len());
                spec.recordAddresses.emplace_back(codes[record], address);
                if (codes[record] == spec.sourceCode) spec.address = address;
            }
            out.push_back(std::move(spec));
        }
    }
    return out;
}
}
