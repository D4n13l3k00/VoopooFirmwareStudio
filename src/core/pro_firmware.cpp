// Native DRAG S PRO Factory / TC and DRAG X PRO TC profiles.
// Bitmap dimensions and color producers were recovered from each Thumb renderer.
// Hardware-keyed addresses and baked per-site stock bytes survive asset/color edits.
#include "pro_firmware.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

namespace ProFirmware {
namespace {
constexpr uint32_t kBase = 0x08003000;
struct ColorSite { uint32_t offset, stock; uint16_t value; uint8_t role; int8_t reg; };
struct FontBank { uint32_t address; uint16_t width, height, count; uint8_t selector; };
struct Resource { const char* title; const char* group; uint32_t address; uint16_t width, height; uint8_t role; bool rgb, mode; uint32_t foreground; };
struct Hardware { uint16_t code; uint32_t size, sp; uint8_t profile; uint32_t bitmap, digit, tableReference; uint8_t guards[24]; const ColorSite* sites; size_t siteCount; const FontBank* banks; size_t bankCount; const Resource* resources; size_t resourceCount; };
constexpr const char* kRoleIds[] = { "pro_mode_smart_dim", "pro_mode_smart", "pro_mode_rba_dim", "pro_mode_rba", "pro_mode_third_dim", "pro_mode_third", "pro_text", "pro_digits_wattage", "pro_digits_stats", "pro_warning_red", "pro_warning_orange", "pro_alert_cyan", "pro_unlock_green", "pro_highlight_yellow", "pro_frame_gray" };
constexpr const char* kRoleLabels[] = { "SMART dim", "SMART yellow", "RBA dim", "RBA cyan", "Third mode dim", "Third mode / TC green", "Labels and dialogs", "Power digits", "Statistics digits", "Warning red", "Warning orange", "Atomizer cyan", "Unlock green", "Boot and service yellow", "Native frame gray" };
constexpr ColorSite ksf3119Sites[] = {
    { 0x1e58, 0x72e0f64f, 0xffe0, 13, 2 },
    { 0x1e7e, 0x72fff64f, 0xffff, 6, 2 },
    { 0x254c, 0x4168f44f, 0xe800, 9, 1 },
    { 0x2564, 0x71fff64f, 0xffff, 6, 1 },
    { 0x25b0, 0x4168f44f, 0xe800, 9, 1 },
    { 0x25c8, 0x71fff64f, 0xffff, 6, 1 },
    { 0x2684, 0x76fff64f, 0xffff, 8, 6 },
    { 0x285c, 0x4168f44f, 0xe800, 9, 1 },
    { 0x2874, 0x715ff240, 0x75f, 11, 1 },
    { 0x288c, 0x71fff64f, 0xffff, 6, 1 },
    { 0x28de, 0x4168f44f, 0xe800, 9, 1 },
    { 0x28f6, 0x71fff64f, 0xffff, 6, 1 },
    { 0x2946, 0x4168f44f, 0xe800, 9, 1 },
    { 0x295e, 0x71fff64f, 0xffff, 6, 1 },
    { 0x29ae, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x29c6, 0x71fff64f, 0xffff, 6, 1 },
    { 0x2a3a, 0x73fff64f, 0xffff, 6, 3 },
    { 0x2c2e, 0x73fff64f, 0xffff, 6, 3 },
    { 0x2d7e, 0x71e0f64f, 0xffe0, 13, 1 },
    { 0x2d96, 0x71fff64f, 0xffff, 6, 1 },
    { 0x2e06, 0x73fff64f, 0xffff, 6, 3 },
    { 0x2f54, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x2f6c, 0x71fff64f, 0xffff, 6, 1 },
    { 0x2fba, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x2fd2, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3038, 0x71e0f645, 0x5fe0, 12, 1 },
    { 0x3050, 0x70fff64f, 0xffff, 6, 0 },
    { 0x30b4, 0x4168f44f, 0xe800, 9, 1 },
    { 0x30cc, 0x70fff64f, 0xffff, 6, 0 },
    { 0x3456, 0x71fff64f, 0xffff, 6, 1 },
    { 0x37ba, 0x4410f248, 0x8410, 14, 4 },
    { 0x3930, 0x76fff64f, 0xffff, 8, 6 },
    { 0x3ad8, 0x74fff64f, 0xffff, 8, 4 },
    { 0x3c3e, 0x76fff64f, 0xffff, 8, 6 },
    { 0x3db6, 0x77fff64f, 0xffff, 8, 7 },
    { 0x3ee0, 0x78fff64f, 0xffff, 6, 8 },
    { 0x55dc, 0x4868f44f, 0xe800, 9, 8 },
    { 0x5646, 0x71fff64f, 0xffff, 7, 1 },
    { 0x5692, 0x71fff64f, 0xffff, 7, 1 },
    { 0x56f2, 0x72fff64f, 0xffff, 6, 2 },
    { 0x574a, 0x4968f44f, 0xe800, 9, 9 },
    { 0x57a6, 0x70fff64f, 0xffff, 7, 0 },
    { 0x57f0, 0x71fff64f, 0xffff, 7, 1 },
    { 0x583c, 0x71fff64f, 0xffff, 7, 1 },
    { 0x589e, 0x72fff64f, 0xffff, 6, 2 },
    { 0x7598, 0x7ae0f64f, 0xffe0, 13, 10 },
    { 0x7638, 0x76fff64f, 0xffff, 8, 6 },
    { 0xc13c, 0x7c00, 0x7c00, 0, -1 },
    { 0xc13e, 0xffe0, 0xffe0, 1, -1 },
    { 0xc140, 0x3cf, 0x3cf, 2, -1 },
    { 0xc142, 0x7ff, 0x7ff, 3, -1 },
    { 0xc144, 0x9844, 0x9844, 4, -1 },
    { 0xc146, 0xe800, 0xe800, 5, -1 },
};
constexpr FontBank ksf3119Banks[] = {
    { 0x8014bf8, 5, 8, 11, 0 },
    { 0x8013186, 6, 10, 11, 6 },
    { 0x80131f4, 7, 13, 16, 7 },
    { 0x8014a68, 11, 20, 10, 11 },
    { 0x8014c50, 10, 6, 11, 12 },
    { 0x8014f04, 23, 13, 2, 13 },
    { 0x8014cd4, 30, 14, 10, 14 },
    { 0x8014f52, 23, 6, 1, 15 },
    { 0x80132c4, 19, 40, 10, 19 },
};
constexpr Resource ksf3119Resources[] = {
    { "Logo DRAG (standby)", "Screens", 0x800f1ab, 30, 10, 6, false, false, 0x3456 },
    { "Battery terminal", "Controls", 0x800f1d3, 2, 9, 6, false, false, 0x3ee0 },
    { "Unit W", "Units", 0x800f1dc, 17, 15, 6, false, false, 0x589e },
    { "Resistance label R", "Labels", 0x800f209, 38, 13, 1, false, true, 0xc142 },
    { "Decimal separator (statistics)", "Units", 0x800f24a, 6, 10, 8, false, false, 0x3ad8 },
    { "Unit Ohm", "Units", 0x800f254, 16, 10, 8, false, false, 0x3ad8 },
    { "Voltage panel", "Labels", 0x800f268, 41, 17, 1, false, true, 0xc142 },
    { "Decimal separator (statistics)", "Units", 0x800f2ce, 6, 10, 8, false, false, 0x3db6 },
    { "Time panel top edge", "Labels", 0x800f2d8, 39, 7, 1, false, true, 0xc142 },
    { "Unit V", "Units", 0x800f2fb, 10, 10, 8, false, false, 0x3db6 },
    { "Time panel", "Labels", 0x800f30f, 41, 17, 1, false, true, 0xc142 },
    { "Puff panel top edge", "Labels", 0x800f375, 39, 7, 1, false, true, 0xc142 },
    { "Decimal separator (statistics)", "Units", 0x800f398, 6, 10, 8, false, false, 0x3c3e },
    { "Unit S", "Units", 0x800f3a2, 6, 10, 8, false, false, 0x3c3e },
    { "Puff panel", "Labels", 0x800f3ac, 41, 13, 1, false, true, 0xc13e },
    { "Menu SETTING", "Controls", 0x800f42c, 80, 8, 6, false, false, 0x2a3a },
    { "SWITCH option outline", "Controls", 0x800f47c, 80, 19, 1, false, true, 0xc142 },
    { "SWITCH option focus", "Controls", 0x800f53a, 80, 19, 1, false, true, 0xc13e },
    { "SWITCH option selected", "Controls", 0x800f5f8, 80, 19, 1, false, true, 0xc13e },
    { "MODE option outline", "Controls", 0x800f6b6, 80, 19, 1, false, true, 0xc13e },
    { "MODE option focus", "Controls", 0x800f774, 80, 19, 1, false, true, 0xc142 },
    { "MODE option selected", "Controls", 0x800f832, 80, 19, 1, false, true, 0xc142 },
    { "EXIT option outline", "Controls", 0x800f8f0, 80, 19, 1, false, true, 0xc13e },
    { "EXIT option focus", "Controls", 0x800f9ae, 80, 19, 1, false, true, 0xc142 },
    { "EXIT option selected", "Controls", 0x800fa6c, 80, 19, 1, false, true, 0xc142 },
    { "Menu SWITCH", "Controls", 0x800fb2a, 80, 7, 6, false, false, 0x2c2e },
    { "Lock switch outline", "Controls", 0x800fb70, 80, 25, 1, false, true, 0xc142 },
    { "Lock switch focus", "Controls", 0x800fc6a, 80, 25, 1, false, true, 0xc13e },
    { "Lock switch selected", "Controls", 0x800fd64, 80, 25, 1, false, true, 0xc13e },
    { "Power switch outline", "Controls", 0x800fe5e, 80, 25, 1, false, true, 0xc13e },
    { "Power switch focus", "Controls", 0x800ff58, 80, 25, 1, false, true, 0xc142 },
    { "Power switch selected", "Controls", 0x8010052, 80, 25, 1, false, true, 0xc142 },
    { "Menu MODE", "Controls", 0x801014c, 80, 8, 6, false, false, 0x2e06 },
    { "SMART option outline", "Controls", 0x801019c, 80, 19, 1, false, true, 0xc142 },
    { "SMART option focus", "Controls", 0x801025a, 80, 19, 1, false, true, 0xc13e },
    { "SMART option selected", "Controls", 0x8010318, 80, 19, 1, false, true, 0xc13e },
    { "RBA option outline", "Controls", 0x80103d6, 80, 19, 1, false, true, 0xc13e },
    { "RBA option focus", "Controls", 0x8010494, 80, 19, 1, false, true, 0xc142 },
    { "RBA option selected", "Controls", 0x8010552, 80, 19, 1, false, true, 0xc142 },
    { "Logo VOOPOO (service)", "Screens", 0x8010610, 80, 10, 13, false, false, 0x7598 },
    { "CHIP ID panel", "Screens", 0x8010674, 74, 63, 13, false, false, 0x7598 },
    { "LEVEL CODE panel", "Screens", 0x80108ea, 74, 46, 13, false, false, 0x7598 },
    { "Logo VOOPOO (boot, rotated clockwise)", "Screens", 0x8010ab6, 22, 136, 6, true, false, 0x1f38 },
    { "OFF", "Screens", 0x8012216, 39, 18, 6, true, false, 0x1f2c },
    { "Logo DRAG S PRO (boot)", "Screens", 0x8012792, 80, 37, 13, false, false, 0x1e58 },
    { "GENE.FAN version (boot)", "Screens", 0x8012904, 80, 32, 6, false, false, 0x1e7e },
    { "Battery charging (empty)", "Animation", 0x8012a44, 47, 17, 8, false, false, 0x2684 },
    { "Battery charging (1 segment)", "Animation", 0x8012aaa, 47, 17, 8, false, false, 0x2684 },
    { "Battery charging (2 segments)", "Animation", 0x8012b10, 47, 17, 8, false, false, 0x2684 },
    { "Battery charging (3 segments)", "Animation", 0x8012b76, 47, 17, 8, false, false, 0x2684 },
    { "Battery charging (4 segments)", "Animation", 0x8012bdc, 47, 17, 8, false, false, 0x2684 },
    { "Battery charging (full)", "Animation", 0x8012c42, 47, 17, 8, false, false, 0x2684 },
    { "CHARGING", "Screens", 0x8012ca8, 61, 10, 6, true, false, 0x27fc },
    { "Battery charging bolt", "Controls", 0x801316c, 11, 13, 8, false, false, 0x2684 },
    { "Icon Time Out", "Alerts", 0x8013774, 29, 29, 10, false, false, 0x2fba },
    { "Time Out", "Alerts", 0x80137e8, 56, 43, 6, false, false, 0x2fd2 },
    { "Icon Max Power", "Alerts", 0x8013915, 23, 39, 10, false, false, 0x29ae },
    { "Max Power", "Alerts", 0x801398a, 56, 43, 6, false, false, 0x29c6 },
    { "Icon Atomizer Short", "Alerts", 0x8013ab7, 45, 30, 9, false, false, 0x25b0 },
    { "Atomizer Short", "Alerts", 0x8013b6b, 56, 43, 6, false, false, 0x25c8 },
    { "Icon Low Resistance", "Alerts", 0x8013c98, 25, 41, 9, false, false, 0x254c },
    { "Low Resistance", "Alerts", 0x8013d3c, 56, 43, 6, false, false, 0x2564 },
    { "Icon Check Battery", "Alerts", 0x8013e69, 20, 39, 9, false, false, 0x28de },
    { "Check Battery", "Alerts", 0x8013ede, 56, 43, 6, false, false, 0x28f6 },
    { "Icon Low Power", "Alerts", 0x801400b, 20, 39, 9, false, false, 0x2946 },
    { "Low Power", "Alerts", 0x8014080, 56, 43, 6, false, false, 0x295e },
    { "Icon Lock", "Controls", 0x80141ad, 29, 31, 9, false, false, 0x30b4 },
    { "Press Fire 3 times", "Alerts", 0x8014229, 56, 43, 6, false, false, 0x3050 },
    { "Slide The Switch", "Alerts", 0x8014379, 56, 43, 6, false, false, 0x3050 },
    { "Icon Unlock", "Controls", 0x80144c9, 29, 31, 12, false, false, 0x3038 },
    { "Icon Check Atomizer (tank)", "Alerts", 0x8014545, 20, 19, 9, false, false, 0x285c },
    { "Icon Check Atomizer (base)", "Alerts", 0x801457e, 20, 29, 11, false, false, 0x2874 },
    { "Check Atomizer", "Alerts", 0x80145d5, 56, 43, 6, false, false, 0x288c },
    { "Icon High Temp", "Alerts", 0x8014702, 31, 34, 10, false, false, 0x2f54 },
    { "High Temp", "Alerts", 0x801478a, 56, 43, 6, false, false, 0x2f6c },
    { "Icon Puff Cleared", "Alerts", 0x80148b7, 44, 22, 13, false, false, 0x2d7e },
    { "Puff Cleared", "Alerts", 0x801493b, 56, 43, 6, false, false, 0x2d96 },
    { "Icon Lock (main screen)", "Controls", 0x8014f64, 17, 25, 9, false, false, 0x55dc },
    { "Mode SMART (badge)", "Labels", 0x8014faf, 17, 25, 1, false, true, 0xc13e },
    { "Mode RBA (badge)", "Labels", 0x8014ffa, 17, 25, 1, false, true, 0xc142 },
    { "Unit W", "Units", 0x8015045, 17, 15, 6, false, false, 0x56f2 },
};
constexpr ColorSite kst3118Sites[] = {
    { 0x2434, 0x71e0f64f, 0xffe0, 13, 1 },
    { 0x245e, 0x71fff64f, 0xffff, 6, 1 },
    { 0x306c, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3084, 0x71fff64f, 0xffff, 6, 1 },
    { 0x30d0, 0x4168f44f, 0xe800, 9, 1 },
    { 0x30e8, 0x71fff64f, 0xffff, 6, 1 },
    { 0x31a6, 0x77fff64f, 0xffff, 8, 7 },
    { 0x3378, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3390, 0x715ff240, 0x75f, 11, 1 },
    { 0x33a8, 0x71fff64f, 0xffff, 6, 1 },
    { 0x33fa, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3412, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3462, 0x4168f44f, 0xe800, 9, 1 },
    { 0x347a, 0x71fff64f, 0xffff, 6, 1 },
    { 0x34ca, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x34e2, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3546, 0x72fff64f, 0xffff, 6, 2 },
    { 0x3726, 0x73fff64f, 0xffff, 6, 3 },
    { 0x387a, 0x71e0f64f, 0xffe0, 13, 1 },
    { 0x3892, 0x71fff64f, 0xffff, 6, 1 },
    { 0x38f6, 0x73fff64f, 0xffff, 6, 3 },
    { 0x3ab8, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x3ad0, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3b1e, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x3b36, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3b9c, 0x71e0f645, 0x5fe0, 12, 1 },
    { 0x3bb4, 0x70fff64f, 0xffff, 6, 0 },
    { 0x3c18, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3c30, 0x70fff64f, 0xffff, 6, 0 },
    { 0x3fde, 0x71fff64f, 0xffff, 6, 1 },
    { 0x4302, 0x4410f248, 0x8410, 14, 4 },
    { 0x44a0, 0x76fff64f, 0xffff, 8, 6 },
    { 0x4624, 0x77fff64f, 0xffff, 8, 7 },
    { 0x4774, 0x77fff64f, 0xffff, 8, 7 },
    { 0x48e8, 0x76fff64f, 0xffff, 8, 6 },
    { 0x4a1a, 0x76fff64f, 0xffff, 6, 6 },
    { 0x5dfe, 0x4768f44f, 0xe800, 9, 7 },
    { 0x5e60, 0x70fff64f, 0xffff, 7, 0 },
    { 0x5eb2, 0x71fff64f, 0xffff, 7, 1 },
    { 0x5f14, 0x70fff64f, 0xffff, 6, 0 },
    { 0x5f6e, 0x4968f44f, 0xe800, 9, 9 },
    { 0x5fc8, 0x71fff64f, 0xffff, 7, 1 },
    { 0x6010, 0x71fff64f, 0xffff, 7, 1 },
    { 0x605e, 0x71fff64f, 0xffff, 7, 1 },
    { 0x60be, 0x70fff64f, 0xffff, 6, 0 },
    { 0x65c2, 0x70fff64f, 0xffff, 6, 0 },
    { 0x6658, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x672e, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x6956, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6990, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x69c4, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x69fe, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x6a5a, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x6d06, 0x6080f44f, 0x400, 4, 0 },
    { 0x6d0a, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d2c, 0x6080f44f, 0x400, 4, 0 },
    { 0x6d30, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d4a, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x6d82, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d86, 0x6080f44f, 0x400, 4, 0 },
    { 0x6da8, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6dac, 0x6080f44f, 0x400, 4, 0 },
    { 0x6dce, 0x79fff64f, 0xffff, 8, 9 },
    { 0x705a, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x705e, 0x6180f44f, 0x400, 4, 1 },
    { 0x71a2, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x71ec, 0x77fff64f, 0xffff, 8, 7 },
    { 0x72f0, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x7336, 0x78fff64f, 0xffff, 8, 8 },
    { 0x7474, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x74f2, 0x78fff64f, 0xffff, 8, 8 },
    { 0x769a, 0x79fff64f, 0xffff, 7, 9 },
    { 0x776e, 0x4168f44f, 0xe800, 9, 1 },
    { 0x77ae, 0x66fcf44f, 0x7e0, 5, 6 },
    { 0x77f4, 0x76fff64f, 0xffff, 8, 6 },
    { 0x8fb8, 0x7be0f64f, 0xffe0, 13, 11 },
    { 0x907c, 0x78fff64f, 0xffff, 8, 8 },
    { 0xe228, 0x7c00, 0x7c00, 0, -1 },
    { 0xe22a, 0xffe0, 0xffe0, 1, -1 },
    { 0xe22c, 0x3cf, 0x3cf, 2, -1 },
    { 0xe22e, 0x7ff, 0x7ff, 3, -1 },
    { 0xe230, 0x400, 0x400, 4, -1 },
    { 0xe232, 0x7e0, 0x7e0, 5, -1 },
};
constexpr FontBank kst3118Banks[] = {
    { 0x8017693, 5, 8, 11, 0 },
    { 0x8015c14, 6, 10, 11, 6 },
    { 0x8015c82, 7, 13, 17, 7 },
    { 0x8017503, 11, 20, 10, 11 },
    { 0x80176eb, 10, 6, 11, 12 },
    { 0x801799f, 23, 13, 2, 13 },
    { 0x801776f, 30, 14, 10, 14 },
    { 0x80179ed, 23, 6, 1, 15 },
    { 0x8015d5f, 19, 40, 10, 19 },
};
constexpr Resource kst3118Resources[] = {
    { "Logo DRAG (standby)", "Screens", 0x8011297, 30, 10, 6, false, false, 0x3fde },
    { "Battery terminal", "Controls", 0x80112bf, 2, 9, 6, false, false, 0x4a1a },
    { "Unit W", "Units", 0x80112c8, 17, 15, 6, false, false, 0x60be },
    { "Resistance label R", "Labels", 0x80112f5, 38, 13, 1, false, true, 0xe22e },
    { "Decimal separator (statistics)", "Units", 0x8011336, 6, 10, 8, false, false, 0x4624 },
    { "Unit Ohm", "Units", 0x8011340, 16, 10, 8, false, false, 0x4624 },
    { "Voltage panel", "Labels", 0x8011354, 41, 17, 1, false, true, 0xe22e },
    { "Decimal separator (statistics)", "Units", 0x80113ba, 6, 10, 8, false, false, 0x48e8 },
    { "Time panel top edge", "Labels", 0x80113c4, 39, 7, 1, false, true, 0xe22e },
    { "Unit V", "Units", 0x80113e7, 10, 10, 8, false, false, 0x48e8 },
    { "Time panel", "Labels", 0x80113fb, 41, 17, 1, false, true, 0xe22e },
    { "Puff panel top edge", "Labels", 0x8011461, 39, 7, 1, false, true, 0xe22e },
    { "Decimal separator (statistics)", "Units", 0x8011484, 6, 10, 8, false, false, 0x4774 },
    { "Unit S", "Units", 0x801148e, 6, 10, 8, false, false, 0x4774 },
    { "Puff panel", "Labels", 0x8011498, 41, 13, 1, false, true, 0xe22a },
    { "Material NI", "Labels", 0x8011518, 34, 23, 5, false, false, 0x6a5a },
    { "Material TI", "Labels", 0x801158b, 34, 23, 5, false, false, 0x6a5a },
    { "Material SS", "Labels", 0x80115fe, 34, 23, 5, false, false, 0x6a5a },
    { "TC panel left edge", "Labels", 0x8011671, 8, 33, 5, false, false, 0x6a5a },
    { "TC panel right edge", "Labels", 0x8011692, 8, 33, 5, false, false, 0x6a5a },
    { "TC panel lower-left corner", "Labels", 0x80116b3, 14, 18, 5, false, false, 0x6d0a },
    { "TC panel lower-right corner", "Labels", 0x80116d7, 14, 18, 5, false, false, 0x6d30 },
    { "Material NI option", "Labels", 0x80116fb, 28, 18, 5, false, false, 0x6d4a },
    { "Material TI option", "Labels", 0x8011743, 28, 18, 5, false, false, 0x6d4a },
    { "Material SS option", "Labels", 0x801178b, 28, 18, 5, false, false, 0x6d4a },
    { "Unit W (menu)", "Units", 0x80117d3, 28, 18, 5, false, false, 0x6d4a },
    { "Unit Ohm (menu)", "Units", 0x801181b, 28, 18, 5, false, false, 0x6d4a },
    { "Decimal separator (TC)", "Units", 0x8011863, 2, 13, 8, false, false, 0x6dce },
    { "TC panel bottom edge", "Labels", 0x8011870, 56, 3, 4, false, false, 0x6d86 },
    { "Icon Lock (TC)", "Controls", 0x8011885, 20, 20, 9, false, false, 0x776e },
    { "Unit degrees F", "Units", 0x80118c1, 20, 20, 7, false, false, 0x769a },
    { "Unit degrees C", "Units", 0x8011939, 20, 20, 7, false, false, 0x769a },
    { "TC Watt / Time panel", "Labels", 0x80119b1, 30, 26, 5, false, false, 0x6658 },
    { "TC Ohms / Puff panel", "Labels", 0x8011a19, 30, 26, 5, false, false, 0x6658 },
    { "Material NI (round badge)", "Labels", 0x8011a81, 20, 26, 5, false, false, 0x6658 },
    { "Material TI (round badge)", "Labels", 0x8011acf, 20, 26, 5, false, false, 0x6658 },
    { "Material SS (round badge)", "Labels", 0x8011b1d, 20, 26, 5, false, false, 0x6658 },
    { "TC panel upper-left corner", "Labels", 0x8011b6b, 5, 16, 5, false, false, 0x71a2 },
    { "Decimal separator (TC)", "Units", 0x8011b7b, 2, 13, 8, false, false, 0x71ec },
    { "TC panel upper-right corner", "Labels", 0x8011b88, 5, 16, 5, false, false, 0x7474 },
    { "Decimal separator (TC)", "Units", 0x8011b98, 2, 13, 8, false, false, 0x74f2 },
    { "TC panel lower-left edge", "Labels", 0x8011ba5, 34, 3, 5, false, false, 0x77ae },
    { "Decimal separator (TC)", "Units", 0x8011bb4, 2, 13, 8, false, false, 0x77f4 },
    { "TC panel lower-right edge", "Labels", 0x8011bc1, 46, 3, 5, false, false, 0x72f0 },
    { "TC panel top border", "Labels", 0x8011bd3, 80, 6, 5, false, false, 0x672e },
    { "TC panel bottom border", "Labels", 0x8011c0f, 80, 7, 5, false, false, 0x672e },
    { "Menu SETTING", "Controls", 0x8011c80, 80, 8, 6, false, false, 0x3546 },
    { "SWITCH option outline", "Controls", 0x8011cd0, 80, 19, 1, false, true, 0xe22e },
    { "SWITCH option focus", "Controls", 0x8011d8e, 80, 19, 1, false, true, 0xe22a },
    { "SWITCH option selected", "Controls", 0x8011e4c, 80, 19, 1, false, true, 0xe22a },
    { "MODE option outline", "Controls", 0x8011f0a, 80, 19, 1, false, true, 0xe22a },
    { "MODE option focus", "Controls", 0x8011fc8, 80, 19, 1, false, true, 0xe22e },
    { "MODE option selected", "Controls", 0x8012086, 80, 19, 1, false, true, 0xe22e },
    { "EXIT option outline", "Controls", 0x8012144, 80, 19, 1, false, true, 0xe22a },
    { "EXIT option focus", "Controls", 0x8012202, 80, 19, 1, false, true, 0xe232 },
    { "EXIT option selected", "Controls", 0x80122c0, 80, 19, 1, false, true, 0xe232 },
    { "Menu SWITCH", "Controls", 0x801237e, 80, 7, 6, false, false, 0x3726 },
    { "Lock switch outline", "Controls", 0x80123c4, 80, 25, 1, false, true, 0xe22e },
    { "Lock switch focus", "Controls", 0x80124be, 80, 25, 1, false, true, 0xe22a },
    { "Lock switch selected", "Controls", 0x80125b8, 80, 25, 1, false, true, 0xe22a },
    { "Power switch outline", "Controls", 0x80126b2, 80, 25, 1, false, true, 0xe22a },
    { "Power switch focus", "Controls", 0x80127ac, 80, 25, 1, false, true, 0xe22e },
    { "Power switch selected", "Controls", 0x80128a6, 80, 25, 1, false, true, 0xe22e },
    { "Menu MODE", "Controls", 0x80129a0, 80, 8, 6, false, false, 0x38f6 },
    { "SMART option outline", "Controls", 0x80129f0, 80, 19, 1, false, true, 0xe22e },
    { "SMART option focus", "Controls", 0x8012aae, 80, 19, 1, false, true, 0xe22a },
    { "SMART option selected", "Controls", 0x8012b6c, 80, 19, 1, false, true, 0xe22a },
    { "RBA option outline", "Controls", 0x8012c2a, 80, 19, 1, false, true, 0xe22a },
    { "RBA option focus", "Controls", 0x8012ce8, 80, 19, 1, false, true, 0xe22e },
    { "RBA option selected", "Controls", 0x8012da6, 80, 19, 1, false, true, 0xe22e },
    { "TC option outline", "Controls", 0x8012e64, 80, 19, 1, false, true, 0xe22a },
    { "TC option focus", "Controls", 0x8012f22, 80, 19, 1, false, true, 0xe232 },
    { "TC option selected", "Controls", 0x8012fe0, 80, 19, 1, false, true, 0xe232 },
    { "Logo VOOPOO (service)", "Screens", 0x801309e, 80, 10, 13, false, false, 0x8fb8 },
    { "CHIP ID panel", "Screens", 0x8013102, 74, 63, 13, false, false, 0x8fb8 },
    { "LEVEL CODE panel", "Screens", 0x8013378, 74, 46, 13, false, false, 0x8fb8 },
    { "Logo VOOPOO (boot, rotated clockwise)", "Screens", 0x8013544, 22, 136, 6, true, false, 0x2528 },
    { "OFF", "Screens", 0x8014ca4, 39, 18, 6, true, false, 0x251c },
    { "Logo DRAG S PRO (boot)", "Screens", 0x8015220, 80, 37, 13, false, false, 0x2434 },
    { "GENE.FAN version (boot)", "Screens", 0x8015392, 80, 32, 6, false, false, 0x245e },
    { "Battery charging (empty)", "Animation", 0x80154d2, 47, 17, 8, false, false, 0x31a6 },
    { "Battery charging (1 segment)", "Animation", 0x8015538, 47, 17, 8, false, false, 0x31a6 },
    { "Battery charging (2 segments)", "Animation", 0x801559e, 47, 17, 8, false, false, 0x31a6 },
    { "Battery charging (3 segments)", "Animation", 0x8015604, 47, 17, 8, false, false, 0x31a6 },
    { "Battery charging (4 segments)", "Animation", 0x801566a, 47, 17, 8, false, false, 0x31a6 },
    { "Battery charging (full)", "Animation", 0x80156d0, 47, 17, 8, false, false, 0x31a6 },
    { "CHARGING", "Screens", 0x8015736, 61, 10, 6, true, false, 0x3318 },
    { "Battery charging bolt", "Controls", 0x8015bfa, 11, 13, 8, false, false, 0x31a6 },
    { "Icon Time Out", "Alerts", 0x801620f, 29, 29, 10, false, false, 0x3b1e },
    { "Time Out", "Alerts", 0x8016283, 56, 43, 6, false, false, 0x3b36 },
    { "Icon Max Power", "Alerts", 0x80163b0, 23, 39, 10, false, false, 0x34ca },
    { "Max Power", "Alerts", 0x8016425, 56, 43, 6, false, false, 0x34e2 },
    { "Icon Atomizer Short", "Alerts", 0x8016552, 45, 30, 9, false, false, 0x30d0 },
    { "Atomizer Short", "Alerts", 0x8016606, 56, 43, 6, false, false, 0x30e8 },
    { "Icon Low Resistance", "Alerts", 0x8016733, 25, 41, 9, false, false, 0x306c },
    { "Low Resistance", "Alerts", 0x80167d7, 56, 43, 6, false, false, 0x3084 },
    { "Icon Check Battery", "Alerts", 0x8016904, 20, 39, 9, false, false, 0x33fa },
    { "Check Battery", "Alerts", 0x8016979, 56, 43, 6, false, false, 0x3412 },
    { "Icon Low Power", "Alerts", 0x8016aa6, 20, 39, 9, false, false, 0x3462 },
    { "Low Power", "Alerts", 0x8016b1b, 56, 43, 6, false, false, 0x347a },
    { "Icon Lock", "Controls", 0x8016c48, 29, 31, 9, false, false, 0x3c18 },
    { "Press Fire 3 times", "Alerts", 0x8016cc4, 56, 43, 6, false, false, 0x3bb4 },
    { "Slide The Switch", "Alerts", 0x8016e14, 56, 43, 6, false, false, 0x3bb4 },
    { "Icon Unlock", "Controls", 0x8016f64, 29, 31, 12, false, false, 0x3b9c },
    { "Icon Check Atomizer (tank)", "Alerts", 0x8016fe0, 20, 19, 9, false, false, 0x3378 },
    { "Icon Check Atomizer (base)", "Alerts", 0x8017019, 20, 29, 11, false, false, 0x3390 },
    { "Check Atomizer", "Alerts", 0x8017070, 56, 43, 6, false, false, 0x33a8 },
    { "Icon High Temp", "Alerts", 0x801719d, 31, 34, 10, false, false, 0x3ab8 },
    { "High Temp", "Alerts", 0x8017225, 56, 43, 6, false, false, 0x3ad0 },
    { "Icon Puff Cleared", "Alerts", 0x8017352, 44, 22, 13, false, false, 0x387a },
    { "Puff Cleared", "Alerts", 0x80173d6, 56, 43, 6, false, false, 0x3892 },
    { "Icon Lock (main screen)", "Controls", 0x80179ff, 17, 25, 9, false, false, 0x5dfe },
    { "Mode SMART (badge)", "Labels", 0x8017a4a, 17, 25, 1, false, true, 0xe22a },
    { "Mode RBA (badge)", "Labels", 0x8017a95, 17, 25, 1, false, true, 0xe22e },
    { "Unit W", "Units", 0x8017ae0, 17, 15, 6, false, false, 0x5f14 },
};
constexpr ColorSite kst3119Sites[] = {
    { 0x2434, 0x71e0f64f, 0xffe0, 13, 1 },
    { 0x245e, 0x71fff64f, 0xffff, 6, 1 },
    { 0x306c, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3084, 0x71fff64f, 0xffff, 6, 1 },
    { 0x30d0, 0x4168f44f, 0xe800, 9, 1 },
    { 0x30e8, 0x71fff64f, 0xffff, 6, 1 },
    { 0x31a6, 0x77fff64f, 0xffff, 8, 7 },
    { 0x3378, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3390, 0x715ff240, 0x75f, 11, 1 },
    { 0x33a8, 0x71fff64f, 0xffff, 6, 1 },
    { 0x33fa, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3412, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3462, 0x4168f44f, 0xe800, 9, 1 },
    { 0x347a, 0x71fff64f, 0xffff, 6, 1 },
    { 0x34ca, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x34e2, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3546, 0x72fff64f, 0xffff, 6, 2 },
    { 0x3726, 0x73fff64f, 0xffff, 6, 3 },
    { 0x387a, 0x71e0f64f, 0xffe0, 13, 1 },
    { 0x3892, 0x71fff64f, 0xffff, 6, 1 },
    { 0x38f6, 0x73fff64f, 0xffff, 6, 3 },
    { 0x3ab8, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x3ad0, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3b1e, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x3b36, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3b9c, 0x71e0f645, 0x5fe0, 12, 1 },
    { 0x3bb4, 0x70fff64f, 0xffff, 6, 0 },
    { 0x3c18, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3c30, 0x70fff64f, 0xffff, 6, 0 },
    { 0x3fde, 0x71fff64f, 0xffff, 6, 1 },
    { 0x4302, 0x4410f248, 0x8410, 14, 4 },
    { 0x44a0, 0x76fff64f, 0xffff, 8, 6 },
    { 0x4624, 0x77fff64f, 0xffff, 8, 7 },
    { 0x4774, 0x77fff64f, 0xffff, 8, 7 },
    { 0x48e8, 0x76fff64f, 0xffff, 8, 6 },
    { 0x4a1a, 0x76fff64f, 0xffff, 6, 6 },
    { 0x5dfe, 0x4768f44f, 0xe800, 9, 7 },
    { 0x5e60, 0x70fff64f, 0xffff, 7, 0 },
    { 0x5eb2, 0x71fff64f, 0xffff, 7, 1 },
    { 0x5f14, 0x70fff64f, 0xffff, 6, 0 },
    { 0x5f6e, 0x4968f44f, 0xe800, 9, 9 },
    { 0x5fc8, 0x71fff64f, 0xffff, 7, 1 },
    { 0x6010, 0x71fff64f, 0xffff, 7, 1 },
    { 0x605e, 0x71fff64f, 0xffff, 7, 1 },
    { 0x60be, 0x70fff64f, 0xffff, 6, 0 },
    { 0x65c2, 0x70fff64f, 0xffff, 6, 0 },
    { 0x6658, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x672e, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x6956, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6990, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x69c4, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x69fe, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x6a5a, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x6d06, 0x6080f44f, 0x400, 4, 0 },
    { 0x6d0a, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d2c, 0x6080f44f, 0x400, 4, 0 },
    { 0x6d30, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d4a, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x6d82, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d86, 0x6080f44f, 0x400, 4, 0 },
    { 0x6da8, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6dac, 0x6080f44f, 0x400, 4, 0 },
    { 0x6dce, 0x79fff64f, 0xffff, 8, 9 },
    { 0x705a, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x705e, 0x6180f44f, 0x400, 4, 1 },
    { 0x71a2, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x71ec, 0x77fff64f, 0xffff, 8, 7 },
    { 0x72f0, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x7336, 0x78fff64f, 0xffff, 8, 8 },
    { 0x7474, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x74f2, 0x78fff64f, 0xffff, 8, 8 },
    { 0x769a, 0x79fff64f, 0xffff, 7, 9 },
    { 0x776e, 0x4168f44f, 0xe800, 9, 1 },
    { 0x77ae, 0x66fcf44f, 0x7e0, 5, 6 },
    { 0x77f4, 0x76fff64f, 0xffff, 8, 6 },
    { 0x8fb8, 0x7be0f64f, 0xffe0, 13, 11 },
    { 0x907c, 0x78fff64f, 0xffff, 8, 8 },
    { 0xe608, 0x7c00, 0x7c00, 0, -1 },
    { 0xe60a, 0xffe0, 0xffe0, 1, -1 },
    { 0xe60c, 0x3cf, 0x3cf, 2, -1 },
    { 0xe60e, 0x7ff, 0x7ff, 3, -1 },
    { 0xe610, 0x400, 0x400, 4, -1 },
    { 0xe612, 0x7e0, 0x7e0, 5, -1 },
};
constexpr FontBank kst3119Banks[] = {
    { 0x8017a73, 5, 8, 11, 0 },
    { 0x8015ff4, 6, 10, 11, 6 },
    { 0x8016062, 7, 13, 17, 7 },
    { 0x80178e3, 11, 20, 10, 11 },
    { 0x8017acb, 10, 6, 11, 12 },
    { 0x8017d7f, 23, 13, 2, 13 },
    { 0x8017b4f, 30, 14, 10, 14 },
    { 0x8017dcd, 23, 6, 1, 15 },
    { 0x801613f, 19, 40, 10, 19 },
};
constexpr Resource kst3119Resources[] = {
    { "Logo DRAG (standby)", "Screens", 0x8011677, 30, 10, 6, false, false, 0x3fde },
    { "Battery terminal", "Controls", 0x801169f, 2, 9, 6, false, false, 0x4a1a },
    { "Unit W", "Units", 0x80116a8, 17, 15, 6, false, false, 0x60be },
    { "Resistance label R", "Labels", 0x80116d5, 38, 13, 1, false, true, 0xe60e },
    { "Decimal separator (statistics)", "Units", 0x8011716, 6, 10, 8, false, false, 0x4624 },
    { "Unit Ohm", "Units", 0x8011720, 16, 10, 8, false, false, 0x4624 },
    { "Voltage panel", "Labels", 0x8011734, 41, 17, 1, false, true, 0xe60e },
    { "Decimal separator (statistics)", "Units", 0x801179a, 6, 10, 8, false, false, 0x48e8 },
    { "Time panel top edge", "Labels", 0x80117a4, 39, 7, 1, false, true, 0xe60e },
    { "Unit V", "Units", 0x80117c7, 10, 10, 8, false, false, 0x48e8 },
    { "Time panel", "Labels", 0x80117db, 41, 17, 1, false, true, 0xe60e },
    { "Puff panel top edge", "Labels", 0x8011841, 39, 7, 1, false, true, 0xe60e },
    { "Decimal separator (statistics)", "Units", 0x8011864, 6, 10, 8, false, false, 0x4774 },
    { "Unit S", "Units", 0x801186e, 6, 10, 8, false, false, 0x4774 },
    { "Puff panel", "Labels", 0x8011878, 41, 13, 1, false, true, 0xe60a },
    { "Material NI", "Labels", 0x80118f8, 34, 23, 5, false, false, 0x6a5a },
    { "Material TI", "Labels", 0x801196b, 34, 23, 5, false, false, 0x6a5a },
    { "Material SS", "Labels", 0x80119de, 34, 23, 5, false, false, 0x6a5a },
    { "TC panel left edge", "Labels", 0x8011a51, 8, 33, 5, false, false, 0x6a5a },
    { "TC panel right edge", "Labels", 0x8011a72, 8, 33, 5, false, false, 0x6a5a },
    { "TC panel lower-left corner", "Labels", 0x8011a93, 14, 18, 5, false, false, 0x6d0a },
    { "TC panel lower-right corner", "Labels", 0x8011ab7, 14, 18, 5, false, false, 0x6d30 },
    { "Material NI option", "Labels", 0x8011adb, 28, 18, 5, false, false, 0x6d4a },
    { "Material TI option", "Labels", 0x8011b23, 28, 18, 5, false, false, 0x6d4a },
    { "Material SS option", "Labels", 0x8011b6b, 28, 18, 5, false, false, 0x6d4a },
    { "Unit W (menu)", "Units", 0x8011bb3, 28, 18, 5, false, false, 0x6d4a },
    { "Unit Ohm (menu)", "Units", 0x8011bfb, 28, 18, 5, false, false, 0x6d4a },
    { "Decimal separator (TC)", "Units", 0x8011c43, 2, 13, 8, false, false, 0x6dce },
    { "TC panel bottom edge", "Labels", 0x8011c50, 56, 3, 4, false, false, 0x6d86 },
    { "Icon Lock (TC)", "Controls", 0x8011c65, 20, 20, 9, false, false, 0x776e },
    { "Unit degrees F", "Units", 0x8011ca1, 20, 20, 7, false, false, 0x769a },
    { "Unit degrees C", "Units", 0x8011d19, 20, 20, 7, false, false, 0x769a },
    { "TC Watt / Time panel", "Labels", 0x8011d91, 30, 26, 5, false, false, 0x6658 },
    { "TC Ohms / Puff panel", "Labels", 0x8011df9, 30, 26, 5, false, false, 0x6658 },
    { "Material NI (round badge)", "Labels", 0x8011e61, 20, 26, 5, false, false, 0x6658 },
    { "Material TI (round badge)", "Labels", 0x8011eaf, 20, 26, 5, false, false, 0x6658 },
    { "Material SS (round badge)", "Labels", 0x8011efd, 20, 26, 5, false, false, 0x6658 },
    { "TC panel upper-left corner", "Labels", 0x8011f4b, 5, 16, 5, false, false, 0x71a2 },
    { "Decimal separator (TC)", "Units", 0x8011f5b, 2, 13, 8, false, false, 0x71ec },
    { "TC panel upper-right corner", "Labels", 0x8011f68, 5, 16, 5, false, false, 0x7474 },
    { "Decimal separator (TC)", "Units", 0x8011f78, 2, 13, 8, false, false, 0x74f2 },
    { "TC panel lower-left edge", "Labels", 0x8011f85, 34, 3, 5, false, false, 0x77ae },
    { "Decimal separator (TC)", "Units", 0x8011f94, 2, 13, 8, false, false, 0x77f4 },
    { "TC panel lower-right edge", "Labels", 0x8011fa1, 46, 3, 5, false, false, 0x72f0 },
    { "TC panel top border", "Labels", 0x8011fb3, 80, 6, 5, false, false, 0x672e },
    { "TC panel bottom border", "Labels", 0x8011fef, 80, 7, 5, false, false, 0x672e },
    { "Menu SETTING", "Controls", 0x8012060, 80, 8, 6, false, false, 0x3546 },
    { "SWITCH option outline", "Controls", 0x80120b0, 80, 19, 1, false, true, 0xe60e },
    { "SWITCH option focus", "Controls", 0x801216e, 80, 19, 1, false, true, 0xe60a },
    { "SWITCH option selected", "Controls", 0x801222c, 80, 19, 1, false, true, 0xe60a },
    { "MODE option outline", "Controls", 0x80122ea, 80, 19, 1, false, true, 0xe60a },
    { "MODE option focus", "Controls", 0x80123a8, 80, 19, 1, false, true, 0xe60e },
    { "MODE option selected", "Controls", 0x8012466, 80, 19, 1, false, true, 0xe60e },
    { "EXIT option outline", "Controls", 0x8012524, 80, 19, 1, false, true, 0xe60a },
    { "EXIT option focus", "Controls", 0x80125e2, 80, 19, 1, false, true, 0xe612 },
    { "EXIT option selected", "Controls", 0x80126a0, 80, 19, 1, false, true, 0xe612 },
    { "Menu SWITCH", "Controls", 0x801275e, 80, 7, 6, false, false, 0x3726 },
    { "Lock switch outline", "Controls", 0x80127a4, 80, 25, 1, false, true, 0xe60e },
    { "Lock switch focus", "Controls", 0x801289e, 80, 25, 1, false, true, 0xe60a },
    { "Lock switch selected", "Controls", 0x8012998, 80, 25, 1, false, true, 0xe60a },
    { "Power switch outline", "Controls", 0x8012a92, 80, 25, 1, false, true, 0xe60a },
    { "Power switch focus", "Controls", 0x8012b8c, 80, 25, 1, false, true, 0xe60e },
    { "Power switch selected", "Controls", 0x8012c86, 80, 25, 1, false, true, 0xe60e },
    { "Menu MODE", "Controls", 0x8012d80, 80, 8, 6, false, false, 0x38f6 },
    { "SMART option outline", "Controls", 0x8012dd0, 80, 19, 1, false, true, 0xe60e },
    { "SMART option focus", "Controls", 0x8012e8e, 80, 19, 1, false, true, 0xe60a },
    { "SMART option selected", "Controls", 0x8012f4c, 80, 19, 1, false, true, 0xe60a },
    { "RBA option outline", "Controls", 0x801300a, 80, 19, 1, false, true, 0xe60a },
    { "RBA option focus", "Controls", 0x80130c8, 80, 19, 1, false, true, 0xe60e },
    { "RBA option selected", "Controls", 0x8013186, 80, 19, 1, false, true, 0xe60e },
    { "TC option outline", "Controls", 0x8013244, 80, 19, 1, false, true, 0xe60a },
    { "TC option focus", "Controls", 0x8013302, 80, 19, 1, false, true, 0xe612 },
    { "TC option selected", "Controls", 0x80133c0, 80, 19, 1, false, true, 0xe612 },
    { "Logo VOOPOO (service)", "Screens", 0x801347e, 80, 10, 13, false, false, 0x8fb8 },
    { "CHIP ID panel", "Screens", 0x80134e2, 74, 63, 13, false, false, 0x8fb8 },
    { "LEVEL CODE panel", "Screens", 0x8013758, 74, 46, 13, false, false, 0x8fb8 },
    { "Logo VOOPOO (boot, rotated clockwise)", "Screens", 0x8013924, 22, 136, 6, true, false, 0x2528 },
    { "OFF", "Screens", 0x8015084, 39, 18, 6, true, false, 0x251c },
    { "Logo DRAG S PRO (boot)", "Screens", 0x8015600, 80, 37, 13, false, false, 0x2434 },
    { "GENE.FAN version (boot)", "Screens", 0x8015772, 80, 32, 6, false, false, 0x245e },
    { "Battery charging (empty)", "Animation", 0x80158b2, 47, 17, 8, false, false, 0x31a6 },
    { "Battery charging (1 segment)", "Animation", 0x8015918, 47, 17, 8, false, false, 0x31a6 },
    { "Battery charging (2 segments)", "Animation", 0x801597e, 47, 17, 8, false, false, 0x31a6 },
    { "Battery charging (3 segments)", "Animation", 0x80159e4, 47, 17, 8, false, false, 0x31a6 },
    { "Battery charging (4 segments)", "Animation", 0x8015a4a, 47, 17, 8, false, false, 0x31a6 },
    { "Battery charging (full)", "Animation", 0x8015ab0, 47, 17, 8, false, false, 0x31a6 },
    { "CHARGING", "Screens", 0x8015b16, 61, 10, 6, true, false, 0x3318 },
    { "Battery charging bolt", "Controls", 0x8015fda, 11, 13, 8, false, false, 0x31a6 },
    { "Icon Time Out", "Alerts", 0x80165ef, 29, 29, 10, false, false, 0x3b1e },
    { "Time Out", "Alerts", 0x8016663, 56, 43, 6, false, false, 0x3b36 },
    { "Icon Max Power", "Alerts", 0x8016790, 23, 39, 10, false, false, 0x34ca },
    { "Max Power", "Alerts", 0x8016805, 56, 43, 6, false, false, 0x34e2 },
    { "Icon Atomizer Short", "Alerts", 0x8016932, 45, 30, 9, false, false, 0x30d0 },
    { "Atomizer Short", "Alerts", 0x80169e6, 56, 43, 6, false, false, 0x30e8 },
    { "Icon Low Resistance", "Alerts", 0x8016b13, 25, 41, 9, false, false, 0x306c },
    { "Low Resistance", "Alerts", 0x8016bb7, 56, 43, 6, false, false, 0x3084 },
    { "Icon Check Battery", "Alerts", 0x8016ce4, 20, 39, 9, false, false, 0x33fa },
    { "Check Battery", "Alerts", 0x8016d59, 56, 43, 6, false, false, 0x3412 },
    { "Icon Low Power", "Alerts", 0x8016e86, 20, 39, 9, false, false, 0x3462 },
    { "Low Power", "Alerts", 0x8016efb, 56, 43, 6, false, false, 0x347a },
    { "Icon Lock", "Controls", 0x8017028, 29, 31, 9, false, false, 0x3c18 },
    { "Press Fire 3 times", "Alerts", 0x80170a4, 56, 43, 6, false, false, 0x3bb4 },
    { "Slide The Switch", "Alerts", 0x80171f4, 56, 43, 6, false, false, 0x3bb4 },
    { "Icon Unlock", "Controls", 0x8017344, 29, 31, 12, false, false, 0x3b9c },
    { "Icon Check Atomizer (tank)", "Alerts", 0x80173c0, 20, 19, 9, false, false, 0x3378 },
    { "Icon Check Atomizer (base)", "Alerts", 0x80173f9, 20, 29, 11, false, false, 0x3390 },
    { "Check Atomizer", "Alerts", 0x8017450, 56, 43, 6, false, false, 0x33a8 },
    { "Icon High Temp", "Alerts", 0x801757d, 31, 34, 10, false, false, 0x3ab8 },
    { "High Temp", "Alerts", 0x8017605, 56, 43, 6, false, false, 0x3ad0 },
    { "Icon Puff Cleared", "Alerts", 0x8017732, 44, 22, 13, false, false, 0x387a },
    { "Puff Cleared", "Alerts", 0x80177b6, 56, 43, 6, false, false, 0x3892 },
    { "Icon Lock (main screen)", "Controls", 0x8017ddf, 17, 25, 9, false, false, 0x5dfe },
    { "Mode SMART (badge)", "Labels", 0x8017e2a, 17, 25, 1, false, true, 0xe60a },
    { "Mode RBA (badge)", "Labels", 0x8017e75, 17, 25, 1, false, true, 0xe60e },
    { "Unit W", "Units", 0x8017ec0, 17, 15, 6, false, false, 0x5f14 },
};
constexpr ColorSite kxt3144Sites[] = {
    { 0x24a4, 0x71e0f64f, 0xffe0, 13, 1 },
    { 0x24ce, 0x71fff64f, 0xffff, 6, 1 },
    { 0x30dc, 0x4168f44f, 0xe800, 9, 1 },
    { 0x30f4, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3140, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3158, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3216, 0x77fff64f, 0xffff, 8, 7 },
    { 0x33e8, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3400, 0x715ff240, 0x75f, 11, 1 },
    { 0x3418, 0x71fff64f, 0xffff, 6, 1 },
    { 0x346a, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3482, 0x71fff64f, 0xffff, 6, 1 },
    { 0x34d2, 0x4168f44f, 0xe800, 9, 1 },
    { 0x34ea, 0x71fff64f, 0xffff, 6, 1 },
    { 0x353a, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x3552, 0x71fff64f, 0xffff, 6, 1 },
    { 0x35b6, 0x72fff64f, 0xffff, 6, 2 },
    { 0x3796, 0x73fff64f, 0xffff, 6, 3 },
    { 0x38ea, 0x71e0f64f, 0xffe0, 13, 1 },
    { 0x3902, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3966, 0x73fff64f, 0xffff, 6, 3 },
    { 0x3b28, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x3b40, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3b8e, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x3ba6, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3c0c, 0x71e0f645, 0x5fe0, 12, 1 },
    { 0x3c24, 0x70fff64f, 0xffff, 6, 0 },
    { 0x3c88, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3ca0, 0x70fff64f, 0xffff, 6, 0 },
    { 0x40ba, 0x71fff64f, 0xffff, 6, 1 },
    { 0x43de, 0x4410f248, 0x8410, 14, 4 },
    { 0x457c, 0x76fff64f, 0xffff, 8, 6 },
    { 0x4700, 0x77fff64f, 0xffff, 8, 7 },
    { 0x4850, 0x77fff64f, 0xffff, 8, 7 },
    { 0x49c4, 0x76fff64f, 0xffff, 8, 6 },
    { 0x4aac, 0x75fff64f, 0xffff, 6, 5 },
    { 0x4af0, 0x4568f44f, 0xe800, 9, 5 },
    { 0x4af6, 0x76fff64f, 0xffff, 6, 6 },
    { 0x5e82, 0x4768f44f, 0xe800, 9, 7 },
    { 0x5ee4, 0x70fff64f, 0xffff, 7, 0 },
    { 0x5f36, 0x71fff64f, 0xffff, 7, 1 },
    { 0x5f98, 0x70fff64f, 0xffff, 6, 0 },
    { 0x5ff2, 0x4968f44f, 0xe800, 9, 9 },
    { 0x604c, 0x71fff64f, 0xffff, 7, 1 },
    { 0x6094, 0x71fff64f, 0xffff, 7, 1 },
    { 0x60e2, 0x71fff64f, 0xffff, 7, 1 },
    { 0x6142, 0x70fff64f, 0xffff, 6, 0 },
    { 0x65aa, 0x70fff64f, 0xffff, 6, 0 },
    { 0x6640, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x6716, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x693e, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6978, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x69ac, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x69e6, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x6a42, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x6cee, 0x6080f44f, 0x400, 4, 0 },
    { 0x6cf2, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d14, 0x6080f44f, 0x400, 4, 0 },
    { 0x6d18, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d32, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x6d6a, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d6e, 0x6080f44f, 0x400, 4, 0 },
    { 0x6d90, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d94, 0x6080f44f, 0x400, 4, 0 },
    { 0x6db6, 0x79fff64f, 0xffff, 8, 9 },
    { 0x7042, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x7046, 0x6180f44f, 0x400, 4, 1 },
    { 0x718a, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x71d4, 0x77fff64f, 0xffff, 8, 7 },
    { 0x72d8, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x731e, 0x78fff64f, 0xffff, 8, 8 },
    { 0x745c, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x74da, 0x78fff64f, 0xffff, 8, 8 },
    { 0x7682, 0x79fff64f, 0xffff, 7, 9 },
    { 0x7756, 0x4168f44f, 0xe800, 9, 1 },
    { 0x7796, 0x66fcf44f, 0x7e0, 5, 6 },
    { 0x77dc, 0x76fff64f, 0xffff, 8, 6 },
    { 0x917c, 0x7be0f64f, 0xffe0, 13, 11 },
    { 0x9240, 0x78fff64f, 0xffff, 8, 8 },
    { 0xe1fc, 0x7c00, 0x7c00, 0, -1 },
    { 0xe1fe, 0xffe0, 0xffe0, 1, -1 },
    { 0xe200, 0x3cf, 0x3cf, 2, -1 },
    { 0xe202, 0x7ff, 0x7ff, 3, -1 },
    { 0xe204, 0x400, 0x400, 4, -1 },
    { 0xe206, 0x7e0, 0x7e0, 5, -1 },
};
constexpr FontBank kxt3144Banks[] = {
    { 0x8017667, 5, 8, 11, 0 },
    { 0x8015be8, 6, 10, 11, 6 },
    { 0x8015c56, 7, 13, 17, 7 },
    { 0x80174d7, 11, 20, 10, 11 },
    { 0x80176bf, 10, 6, 11, 12 },
    { 0x8017973, 23, 13, 2, 13 },
    { 0x8017743, 30, 14, 10, 14 },
    { 0x80179c1, 23, 6, 1, 15 },
    { 0x8015d33, 19, 40, 10, 19 },
};
constexpr Resource kxt3144Resources[] = {
    { "Logo DRAG (standby)", "Screens", 0x801126b, 30, 10, 6, false, false, 0x40ba },
    { "Battery terminal", "Controls", 0x8011293, 2, 9, 6, false, false, 0x4af6 },
    { "Unit W", "Units", 0x801129c, 17, 15, 6, false, false, 0x6142 },
    { "Resistance label R", "Labels", 0x80112c9, 38, 13, 1, false, true, 0xe202 },
    { "Decimal separator (statistics)", "Units", 0x801130a, 6, 10, 8, false, false, 0x4700 },
    { "Unit Ohm", "Units", 0x8011314, 16, 10, 8, false, false, 0x4700 },
    { "Voltage panel", "Labels", 0x8011328, 41, 17, 1, false, true, 0xe202 },
    { "Decimal separator (statistics)", "Units", 0x801138e, 6, 10, 8, false, false, 0x49c4 },
    { "Time panel top edge", "Labels", 0x8011398, 39, 7, 1, false, true, 0xe202 },
    { "Unit V", "Units", 0x80113bb, 10, 10, 8, false, false, 0x49c4 },
    { "Time panel", "Labels", 0x80113cf, 41, 17, 1, false, true, 0xe202 },
    { "Puff panel top edge", "Labels", 0x8011435, 39, 7, 1, false, true, 0xe202 },
    { "Decimal separator (statistics)", "Units", 0x8011458, 6, 10, 8, false, false, 0x4850 },
    { "Unit S", "Units", 0x8011462, 6, 10, 8, false, false, 0x4850 },
    { "Puff panel", "Labels", 0x801146c, 41, 13, 1, false, true, 0xe1fe },
    { "Material NI", "Labels", 0x80114ec, 34, 23, 5, false, false, 0x6a42 },
    { "Material TI", "Labels", 0x801155f, 34, 23, 5, false, false, 0x6a42 },
    { "Material SS", "Labels", 0x80115d2, 34, 23, 5, false, false, 0x6a42 },
    { "TC panel left edge", "Labels", 0x8011645, 8, 33, 5, false, false, 0x6a42 },
    { "TC panel right edge", "Labels", 0x8011666, 8, 33, 5, false, false, 0x6a42 },
    { "TC panel lower-left corner", "Labels", 0x8011687, 14, 18, 5, false, false, 0x6cf2 },
    { "TC panel lower-right corner", "Labels", 0x80116ab, 14, 18, 5, false, false, 0x6d18 },
    { "Material NI option", "Labels", 0x80116cf, 28, 18, 5, false, false, 0x6d32 },
    { "Material TI option", "Labels", 0x8011717, 28, 18, 5, false, false, 0x6d32 },
    { "Material SS option", "Labels", 0x801175f, 28, 18, 5, false, false, 0x6d32 },
    { "Unit W (menu)", "Units", 0x80117a7, 28, 18, 5, false, false, 0x6d32 },
    { "Unit Ohm (menu)", "Units", 0x80117ef, 28, 18, 5, false, false, 0x6d32 },
    { "Decimal separator (TC)", "Units", 0x8011837, 2, 13, 8, false, false, 0x6db6 },
    { "TC panel bottom edge", "Labels", 0x8011844, 56, 3, 4, false, false, 0x6d6e },
    { "Icon Lock (TC)", "Controls", 0x8011859, 20, 20, 9, false, false, 0x7756 },
    { "Unit degrees F", "Units", 0x8011895, 20, 20, 7, false, false, 0x7682 },
    { "Unit degrees C", "Units", 0x801190d, 20, 20, 7, false, false, 0x7682 },
    { "TC Watt / Time panel", "Labels", 0x8011985, 30, 26, 5, false, false, 0x6640 },
    { "TC Ohms / Puff panel", "Labels", 0x80119ed, 30, 26, 5, false, false, 0x6640 },
    { "Material NI (round badge)", "Labels", 0x8011a55, 20, 26, 5, false, false, 0x6640 },
    { "Material TI (round badge)", "Labels", 0x8011aa3, 20, 26, 5, false, false, 0x6640 },
    { "Material SS (round badge)", "Labels", 0x8011af1, 20, 26, 5, false, false, 0x6640 },
    { "TC panel upper-left corner", "Labels", 0x8011b3f, 5, 16, 5, false, false, 0x718a },
    { "Decimal separator (TC)", "Units", 0x8011b4f, 2, 13, 8, false, false, 0x71d4 },
    { "TC panel upper-right corner", "Labels", 0x8011b5c, 5, 16, 5, false, false, 0x745c },
    { "Decimal separator (TC)", "Units", 0x8011b6c, 2, 13, 8, false, false, 0x74da },
    { "TC panel lower-left edge", "Labels", 0x8011b79, 34, 3, 5, false, false, 0x7796 },
    { "Decimal separator (TC)", "Units", 0x8011b88, 2, 13, 8, false, false, 0x77dc },
    { "TC panel lower-right edge", "Labels", 0x8011b95, 46, 3, 5, false, false, 0x72d8 },
    { "TC panel top border", "Labels", 0x8011ba7, 80, 6, 5, false, false, 0x6716 },
    { "TC panel bottom border", "Labels", 0x8011be3, 80, 7, 5, false, false, 0x6716 },
    { "Menu SETTING", "Controls", 0x8011c54, 80, 8, 6, false, false, 0x35b6 },
    { "SWITCH option outline", "Controls", 0x8011ca4, 80, 19, 1, false, true, 0xe202 },
    { "SWITCH option focus", "Controls", 0x8011d62, 80, 19, 1, false, true, 0xe1fe },
    { "SWITCH option selected", "Controls", 0x8011e20, 80, 19, 1, false, true, 0xe1fe },
    { "MODE option outline", "Controls", 0x8011ede, 80, 19, 1, false, true, 0xe1fe },
    { "MODE option focus", "Controls", 0x8011f9c, 80, 19, 1, false, true, 0xe202 },
    { "MODE option selected", "Controls", 0x801205a, 80, 19, 1, false, true, 0xe202 },
    { "EXIT option outline", "Controls", 0x8012118, 80, 19, 1, false, true, 0xe1fe },
    { "EXIT option focus", "Controls", 0x80121d6, 80, 19, 1, false, true, 0xe206 },
    { "EXIT option selected", "Controls", 0x8012294, 80, 19, 1, false, true, 0xe206 },
    { "Menu SWITCH", "Controls", 0x8012352, 80, 7, 6, false, false, 0x3796 },
    { "Lock switch outline", "Controls", 0x8012398, 80, 25, 1, false, true, 0xe202 },
    { "Lock switch focus", "Controls", 0x8012492, 80, 25, 1, false, true, 0xe1fe },
    { "Lock switch selected", "Controls", 0x801258c, 80, 25, 1, false, true, 0xe1fe },
    { "Power switch outline", "Controls", 0x8012686, 80, 25, 1, false, true, 0xe1fe },
    { "Power switch focus", "Controls", 0x8012780, 80, 25, 1, false, true, 0xe202 },
    { "Power switch selected", "Controls", 0x801287a, 80, 25, 1, false, true, 0xe202 },
    { "Menu MODE", "Controls", 0x8012974, 80, 8, 6, false, false, 0x3966 },
    { "SMART option outline", "Controls", 0x80129c4, 80, 19, 1, false, true, 0xe202 },
    { "SMART option focus", "Controls", 0x8012a82, 80, 19, 1, false, true, 0xe1fe },
    { "SMART option selected", "Controls", 0x8012b40, 80, 19, 1, false, true, 0xe1fe },
    { "RBA option outline", "Controls", 0x8012bfe, 80, 19, 1, false, true, 0xe1fe },
    { "RBA option focus", "Controls", 0x8012cbc, 80, 19, 1, false, true, 0xe202 },
    { "RBA option selected", "Controls", 0x8012d7a, 80, 19, 1, false, true, 0xe202 },
    { "TC option outline", "Controls", 0x8012e38, 80, 19, 1, false, true, 0xe1fe },
    { "TC option focus", "Controls", 0x8012ef6, 80, 19, 1, false, true, 0xe206 },
    { "TC option selected", "Controls", 0x8012fb4, 80, 19, 1, false, true, 0xe206 },
    { "Logo VOOPOO (service)", "Screens", 0x8013072, 80, 10, 13, false, false, 0x917c },
    { "CHIP ID panel", "Screens", 0x80130d6, 74, 63, 13, false, false, 0x917c },
    { "LEVEL CODE panel", "Screens", 0x801334c, 74, 46, 13, false, false, 0x917c },
    { "Logo VOOPOO (boot, rotated clockwise)", "Screens", 0x8013518, 22, 136, 6, true, false, 0x2598 },
    { "OFF", "Screens", 0x8014c78, 39, 18, 6, true, false, 0x258c },
    { "Logo DRAG X PRO (boot)", "Screens", 0x80151f4, 80, 37, 13, false, false, 0x24a4 },
    { "GENE.FAN version (boot)", "Screens", 0x8015366, 80, 32, 6, false, false, 0x24ce },
    { "Battery charging (empty)", "Animation", 0x80154a6, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (1 segment)", "Animation", 0x801550c, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (2 segments)", "Animation", 0x8015572, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (3 segments)", "Animation", 0x80155d8, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (4 segments)", "Animation", 0x801563e, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (full)", "Animation", 0x80156a4, 47, 17, 8, false, false, 0x3216 },
    { "CHARGING", "Screens", 0x801570a, 61, 10, 6, true, false, 0x3388 },
    { "Battery charging bolt", "Controls", 0x8015bce, 11, 13, 8, false, false, 0x3216 },
    { "Icon Time Out", "Alerts", 0x80161e3, 29, 29, 10, false, false, 0x3b8e },
    { "Time Out", "Alerts", 0x8016257, 56, 43, 6, false, false, 0x3ba6 },
    { "Icon Max Power", "Alerts", 0x8016384, 23, 39, 10, false, false, 0x353a },
    { "Max Power", "Alerts", 0x80163f9, 56, 43, 6, false, false, 0x3552 },
    { "Icon Atomizer Short", "Alerts", 0x8016526, 45, 30, 9, false, false, 0x3140 },
    { "Atomizer Short", "Alerts", 0x80165da, 56, 43, 6, false, false, 0x3158 },
    { "Icon Low Resistance", "Alerts", 0x8016707, 25, 41, 9, false, false, 0x30dc },
    { "Low Resistance", "Alerts", 0x80167ab, 56, 43, 6, false, false, 0x30f4 },
    { "Icon Check Battery", "Alerts", 0x80168d8, 20, 39, 9, false, false, 0x346a },
    { "Check Battery", "Alerts", 0x801694d, 56, 43, 6, false, false, 0x3482 },
    { "Icon Low Power", "Alerts", 0x8016a7a, 20, 39, 9, false, false, 0x34d2 },
    { "Low Power", "Alerts", 0x8016aef, 56, 43, 6, false, false, 0x34ea },
    { "Icon Lock", "Controls", 0x8016c1c, 29, 31, 9, false, false, 0x3c88 },
    { "Press Fire 3 times", "Alerts", 0x8016c98, 56, 43, 6, false, false, 0x3c24 },
    { "Slide The Switch", "Alerts", 0x8016de8, 56, 43, 6, false, false, 0x3c24 },
    { "Icon Unlock", "Controls", 0x8016f38, 29, 31, 12, false, false, 0x3c0c },
    { "Icon Check Atomizer (tank)", "Alerts", 0x8016fb4, 20, 19, 9, false, false, 0x33e8 },
    { "Icon Check Atomizer (base)", "Alerts", 0x8016fed, 20, 29, 11, false, false, 0x3400 },
    { "Check Atomizer", "Alerts", 0x8017044, 56, 43, 6, false, false, 0x3418 },
    { "Icon High Temp", "Alerts", 0x8017171, 31, 34, 10, false, false, 0x3b28 },
    { "High Temp", "Alerts", 0x80171f9, 56, 43, 6, false, false, 0x3b40 },
    { "Icon Puff Cleared", "Alerts", 0x8017326, 44, 22, 13, false, false, 0x38ea },
    { "Puff Cleared", "Alerts", 0x80173aa, 56, 43, 6, false, false, 0x3902 },
    { "Icon Lock (main screen)", "Controls", 0x80179d3, 17, 25, 9, false, false, 0x5e82 },
    { "Mode SMART (badge)", "Labels", 0x8017a1e, 17, 25, 1, false, true, 0xe1fe },
    { "Mode RBA (badge)", "Labels", 0x8017a69, 17, 25, 1, false, true, 0xe202 },
    { "Unit W", "Units", 0x8017ab4, 17, 15, 6, false, false, 0x5f98 },
};
constexpr ColorSite kxt3110Sites[] = {
    { 0x24a4, 0x71e0f64f, 0xffe0, 13, 1 },
    { 0x24ce, 0x71fff64f, 0xffff, 6, 1 },
    { 0x30dc, 0x4168f44f, 0xe800, 9, 1 },
    { 0x30f4, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3140, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3158, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3216, 0x77fff64f, 0xffff, 8, 7 },
    { 0x33e8, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3400, 0x715ff240, 0x75f, 11, 1 },
    { 0x3418, 0x71fff64f, 0xffff, 6, 1 },
    { 0x346a, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3482, 0x71fff64f, 0xffff, 6, 1 },
    { 0x34d2, 0x4168f44f, 0xe800, 9, 1 },
    { 0x34ea, 0x71fff64f, 0xffff, 6, 1 },
    { 0x353a, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x3552, 0x71fff64f, 0xffff, 6, 1 },
    { 0x35b6, 0x72fff64f, 0xffff, 6, 2 },
    { 0x3796, 0x73fff64f, 0xffff, 6, 3 },
    { 0x38ea, 0x71e0f64f, 0xffe0, 13, 1 },
    { 0x3902, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3966, 0x73fff64f, 0xffff, 6, 3 },
    { 0x3b28, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x3b40, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3b8e, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x3ba6, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3c0c, 0x71e0f645, 0x5fe0, 12, 1 },
    { 0x3c24, 0x70fff64f, 0xffff, 6, 0 },
    { 0x3c88, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3ca0, 0x70fff64f, 0xffff, 6, 0 },
    { 0x40ba, 0x71fff64f, 0xffff, 6, 1 },
    { 0x43de, 0x4410f248, 0x8410, 14, 4 },
    { 0x457c, 0x76fff64f, 0xffff, 8, 6 },
    { 0x4700, 0x77fff64f, 0xffff, 8, 7 },
    { 0x4850, 0x77fff64f, 0xffff, 8, 7 },
    { 0x49c4, 0x76fff64f, 0xffff, 8, 6 },
    { 0x4af6, 0x76fff64f, 0xffff, 6, 6 },
    { 0x5ed6, 0x4768f44f, 0xe800, 9, 7 },
    { 0x5f38, 0x70fff64f, 0xffff, 7, 0 },
    { 0x5f8a, 0x71fff64f, 0xffff, 7, 1 },
    { 0x5fec, 0x70fff64f, 0xffff, 6, 0 },
    { 0x6046, 0x4968f44f, 0xe800, 9, 9 },
    { 0x60a0, 0x71fff64f, 0xffff, 7, 1 },
    { 0x60e8, 0x71fff64f, 0xffff, 7, 1 },
    { 0x6136, 0x71fff64f, 0xffff, 7, 1 },
    { 0x6196, 0x70fff64f, 0xffff, 6, 0 },
    { 0x65fe, 0x70fff64f, 0xffff, 6, 0 },
    { 0x6694, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x676a, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x6992, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x69cc, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x6a00, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6a3a, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x6a96, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x6d42, 0x6080f44f, 0x400, 4, 0 },
    { 0x6d46, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d68, 0x6080f44f, 0x400, 4, 0 },
    { 0x6d6c, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d86, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x6dbe, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6dc2, 0x6080f44f, 0x400, 4, 0 },
    { 0x6de4, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6de8, 0x6080f44f, 0x400, 4, 0 },
    { 0x6e0a, 0x79fff64f, 0xffff, 8, 9 },
    { 0x7096, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x709a, 0x6180f44f, 0x400, 4, 1 },
    { 0x71de, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x7228, 0x77fff64f, 0xffff, 8, 7 },
    { 0x732c, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x7372, 0x78fff64f, 0xffff, 8, 8 },
    { 0x74b0, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x752e, 0x78fff64f, 0xffff, 8, 8 },
    { 0x76d6, 0x79fff64f, 0xffff, 7, 9 },
    { 0x77aa, 0x4168f44f, 0xe800, 9, 1 },
    { 0x77ea, 0x66fcf44f, 0x7e0, 5, 6 },
    { 0x7830, 0x76fff64f, 0xffff, 8, 6 },
    { 0x91d0, 0x7be0f64f, 0xffe0, 13, 11 },
    { 0x9294, 0x78fff64f, 0xffff, 8, 8 },
    { 0xe3c8, 0x7c00, 0x7c00, 0, -1 },
    { 0xe3ca, 0xffe0, 0xffe0, 1, -1 },
    { 0xe3cc, 0x3cf, 0x3cf, 2, -1 },
    { 0xe3ce, 0x7ff, 0x7ff, 3, -1 },
    { 0xe3d0, 0x400, 0x400, 4, -1 },
    { 0xe3d2, 0x7e0, 0x7e0, 5, -1 },
};
constexpr FontBank kxt3110Banks[] = {
    { 0x8017833, 5, 8, 11, 0 },
    { 0x8015db4, 6, 10, 11, 6 },
    { 0x8015e22, 7, 13, 17, 7 },
    { 0x80176a3, 11, 20, 10, 11 },
    { 0x801788b, 10, 6, 11, 12 },
    { 0x8017b3f, 23, 13, 2, 13 },
    { 0x801790f, 30, 14, 10, 14 },
    { 0x8017b8d, 23, 6, 1, 15 },
    { 0x8015eff, 19, 40, 10, 19 },
};
constexpr Resource kxt3110Resources[] = {
    { "Logo DRAG (standby)", "Screens", 0x8011437, 30, 10, 6, false, false, 0x40ba },
    { "Battery terminal", "Controls", 0x801145f, 2, 9, 6, false, false, 0x4af6 },
    { "Unit W", "Units", 0x8011468, 17, 15, 6, false, false, 0x6196 },
    { "Resistance label R", "Labels", 0x8011495, 38, 13, 1, false, true, 0xe3ce },
    { "Decimal separator (statistics)", "Units", 0x80114d6, 6, 10, 8, false, false, 0x4700 },
    { "Unit Ohm", "Units", 0x80114e0, 16, 10, 8, false, false, 0x4700 },
    { "Voltage panel", "Labels", 0x80114f4, 41, 17, 1, false, true, 0xe3ce },
    { "Decimal separator (statistics)", "Units", 0x801155a, 6, 10, 8, false, false, 0x49c4 },
    { "Time panel top edge", "Labels", 0x8011564, 39, 7, 1, false, true, 0xe3ce },
    { "Unit V", "Units", 0x8011587, 10, 10, 8, false, false, 0x49c4 },
    { "Time panel", "Labels", 0x801159b, 41, 17, 1, false, true, 0xe3ce },
    { "Puff panel top edge", "Labels", 0x8011601, 39, 7, 1, false, true, 0xe3ce },
    { "Decimal separator (statistics)", "Units", 0x8011624, 6, 10, 8, false, false, 0x4850 },
    { "Unit S", "Units", 0x801162e, 6, 10, 8, false, false, 0x4850 },
    { "Puff panel", "Labels", 0x8011638, 41, 13, 1, false, true, 0xe3ca },
    { "Material NI", "Labels", 0x80116b8, 34, 23, 5, false, false, 0x6a96 },
    { "Material TI", "Labels", 0x801172b, 34, 23, 5, false, false, 0x6a96 },
    { "Material SS", "Labels", 0x801179e, 34, 23, 5, false, false, 0x6a96 },
    { "TC panel left edge", "Labels", 0x8011811, 8, 33, 5, false, false, 0x6a96 },
    { "TC panel right edge", "Labels", 0x8011832, 8, 33, 5, false, false, 0x6a96 },
    { "TC panel lower-left corner", "Labels", 0x8011853, 14, 18, 5, false, false, 0x6d46 },
    { "TC panel lower-right corner", "Labels", 0x8011877, 14, 18, 5, false, false, 0x6d6c },
    { "Material NI option", "Labels", 0x801189b, 28, 18, 5, false, false, 0x6d86 },
    { "Material TI option", "Labels", 0x80118e3, 28, 18, 5, false, false, 0x6d86 },
    { "Material SS option", "Labels", 0x801192b, 28, 18, 5, false, false, 0x6d86 },
    { "Unit W (menu)", "Units", 0x8011973, 28, 18, 5, false, false, 0x6d86 },
    { "Unit Ohm (menu)", "Units", 0x80119bb, 28, 18, 5, false, false, 0x6d86 },
    { "Decimal separator (TC)", "Units", 0x8011a03, 2, 13, 8, false, false, 0x6e0a },
    { "TC panel bottom edge", "Labels", 0x8011a10, 56, 3, 4, false, false, 0x6dc2 },
    { "Icon Lock (TC)", "Controls", 0x8011a25, 20, 20, 9, false, false, 0x77aa },
    { "Unit degrees F", "Units", 0x8011a61, 20, 20, 7, false, false, 0x76d6 },
    { "Unit degrees C", "Units", 0x8011ad9, 20, 20, 7, false, false, 0x76d6 },
    { "TC Watt / Time panel", "Labels", 0x8011b51, 30, 26, 5, false, false, 0x6694 },
    { "TC Ohms / Puff panel", "Labels", 0x8011bb9, 30, 26, 5, false, false, 0x6694 },
    { "Material NI (round badge)", "Labels", 0x8011c21, 20, 26, 5, false, false, 0x6694 },
    { "Material TI (round badge)", "Labels", 0x8011c6f, 20, 26, 5, false, false, 0x6694 },
    { "Material SS (round badge)", "Labels", 0x8011cbd, 20, 26, 5, false, false, 0x6694 },
    { "TC panel upper-left corner", "Labels", 0x8011d0b, 5, 16, 5, false, false, 0x71de },
    { "Decimal separator (TC)", "Units", 0x8011d1b, 2, 13, 8, false, false, 0x7228 },
    { "TC panel upper-right corner", "Labels", 0x8011d28, 5, 16, 5, false, false, 0x74b0 },
    { "Decimal separator (TC)", "Units", 0x8011d38, 2, 13, 8, false, false, 0x752e },
    { "TC panel lower-left edge", "Labels", 0x8011d45, 34, 3, 5, false, false, 0x77ea },
    { "Decimal separator (TC)", "Units", 0x8011d54, 2, 13, 8, false, false, 0x7830 },
    { "TC panel lower-right edge", "Labels", 0x8011d61, 46, 3, 5, false, false, 0x732c },
    { "TC panel top border", "Labels", 0x8011d73, 80, 6, 5, false, false, 0x676a },
    { "TC panel bottom border", "Labels", 0x8011daf, 80, 7, 5, false, false, 0x676a },
    { "Menu SETTING", "Controls", 0x8011e20, 80, 8, 6, false, false, 0x35b6 },
    { "SWITCH option outline", "Controls", 0x8011e70, 80, 19, 1, false, true, 0xe3ce },
    { "SWITCH option focus", "Controls", 0x8011f2e, 80, 19, 1, false, true, 0xe3ca },
    { "SWITCH option selected", "Controls", 0x8011fec, 80, 19, 1, false, true, 0xe3ca },
    { "MODE option outline", "Controls", 0x80120aa, 80, 19, 1, false, true, 0xe3ca },
    { "MODE option focus", "Controls", 0x8012168, 80, 19, 1, false, true, 0xe3ce },
    { "MODE option selected", "Controls", 0x8012226, 80, 19, 1, false, true, 0xe3ce },
    { "EXIT option outline", "Controls", 0x80122e4, 80, 19, 1, false, true, 0xe3ca },
    { "EXIT option focus", "Controls", 0x80123a2, 80, 19, 1, false, true, 0xe3d2 },
    { "EXIT option selected", "Controls", 0x8012460, 80, 19, 1, false, true, 0xe3d2 },
    { "Menu SWITCH", "Controls", 0x801251e, 80, 7, 6, false, false, 0x3796 },
    { "Lock switch outline", "Controls", 0x8012564, 80, 25, 1, false, true, 0xe3ce },
    { "Lock switch focus", "Controls", 0x801265e, 80, 25, 1, false, true, 0xe3ca },
    { "Lock switch selected", "Controls", 0x8012758, 80, 25, 1, false, true, 0xe3ca },
    { "Power switch outline", "Controls", 0x8012852, 80, 25, 1, false, true, 0xe3ca },
    { "Power switch focus", "Controls", 0x801294c, 80, 25, 1, false, true, 0xe3ce },
    { "Power switch selected", "Controls", 0x8012a46, 80, 25, 1, false, true, 0xe3ce },
    { "Menu MODE", "Controls", 0x8012b40, 80, 8, 6, false, false, 0x3966 },
    { "SMART option outline", "Controls", 0x8012b90, 80, 19, 1, false, true, 0xe3ce },
    { "SMART option focus", "Controls", 0x8012c4e, 80, 19, 1, false, true, 0xe3ca },
    { "SMART option selected", "Controls", 0x8012d0c, 80, 19, 1, false, true, 0xe3ca },
    { "RBA option outline", "Controls", 0x8012dca, 80, 19, 1, false, true, 0xe3ca },
    { "RBA option focus", "Controls", 0x8012e88, 80, 19, 1, false, true, 0xe3ce },
    { "RBA option selected", "Controls", 0x8012f46, 80, 19, 1, false, true, 0xe3ce },
    { "TC option outline", "Controls", 0x8013004, 80, 19, 1, false, true, 0xe3ca },
    { "TC option focus", "Controls", 0x80130c2, 80, 19, 1, false, true, 0xe3d2 },
    { "TC option selected", "Controls", 0x8013180, 80, 19, 1, false, true, 0xe3d2 },
    { "Logo VOOPOO (service)", "Screens", 0x801323e, 80, 10, 13, false, false, 0x91d0 },
    { "CHIP ID panel", "Screens", 0x80132a2, 74, 63, 13, false, false, 0x91d0 },
    { "LEVEL CODE panel", "Screens", 0x8013518, 74, 46, 13, false, false, 0x91d0 },
    { "Logo VOOPOO (boot, rotated clockwise)", "Screens", 0x80136e4, 22, 136, 6, true, false, 0x2598 },
    { "OFF", "Screens", 0x8014e44, 39, 18, 6, true, false, 0x258c },
    { "Logo DRAG X PRO (boot)", "Screens", 0x80153c0, 80, 37, 13, false, false, 0x24a4 },
    { "GENE.FAN version (boot)", "Screens", 0x8015532, 80, 32, 6, false, false, 0x24ce },
    { "Battery charging (empty)", "Animation", 0x8015672, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (1 segment)", "Animation", 0x80156d8, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (2 segments)", "Animation", 0x801573e, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (3 segments)", "Animation", 0x80157a4, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (4 segments)", "Animation", 0x801580a, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (full)", "Animation", 0x8015870, 47, 17, 8, false, false, 0x3216 },
    { "CHARGING", "Screens", 0x80158d6, 61, 10, 6, true, false, 0x3388 },
    { "Battery charging bolt", "Controls", 0x8015d9a, 11, 13, 8, false, false, 0x3216 },
    { "Icon Time Out", "Alerts", 0x80163af, 29, 29, 10, false, false, 0x3b8e },
    { "Time Out", "Alerts", 0x8016423, 56, 43, 6, false, false, 0x3ba6 },
    { "Icon Max Power", "Alerts", 0x8016550, 23, 39, 10, false, false, 0x353a },
    { "Max Power", "Alerts", 0x80165c5, 56, 43, 6, false, false, 0x3552 },
    { "Icon Atomizer Short", "Alerts", 0x80166f2, 45, 30, 9, false, false, 0x3140 },
    { "Atomizer Short", "Alerts", 0x80167a6, 56, 43, 6, false, false, 0x3158 },
    { "Icon Low Resistance", "Alerts", 0x80168d3, 25, 41, 9, false, false, 0x30dc },
    { "Low Resistance", "Alerts", 0x8016977, 56, 43, 6, false, false, 0x30f4 },
    { "Icon Check Battery", "Alerts", 0x8016aa4, 20, 39, 9, false, false, 0x346a },
    { "Check Battery", "Alerts", 0x8016b19, 56, 43, 6, false, false, 0x3482 },
    { "Icon Low Power", "Alerts", 0x8016c46, 20, 39, 9, false, false, 0x34d2 },
    { "Low Power", "Alerts", 0x8016cbb, 56, 43, 6, false, false, 0x34ea },
    { "Icon Lock", "Controls", 0x8016de8, 29, 31, 9, false, false, 0x3c88 },
    { "Press Fire 3 times", "Alerts", 0x8016e64, 56, 43, 6, false, false, 0x3c24 },
    { "Slide The Switch", "Alerts", 0x8016fb4, 56, 43, 6, false, false, 0x3c24 },
    { "Icon Unlock", "Controls", 0x8017104, 29, 31, 12, false, false, 0x3c0c },
    { "Icon Check Atomizer (tank)", "Alerts", 0x8017180, 20, 19, 9, false, false, 0x33e8 },
    { "Icon Check Atomizer (base)", "Alerts", 0x80171b9, 20, 29, 11, false, false, 0x3400 },
    { "Check Atomizer", "Alerts", 0x8017210, 56, 43, 6, false, false, 0x3418 },
    { "Icon High Temp", "Alerts", 0x801733d, 31, 34, 10, false, false, 0x3b28 },
    { "High Temp", "Alerts", 0x80173c5, 56, 43, 6, false, false, 0x3b40 },
    { "Icon Puff Cleared", "Alerts", 0x80174f2, 44, 22, 13, false, false, 0x38ea },
    { "Puff Cleared", "Alerts", 0x8017576, 56, 43, 6, false, false, 0x3902 },
    { "Icon Lock (main screen)", "Controls", 0x8017b9f, 17, 25, 9, false, false, 0x5ed6 },
    { "Mode SMART (badge)", "Labels", 0x8017bea, 17, 25, 1, false, true, 0xe3ca },
    { "Mode RBA (badge)", "Labels", 0x8017c35, 17, 25, 1, false, true, 0xe3ce },
    { "Unit W", "Units", 0x8017c80, 17, 15, 6, false, false, 0x5fec },
};
constexpr ColorSite kxt3111Sites[] = {
    { 0x24a4, 0x71e0f64f, 0xffe0, 13, 1 },
    { 0x24ce, 0x71fff64f, 0xffff, 6, 1 },
    { 0x30dc, 0x4168f44f, 0xe800, 9, 1 },
    { 0x30f4, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3140, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3158, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3216, 0x77fff64f, 0xffff, 8, 7 },
    { 0x33e8, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3400, 0x715ff240, 0x75f, 11, 1 },
    { 0x3418, 0x71fff64f, 0xffff, 6, 1 },
    { 0x346a, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3482, 0x71fff64f, 0xffff, 6, 1 },
    { 0x34d2, 0x4168f44f, 0xe800, 9, 1 },
    { 0x34ea, 0x71fff64f, 0xffff, 6, 1 },
    { 0x353a, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x3552, 0x71fff64f, 0xffff, 6, 1 },
    { 0x35b6, 0x72fff64f, 0xffff, 6, 2 },
    { 0x3796, 0x73fff64f, 0xffff, 6, 3 },
    { 0x38ea, 0x71e0f64f, 0xffe0, 13, 1 },
    { 0x3902, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3966, 0x73fff64f, 0xffff, 6, 3 },
    { 0x3b28, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x3b40, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3b8e, 0x416bf44f, 0xeb00, 10, 1 },
    { 0x3ba6, 0x71fff64f, 0xffff, 6, 1 },
    { 0x3c0c, 0x71e0f645, 0x5fe0, 12, 1 },
    { 0x3c24, 0x70fff64f, 0xffff, 6, 0 },
    { 0x3c88, 0x4168f44f, 0xe800, 9, 1 },
    { 0x3ca0, 0x70fff64f, 0xffff, 6, 0 },
    { 0x40ba, 0x71fff64f, 0xffff, 6, 1 },
    { 0x43de, 0x4410f248, 0x8410, 14, 4 },
    { 0x457c, 0x76fff64f, 0xffff, 8, 6 },
    { 0x4700, 0x77fff64f, 0xffff, 8, 7 },
    { 0x4850, 0x77fff64f, 0xffff, 8, 7 },
    { 0x49c4, 0x76fff64f, 0xffff, 8, 6 },
    { 0x4af6, 0x76fff64f, 0xffff, 6, 6 },
    { 0x5ed6, 0x4768f44f, 0xe800, 9, 7 },
    { 0x5f38, 0x70fff64f, 0xffff, 7, 0 },
    { 0x5f8a, 0x71fff64f, 0xffff, 7, 1 },
    { 0x5fec, 0x70fff64f, 0xffff, 6, 0 },
    { 0x6046, 0x4968f44f, 0xe800, 9, 9 },
    { 0x60a0, 0x71fff64f, 0xffff, 7, 1 },
    { 0x60e8, 0x71fff64f, 0xffff, 7, 1 },
    { 0x6136, 0x71fff64f, 0xffff, 7, 1 },
    { 0x6196, 0x70fff64f, 0xffff, 6, 0 },
    { 0x65fe, 0x70fff64f, 0xffff, 6, 0 },
    { 0x6694, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x676a, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x6992, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x69cc, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x6a00, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6a3a, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x6a96, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x6d42, 0x6080f44f, 0x400, 4, 0 },
    { 0x6d46, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d68, 0x6080f44f, 0x400, 4, 0 },
    { 0x6d6c, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6d86, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x6dbe, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6dc2, 0x6080f44f, 0x400, 4, 0 },
    { 0x6de4, 0x60fcf44f, 0x7e0, 5, 0 },
    { 0x6de8, 0x6080f44f, 0x400, 4, 0 },
    { 0x6e0a, 0x79fff64f, 0xffff, 8, 9 },
    { 0x7096, 0x61fcf44f, 0x7e0, 5, 1 },
    { 0x709a, 0x6180f44f, 0x400, 4, 1 },
    { 0x71de, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x7228, 0x77fff64f, 0xffff, 8, 7 },
    { 0x732c, 0x67fcf44f, 0x7e0, 5, 7 },
    { 0x7372, 0x78fff64f, 0xffff, 8, 8 },
    { 0x74b0, 0x68fcf44f, 0x7e0, 5, 8 },
    { 0x752e, 0x78fff64f, 0xffff, 8, 8 },
    { 0x76d6, 0x79fff64f, 0xffff, 7, 9 },
    { 0x77aa, 0x4168f44f, 0xe800, 9, 1 },
    { 0x77ea, 0x66fcf44f, 0x7e0, 5, 6 },
    { 0x7830, 0x76fff64f, 0xffff, 8, 6 },
    { 0x91d0, 0x7be0f64f, 0xffe0, 13, 11 },
    { 0x9294, 0x78fff64f, 0xffff, 8, 8 },
    { 0xe7a8, 0x7c00, 0x7c00, 0, -1 },
    { 0xe7aa, 0xffe0, 0xffe0, 1, -1 },
    { 0xe7ac, 0x3cf, 0x3cf, 2, -1 },
    { 0xe7ae, 0x7ff, 0x7ff, 3, -1 },
    { 0xe7b0, 0x400, 0x400, 4, -1 },
    { 0xe7b2, 0x7e0, 0x7e0, 5, -1 },
};
constexpr FontBank kxt3111Banks[] = {
    { 0x8017c13, 5, 8, 11, 0 },
    { 0x8016194, 6, 10, 11, 6 },
    { 0x8016202, 7, 13, 17, 7 },
    { 0x8017a83, 11, 20, 10, 11 },
    { 0x8017c6b, 10, 6, 11, 12 },
    { 0x8017f1f, 23, 13, 2, 13 },
    { 0x8017cef, 30, 14, 10, 14 },
    { 0x8017f6d, 23, 6, 1, 15 },
    { 0x80162df, 19, 40, 10, 19 },
};
constexpr Resource kxt3111Resources[] = {
    { "Logo DRAG (standby)", "Screens", 0x8011817, 30, 10, 6, false, false, 0x40ba },
    { "Battery terminal", "Controls", 0x801183f, 2, 9, 6, false, false, 0x4af6 },
    { "Unit W", "Units", 0x8011848, 17, 15, 6, false, false, 0x6196 },
    { "Resistance label R", "Labels", 0x8011875, 38, 13, 1, false, true, 0xe7ae },
    { "Decimal separator (statistics)", "Units", 0x80118b6, 6, 10, 8, false, false, 0x4700 },
    { "Unit Ohm", "Units", 0x80118c0, 16, 10, 8, false, false, 0x4700 },
    { "Voltage panel", "Labels", 0x80118d4, 41, 17, 1, false, true, 0xe7ae },
    { "Decimal separator (statistics)", "Units", 0x801193a, 6, 10, 8, false, false, 0x49c4 },
    { "Time panel top edge", "Labels", 0x8011944, 39, 7, 1, false, true, 0xe7ae },
    { "Unit V", "Units", 0x8011967, 10, 10, 8, false, false, 0x49c4 },
    { "Time panel", "Labels", 0x801197b, 41, 17, 1, false, true, 0xe7ae },
    { "Puff panel top edge", "Labels", 0x80119e1, 39, 7, 1, false, true, 0xe7ae },
    { "Decimal separator (statistics)", "Units", 0x8011a04, 6, 10, 8, false, false, 0x4850 },
    { "Unit S", "Units", 0x8011a0e, 6, 10, 8, false, false, 0x4850 },
    { "Puff panel", "Labels", 0x8011a18, 41, 13, 1, false, true, 0xe7aa },
    { "Material NI", "Labels", 0x8011a98, 34, 23, 5, false, false, 0x6a96 },
    { "Material TI", "Labels", 0x8011b0b, 34, 23, 5, false, false, 0x6a96 },
    { "Material SS", "Labels", 0x8011b7e, 34, 23, 5, false, false, 0x6a96 },
    { "TC panel left edge", "Labels", 0x8011bf1, 8, 33, 5, false, false, 0x6a96 },
    { "TC panel right edge", "Labels", 0x8011c12, 8, 33, 5, false, false, 0x6a96 },
    { "TC panel lower-left corner", "Labels", 0x8011c33, 14, 18, 5, false, false, 0x6d46 },
    { "TC panel lower-right corner", "Labels", 0x8011c57, 14, 18, 5, false, false, 0x6d6c },
    { "Material NI option", "Labels", 0x8011c7b, 28, 18, 5, false, false, 0x6d86 },
    { "Material TI option", "Labels", 0x8011cc3, 28, 18, 5, false, false, 0x6d86 },
    { "Material SS option", "Labels", 0x8011d0b, 28, 18, 5, false, false, 0x6d86 },
    { "Unit W (menu)", "Units", 0x8011d53, 28, 18, 5, false, false, 0x6d86 },
    { "Unit Ohm (menu)", "Units", 0x8011d9b, 28, 18, 5, false, false, 0x6d86 },
    { "Decimal separator (TC)", "Units", 0x8011de3, 2, 13, 8, false, false, 0x6e0a },
    { "TC panel bottom edge", "Labels", 0x8011df0, 56, 3, 4, false, false, 0x6dc2 },
    { "Icon Lock (TC)", "Controls", 0x8011e05, 20, 20, 9, false, false, 0x77aa },
    { "Unit degrees F", "Units", 0x8011e41, 20, 20, 7, false, false, 0x76d6 },
    { "Unit degrees C", "Units", 0x8011eb9, 20, 20, 7, false, false, 0x76d6 },
    { "TC Watt / Time panel", "Labels", 0x8011f31, 30, 26, 5, false, false, 0x6694 },
    { "TC Ohms / Puff panel", "Labels", 0x8011f99, 30, 26, 5, false, false, 0x6694 },
    { "Material NI (round badge)", "Labels", 0x8012001, 20, 26, 5, false, false, 0x6694 },
    { "Material TI (round badge)", "Labels", 0x801204f, 20, 26, 5, false, false, 0x6694 },
    { "Material SS (round badge)", "Labels", 0x801209d, 20, 26, 5, false, false, 0x6694 },
    { "TC panel upper-left corner", "Labels", 0x80120eb, 5, 16, 5, false, false, 0x71de },
    { "Decimal separator (TC)", "Units", 0x80120fb, 2, 13, 8, false, false, 0x7228 },
    { "TC panel upper-right corner", "Labels", 0x8012108, 5, 16, 5, false, false, 0x74b0 },
    { "Decimal separator (TC)", "Units", 0x8012118, 2, 13, 8, false, false, 0x752e },
    { "TC panel lower-left edge", "Labels", 0x8012125, 34, 3, 5, false, false, 0x77ea },
    { "Decimal separator (TC)", "Units", 0x8012134, 2, 13, 8, false, false, 0x7830 },
    { "TC panel lower-right edge", "Labels", 0x8012141, 46, 3, 5, false, false, 0x732c },
    { "TC panel top border", "Labels", 0x8012153, 80, 6, 5, false, false, 0x676a },
    { "TC panel bottom border", "Labels", 0x801218f, 80, 7, 5, false, false, 0x676a },
    { "Menu SETTING", "Controls", 0x8012200, 80, 8, 6, false, false, 0x35b6 },
    { "SWITCH option outline", "Controls", 0x8012250, 80, 19, 1, false, true, 0xe7ae },
    { "SWITCH option focus", "Controls", 0x801230e, 80, 19, 1, false, true, 0xe7aa },
    { "SWITCH option selected", "Controls", 0x80123cc, 80, 19, 1, false, true, 0xe7aa },
    { "MODE option outline", "Controls", 0x801248a, 80, 19, 1, false, true, 0xe7aa },
    { "MODE option focus", "Controls", 0x8012548, 80, 19, 1, false, true, 0xe7ae },
    { "MODE option selected", "Controls", 0x8012606, 80, 19, 1, false, true, 0xe7ae },
    { "EXIT option outline", "Controls", 0x80126c4, 80, 19, 1, false, true, 0xe7aa },
    { "EXIT option focus", "Controls", 0x8012782, 80, 19, 1, false, true, 0xe7b2 },
    { "EXIT option selected", "Controls", 0x8012840, 80, 19, 1, false, true, 0xe7b2 },
    { "Menu SWITCH", "Controls", 0x80128fe, 80, 7, 6, false, false, 0x3796 },
    { "Lock switch outline", "Controls", 0x8012944, 80, 25, 1, false, true, 0xe7ae },
    { "Lock switch focus", "Controls", 0x8012a3e, 80, 25, 1, false, true, 0xe7aa },
    { "Lock switch selected", "Controls", 0x8012b38, 80, 25, 1, false, true, 0xe7aa },
    { "Power switch outline", "Controls", 0x8012c32, 80, 25, 1, false, true, 0xe7aa },
    { "Power switch focus", "Controls", 0x8012d2c, 80, 25, 1, false, true, 0xe7ae },
    { "Power switch selected", "Controls", 0x8012e26, 80, 25, 1, false, true, 0xe7ae },
    { "Menu MODE", "Controls", 0x8012f20, 80, 8, 6, false, false, 0x3966 },
    { "SMART option outline", "Controls", 0x8012f70, 80, 19, 1, false, true, 0xe7ae },
    { "SMART option focus", "Controls", 0x801302e, 80, 19, 1, false, true, 0xe7aa },
    { "SMART option selected", "Controls", 0x80130ec, 80, 19, 1, false, true, 0xe7aa },
    { "RBA option outline", "Controls", 0x80131aa, 80, 19, 1, false, true, 0xe7aa },
    { "RBA option focus", "Controls", 0x8013268, 80, 19, 1, false, true, 0xe7ae },
    { "RBA option selected", "Controls", 0x8013326, 80, 19, 1, false, true, 0xe7ae },
    { "TC option outline", "Controls", 0x80133e4, 80, 19, 1, false, true, 0xe7aa },
    { "TC option focus", "Controls", 0x80134a2, 80, 19, 1, false, true, 0xe7b2 },
    { "TC option selected", "Controls", 0x8013560, 80, 19, 1, false, true, 0xe7b2 },
    { "Logo VOOPOO (service)", "Screens", 0x801361e, 80, 10, 13, false, false, 0x91d0 },
    { "CHIP ID panel", "Screens", 0x8013682, 74, 63, 13, false, false, 0x91d0 },
    { "LEVEL CODE panel", "Screens", 0x80138f8, 74, 46, 13, false, false, 0x91d0 },
    { "Logo VOOPOO (boot, rotated clockwise)", "Screens", 0x8013ac4, 22, 136, 6, true, false, 0x2598 },
    { "OFF", "Screens", 0x8015224, 39, 18, 6, true, false, 0x258c },
    { "Logo DRAG X PRO (boot)", "Screens", 0x80157a0, 80, 37, 13, false, false, 0x24a4 },
    { "GENE.FAN version (boot)", "Screens", 0x8015912, 80, 32, 6, false, false, 0x24ce },
    { "Battery charging (empty)", "Animation", 0x8015a52, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (1 segment)", "Animation", 0x8015ab8, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (2 segments)", "Animation", 0x8015b1e, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (3 segments)", "Animation", 0x8015b84, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (4 segments)", "Animation", 0x8015bea, 47, 17, 8, false, false, 0x3216 },
    { "Battery charging (full)", "Animation", 0x8015c50, 47, 17, 8, false, false, 0x3216 },
    { "CHARGING", "Screens", 0x8015cb6, 61, 10, 6, true, false, 0x3388 },
    { "Battery charging bolt", "Controls", 0x801617a, 11, 13, 8, false, false, 0x3216 },
    { "Icon Time Out", "Alerts", 0x801678f, 29, 29, 10, false, false, 0x3b8e },
    { "Time Out", "Alerts", 0x8016803, 56, 43, 6, false, false, 0x3ba6 },
    { "Icon Max Power", "Alerts", 0x8016930, 23, 39, 10, false, false, 0x353a },
    { "Max Power", "Alerts", 0x80169a5, 56, 43, 6, false, false, 0x3552 },
    { "Icon Atomizer Short", "Alerts", 0x8016ad2, 45, 30, 9, false, false, 0x3140 },
    { "Atomizer Short", "Alerts", 0x8016b86, 56, 43, 6, false, false, 0x3158 },
    { "Icon Low Resistance", "Alerts", 0x8016cb3, 25, 41, 9, false, false, 0x30dc },
    { "Low Resistance", "Alerts", 0x8016d57, 56, 43, 6, false, false, 0x30f4 },
    { "Icon Check Battery", "Alerts", 0x8016e84, 20, 39, 9, false, false, 0x346a },
    { "Check Battery", "Alerts", 0x8016ef9, 56, 43, 6, false, false, 0x3482 },
    { "Icon Low Power", "Alerts", 0x8017026, 20, 39, 9, false, false, 0x34d2 },
    { "Low Power", "Alerts", 0x801709b, 56, 43, 6, false, false, 0x34ea },
    { "Icon Lock", "Controls", 0x80171c8, 29, 31, 9, false, false, 0x3c88 },
    { "Press Fire 3 times", "Alerts", 0x8017244, 56, 43, 6, false, false, 0x3c24 },
    { "Slide The Switch", "Alerts", 0x8017394, 56, 43, 6, false, false, 0x3c24 },
    { "Icon Unlock", "Controls", 0x80174e4, 29, 31, 12, false, false, 0x3c0c },
    { "Icon Check Atomizer (tank)", "Alerts", 0x8017560, 20, 19, 9, false, false, 0x33e8 },
    { "Icon Check Atomizer (base)", "Alerts", 0x8017599, 20, 29, 11, false, false, 0x3400 },
    { "Check Atomizer", "Alerts", 0x80175f0, 56, 43, 6, false, false, 0x3418 },
    { "Icon High Temp", "Alerts", 0x801771d, 31, 34, 10, false, false, 0x3b28 },
    { "High Temp", "Alerts", 0x80177a5, 56, 43, 6, false, false, 0x3b40 },
    { "Icon Puff Cleared", "Alerts", 0x80178d2, 44, 22, 13, false, false, 0x38ea },
    { "Puff Cleared", "Alerts", 0x8017956, 56, 43, 6, false, false, 0x3902 },
    { "Icon Lock (main screen)", "Controls", 0x8017f7f, 17, 25, 9, false, false, 0x5ed6 },
    { "Mode SMART (badge)", "Labels", 0x8017fca, 17, 25, 1, false, true, 0xe7aa },
    { "Mode RBA (badge)", "Labels", 0x8018015, 17, 25, 1, false, true, 0xe7ae },
    { "Unit W", "Units", 0x8018060, 17, 15, 6, false, false, 0x5fec },
};
constexpr Hardware kHardware[] = {
    { 0x3119, 74672, 0x20002738, 0, 0x48c0, 0x43b0, 0x2bb8, { 0x2d, 0xe9, 0xff, 0x4f, 0x85, 0xb0, 0xc2, 0xf3, 0xc7, 0x06, 0xdd, 0xe9, 0x2d, 0xe9, 0xf0, 0x4f, 0x85, 0xb0, 0x0f, 0x2a, 0xdd, 0xe9, 0x0e, 0xba }, ksf3119Sites, std::size(ksf3119Sites), ksf3119Banks, std::size(ksf3119Banks), ksf3119Resources, std::size(ksf3119Resources) },
    { 0x3118, 85588, 0x20002758, 1, 0x53de, 0x4ee4, 0x36c4, { 0x2d, 0xe9, 0xff, 0x4f, 0x83, 0xb0, 0xc2, 0xf3, 0xc7, 0x08, 0xdd, 0xe9, 0x2d, 0xe9, 0xf0, 0x4f, 0x0c, 0x25, 0x12, 0x24, 0x83, 0xb0, 0x0c, 0x2b }, kst3118Sites, std::size(kst3118Sites), kst3118Banks, std::size(kst3118Banks), kst3118Resources, std::size(kst3118Resources) },
    { 0x3119, 86580, 0x20002758, 1, 0x53de, 0x4ee4, 0x36c4, { 0x2d, 0xe9, 0xff, 0x4f, 0x83, 0xb0, 0xc2, 0xf3, 0xc7, 0x08, 0xdd, 0xe9, 0x2d, 0xe9, 0xf0, 0x4f, 0x0c, 0x25, 0x12, 0x24, 0x83, 0xb0, 0x0c, 0x2b }, kst3119Sites, std::size(kst3119Sites), kst3119Banks, std::size(kst3119Banks), kst3119Resources, std::size(kst3119Resources) },
    { 0x3144, 85540, 0x200026e0, 2, 0x54c6, 0x4fcc, 0x3734, { 0x2d, 0xe9, 0xff, 0x4f, 0x83, 0xb0, 0xc2, 0xf3, 0xc7, 0x08, 0xdd, 0xe9, 0x2d, 0xe9, 0xf0, 0x4f, 0x0c, 0x25, 0x12, 0x24, 0x83, 0xb0, 0x0c, 0x2b }, kxt3144Sites, std::size(kxt3144Sites), kxt3144Banks, std::size(kxt3144Banks), kxt3144Resources, std::size(kxt3144Resources) },
    { 0x3110, 86000, 0x200026e0, 2, 0x54ba, 0x4fc0, 0x3734, { 0x2d, 0xe9, 0xff, 0x4f, 0x83, 0xb0, 0xc2, 0xf3, 0xc7, 0x08, 0xdd, 0xe9, 0x2d, 0xe9, 0xf0, 0x4f, 0x0c, 0x25, 0x12, 0x24, 0x83, 0xb0, 0x0c, 0x2b }, kxt3110Sites, std::size(kxt3110Sites), kxt3110Banks, std::size(kxt3110Banks), kxt3110Resources, std::size(kxt3110Resources) },
    { 0x3111, 86992, 0x200026e0, 2, 0x54ba, 0x4fc0, 0x3734, { 0x2d, 0xe9, 0xff, 0x4f, 0x83, 0xb0, 0xc2, 0xf3, 0xc7, 0x08, 0xdd, 0xe9, 0x2d, 0xe9, 0xf0, 0x4f, 0x0c, 0x25, 0x12, 0x24, 0x83, 0xb0, 0x0c, 0x2b }, kxt3111Sites, std::size(kxt3111Sites), kxt3111Banks, std::size(kxt3111Banks), kxt3111Resources, std::size(kxt3111Resources) },
};

const Hardware* Identity(const HardwareRecord& record) {
    const uint16_t code = uint16_t(record.device_code[0] << 8) | record.device_code[1];
    for (const auto& hw : kHardware) {
        if (code != hw.code || record.image.size() != hw.size || record.linked_base != kBase ||
            record.sp != hw.sp || record.reset != 0x080032b9) continue;
        const auto* image = record.image.data();
        if (std::memcmp(image + hw.bitmap, hw.guards, 12) ||
            std::memcmp(image + hw.digit, hw.guards + 12, 12)) continue;
        const uint32_t table = kBase + hw.sites[hw.siteCount - 6].offset;
        if (image[hw.tableReference] != uint8_t(table) || image[hw.tableReference + 1] != uint8_t(table >> 8) ||
            image[hw.tableReference + 2] != uint8_t(table >> 16) || image[hw.tableReference + 3] != uint8_t(table >> 24)) continue;
        return &hw;
    }
    return nullptr;
}

uint16_t Read16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1] << 8); }

bool SiteColor(const HardwareRecord& record, const ColorSite& site, uint16_t& value) {
    const auto* p = record.image.data() + site.offset;
    if (site.reg < 0) { value = Read16(p); return true; }
    const uint32_t word = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    if (word == site.stock) { value = site.value; return true; }
    // Saved edits use equal-width MOVW, preserving the original destination register.
    return DecodeMovwInstruction(p, site.reg, value);
}

const Hardware* Canonical(uint8_t profile) {
    const uint16_t code = profile == 2 ? 0x3110 : 0x3119;
    for (const auto& hw : kHardware) if (hw.profile == profile && hw.code == code) return &hw;
    return nullptr;
}

const HardwareRecord* FindRecord(const FirmwarePackage& fw, uint16_t code) {
    for (const auto& record : fw.records)
        if ((uint16_t(record.device_code[0] << 8) | record.device_code[1]) == code) return &record;
    return nullptr;
}

const ColorSite* RoleSite(const Hardware& hw, size_t role) {
    for (size_t i = 0; i < hw.siteCount; ++i) if (hw.sites[i].role == role) return hw.sites + i;
    return nullptr;
}

int Profile(FirmwareModel model, FirmwareEdition edition) {
    if (model == FirmwareModel::DragSPro) {
        if (edition == FirmwareEdition::Factory) return 0;
        if (edition == FirmwareEdition::Tc) return 1;
    }
    return model == FirmwareModel::DragXPro && edition == FirmwareEdition::Tc ? 2 : -1;
}

const Resource* ResourceFor(const AssetSpec& asset) {
    const char* prefix = asset.id.compare(0, 12, "pro_factory_") == 0 ? "pro_factory_" :
        asset.id.compare(0, 7, "pro_tc_") == 0 ? "pro_tc_" : nullptr;
    if (!prefix) return nullptr;
    const char* p = asset.id.c_str() + std::strlen(prefix);
    if (*p < '0' || *p > '9') return nullptr;
    size_t index = 0;
    while (*p >= '0' && *p <= '9') { index = index * 10 + size_t(*p++ - '0'); if (index >= 1000) return nullptr; }
    if (*p) return nullptr;
    for (const auto& hw : kHardware)
        if (hw.code == asset.sourceCode && index < hw.resourceCount && hw.resources[index].address == asset.address)
            return hw.resources + index;
    return nullptr;
}

int FontRole(const AssetSpec& asset) {
    if (asset.id.compare(0, 5, "font_") != 0 && asset.id.compare(0, 10, "pro_glyph_") != 0) return -1;
    for (const auto& hw : kHardware) {
        if (hw.code != asset.sourceCode) continue;
        for (size_t i = 0; i < hw.bankCount; ++i) {
            const auto& bank = hw.banks[i];
            if (asset.width == bank.width && asset.height == bank.height && asset.address >= bank.address &&
                asset.address < bank.address + size_t((bank.width + 7) / 8) * bank.height * bank.count)
                return bank.selector == 19 || bank.selector == 14 ? 7 : 8;
        }
    }
    return -1;
}

int RoleIndex(const char* id) {
    if (id) for (size_t i = 0; i < std::size(kRoleIds); ++i) if (std::strcmp(id, kRoleIds[i]) == 0) return int(i);
    return -1;
}
} // namespace

bool Detect(const FirmwarePackage& fw, FirmwareModel& model, FirmwareEdition& edition) {
    if (fw.records.empty()) return false;
    int profile = -1;
    uint32_t seen = 0;
    for (const auto& record : fw.records) {
        const auto* hw = Identity(record);
        if (!hw || (profile >= 0 && profile != hw->profile)) return false;
        const size_t index = size_t(hw - kHardware);
        if (seen & (1u << index)) return false;
        seen |= 1u << index;
        profile = hw->profile;
    }
    const uint32_t required = profile == 0 ? 1u : profile == 1 ? 6u : 56u;
    if (seen != required) return false;
    model = profile == 2 ? FirmwareModel::DragXPro : FirmwareModel::DragSPro;
    edition = profile == 0 ? FirmwareEdition::Factory : FirmwareEdition::Tc;
    return true;
}

std::vector<AssetSpec> Assets(FirmwareModel model, FirmwareEdition edition) {
    const int profile = Profile(model, edition);
    if (profile < 0) return {};
    const auto& source = *Canonical(uint8_t(profile));
    std::vector<AssetSpec> result;
    result.reserve(source.resourceCount + 84);
    for (size_t i = 0; i < source.resourceCount; ++i) {
        const auto& r = source.resources[i];
        AssetSpec asset;
        char id[48];
        std::snprintf(id, sizeof(id), "pro_%s_%03zu", profile == 0 ? "factory" : "tc", i);
        asset.id = id; asset.title = r.title;
        asset.group = r.group;
        asset.address = r.address; asset.width = r.width; asset.height = r.height;
        asset.format = r.rgb ? AssetFormat::Rgb565 : AssetFormat::Mask1Bit;
        asset.sourceCode = source.code; asset.foregroundOffset = r.foreground;
        for (const auto& hw : kHardware) if (hw.profile == profile)
            asset.recordAddresses.emplace_back(hw.code, hw.resources[i].address);
        result.push_back(std::move(asset));
    }
    for (size_t b = 0; b < source.bankCount; ++b) {
        const auto& bank = source.banks[b];
        const bool specialBank = bank.selector == 13 || bank.selector == 15;
        const bool rotated = bank.selector >= 12 && bank.selector <= 15;
        const char* rotation = rotated ? " (rotated clockwise)" : "";
        for (size_t i = 0; i < bank.count; ++i) {
            const bool special = specialBank || i >= (bank.selector == 7 ? 16u : 10u);
            AssetSpec asset;
            char id[64], title[96], group[64];
            std::snprintf(id, sizeof(id), "%s%ux%u_%02zu", special ? "pro_glyph_" : "font_", bank.width, bank.height, i);
            // Presets map decimal/hex banks; the extra slots render minus signs.
            // Selectors 13/15 contain clockwise 0/1 and a narrow clockwise 1.
            if (!special || specialBank) {
                const char glyph = bank.selector == 15 ? '1' :
                    i < 10 ? char('0' + i) : char('A' + i - 10);
                std::snprintf(title, sizeof(title), "%s %c%s", glyph <= '9' ? "Digit" : "Glyph", glyph, rotation);
            } else {
                std::snprintf(title, sizeof(title), "Minus sign%s", rotation);
            }
            std::snprintf(group, sizeof(group), "Font %ux%u%s", bank.width, bank.height,
                rotated ? " / Rotated clockwise" : "");
            asset.id = id; asset.title = title; asset.group = group; asset.width = bank.width; asset.height = bank.height;
            asset.address = bank.address + uint32_t(i * asset.byte_len()); asset.ordinal = i; asset.sourceCode = source.code;
            for (const auto& hw : kHardware) if (hw.profile == profile)
                asset.recordAddresses.emplace_back(hw.code, hw.banks[b].address + uint32_t(i * asset.byte_len()));
            result.push_back(std::move(asset));
        }
    }
    return result;
}

bool RecordPaletteColor(const FirmwarePackage& fw, uint16_t code, const char* id, Rgb8& color) {
    const int role = RoleIndex(id);
    const auto* record = FindRecord(fw, code);
    if (!record || role < 0) return false;
    const auto* hw = Identity(*record);
    if (!hw) return false;
    const auto* site = RoleSite(*hw, size_t(role));
    uint16_t value;
    if (!site || !SiteColor(*record, *site, value)) return false;
    color = Rgb8::FromRgb565(value);
    return true;
}

bool DetectColors(const FirmwarePackage& fw, std::vector<PaletteState>& palette, Rgb8& background) {
    FirmwareModel model; FirmwareEdition edition;
    if (!Detect(fw, model, edition)) return false;
    const auto& source = *Canonical(uint8_t(Profile(model, edition)));
    palette.clear(); palette.reserve(std::size(kRoleIds));
    for (size_t role = 0; role < std::size(kRoleIds); ++role) {
        const auto* site = RoleSite(source, role);
        if (!site) continue;
        Rgb8 current;
        if (!RecordPaletteColor(fw, source.code, kRoleIds[role], current)) return false;
        palette.push_back({ { kRoleIds[role], kRoleLabels[role], "Native Pro renderer", Rgb8::FromRgb565(site->value) }, current });
    }
    background = { 0, 0, 0 };
    return true;
}

bool BuildColorPatches(const FirmwarePackage& fw, const std::vector<PaletteState>& palette, Rgb8 background,
    std::vector<CodePatch>& patches, std::string& error) {
    FirmwareModel model; FirmwareEdition edition;
    if (!Detect(fw, model, edition)) { error = "Unsupported or inconsistent Pro hardware metadata"; return false; }
    if (background != Rgb8(0, 0, 0)) { error = "Only the firmware's black background is supported"; return false; }
    const auto& canonical = *Canonical(uint8_t(Profile(model, edition)));
    uint16_t desired[std::size(kRoleIds)] = {}, reference[std::size(kRoleIds)] = {};
    bool changed[std::size(kRoleIds)] = {};
    for (size_t role = 0; role < std::size(kRoleIds); ++role) {
        Rgb8 color;
        if (!RecordPaletteColor(fw, canonical.code, kRoleIds[role], color)) continue;
        reference[role] = desired[role] = color.ToRgb565();
        for (const auto& state : palette) if (RoleIndex(state.spec.id) == int(role)) desired[role] = state.current.ToRgb565();
        changed[role] = desired[role] != reference[role];
    }
    // Validate every native producer before emitting anything, including no-op saves.
    for (const auto& record : fw.records) {
        const auto& hw = *Identity(record);
        for (size_t i = 0; i < hw.siteCount; ++i) {
            uint16_t value;
            if (!SiteColor(record, hw.sites[i], value)) { error = "A Pro draw-color instruction has an unsupported encoding"; return false; }
        }
    }
    for (size_t r = 0; r < fw.records.size(); ++r) {
        const auto& record = fw.records[r]; const auto& hw = *Identity(record);
        for (size_t i = 0; i < hw.siteCount; ++i) {
            const auto& site = hw.sites[i];
            if (!changed[site.role]) continue;
            const size_t length = site.reg < 0 ? 2u : 4u;
            std::vector<uint8_t> replacement;
            const auto* baseline = RoleSite(canonical, site.role);
            if (baseline && desired[site.role] == baseline->value) {
                for (size_t j = 0; j < length; ++j) replacement.push_back(uint8_t(site.stock >> (j * 8)));
            } else if (site.reg < 0) {
                replacement = { uint8_t(desired[site.role]), uint8_t(desired[site.role] >> 8) };
            } else replacement = MovwInstruction(site.reg, desired[site.role]);
            const auto begin = record.image.begin() + site.offset;
            if (std::equal(replacement.begin(), replacement.end(), begin)) continue;
            CodePatch patch;
            patch.id = std::string(kRoleIds[site.role]) + "_" + std::to_string(hw.code) + "_" + std::to_string(site.offset);
            patch.original.assign(begin, begin + length); patch.replacement = std::move(replacement);
            patch.locations.resize(fw.records.size()); patch.locations[r].push_back(site.offset);
            patches.push_back(std::move(patch));
        }
    }
    return true;
}

const char* PaletteId(const AssetSpec& asset, bool cyanMode) {
    if (const auto* resource = ResourceFor(asset))
        return resource->rgb ? nullptr : kRoleIds[resource->mode ? (cyanMode ? 3 : 1) : resource->role];
    const int role = FontRole(asset);
    return role < 0 ? nullptr : kRoleIds[role];
}

bool UsesModeColor(const AssetSpec& asset) {
    const auto* resource = ResourceFor(asset);
    return resource && resource->mode;
}

Rgb8 Foreground(const FirmwarePackage& fw, const AssetSpec& asset, const std::vector<PaletteState>& palette, bool cyanMode) {
    const char* id = PaletteId(asset, cyanMode);
    if (!id) return { 255, 255, 255 };
    const auto* resource = ResourceFor(asset);
    const auto* record = FindRecord(fw, asset.sourceCode);
    const auto* hw = record ? Identity(*record) : nullptr;
    const int role = RoleIndex(id);
    const ColorSite* nativeSite = hw && role >= 0 ? RoleSite(*hw, size_t(role)) : nullptr;
    if (resource && !resource->mode && resource->foreground && hw) {
        for (size_t i = 0; i < hw->siteCount; ++i)
            if (hw->sites[i].offset == resource->foreground) nativeSite = hw->sites + i;
    }
    Rgb8 reference;
    const bool haveReference = RecordPaletteColor(fw, asset.sourceCode, id, reference);
    for (const auto& state : palette) {
        if (!state.spec.id || std::strcmp(state.spec.id, id) != 0) continue;
        const auto* canonical = hw ? Canonical(hw->profile) : nullptr;
        const auto* baseline = canonical && role >= 0 ? RoleSite(*canonical, size_t(role)) : nullptr;
        if (baseline && nativeSite && state.current.ToRgb565() == baseline->value)
            return Rgb8::FromRgb565(nativeSite->value);
        if (!haveReference || state.current.ToRgb565() != reference.ToRgb565()) return state.current.ScreenColor();
    }
    // Preserve site-local imported colors, not the first producer's color,
    // while an unchanged loaded role is being previewed.
    uint16_t value;
    if (record && nativeSite && SiteColor(*record, *nativeSite, value)) return Rgb8::FromRgb565(value);
    return haveReference ? reference : Rgb8(255, 255, 255);
}

Rgb8 Background(const FirmwarePackage&, const AssetSpec&, const std::vector<PaletteState>&, bool) {
    return { 0, 0, 0 };
}

std::vector<Preset> Presets(FirmwareModel model, FirmwareEdition edition) {
    const int profile = Profile(model, edition);
    if (profile < 0) return {};
    const auto& hw = *Canonical(uint8_t(profile));
    std::vector<Preset> result;
    for (size_t b = 0; b < hw.bankCount; ++b) {
        const auto& bank = hw.banks[b];
        if (bank.selector == 13 || bank.selector == 15) continue;
        Preset preset; char name[64];
        std::snprintf(name, sizeof(name), "%ux%u%s", bank.width, bank.height,
            bank.selector == 12 || bank.selector == 14 ? " (clockwise native bank)" : "");
        preset.name = name; preset.description = "Native Pro digit bank; special slots are preserved";
        preset.width = bank.width; preset.height = bank.height;
        preset.rotationQuarterTurns = bank.selector == 12 || bank.selector == 14 ? 1 : 0;
        const char* chars = bank.selector == 7 ? "0123456789ABCDEF" : "0123456789";
        for (size_t i = 0; chars[i]; ++i) {
            char filename[64];
            std::snprintf(filename, sizeof(filename), "font_%ux%u_%02zu.png", bank.width, bank.height, i);
            preset.glyphs.push_back({ chars[i], filename });
        }
        result.push_back(std::move(preset));
    }
    return result;
}
} // namespace ProFirmware
