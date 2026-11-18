#include "legacy_firmware.h"

#include <cstdio>
#include <cstring>
#include <iterator>
#include <utility>

namespace {
struct BitmapDef {
    uint32_t address, width, height;
    const char* title;
    const char* group;
};
struct FontDef {
    const char* id;
    uint32_t address, width, height;
    size_t count;
    const char* characters;
};

// Native monochrome OLED bytes are vertical eight-pixel pages, LSB at the
// top. DRAG's 0x0800884c and Mini's 0x08008984 renderer divide pixel heights
// into full/remainder pages; their descriptor wrappers are 0x080050e0 and
// 0x08005228. Numeric descriptors pass glyph pointer + index * native stride.
// Refresh 0x08007dac (FIT3 0x08007f10) instead receives a page count and sends
// width * pages bytes directly to the OLED. 0x08009658 draws full 128x32 pages;
// 0x080096a8 consumes {width,pages,padding,pointer} descriptors. Refresh letters
// are native 7x8/10x16 banks, not rotated or synthesized glyphs. The 10x16 text
// helper uses the first bank's top page and alternate bank's bottom page.
// Shared pointers and partial draw windows have one non-overlapping owner.
// Resource relocation: FIT3 +0x324, Mini Refresh -0x10, Mini FIT3 +0x314.

static constexpr BitmapDef kBitmaps0[] = {
    {0x0800bc84, 2, 2, "Decimal separator", "Units"},
    {0x0800bc86, 3, 3, "Decimal separator (large)", "Units"},
    {0x0800bdc0, 20, 30, "Unit W (large)", "Units"},
    {0x0800be10, 20, 30, "Unit V (large)", "Units"},
    {0x0800be60, 16, 14, "Unit W (medium)", "Units"},
    {0x0800be80, 10, 14, "Unit V", "Units"},
    {0x0800be94, 16, 14, "Unit degrees F", "Units"},
    {0x0800beb4, 16, 14, "Unit degrees C", "Units"},
    {0x0800bed4, 10, 14, "Unit W", "Units"},
    {0x0800bee8, 10, 14, "Unit Ohm", "Units"},
    {0x0800befc, 22, 14, "TC mode SS", "Labels"},
    {0x0800bf28, 22, 14, "TC mode NI", "Labels"},
    {0x0800bf54, 22, 14, "TC mode TI", "Labels"},
    {0x0800bf80, 22, 14, "TC mode NC", "Labels"},
    {0x0800bfac, 22, 14, "TC mode TC", "Labels"},
    {0x0800bfd8, 46, 14, "Logo DRAG (standby)", "Screens"},
    {0x0800c03c, 14, 30, "Vertical battery (empty)", "Animation"},
    {0x0800c074, 14, 30, "Vertical battery (1 bar)", "Animation"},
    {0x0800c0ac, 14, 30, "Vertical battery (2 bars)", "Animation"},
    {0x0800c0e4, 14, 30, "Vertical battery (3 bars)", "Animation"},
    {0x0800c11c, 14, 30, "Vertical battery (4 bars)", "Animation"},
    {0x0800c154, 14, 30, "Vertical battery (full)", "Animation"},
    {0x0800c18c, 20, 30, "Unit V (memory)", "Units"},
    {0x0800c1dc, 20, 30, "Memory M", "Labels"},
    {0x0800c22c, 28, 26, "Power menu", "Controls"},
    {0x0800c29c, 8, 31, "Previous menu arrow", "Controls"},
    {0x0800c2bc, 8, 31, "Next menu arrow", "Controls"},
    {0x0800c2dc, 28, 26, "Resistance SET menu", "Controls"},
    {0x0800c34c, 28, 26, "Save menu", "Controls"},
    {0x0800c3bc, 28, 26, "Temperature curve menu", "Controls"},
    {0x0800c42c, 28, 26, "SN menu", "Controls"},
    {0x0800c49c, 28, 26, "Power-to-voltage menu", "Controls"},
    {0x0800c50c, 28, 26, "Voltage-to-power menu", "Controls"},
    {0x0800c57c, 28, 26, "Memory 1-5 menu", "Controls"},
    {0x0800c5ec, 32, 30, "Menu selection border", "Controls"},
    {0x0800c744, 20, 30, "Unit Ohm (large)", "Units"},
    {0x0800c794, 32, 20, "TC material SS", "Labels"},
    {0x0800c7f4, 32, 20, "TC material NI", "Labels"},
    {0x0800c854, 32, 20, "TC material TI", "Labels"},
    {0x0800c8b4, 32, 20, "TC material NC", "Labels"},
    {0x0800c914, 32, 20, "TC material TC", "Labels"},
    {0x0800c974, 104, 16, "GENE CHIP ID header", "Screens"},
    {0x0800cbc8, 128, 1, "Transition line mask", "Animation"},
    {0x0800cc48, 57, 20, "Battery gauge frame", "Controls"},
    {0x0800ccf3, 49, 14, "SUPER label", "Labels"},
    {0x0800cd55, 9, 14, "Battery fill segment", "Controls"},
    {0x0800cd67, 5, 3, "Graph top marker", "Controls"},
    {0x0800cd6c, 3, 5, "Graph right marker", "Controls"},
    {0x0800cd6f, 10, 14, "Unit S (graph)", "Units"},
    {0x0800cd9c, 1, 1, "Graph point mask", "Controls"},
    {0x0800cd9d, 4, 1, "Graph bar mask", "Controls"},
    {0x0800cda1, 6, 2, "Graph baseline marker", "Controls"},
    {0x0800cda7, 4, 4, "Graph square marker", "Controls"},
    {0x0800d600, 128, 32, "Logo VOOPOO (boot)", "Screens"},
    {0x0800d800, 128, 32, "New coil confirmation", "Alerts"},
    {0x0800da00, 128, 32, "Low Atomizer", "Alerts"},
    {0x0800dc00, 128, 32, "High Atomizer", "Alerts"},
    {0x0800de00, 128, 32, "Check Atomizer", "Alerts"},
    {0x0800e000, 128, 32, "Atomizer Short", "Alerts"},
    {0x0800e200, 128, 32, "Time Out", "Alerts"},
    {0x0800e400, 128, 32, "Lock", "Screens"},
    {0x0800e600, 128, 32, "Unlock", "Screens"},
    {0x0800e800, 128, 32, "OFF", "Screens"},
    {0x0800ea00, 128, 32, "Check Battery", "Alerts"},
    {0x0800ec00, 128, 32, "Low Power", "Alerts"},
    {0x0800ee00, 128, 32, "High Temp", "Alerts"},
};

static constexpr FontDef kFonts0[] = {
    {"font_12x30", 0x0800b9f0, 12, 30, 11, "0123456789"},
    {"font_6x14", 0x0800bc00, 6, 14, 11, "0123456789"},
    {"font_hex_5x10", 0x0800cb14, 5, 10, 18, "0123456789ABCDEF"},
};

static constexpr BitmapDef kBitmaps1[] = {
    {0x0800bf46, 1, 2, "Decimal separator (small)", "Units"},
    {0x0800bf47, 2, 2, "Decimal separator", "Units"},
    {0x0800bf49, 2, 5, "Decimal separator (large)", "Units"},
    {0x0800c02c, 18, 30, "Unit W (large)", "Units"},
    {0x0800c074, 7, 8, "Unit V", "Units"},
    {0x0800c07b, 7, 8, "Unit S", "Units"},
    {0x0800c082, 7, 8, "Unit F (graph)", "Units"},
    {0x0800c089, 7, 8, "Unit C (graph)", "Units"},
    {0x0800c090, 18, 15, "Unit degrees F", "Units"},
    {0x0800c0b4, 18, 15, "Unit degrees C", "Units"},
    {0x0800c0d8, 7, 8, "Unit W", "Units"},
    {0x0800c0df, 7, 8, "Unit Ohm", "Units"},
    {0x0800c0e6, 18, 12, "TC mode SS", "Labels"},
    {0x0800c10a, 18, 12, "TC mode NI", "Labels"},
    {0x0800c12e, 18, 12, "TC mode TI", "Labels"},
    {0x0800c152, 18, 12, "TC mode NC", "Labels"},
    {0x0800c176, 18, 12, "TC mode TC", "Labels"},
    {0x0800c19a, 22, 14, "TC menu SS", "Controls"},
    {0x0800c1c6, 22, 14, "TC menu NI", "Controls"},
    {0x0800c1f2, 22, 14, "TC menu TI", "Controls"},
    {0x0800c21e, 22, 14, "TC menu NC", "Controls"},
    {0x0800c24a, 22, 14, "TC menu TCR", "Controls"},
    {0x0800c276, 46, 14, "Logo DRAG (standby)", "Screens"},
    {0x0800c2dc, 11, 30, "Vertical battery (empty)", "Animation"},
    {0x0800c308, 11, 30, "Vertical battery (1 bar)", "Animation"},
    {0x0800c334, 11, 30, "Vertical battery (2 bars)", "Animation"},
    {0x0800c360, 11, 30, "Vertical battery (3 bars)", "Animation"},
    {0x0800c38c, 11, 30, "Vertical battery (4 bars)", "Animation"},
    {0x0800c3b8, 11, 30, "Vertical battery (full)", "Animation"},
    {0x0800c3e4, 10, 12, "Unit V (medium)", "Units"},
    {0x0800c3f8, 20, 30, "Memory M", "Labels"},
    {0x0800c448, 16, 22, "Power menu", "Controls"},
    {0x0800c478, 16, 22, "TC menu icon", "Controls"},
    {0x0800c4a8, 28, 18, "Menu selection border", "Controls"},
    {0x0800c4fc, 8, 31, "Previous menu arrow", "Controls"},
    {0x0800c51c, 8, 31, "Next menu arrow", "Controls"},
    {0x0800c53c, 16, 22, "Resistance SET menu", "Controls"},
    {0x0800c56c, 16, 22, "Version menu", "Controls"},
    {0x0800c59c, 16, 22, "Temperature curve menu", "Controls"},
    {0x0800c5cc, 16, 22, "SN menu", "Controls"},
    {0x0800c5fc, 16, 22, "Memory 1-5 menu", "Controls"},
    {0x0800c62c, 32, 30, "Focused menu border", "Controls"},
    {0x0800c6ac, 30, 20, "Logo FIT", "Screens"},
    {0x0800c706, 12, 22, "FIT choice 1", "Controls"},
    {0x0800c72a, 12, 22, "FIT choice 2", "Controls"},
    {0x0800c74e, 12, 22, "FIT choice 3", "Controls"},
    {0x0800c772, 18, 28, "FIT selected 1", "Controls"},
    {0x0800c7ba, 18, 28, "FIT selected 2", "Controls"},
    {0x0800c802, 18, 28, "FIT selected 3", "Controls"},
    {0x0800c954, 10, 12, "Unit Ohm (medium)", "Units"},
    {0x0800c968, 26, 12, "TC material SS", "Labels"},
    {0x0800c99c, 26, 12, "TC material NI", "Labels"},
    {0x0800c9d0, 26, 12, "TC material TI", "Labels"},
    {0x0800ca04, 26, 12, "TC material NC", "Labels"},
    {0x0800ca38, 26, 12, "TC material TC", "Labels"},
    {0x0800ca6c, 94, 12, "GENE CHIP ID header", "Screens"},
    {0x0800cb28, 18, 15, "Unit W (memory)", "Units"},
    {0x0800cb4c, 18, 12, "Curve chart icon", "Controls"},
    {0x0800cc52, 128, 1, "Transition line mask", "Animation"},
    {0x0800ccd2, 38, 16, "Battery gauge frame", "Controls"},
    {0x0800cd1e, 6, 10, "Battery fill segment", "Controls"},
    {0x0800cd2a, 5, 3, "Graph top marker", "Controls"},
    {0x0800cd2f, 3, 5, "Graph right marker", "Controls"},
    {0x0800cd32, 10, 12, "Unit S (graph)", "Units"},
    {0x0800cd46, 14, 14, "Profile A", "Labels"},
    {0x0800cd62, 14, 14, "Profile B", "Labels"},
    {0x0800cd98, 1, 1, "Graph point mask", "Controls"},
    {0x0800cd99, 4, 1, "Graph bar mask", "Controls"},
    {0x0800cd9d, 6, 2, "Graph baseline marker", "Controls"},
    {0x0800d600, 128, 32, "Logo VOOPOO (boot)", "Screens"},
    {0x0800d800, 128, 32, "New coil confirmation", "Alerts"},
    {0x0800da00, 128, 32, "Low Atomizer", "Alerts"},
    {0x0800dc00, 128, 32, "High Atomizer", "Alerts"},
    {0x0800de00, 128, 32, "Check Atomizer", "Alerts"},
    {0x0800e000, 128, 32, "Atomizer Short", "Alerts"},
    {0x0800e200, 128, 32, "Time Out", "Alerts"},
    {0x0800e400, 128, 32, "Lock", "Screens"},
    {0x0800e600, 128, 32, "Unlock", "Screens"},
    {0x0800e800, 128, 32, "OFF", "Screens"},
    {0x0800ea00, 128, 32, "Check Battery", "Alerts"},
    {0x0800ec00, 128, 32, "Low Power", "Alerts"},
    {0x0800ee00, 128, 32, "High Temp", "Alerts"},
};

static constexpr FontDef kFonts1[] = {
    {"font_10x30", 0x0800bc6c, 10, 30, 11, "0123456789"},
    {"font_10x12", 0x0800be24, 10, 12, 10, "0123456789"},
    {"font_5x8", 0x0800beec, 5, 8, 18, "0123456789 BCDEF"},
    {"font_hex_5x8", 0x0800cbf8, 5, 8, 18, "0123456789ABCDEF"},
};

static constexpr BitmapDef kBitmaps2[] = {
    {0x0800bf5a, 1, 2, "Decimal separator (small)", "Units"},
    {0x0800bf5b, 2, 2, "Decimal separator", "Units"},
    {0x0800bf5d, 2, 5, "Decimal separator (large)", "Units"},
    {0x0800c040, 18, 30, "Unit W (large)", "Units"},
    {0x0800c088, 7, 8, "Unit V", "Units"},
    {0x0800c08f, 7, 8, "Unit S", "Units"},
    {0x0800c096, 7, 8, "Unit F (graph)", "Units"},
    {0x0800c09d, 7, 8, "Unit C (graph)", "Units"},
    {0x0800c0a4, 18, 15, "Unit degrees F", "Units"},
    {0x0800c0c8, 18, 15, "Unit degrees C", "Units"},
    {0x0800c0ec, 7, 8, "Unit W", "Units"},
    {0x0800c0f3, 7, 8, "Unit Ohm", "Units"},
    {0x0800c0fa, 18, 12, "TC mode SS", "Labels"},
    {0x0800c11e, 18, 12, "TC mode NI", "Labels"},
    {0x0800c142, 18, 12, "TC mode TI", "Labels"},
    {0x0800c166, 18, 12, "TC mode NC", "Labels"},
    {0x0800c18a, 18, 12, "TC mode TC", "Labels"},
    {0x0800c1ae, 22, 14, "TC menu SS", "Controls"},
    {0x0800c1da, 22, 14, "TC menu NI", "Controls"},
    {0x0800c206, 22, 14, "TC menu TI", "Controls"},
    {0x0800c232, 22, 14, "TC menu NC", "Controls"},
    {0x0800c25e, 22, 14, "TC menu TCR", "Controls"},
    {0x0800c28a, 46, 14, "Logo DRAG (standby)", "Screens"},
    {0x0800c2f0, 11, 30, "Vertical battery (empty)", "Animation"},
    {0x0800c31c, 11, 30, "Vertical battery (1 bar)", "Animation"},
    {0x0800c348, 11, 30, "Vertical battery (2 bars)", "Animation"},
    {0x0800c374, 11, 30, "Vertical battery (3 bars)", "Animation"},
    {0x0800c3a0, 11, 30, "Vertical battery (4 bars)", "Animation"},
    {0x0800c3cc, 11, 30, "Vertical battery (full)", "Animation"},
    {0x0800c3f8, 10, 12, "Unit V (medium)", "Units"},
    {0x0800c40c, 20, 30, "Memory M", "Labels"},
    {0x0800c45c, 16, 22, "Power menu", "Controls"},
    {0x0800c48c, 16, 22, "TC menu icon", "Controls"},
    {0x0800c4bc, 28, 18, "Menu selection border", "Controls"},
    {0x0800c510, 8, 31, "Previous menu arrow", "Controls"},
    {0x0800c530, 8, 31, "Next menu arrow", "Controls"},
    {0x0800c550, 16, 22, "Resistance SET menu", "Controls"},
    {0x0800c580, 16, 22, "Version menu", "Controls"},
    {0x0800c5b0, 16, 22, "Temperature curve menu", "Controls"},
    {0x0800c5e0, 16, 22, "SN menu", "Controls"},
    {0x0800c610, 16, 22, "Memory 1-5 menu", "Controls"},
    {0x0800c640, 32, 30, "Focused menu border", "Controls"},
    {0x0800c6c0, 30, 20, "Logo FIT", "Screens"},
    {0x0800c71a, 12, 22, "FIT choice 1", "Controls"},
    {0x0800c73e, 12, 22, "FIT choice 2", "Controls"},
    {0x0800c762, 12, 22, "FIT choice 3", "Controls"},
    {0x0800c786, 18, 28, "FIT selected 1", "Controls"},
    {0x0800c7ce, 18, 28, "FIT selected 2", "Controls"},
    {0x0800c816, 18, 28, "FIT selected 3", "Controls"},
    {0x0800c968, 10, 12, "Unit Ohm (medium)", "Units"},
    {0x0800c97c, 26, 12, "TC material SS", "Labels"},
    {0x0800c9b0, 26, 12, "TC material NI", "Labels"},
    {0x0800c9e4, 26, 12, "TC material TI", "Labels"},
    {0x0800ca18, 26, 12, "TC material NC", "Labels"},
    {0x0800ca4c, 26, 12, "TC material TC", "Labels"},
    {0x0800ca80, 94, 12, "GENE CHIP ID header", "Screens"},
    {0x0800cb3c, 18, 15, "Unit W (memory)", "Units"},
    {0x0800cb60, 18, 12, "Curve chart icon", "Controls"},
    {0x0800cc66, 128, 1, "Transition line mask", "Animation"},
    {0x0800cce6, 38, 16, "Battery gauge frame", "Controls"},
    {0x0800cd32, 6, 10, "Battery fill segment", "Controls"},
    {0x0800cd3e, 5, 3, "Graph top marker", "Controls"},
    {0x0800cd43, 3, 5, "Graph right marker", "Controls"},
    {0x0800cd46, 10, 12, "Unit S (graph)", "Units"},
    {0x0800cd5a, 14, 14, "Profile A", "Labels"},
    {0x0800cd76, 14, 14, "Profile B", "Labels"},
    {0x0800cdac, 1, 1, "Graph point mask", "Controls"},
    {0x0800cdad, 4, 1, "Graph bar mask", "Controls"},
    {0x0800cdb1, 6, 2, "Graph baseline marker", "Controls"},
    {0x0800d600, 128, 32, "Logo VOOPOO (boot)", "Screens"},
    {0x0800d800, 128, 32, "New coil confirmation", "Alerts"},
    {0x0800da00, 128, 32, "Low Atomizer", "Alerts"},
    {0x0800dc00, 128, 32, "High Atomizer", "Alerts"},
    {0x0800de00, 128, 32, "Check Atomizer", "Alerts"},
    {0x0800e000, 128, 32, "Atomizer Short", "Alerts"},
    {0x0800e200, 128, 32, "Time Out", "Alerts"},
    {0x0800e400, 128, 32, "Lock", "Screens"},
    {0x0800e600, 128, 32, "Unlock", "Screens"},
    {0x0800e800, 128, 32, "OFF", "Screens"},
    {0x0800ea00, 128, 32, "Check Battery", "Alerts"},
    {0x0800ec00, 128, 32, "Low Power", "Alerts"},
    {0x0800ee00, 128, 32, "High Temp", "Alerts"},
};

static constexpr FontDef kFonts2[] = {
    {"font_10x30", 0x0800bc80, 10, 30, 11, "0123456789"},
    {"font_10x12", 0x0800be38, 10, 12, 10, "0123456789"},
    {"font_5x8", 0x0800bf00, 5, 8, 18, "0123456789 BCDEF"},
    {"font_hex_5x8", 0x0800cc0c, 5, 8, 18, "0123456789ABCDEF"},
};

static constexpr BitmapDef kBitmaps3[] = {
    {0x08012164, 35, 16, "OFF", "Screens"},
    { 0x080121aa, 128, 32, "Logo VOOPOO (boot)", "Logos" },
    {0x080123aa, 128, 32, "Check Atomizer", "Alerts"},
    {0x080125aa, 128, 32, "High Atomizer", "Alerts"},
    {0x080127aa, 128, 32, "Atomizer Short", "Alerts"},
    {0x080129aa, 128, 32, "Time Out", "Alerts"},
    {0x08012baa, 128, 32, "Lock", "Screens"},
    {0x08012daa, 128, 32, "Unlock", "Screens"},
    {0x080131aa, 128, 32, "Check Battery", "Alerts"},
    {0x080133aa, 128, 32, "Low Power", "Alerts"},
    {0x080135aa, 128, 32, "High Temp", "Alerts"},
    {0x080137aa, 91, 16, "Graph axes", "Controls"},
    {0x08013899, 3, 8, "Decimal separator", "Units"},
    {0x08013d91, 20, 32, "Mode RBA wattage", "Labels"},
    {0x08013e81, 20, 32, "Mode SMART wattage", "Labels"},
    {0x08013ed1, 11, 32, "Battery gauge frame", "Controls"},
    {0x08013efd, 9, 32, "Battery gauge frame (compact)", "Controls"},
    {0x08013fbb, 7, 16, "Unit Ohm", "Units"},
    {0x080142db, 19, 16, "TC mode NI", "Labels"},
    {0x08014301, 19, 16, "TC mode TI", "Labels"},
    {0x08014327, 19, 16, "TC mode SS", "Labels"},
    {0x0801434d, 19, 16, "Unit degrees F", "Units"},
    {0x08014373, 19, 16, "Unit degrees C", "Units"},
    {0x080143b4, 45, 32, "Logo FIT", "Screens"},
    {0x08014468, 22, 32, "Memory M", "Labels"},
    {0x080144c0, 10, 32, "FIT choice 1", "Controls"},
    {0x080144e8, 10, 32, "FIT choice 2", "Controls"},
    {0x08014510, 10, 32, "FIT choice 3", "Controls"},
    {0x08014538, 21, 16, "TC material SS (selected)", "Labels"},
    {0x08014564, 21, 16, "TC material SS", "Labels"},
    {0x08014590, 21, 16, "TC material NI (selected)", "Labels"},
    {0x080145bc, 21, 16, "TC material NI", "Labels"},
    {0x080145e8, 21, 16, "TC material TI (selected)", "Labels"},
    {0x08014614, 21, 16, "TC material TI", "Labels"},
    {0x08014640, 28, 32, "TC menu SS (selected)", "Controls"},
    {0x080146b0, 28, 32, "TC menu SS", "Controls"},
    {0x08014720, 28, 32, "TC menu NI (selected)", "Controls"},
    {0x08014790, 28, 32, "TC menu NI", "Controls"},
    {0x08014800, 28, 32, "TC menu TI (selected)", "Controls"},
    {0x08014870, 28, 32, "TC menu TI", "Controls"},
    {0x080148e0, 28, 32, "Menu selection border", "Controls"},
    {0x08014990, 21, 16, "Unit W (selected)", "Units"},
    {0x080149bc, 21, 16, "Unit W", "Units"},
    {0x080149e8, 21, 16, "Unit Ohm (selected)", "Units"},
    {0x08014a14, 21, 16, "Unit Ohm", "Units"},
    {0x08014a40, 128, 32, "Device logo (boot)", "Screens"},
    {0x08014c40, 60, 16, "Rank label Iron", "Score"},
    {0x08014cb8, 60, 16, "Rank label Bronze", "Score"},
    {0x08014d30, 60, 16, "Rank label Silver", "Score"},
    {0x08014da8, 60, 16, "Rank label Gold", "Score"},
    {0x08014e20, 60, 16, "Rank label Platinum", "Score"},
    {0x08014e98, 60, 16, "Rank label Diamond", "Score"},
    {0x08014f10, 60, 16, "Rank label Master", "Score"},
    {0x08014f88, 60, 16, "Rank label King", "Score"},
    {0x08015000, 27, 16, "Battery A left edge", "Controls"},
    {0x08015036, 12, 16, "Battery right edge", "Controls"},
    {0x0801504e, 29, 16, "Battery B left edge", "Controls"},
    {0x08015088, 6, 16, "Battery empty segment", "Controls"},
    {0x08015094, 6, 16, "Battery filled segment", "Controls"},
    {0x08015164, 128, 32, "Max Power", "Alerts"},
};

static constexpr FontDef kFonts3[] = {
    {"font_5x8", 0x08013862, 5, 8, 11, "0123456789"},
    {"font_alpha_7x8", 0x0801389c, 7, 8, 27, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
    {"font_alpha_10x16", 0x08013959, 10, 16, 30, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
    {"font_10x32", 0x08013bb1, 10, 32, 10, "0123456789"},
    {"font_7x16", 0x08013f21, 7, 16, 11, "0123456789"},
    {"font_alpha_alt_10x16", 0x08014083, 10, 16, 30, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
};


struct Profile {
    FirmwareModel model;
    FirmwareEdition edition;
    size_t imageSize;
    uint16_t code;
    uint32_t base;
    uint32_t sp;
    uint32_t reset;
    const char* marker;
    uint32_t signatureAddress;
    const char* signature;
    size_t signatureLength;
    const BitmapDef* bitmaps;
    size_t bitmapCount;
    const FontDef* fonts;
    size_t fontCount;
    int32_t resourceDelta;
};

// Identity guards are immutable Thumb instructions/literal pools, not editable
// pixels or a whole-file digest. The Mini releases have the same ROM length and
// marker; the reset trampoline's absolute entrypoint distinguishes them.
static const Profile kProfiles[] = {
    {FirmwareModel::Drag1, FirmwareEdition::Standard, 50280, 0x0000, 0x08003000, 0x20002570, 0x08003145, "SC002P0018\0\0",
        0x0800884c, "\x2d\xe9\xff\x4f\x83\xb0\x00\x20\xdd\xf8\x40\x80\x03\x9c\xdf\xf8", 16, kBitmaps0, std::size(kBitmaps0), kFonts0, std::size(kFonts0), 0},
    {FirmwareModel::DragMini, FirmwareEdition::V1, 50288, 0x0000, 0x08003000, 0x200025a8, 0x08003145, "SC002P0030\0\0",
        0x08003138, "\x00\x48\x00\x47\x79\xb7\x00\x08", 8, kBitmaps1, std::size(kBitmaps1), kFonts1, std::size(kFonts1), 0},
    {FirmwareModel::DragMini, FirmwareEdition::V2, 50288, 0x0000, 0x08003000, 0x200025a8, 0x08003145, "SC002P0030\0\0",
        0x08003138, "\x00\x48\x00\x47\x8d\xb7\x00\x08", 8, kBitmaps2, std::size(kBitmaps2), kFonts2, std::size(kFonts2), 0},
    {FirmwareModel::Drag2Refresh, FirmwareEdition::Standard, 77124, 0x1006, 0x08002c00, 0x200035a8, 0x08002e55, nullptr,
        0x08007dac, "\x2d\xe9\xf0\x5f\x16\x46\x83\x46\x30\x44\xdd\xf8\x28\x80\x99\x46", 16, kBitmaps3, std::size(kBitmaps3), kFonts3, std::size(kFonts3), 0},
    {FirmwareModel::Drag2Refresh, FirmwareEdition::Fit3, 77924, 0x1006, 0x08002c00, 0x200035a8, 0x08002e55, nullptr,
        0x08007f10, "\x2d\xe9\xf0\x5f\x16\x46\x83\x46\x30\x44\xdd\xf8\x28\x80\x99\x46", 16, kBitmaps3, std::size(kBitmaps3), kFonts3, std::size(kFonts3), 804},
    {FirmwareModel::DragMiniRefresh, FirmwareEdition::Standard, 77108, 0x1008, 0x08002c00, 0x200035a8, 0x08002e55, nullptr,
        0x08007dac, "\x2d\xe9\xf0\x5f\x16\x46\x83\x46\x30\x44\xdd\xf8\x28\x80\x99\x46", 16, kBitmaps3, std::size(kBitmaps3), kFonts3, std::size(kFonts3), -16},
    {FirmwareModel::DragMiniRefresh, FirmwareEdition::Fit3, 77908, 0x1008, 0x08002c00, 0x200035a8, 0x08002e55, nullptr,
        0x08007f10, "\x2d\xe9\xf0\x5f\x16\x46\x83\x46\x30\x44\xdd\xf8\x28\x80\x99\x46", 16, kBitmaps3, std::size(kBitmaps3), kFonts3, std::size(kFonts3), 788},
};

const Profile* Find(FirmwareModel model, FirmwareEdition edition) {
    for (const auto& profile : kProfiles)
        if (profile.model == model && profile.edition == edition) return &profile;
    return nullptr;
}

void AddAsset(std::vector<AssetSpec>& out, const Profile& profile, const std::string& id,
    const std::string& title, const std::string& group, uint32_t address, uint32_t width, uint32_t height) {
    AssetSpec spec;
    spec.id = id;
    spec.title = title;
    spec.group = group;
    spec.address = static_cast<uint32_t>(static_cast<int64_t>(address) + profile.resourceDelta);
    spec.width = width;
    spec.height = height;
    spec.ordinal = out.size();
    spec.format = AssetFormat::Mask1BitPages;
    spec.sourceCode = profile.code;
    spec.recordAddresses.push_back({profile.code, spec.address});
    out.push_back(std::move(spec));
}
} // namespace

namespace LegacyFirmware {
bool Detect(const FirmwarePackage& package, FirmwareModel& model, FirmwareEdition& edition) {
    if (package.records.size() != 1) return false;
    const auto& record = package.records.front();
    const uint16_t code = static_cast<uint16_t>((record.device_code[0] << 8) | record.device_code[1]);
    for (const auto& profile : kProfiles) {
        if (package.format != (profile.code ? FirmwareFormat::Swdf : FirmwareFormat::Xwdf) ||
            code != profile.code || record.image.size() != profile.imageSize ||
            record.linked_base != profile.base || record.sp != profile.sp || record.reset != profile.reset)
            continue;
        if (profile.marker && (record.marker.size() != 12 ||
            std::memcmp(record.marker.data(), profile.marker, 12) != 0)) continue;
        const size_t offset = profile.signatureAddress - profile.base;
        if (offset > record.image.size() || profile.signatureLength > record.image.size() - offset ||
            std::memcmp(record.image.data() + offset, profile.signature, profile.signatureLength) != 0)
            continue;
        model = profile.model;
        edition = profile.edition;
        return true;
    }
    return false;
}

std::vector<AssetSpec> Assets(FirmwareModel model, FirmwareEdition edition) {
    const auto* profile = Find(model, edition);
    if (!profile) return {};
    size_t count = profile->bitmapCount;
    for (size_t i = 0; i < profile->fontCount; ++i) count += profile->fonts[i].count;
    std::vector<AssetSpec> out;
    out.reserve(count);
    for (size_t i = 0; i < profile->fontCount; ++i) {
        const auto& font = profile->fonts[i];
        const size_t generated = std::strlen(font.characters);
        const uint32_t stride = font.width * ((font.height + 7) / 8);
        for (size_t glyph = 0; glyph < font.count; ++glyph) {
            char id[96];
            std::snprintf(id, sizeof(id), "%s_%02zu", font.id, glyph);
            std::string title;
            if (glyph < generated && font.characters[glyph] != ' ') {
                const char ch = font.characters[glyph];
                title = std::string(ch >= '0' && ch <= '9' ? "Digit " : "Glyph ") + ch;
            } else if (std::strstr(font.id, "alpha")) {
                const bool alternate = std::strstr(font.id, "_alt_") != nullptr;
                if (glyph == 26) title = alternate ? "Glyph %" : "Glyph Ohm";
                else if (glyph == 27) title = alternate ? "Glyph Ohm" : "Glyph %";
                else title = glyph == 28 ? "Decimal separator" : "Blank glyph";
            } else if (glyph >= 16) {
                title = "Chip ID fallback (slot " + std::to_string(glyph) + ")";
            } else if (model == FirmwareModel::Drag2Refresh || model == FirmwareModel::DragMiniRefresh) {
                title = "Decimal separator";
            } else {
                title = font.height == 8 ? "Leading-zero glyph" : "Leading-zero blank";
            }
            char group[96];
            std::snprintf(group, sizeof(group), "Font %ux%u%s", font.width, font.height,
                std::strstr(font.id, "hex") ? " / Hex" :
                std::strstr(font.id, "_alt_") ? " / Alternate letters" :
                std::strstr(font.id, "alpha") ? " / Letters" : "");
            AddAsset(out, *profile, id, title, group,
                font.address + static_cast<uint32_t>(glyph) * stride, font.width, font.height);
        }
    }
    for (size_t i = 0; i < profile->bitmapCount; ++i) {
        const auto& bitmap = profile->bitmaps[i];
        char id[32];
        std::snprintf(id, sizeof(id), "native_%08x", bitmap.address);
        AddAsset(out, *profile, id, bitmap.title, bitmap.group, bitmap.address, bitmap.width, bitmap.height);
    }
    return out;
}

bool DetectColors(const FirmwarePackage& package, std::vector<PaletteState>& palette, Rgb8& background) {
    FirmwareModel model;
    FirmwareEdition edition;
    if (!Detect(package, model, edition)) return false;
    palette.clear();
    background = {0, 0, 0};
    return true;
}

bool BuildColorPatches(const FirmwarePackage& package, const std::vector<PaletteState>& palette,
    Rgb8 background, std::vector<CodePatch>& patches, std::string& error) {
    patches.clear();
    FirmwareModel model;
    FirmwareEdition edition;
    if (!Detect(package, model, edition)) {
        error = "Unsupported legacy firmware profile";
        return false;
    }
    if (!palette.empty() || background.r != 0 || background.g != 0 || background.b != 0) {
        error = "This native monochrome display has no editable palette or background color";
        return false;
    }
    error.clear();
    return true;
}

bool RecordPaletteColor(const FirmwarePackage&, uint16_t, const char*, Rgb8&) { return false; }
const char* PaletteId(const AssetSpec&, bool) { return nullptr; }
bool UsesModeColor(const AssetSpec&) { return false; }
Rgb8 Foreground(const FirmwarePackage&, const AssetSpec&, const std::vector<PaletteState>&, bool) {
    return {255, 255, 255};
}
Rgb8 Background(const FirmwarePackage&, const AssetSpec&, const std::vector<PaletteState>&, bool) {
    return {0, 0, 0};
}

std::vector<Preset> Presets(FirmwareModel model, FirmwareEdition edition) {
    const auto* profile = Find(model, edition);
    if (!profile) return {};
    std::vector<Preset> out;
    out.reserve(profile->fontCount);
    for (size_t i = 0; i < profile->fontCount; ++i) {
        const auto& font = profile->fonts[i];
        Preset preset;
        preset.name = font.id;
        preset.description = "Native page-LSB font; special symbols remain unchanged";
        preset.width = static_cast<int>(font.width);
        preset.height = static_cast<int>(font.height);
        for (size_t glyph = 0; font.characters[glyph]; ++glyph) {
            if (font.characters[glyph] == ' ') continue; // Native leading-zero alias, not hexadecimal A.
            char filename[96];
            std::snprintf(filename, sizeof(filename), "%s_%02zu.png", font.id, glyph);
            preset.glyphs.push_back({font.characters[glyph], filename});
        }
        out.push_back(std::move(preset));
    }
    return out;
}
} // namespace LegacyFirmware
