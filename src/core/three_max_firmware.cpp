#include "three_max_firmware.h"
#include "firmware_layout.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ThreeMaxFirmware {
namespace {
struct RecordSpec {
    FirmwareModel model;
    FirmwareEdition edition;
    uint16_t code;
    uint32_t size, base, sp, reset, renderer;
    uint8_t guard[8];
};
struct Resource {
    FirmwareEdition edition;
    FirmwareModel model;
    const char* id;
    const char* title;
    const char* group;
    uint16_t width, height, count, source;
    AssetFormat format;
    const char* characters;
    int rotation;
    const char* role;
    std::pair<uint16_t, uint32_t> addresses[5];
    bool fixedWhite = false;
};
struct ColorSite {
    FirmwareModel model;
    FirmwareEdition edition;
    uint16_t code;
    const char* role;
    uint32_t offset;
    uint16_t stock;
    uint8_t rd;
    uint8_t kind; // 0: MOVW/MOV.W, 1: literal word.
    uint32_t guardOffset;
    uint32_t guard;
    uint32_t stockInstruction;
};
// Recovered from native Thumb draw arguments, not from bitmap signatures:
// mask renderer: r2=width, r3=height, sp[0]=RGB565, sp[2]=pixel pointer;
// RGB renderer: r2=width, r3=height, sp[0]=RGB565 pixel pointer.
// Indirect tables use x/y/width/height bytes followed by a flash pointer.
// Font dimensions/strides come from each renderer's selector branches and
// SMLABB indexing. One-byte placeholder aliases are not real font banks.
// Landscape digits are stored CCW; native non-indexed 23x10/40x10 symbols
// and numeric trailing dash/blank slots stay editable but outside presets.
// Shared and partial bitmap aliases have exactly one non-overlapping owner.
static constexpr RecordSpec kRecords[] = {
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, 81492, 0x08003000, 0x20002588, 0x080032b9, 0x44d8, { 0x2d, 0xe9, 0xff, 0x4f, 0x85, 0xb0, 0xc2, 0xf3 } },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, 81436, 0x08003000, 0x20002588, 0x080032b9, 0x44dc, { 0x2d, 0xe9, 0xff, 0x4f, 0x85, 0xb0, 0xc2, 0xf3 } },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, 85360, 0x08003000, 0x20002588, 0x080032b9, 0x45c8, { 0x2d, 0xe9, 0xff, 0x4f, 0x85, 0xb0, 0xc2, 0xf3 } },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, 85400, 0x08003000, 0x20002588, 0x080032b9, 0x4640, { 0x2d, 0xe9, 0xff, 0x4f, 0x85, 0xb0, 0xc2, 0xf3 } },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, 82932, 0x08005800, 0x20003608, 0x08005ab1, 0x5250, { 0x2d, 0xe9, 0xff, 0x4f, 0x85, 0xb0, 0xc2, 0xf3 } },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, 92412, 0x08003000, 0x200025b0, 0x080032b9, 0x4d82, { 0x2d, 0xe9, 0xff, 0x4f, 0x83, 0xb0, 0xc2, 0xf3 } },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, 97936, 0x08003000, 0x200025b0, 0x080032b9, 0x4e4a, { 0x2d, 0xe9, 0xff, 0x4f, 0x83, 0xb0, 0xc2, 0xf3 } },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, 97944, 0x08003000, 0x200025b0, 0x080032b9, 0x4e4a, { 0x2d, 0xe9, 0xff, 0x4f, 0x83, 0xb0, 0xc2, 0xf3 } },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, 99516, 0x08003000, 0x200025b0, 0x080032b9, 0x50b8, { 0x2d, 0xe9, 0xff, 0x4f, 0x85, 0xb0, 0xc2, 0xf3 } },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, 96588, 0x08005800, 0x20003648, 0x08005ab1, 0x5cf6, { 0x2d, 0xe9, 0xff, 0x4f, 0x83, 0xb0, 0xc2, 0xf3 } },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, 79412, 0x08003000, 0x20002580, 0x080032b9, 0x43cc, { 0x2d, 0xe9, 0xff, 0x4f, 0x85, 0xb0, 0xc2, 0xf3 } },
};
static const Resource kResources[] = {
    // Native HW 0x315c: draw call 0x080069e6; 4x4 at 0x0800fbc0.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_4x4_000", "Panel corner bottom right", "Controls", 4, 4, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fbc0 }, { 0x3151, 0x0800fb88 } } },
    // Native HW 0x315c: draw call 0x0800672a; 4x6 at 0x0800fbc4.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_4x6_001", "Panel corner top left", "Controls", 4, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fbc4 }, { 0x3151, 0x0800fb8c } } },
    // Native HW 0x315c: draw call 0x08008fae; 7x6 at 0x0800fbca.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x6_002", "Panel corner top left (landscape)", "Controls", 7, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fbca }, { 0x3151, 0x0800fb92 } } },
    // Native HW 0x315c: draw call 0x08008dd4; 7x6 at 0x0800fbd0.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x6_003", "Panel corner top right (landscape)", "Controls", 7, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fbd0 }, { 0x3151, 0x0800fb98 } } },
    // Native HW 0x315c: draw call 0x080092f0; 7x6 at 0x0800fbd6.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x6_004", "Panel corner bottom left (landscape)", "Controls", 7, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fbd6 }, { 0x3151, 0x0800fb9e } } },
    // Native HW 0x315c: draw call 0x08009146; 8x6 at 0x0800fbdc.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_8x6_005", "Panel corner bottom right (landscape)", "Controls", 8, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fbdc }, { 0x3151, 0x0800fba4 } } },
    // Native HW 0x315c: draw call 0x08006c06; 5x8 at 0x0800fbe2.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_5x8_006", "Decimal separator", "Units", 5, 8, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0800fbe2 }, { 0x3151, 0x0800fbaa } } },
    // Native HW 0x315c: draw call 0x0800924e; 10x4 at 0x0800fbea.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x4_007", "Decimal separator (landscape)", "Units", 10, 4, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0800fbea }, { 0x3151, 0x0800fbb2 } } },
    // Native HW 0x315c: draw call 0x080062dc; 43x9 at 0x0800fc08.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_43x9_008", "Header DRAG", "Labels", 43, 9, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0800fc08 }, { 0x3151, 0x0800fbd0 } } },
    // Native HW 0x315c: draw call 0x08006da8; 2x9 at 0x0800fc3e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x9_009", "Battery right edge", "Battery", 2, 9, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0800fc3e }, { 0x3151, 0x0800fc06 } } },
    // Native HW 0x315c: draw call 0x08008870; 17x25 at 0x0800fc47.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_17x25_010", "Locked", "Controls", 17, 25, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fc47 }, { 0x3151, 0x0800fc0f } } },
    // Native HW 0x315c: draw call 0x080088c0; 17x15 at 0x0800fd28.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_17x15_011", "Unit W", "Units", 17, 15, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0800fd28 }, { 0x3151, 0x0800fcf0 } } },
    // Native HW 0x315c: draw call 0x080065ea; 96x4 at 0x0800fd55.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x4_012", "Puff panel top edge", "Controls", 96, 4, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x0800fd55 }, { 0x3151, 0x0800fd1d } } },
    // Native HW 0x315c: draw call 0x08006604; 47x10 at 0x0800fd85.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_47x10_013", "Label Puff panel", "Labels", 47, 10, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fd85 }, { 0x3151, 0x0800fd4d } } },
    // Native HW 0x315c: draw call 0x08006740; 92x6 at 0x0800fdc1.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_92x6_014", "Puff panel bottom edge", "Controls", 92, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x0800fdc1 }, { 0x3151, 0x0800fd89 } } },
    // Native HW 0x315c: draw call 0x080067f2; 39x2 at 0x0800fe09.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_39x2_015", "Puff panel divider", "Controls", 39, 2, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fe09 }, { 0x3151, 0x0800fdd1 } } },
    // Native HW 0x315c: draw call 0x080067da; 18x22 at 0x0800fe13.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_18x22_016", "Puff panel side edge", "Controls", 18, 22, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fe13 }, { 0x3151, 0x0800fddb } } },
    // Native HW 0x315c: draw call 0x080068d2; 14x20 at 0x0800fe55.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x20_017", "Unit W panel", "Units", 14, 20, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fe55 }, { 0x3151, 0x0800fe1d } } },
    // Native HW 0x315c: draw call 0x08006bba; 39x2 at 0x0800fe7d.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_39x2_018", "Resistance panel top edge", "Controls", 39, 2, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fe7d }, { 0x3151, 0x0800fe45 } } },
    // Native HW 0x315c: draw call 0x08006bd2; 17x8 at 0x0800fe87.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_17x8_019", "Unit Ohm panel", "Units", 17, 8, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fe87 }, { 0x3151, 0x0800fe4f } } },
    // Native HW 0x315c: draw call 0x08006c84; 39x4 at 0x0800fe9f.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_39x4_020", "Resistance panel bottom edge", "Controls", 39, 4, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fe9f }, { 0x3151, 0x0800fe67 } } },
    // Native HW 0x315c: draw call 0x08006a3c; 8x10 at 0x0800feb3.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_8x10_021", "Decimal separator", "Units", 8, 10, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0800feb3 }, { 0x3151, 0x0800fe7b } } },
    // Native HW 0x315c: draw call 0x080069d0; 92x4 at 0x0800febd.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_92x4_022", "Time panel top edge", "Controls", 92, 4, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x0800febd }, { 0x3151, 0x0800fe85 } } },
    // Native HW 0x315c: draw call 0x08006ac2; 6x10 at 0x0800feed.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_6x10_023", "Unit S", "Units", 6, 10, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0800feed }, { 0x3151, 0x0800feb5 } } },
    // Native HW 0x315c: draw call 0x08006ae6; 37x10 at 0x0800fef7.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_37x10_024", "Label Time panel", "Labels", 37, 10, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800fef7 }, { 0x3151, 0x0800febf } } },
    // Native HW 0x315c: draw call 0x08006b06; 96x4 at 0x0800ff29.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x4_025", "Time panel bottom edge", "Controls", 96, 4, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x0800ff29 }, { 0x3151, 0x0800fef1 } } },
    // Native HW 0x315c: draw call 0x080064fe; 57x13 at 0x0800ff59.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_57x13_026", "Mode P", "Labels", 57, 13, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0800ff59 }, { 0x3151, 0x0800ff21 } } },
    // Native HW 0x315c: draw call 0x0800558c; 96x10 at 0x08010104.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x10_027", "Charging", "Labels", 96, 10, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x315c, 0x08010104 }, { 0x3151, 0x080100cc } } },
    // Native HW 0x315c: draw call 0x080058cc; 13x16 at 0x0801017c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_13x16_028", "Unit %", "Units", 13, 16, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0801017c }, { 0x3151, 0x08010144 }, { 0x3134, 0x0801408a }, { 0x3030, 0x080140b2 }, { 0x311c, 0x08015ef6 } } },
    // Native HW 0x315c: draw call 0x08008946; 17x51 at 0x080101bc.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_17x51_029", "Logo DRAG (landscape, color)", "Logos", 17, 51, 1, 0x315c, AssetFormat::Rgb565, "", 0, "", { { 0x315c, 0x080101bc }, { 0x3151, 0x08010184 } } },
    // Native HW 0x315c: draw call 0x08008938; 17x51 at 0x08010882.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_17x51_030", "Logo SUPER (landscape)", "Logos", 17, 51, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x08010882 }, { 0x3151, 0x0801084a } } },
    // Native HW 0x315c: draw call 0x08009370; 7x51 at 0x0801091b.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x51_031", "Header side edge (landscape)", "Controls", 7, 51, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0801091b }, { 0x3151, 0x080108e3 } } },
    // Native HW 0x315c: draw call 0x08009394; 5x5 at 0x0801094e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_5x5_032", "Decimal separator (landscape)", "Units", 5, 5, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0801094e }, { 0x3151, 0x08010916 } } },
    // Native HW 0x315c: draw call 0x080093a8; 5x5 at 0x08010953.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_5x5_033", "Decimal separator (landscape)", "Units", 5, 5, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x08010953 }, { 0x3151, 0x0801091b } } },
    // Native HW 0x315c: draw call 0x08008e86; 18x55 at 0x08010958.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_18x55_034", "Mode R panel (landscape)", "Controls", 18, 55, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x08010958 }, { 0x3151, 0x08010920 } } },
    // Native HW 0x315c: draw call 0x08008f98; 16x20 at 0x08010aa2.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_16x20_035", "Mode R panel pointer (landscape)", "Controls", 16, 20, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x08010aa2 }, { 0x3151, 0x08010a6a } } },
    // Native HW 0x315c: draw call 0x08008f82; 10x7 at 0x08010aca.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x7_036", "Unit Ohm (landscape)", "Units", 10, 7, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x08010aca }, { 0x3151, 0x08010a92 } } },
    // Native HW 0x315c: draw call 0x08008bc6; 27x22 at 0x08010ad8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_27x22_037", "Mode panel (landscape)", "Controls", 27, 22, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x08010ad8 }, { 0x3151, 0x08010aa0 } } },
    // Native HW 0x315c: draw call 0x08008c18; 27x22 at 0x08010fd4.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_27x22_038", "Mode panel fill (landscape)", "Controls", 27, 22, 1, 0x315c, AssetFormat::Rgb565, "", 0, "", { { 0x315c, 0x08010fd4 }, { 0x3151, 0x08010f9c } } },
    // Native HW 0x315c: draw call 0x08008bfa; 27x22 at 0x08011478.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_27x22_039", "Mode P panel (landscape)", "Controls", 27, 22, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x08011478 }, { 0x3151, 0x08011440 } } },
    // Native HW 0x315c: draw call 0x08008ca2; 18x35 at 0x080114d0.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_18x35_040", "Puff panel (landscape)", "Controls", 18, 35, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x080114d0 }, { 0x3151, 0x08011498 } } },
    // Native HW 0x315c: draw call 0x08008dbe; 35x20 at 0x08011539.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_35x20_041", "Puff panel pointer (landscape)", "Controls", 35, 20, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x08011539 }, { 0x3151, 0x08011501 } } },
    // Native HW 0x315c: draw call 0x08008a12; 10x10 at 0x0801159d.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x10_042", "Unit S (landscape)", "Units", 10, 10, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0801159d }, { 0x3151, 0x08011565 } } },
    // Native HW 0x315c: draw call 0x080091ea; 18x54 at 0x080115b1.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_18x54_043", "Voltage panel (landscape)", "Controls", 18, 54, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x080115b1 }, { 0x3151, 0x08011579 } } },
    // Native HW 0x315c: draw call 0x08009204; 16x20 at 0x080116f5.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_16x20_044", "Voltage panel pointer (landscape)", "Controls", 16, 20, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x080116f5 }, { 0x3151, 0x080116bd } } },
    // Native HW 0x315c: draw call 0x080092da; 10x9 at 0x0801171d.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x9_045", "Unit V (landscape)", "Units", 10, 9, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0801171d }, { 0x3151, 0x080116e5 } } },
    // Native HW 0x315c: draw call 0x08009040; 18x54 at 0x0801172f.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_18x54_046", "Time panel (landscape)", "Controls", 18, 54, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0801172f }, { 0x3151, 0x080116f7 } } },
    // Native HW 0x315c: draw call 0x08009056; 16x20 at 0x080117d1.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_16x20_047", "Time panel pointer (landscape)", "Controls", 16, 20, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x080117d1 }, { 0x3151, 0x08011799 } } },
    // Native HW 0x315c: draw call 0x08009132; 10x9 at 0x080117f9.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x9_048", "Unit W (landscape)", "Units", 10, 9, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x080117f9 }, { 0x3151, 0x080117c1 } } },
    // Native HW 0x315c: draw call 0x08008a70; 28x49 at 0x0801180b.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_28x49_049", "Mode S panel (landscape)", "Controls", 28, 49, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0801180b }, { 0x3151, 0x080117d3 } } },
    // Native HW 0x315c: draw call 0x08005c14; 96x12 at 0x08011a24.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x12_050", "Label Theme", "Labels", 96, 12, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x08011a24 }, { 0x3151, 0x080119ec } } },
    // Native HW 0x315c: draw call 0x08005cd2; 15x42 at 0x08011ab4.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x42_051", "Theme panel left edge", "Controls", 15, 42, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "", { { 0x315c, 0x08011ab4 }, { 0x3151, 0x08011a7c } } },
    // Native HW 0x315c: draw call 0x08005cf0; 15x42 at 0x08011b08.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x42_052", "Theme panel right edge", "Controls", 15, 42, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "", { { 0x315c, 0x08011b08 }, { 0x3151, 0x08011ad0 } } },
    // Native HW 0x315c: draw call 0x08005c86; 66x42 at 0x08011b5c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_66x42_053", "Theme IRON panel", "Controls", 66, 42, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x315c, 0x08011b5c }, { 0x3151, 0x08011b24 } } },
    // Native HW 0x315c: draw call 0x08005c9c; 30x8 at 0x08011cd6.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_30x8_054", "Label IRON", "Labels", 30, 8, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x08011cd6 }, { 0x3151, 0x08011c9e }, { 0x3134, 0x0801236c }, { 0x3030, 0x08012394 }, { 0x311c, 0x080141d8 } } },
    // Native HW 0x315c: draw call 0x08005d08; 66x42 at 0x08011cf6.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_66x42_055", "Theme CORE panel", "Controls", 66, 42, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x315c, 0x08011cf6 }, { 0x3151, 0x08011cbe } } },
    // Native HW 0x315c: draw call 0x08005d1e; 34x8 at 0x08011e70.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_34x8_056", "Label CORE", "Labels", 34, 8, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x08011e70 }, { 0x3151, 0x08011e38 }, { 0x3134, 0x08012584 }, { 0x3030, 0x080125ac }, { 0x311c, 0x080143f0 } } },
    // Native HW 0x315c: draw call 0x0800b1ec; 96x11 at 0x08011e98.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x11_057", "Logo VOOPOO (chip information)", "Logos", 96, 11, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x315c, 0x08011e98 }, { 0x3151, 0x08011e60 } } },
    // Native HW 0x315c: draw call 0x0800b210; 96x63 at 0x08011f1c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x63_058", "Chip ID panel", "Labels", 96, 63, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x315c, 0x08011f1c }, { 0x3151, 0x08011ee4 } } },
    // Native HW 0x315c: draw call 0x0800b2ce; 96x46 at 0x08012210.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x46_059", "Level Code panel", "Labels", 96, 46, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x315c, 0x08012210 }, { 0x3151, 0x080121d8 } } },
    // Native HW 0x315c: draw call 0x08006534; 5x3 at 0x08012438.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_5x3_060", "Animation dot", "Animations", 5, 3, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x08012438 }, { 0x3151, 0x08012400 } } },
    // Native HW 0x315c: draw call 0x080064b4; 9x6 at 0x0801243b.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_9x6_061", "Animated segment 01", "Animations / Portrait", 9, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x0801243b }, { 0x3151, 0x08012403 } } },
    // Native HW 0x315c: draw call 0x080064b4; 13x7 at 0x08012447.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_13x7_062", "Animated segment 02", "Animations / Portrait", 13, 7, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x08012447 }, { 0x3151, 0x0801240f } } },
    // Native HW 0x315c: draw call 0x080064b4; 15x9 at 0x08012455.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x9_063", "Animated segment 03", "Animations / Portrait", 15, 9, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x08012455 }, { 0x3151, 0x0801241d } } },
    // Native HW 0x315c: draw call 0x080064b4; 17x7 at 0x08012467.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_17x7_064", "Animated segment 04", "Animations / Portrait", 17, 7, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x08012467 }, { 0x3151, 0x0801242f } } },
    // Native HW 0x315c: draw call 0x080064b4; 13x7 at 0x0801247c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_13x7_065", "Animated segment 05", "Animations / Portrait", 13, 7, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x0801247c }, { 0x3151, 0x08012444 } } },
    // Native HW 0x315c: draw call 0x080064b4; 15x9 at 0x0801248a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x9_066", "Animated segment 06", "Animations / Portrait", 15, 9, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x0801248a }, { 0x3151, 0x08012452 } } },
    // Native HW 0x315c: draw call 0x080064b4; 14x6 at 0x0801249c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x6_067", "Animated segment 07", "Animations / Portrait", 14, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x0801249c }, { 0x3151, 0x08012464 } } },
    // Native HW 0x315c: draw call 0x080064b4; 5x3 at 0x080124a8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_5x3_068", "Animated segment 08", "Animations / Portrait", 5, 3, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x080124a8 }, { 0x3151, 0x08012470 } } },
    // Native HW 0x315c: draw call 0x080064b4; 10x5 at 0x080124ab.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x5_069", "Animated segment 09", "Animations / Portrait", 10, 5, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x080124ab }, { 0x3151, 0x08012473 } } },
    // Native HW 0x315c: draw call 0x080064b4; 6x3 at 0x080124b5.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_6x3_070", "Animated segment 10", "Animations / Portrait", 6, 3, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x080124b5 }, { 0x3151, 0x0801247d } } },
    // Native HW 0x315c: draw call 0x080064b4; 4x2 at 0x080124b8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_4x2_071", "Animated segment 11", "Animations / Portrait", 4, 2, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x080124b8 }, { 0x3151, 0x08012480 } } },
    // Native HW 0x315c: draw call 0x080064b4; 5x6 at 0x080124ba.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_5x6_072", "Animated segment 12", "Animations / Portrait", 5, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x080124ba }, { 0x3151, 0x08012482 } } },
    // Native HW 0x315c: draw call 0x080064b4; 4x9 at 0x080124c0.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_4x9_073", "Animated segment 13", "Animations / Portrait", 4, 9, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x080124c0 }, { 0x3151, 0x08012488 } } },
    // Native HW 0x315c: draw call 0x080064b4; 10x14 at 0x080124c9.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x14_074", "Animated segment 14", "Animations / Portrait", 10, 14, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x080124c9 }, { 0x3151, 0x08012491 } } },
    // Native HW 0x315c: draw call 0x080064b4; 14x4 at 0x080124e5.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x4_075", "Animated segment 15", "Animations / Portrait", 14, 4, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x080124e5 }, { 0x3151, 0x080124ad } } },
    // Native HW 0x315c: draw call 0x080064b4; 30x6 at 0x080124ed.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_30x6_076", "Animated segment 16", "Animations / Portrait", 30, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x080124ed }, { 0x3151, 0x080124b5 } } },
    // Native HW 0x315c: draw call 0x080064b4; 13x4 at 0x08012505.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_13x4_077", "Animated segment 17", "Animations / Portrait", 13, 4, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x08012505 }, { 0x3151, 0x080124cd } } },
    // Native HW 0x315c: draw call 0x080064b4; 5x15 at 0x0801250d.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_5x15_078", "Animated segment 18", "Animations / Portrait", 5, 15, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x0801250d }, { 0x3151, 0x080124d5 } } },
    // Native HW 0x315c: draw call 0x080064b4; 5x12 at 0x0801251c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_5x12_079", "Animated segment 19", "Animations / Portrait", 5, 12, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x0801251c }, { 0x3151, 0x080124e4 } } },
    // Native HW 0x315c: draw call 0x080064b4; 4x10 at 0x08012528.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_4x10_080", "Animated segment 20", "Animations / Portrait", 4, 10, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x08012528 }, { 0x3151, 0x080124f0 } } },
    // Native HW 0x315c: draw call 0x080064b4; 2x7 at 0x08012532.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x7_081", "Animated segment 21", "Animations / Portrait", 2, 7, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x08012532 }, { 0x3151, 0x080124fa } } },
    // Native HW 0x315c: draw call 0x080064b4; 4x5 at 0x08012539.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_4x5_082", "Animated segment 22", "Animations / Portrait", 4, 5, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x315c, 0x08012539 }, { 0x3151, 0x08012501 } } },
    // Native HW 0x315c: draw call 0x08008b20; 10x6 at 0x0801253e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x6_083", "Animated block 01", "Animations / Landscape", 10, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x0801253e }, { 0x3151, 0x08012506 } } },
    // Native HW 0x315c: draw call 0x08008b20; 14x6 at 0x0801254a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x6_084", "Animated block 02", "Animations / Landscape", 14, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x0801254a }, { 0x3151, 0x08012512 } } },
    // Native HW 0x315c: draw call 0x08008b20; 14x15 at 0x08012556.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x15_085", "Animated block 03", "Animations / Landscape", 14, 15, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x08012556 }, { 0x3151, 0x0801251e } } },
    // Native HW 0x315c: draw call 0x08008b20; 14x12 at 0x08012574.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x12_086", "Animated block 04", "Animations / Landscape", 14, 12, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x08012574 }, { 0x3151, 0x0801253c } } },
    // Native HW 0x315c: draw call 0x08008b20; 14x6 at 0x0801258c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x6_087", "Animated block 05", "Animations / Landscape", 14, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x0801258c }, { 0x3151, 0x08012554 } } },
    // Native HW 0x315c: draw call 0x08008b20; 8x6 at 0x08012598.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_8x6_088", "Animated block 06", "Animations / Landscape", 8, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x08012598 }, { 0x3151, 0x08012560 } } },
    // Native HW 0x315c: draw call 0x08008b20; 13x11 at 0x0801259e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_13x11_089", "Animated block 07", "Animations / Landscape", 13, 11, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x0801259e }, { 0x3151, 0x08012566 } } },
    // Native HW 0x315c: draw call 0x08008b20; 15x11 at 0x080125b4.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x11_090", "Animated block 08", "Animations / Landscape", 15, 11, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x080125b4 }, { 0x3151, 0x0801257c } } },
    // Native HW 0x315c: draw call 0x08008b20; 10x6 at 0x080125ca.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x6_091", "Animated block 09", "Animations / Landscape", 10, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x080125ca }, { 0x3151, 0x08012592 } } },
    // Native HW 0x315c: draw call 0x08008b20; 17x6 at 0x080125d6.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_17x6_092", "Animated block 10", "Animations / Landscape", 17, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x080125d6 }, { 0x3151, 0x0801259e } } },
    // Native HW 0x315c: draw call 0x08008b20; 17x8 at 0x080125e8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_17x8_093", "Animated block 11", "Animations / Landscape", 17, 8, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x080125e8 }, { 0x3151, 0x080125b0 } } },
    // Native HW 0x315c: draw call 0x08008b20; 7x7 at 0x08012600.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x7_094", "Animated block 12", "Animations / Landscape", 7, 7, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x08012600 }, { 0x3151, 0x080125c8 } } },
    // Native HW 0x315c: draw call 0x08008b20; 7x3 at 0x08012607.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x3_095", "Animated block 13", "Animations / Landscape", 7, 3, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x08012607 }, { 0x3151, 0x080125cf } } },
    // Native HW 0x315c: draw call 0x08008b20; 17x9 at 0x0801260a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_17x9_096", "Animated block 14", "Animations / Landscape", 17, 9, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x0801260a }, { 0x3151, 0x080125d2 } } },
    // Native HW 0x315c: draw call 0x08008b20; 17x6 at 0x08012625.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_17x6_097", "Animated block 15", "Animations / Landscape", 17, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x08012625 }, { 0x3151, 0x080125ed } } },
    // Native HW 0x315c: draw call 0x08008b20; 10x6 at 0x08012637.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x6_098", "Animated block 16", "Animations / Landscape", 10, 6, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x08012637 }, { 0x3151, 0x080125ff } } },
    // Native HW 0x315c: draw call 0x08008b20; 15x11 at 0x08012643.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x11_099", "Animated block 17", "Animations / Landscape", 15, 11, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x08012643 }, { 0x3151, 0x0801260b } } },
    // Native HW 0x315c: draw call 0x08008b20; 12x11 at 0x08012659.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x11_100", "Animated block 18", "Animations / Landscape", 12, 11, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x315c, 0x08012659 }, { 0x3151, 0x08012621 } } },
    // Native HW 0x315c: draw call 0x0800506c; 59x29 at 0x0801284f.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_59x29_101", "OFF", "Messages", 59, 29, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0801284f }, { 0x3151, 0x08012817 } } },
    // Native HW 0x315c: draw call 0x08004ff8; 96x20 at 0x08012937.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x20_102", "Logo DRAG 3 (boot)", "Logos", 96, 20, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x315c, 0x08012937 }, { 0x3151, 0x080128ff } } },
    // Native HW 0x315c: draw call 0x0800501e; 96x53 at 0x08012a27.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x53_103", "Logo GENE FAN (boot)", "Logos", 96, 53, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x08012a27 }, { 0x3151, 0x080129ef } } },
    // Native HW 0x315c: draw call 0x080057a4; 63x24 at 0x08013063.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_63x24_104", "Battery gauge frame", "Battery", 63, 24, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x08013063 }, { 0x3151, 0x0801302b }, { 0x3134, 0x08013f7a }, { 0x3030, 0x08013fa2 }, { 0x311c, 0x08015de6 } } },
    // Native HW 0x315c: font selector 1; 6x10 at 0x08013123.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_6x10_105", "Font 6x10", "Font 6x10", 6, 10, 11, 0x315c, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x315c, 0x08013123 }, { 0x3151, 0x080130eb }, { 0x3134, 0x080140aa }, { 0x3030, 0x080140d2 }, { 0x311c, 0x08015f16 } } },
    // Native HW 0x315c: font selector 2; 5x8 at 0x08013191.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_5x8_106", "Font 5x8", "Font 5x8", 5, 8, 11, 0x315c, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x315c, 0x08013191 }, { 0x3151, 0x08013159 } } },
    // Native HW 0x315c: font selector 4; 7x13 at 0x080131e9.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_7x13_107", "Font 7x13", "Font 7x13", 7, 13, 16, 0x315c, AssetFormat::Mask1Bit, "0123456789ABCDEF", 0, "digits_wattage", { { 0x315c, 0x080131e9 }, { 0x3151, 0x080131b1 }, { 0x3134, 0x08014118 }, { 0x3030, 0x08014140 }, { 0x311c, 0x08015f84 } } },
    // Native HW 0x315c: font selector 7; 19x40 at 0x080132b9.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_19x40_108", "Font 19x40", "Font 19x40", 19, 40, 10, 0x315c, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x315c, 0x080132b9 }, { 0x3151, 0x08013281 }, { 0x3134, 0x080141e8 }, { 0x3030, 0x08014210 }, { 0x311c, 0x08016054 } } },
    // Native HW 0x315c: draw call 0x08005e98; 96x40 at 0x0801376a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x40_109", "Icon Time Out", "Messages", 96, 40, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x315c, 0x0801376a }, { 0x3151, 0x08013732 } } },
    // Native HW 0x315c: draw call 0x08005ebe; 96x58 at 0x0801394a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x58_110", "Time Out", "Messages", 96, 58, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0801394a }, { 0x3151, 0x08013912 } } },
    // Native HW 0x315c: draw call 0x08005b96; 96x54 at 0x08013c02.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x54_111", "Icon Atomizer", "Messages", 96, 54, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x315c, 0x08013c02 }, { 0x3151, 0x08013bca } } },
    // Native HW 0x315c: draw call 0x08005bbc; 96x58 at 0x08013e8a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x58_112", "Max Power", "Messages", 96, 58, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x08013e8a }, { 0x3151, 0x08013e52 } } },
    // Native HW 0x315c: draw call 0x080054e0; 96x41 at 0x08014142.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x41_113", "Icon Atomizer Short", "Messages", 96, 41, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x08014142 }, { 0x3151, 0x0801410a } } },
    // Native HW 0x315c: draw call 0x08005506; 96x58 at 0x0801432e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x58_114", "Atomizer Short", "Messages", 96, 58, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0801432e }, { 0x3151, 0x080142f6 } } },
    // Native HW 0x315c: draw call 0x08005a90; 96x54 at 0x080145e6.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x54_115", "Icon Check Battery", "Messages", 96, 54, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x080145e6 }, { 0x3151, 0x080145ae } } },
    // Native HW 0x315c: draw call 0x08005ab6; 96x58 at 0x0801486e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x58_116", "Check Battery", "Messages", 96, 58, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0801486e }, { 0x3151, 0x08014836 } } },
    // Native HW 0x315c: draw call 0x08005b12; 96x53 at 0x08014b26.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x53_117", "Icon Low Power", "Messages", 96, 53, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x08014b26 }, { 0x3151, 0x08014aee } } },
    // Native HW 0x315c: draw call 0x08005b38; 96x58 at 0x08014da2.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x58_118", "Low Power", "Messages", 96, 58, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x08014da2 }, { 0x3151, 0x08014d6a } } },
    // Native HW 0x315c: draw call 0x08005f9c; 96x43 at 0x0801505a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x43_119", "Locked", "Messages", 96, 43, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0801505a }, { 0x3151, 0x08015022 } } },
    // Native HW 0x315c: draw call 0x08005fc2; 96x58 at 0x0801525e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x58_120", "Press Fire & +", "Messages", 96, 58, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0801525e }, { 0x3151, 0x08015226 } } },
    // Native HW 0x315c: draw call 0x08005f1a; 96x43 at 0x08015516.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x43_121", "Unlocked", "Messages", 96, 43, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_lime", { { 0x315c, 0x08015516 }, { 0x3151, 0x080154de } } },
    // Native HW 0x315c: draw call 0x080059e6; 96x26 at 0x0801571a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x26_122", "Icon Check Atomizer", "Messages", 96, 26, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x315c, 0x0801571a }, { 0x3151, 0x080156e2 } } },
    // Native HW 0x315c: draw call 0x08005a0c; 96x40 at 0x08015852.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x40_123", "Atomizer silhouette", "Messages", 96, 40, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "accent_cyan", { { 0x315c, 0x08015852 }, { 0x3151, 0x0801581a } } },
    // Native HW 0x315c: draw call 0x08005a32; 96x58 at 0x08015a32.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x58_124", "Check Atomizer", "Messages", 96, 58, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x08015a32 }, { 0x3151, 0x080159fa } } },
    // Native HW 0x315c: draw call 0x08005e18; 96x47 at 0x08015cea.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x47_125", "Icon High Temp", "Messages", 96, 47, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x315c, 0x08015cea }, { 0x3151, 0x08015cb2 } } },
    // Native HW 0x315c: draw call 0x08005e3e; 96x58 at 0x08015f1e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x58_126", "High Temp", "Messages", 96, 58, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x08015f1e }, { 0x3151, 0x08015ee6 } } },
    // Native HW 0x315c: draw call 0x08005d96; 96x31 at 0x080161d6.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x31_127", "Icon Puff reset", "Messages", 96, 31, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x315c, 0x080161d6 }, { 0x3151, 0x0801619e } } },
    // Native HW 0x315c: draw call 0x08005dbc; 96x58 at 0x0801634a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_96x58_128", "Puff Cleared", "Messages", 96, 58, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x0801634a }, { 0x3151, 0x08016312 } } },
    // Native HW 0x315c: font selector 5; 8x16 at 0x08016603.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_8x16_129", "Font 8x16", "Font 8x16", 8, 16, 11, 0x315c, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x315c, 0x08016603 }, { 0x3151, 0x080165cb }, { 0x3134, 0x080170f0 }, { 0x3030, 0x08017118 }, { 0x311c, 0x08018f5c } } },
    // Native HW 0x315c: font selector 14; 10x6 at 0x080166b8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_10x6_130", "Font 10x6 / Landscape", "Font 10x6 / Landscape", 10, 6, 11, 0x315c, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x315c, 0x080166b8 }, { 0x3151, 0x08016680 } } },
    // Native HW 0x315c: font selector 18; 30x14 at 0x0801673c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_30x14_131", "Font 30x14 / Landscape", "Font 30x14 / Landscape", 30, 14, 10, 0x315c, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x315c, 0x0801673c }, { 0x3151, 0x08016704 } } },
    // Native HW 0x315c: font selector 15; 23x13 at 0x0801696c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_23x13_132", "Font 23x13 / Landscape", "Font 23x13 / Landscape", 23, 13, 10, 0x315c, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x315c, 0x0801696c }, { 0x3151, 0x08016934 } } },
    // Native HW 0x315c: font selector 16; 23x10 at 0x08016af2.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "symbol_native_23x10_133", "Bar with raised tip", "Special glyphs 23x10 / Landscape", 23, 10, 1, 0x315c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x315c, 0x08016af2 }, { 0x3151, 0x08016aba } } },
    // Native HW 0x3134: draw call 0x0800934e; 2x3 at 0x080104fc.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_134", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x080104fc }, { 0x3030, 0x08010524 }, { 0x311c, 0x08012368 } } },
    // Native HW 0x3134: draw call 0x080095e0; 2x3 at 0x080104ff.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_135", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x080104ff } } },
    // Native HW 0x3134: draw call 0x080090f8; 2x3 at 0x08010502.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_136", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08010502 }, { 0x3030, 0x0801052a }, { 0x311c, 0x0801236e } } },
    // Native HW 0x3134: draw call 0x080098d0; 2x3 at 0x08010505.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_137", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08010505 } } },
    // Native HW 0x3134: draw call 0x08009b0c; 2x3 at 0x08010508.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_138", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08010508 } } },
    // Native HW 0x3134: draw call 0x080096d4; 2x3 at 0x0801050b.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_139", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x0801050b } } },
    // Native HW 0x3134: draw call 0x08009238; 15x2 at 0x0801050e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x2_140", "Decimal separator (landscape)", "Units", 15, 2, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x0801050e }, { 0x3030, 0x08010536 }, { 0x311c, 0x0801237a } } },
    // Native HW 0x3134: draw call 0x080094ca; 15x2 at 0x08010512.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x2_141", "Decimal separator (landscape)", "Units", 15, 2, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08010512 } } },
    // Native HW 0x3134: draw call 0x0800993a; 14x2 at 0x08010516.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x2_142", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08010516 }, { 0x3030, 0x0801053e }, { 0x311c, 0x08012382 } } },
    // Native HW 0x3134: draw call 0x08009b76; 14x2 at 0x0801051a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x2_143", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x0801051a } } },
    // Native HW 0x3134: draw call 0x08009742; 14x2 at 0x0801051e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x2_144", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x0801051e } } },
    // Native HW 0x3134: draw call 0x0800678e; 5x6 at 0x08010528.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_5x6_145", "Panel corner top left", "Controls", 5, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08010528 }, { 0x3030, 0x08010550 }, { 0x311c, 0x08012394 } } },
    // Native HW 0x3134: draw call 0x08006a84; 5x6 at 0x0801052e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_5x6_146", "Panel corner bottom right", "Controls", 5, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x0801052e }, { 0x3030, 0x08010556 }, { 0x311c, 0x0801239a } } },
    // Native HW 0x3134: draw call 0x08006cb2; 2x12 at 0x08010550.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x12_147", "Decimal separator", "Units", 2, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08010550 }, { 0x3030, 0x08010578 }, { 0x311c, 0x080123bc } } },
    // Native HW 0x3134: draw call 0x08006af8; 2x16 at 0x0801055c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x16_148", "Decimal separator", "Units", 2, 16, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x0801055c }, { 0x3030, 0x08010584 }, { 0x311c, 0x080123c8 } } },
    // Native HW 0x3134: draw call 0x080061f2; 32x10 at 0x080105cf.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_32x10_149", "Header DRAG", "Labels", 32, 10, 1, 0x3134, AssetFormat::Rgb565, "", 0, "", { { 0x3134, 0x080105cf }, { 0x3030, 0x080105f7 }, { 0x311c, 0x0801243b } } },
    // Native HW 0x3134: draw call 0x08006ece; 2x10 at 0x0801084f.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x10_150", "Battery right edge", "Battery", 2, 10, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x0801084f }, { 0x3030, 0x08010877 }, { 0x311c, 0x080126bb } } },
    // Native HW 0x3134: draw call 0x08006632; 128x6 at 0x08010859.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_128x6_151", "Puff panel top edge", "Controls", 128, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08010859 }, { 0x3030, 0x08010881 }, { 0x311c, 0x080126c5 } } },
    // Native HW 0x3134: draw call 0x0800664c; 50x16 at 0x080108b9.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_50x16_152", "Label Puff panel", "Labels", 50, 16, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x080108b9 }, { 0x3030, 0x080108e1 }, { 0x311c, 0x08012725 } } },
    // Native HW 0x3134: draw call 0x080067a4; 123x6 at 0x08010929.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_123x6_153", "Puff panel bottom edge", "Controls", 123, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08010929 }, { 0x3030, 0x08010951 }, { 0x311c, 0x08012795 } } },
    // Native HW 0x3134: draw call 0x08006840; 63x7 at 0x08010989.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_63x7_154", "Puff panel divider", "Controls", 63, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08010989 }, { 0x3030, 0x080109b1 }, { 0x311c, 0x080127f5 } } },
    // Native HW 0x3134: draw call 0x0800685a; 8x12 at 0x080109c1.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_8x12_155", "Unit V panel", "Units", 8, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x080109c1 }, { 0x3030, 0x080109e9 }, { 0x311c, 0x0801282d } } },
    // Native HW 0x3134: draw call 0x0800694e; 7x12 at 0x080109cd.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x12_156", "Unit Ohm", "Units", 7, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x080109cd }, { 0x3030, 0x080109f5 }, { 0x311c, 0x08012839 } } },
    // Native HW 0x3134: draw call 0x08006964; 19x12 at 0x080109d9.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_19x12_157", "Unit W panel", "Units", 19, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x080109d9 }, { 0x3030, 0x08010a01 }, { 0x311c, 0x08012845 } } },
    // Native HW 0x3134: draw call 0x08006988; 63x3 at 0x080109fd.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_63x3_158", "Resistance panel top edge", "Controls", 63, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x080109fd }, { 0x3030, 0x08010a25 }, { 0x311c, 0x08012869 } } },
    // Native HW 0x3134: draw call 0x080069ac; 63x5 at 0x08010a15.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_63x5_159", "Time panel top edge", "Controls", 63, 5, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08010a15 }, { 0x3030, 0x08010a3d }, { 0x311c, 0x08012881 } } },
    // Native HW 0x3134: draw call 0x08006c56; 65x16 at 0x08010a3d.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_65x16_160", "Voltage panel top edge", "Controls", 65, 16, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08010a3d }, { 0x3030, 0x08010a65 }, { 0x311c, 0x080128a9 } } },
    // Native HW 0x3134: draw call 0x08006c6e; 20x12 at 0x08010acd.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_20x12_161", "Unit Ohm panel", "Units", 20, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08010acd }, { 0x3030, 0x08010af5 }, { 0x311c, 0x08012939 } } },
    // Native HW 0x3134: draw call 0x08006d48; 7x12 at 0x08010af1.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x12_162", "Unit V", "Units", 7, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08010af1 }, { 0x3030, 0x08010b19 }, { 0x311c, 0x0801295d } } },
    // Native HW 0x3134: draw call 0x08006d6a; 8x12 at 0x08010afd.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_8x12_163", "Decimal separator", "Units", 8, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08010afd }, { 0x3030, 0x08010b25 }, { 0x311c, 0x08012969 } } },
    // Native HW 0x3134: draw call 0x08006d88; 65x7 at 0x08010b09.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_65x7_164", "Resistance panel bottom edge", "Controls", 65, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08010b09 }, { 0x3030, 0x08010b31 }, { 0x311c, 0x08012975 } } },
    // Native HW 0x3134: draw call 0x08006a6e; 123x6 at 0x08010b48.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_123x6_165", "Voltage panel bottom edge", "Controls", 123, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08010b48 }, { 0x3030, 0x08010b70 }, { 0x311c, 0x080129b4 } } },
    // Native HW 0x3134: draw call 0x08006bd0; 128x6 at 0x08010ba8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_128x6_166", "Time panel bottom edge", "Controls", 128, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08010ba8 }, { 0x3030, 0x08010bd0 }, { 0x311c, 0x08012a14 } } },
    // Native HW 0x3134: draw call 0x08008b24; 12x32 at 0x08010e60.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x32_167", "Logo DRAG (landscape)", "Logos", 12, 32, 1, 0x3134, AssetFormat::Rgb565, "", 0, "", { { 0x3134, 0x08010e60 }, { 0x3030, 0x08010e88 }, { 0x311c, 0x08012ccc } } },
    // Native HW 0x3134: draw call 0x08008af2; 21x79 at 0x08011160.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_21x79_168", "Logo SUPER (landscape)", "Logos", 21, 79, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011160 }, { 0x3030, 0x08011188 }, { 0x311c, 0x08012fcc } } },
    // Native HW 0x3134: draw call 0x08009ca2; 10x79 at 0x0801124d.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x79_169", "Header side edge (landscape)", "Controls", 10, 79, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x0801124d }, { 0x3030, 0x08011275 }, { 0x311c, 0x080130b9 } } },
    // Native HW 0x3134: draw call 0x08009cc8; 7x13 at 0x080112eb.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x13_170", "Unit Ohm (landscape)", "Units", 7, 13, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x080112eb }, { 0x3030, 0x08011313 }, { 0x311c, 0x08013157 } } },
    // Native HW 0x3134: draw call 0x08009cea; 7x13 at 0x080112f8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x13_171", "Unit S (landscape)", "Units", 7, 13, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x080112f8 }, { 0x3030, 0x08011320 }, { 0x311c, 0x08013164 } } },
    // Native HW 0x3134: draw call 0x080091c4; 23x72 at 0x08011305.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_23x72_172", "Mode R panel (landscape)", "Controls", 23, 72, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011305 }, { 0x3030, 0x0801132d }, { 0x311c, 0x08013171 } } },
    // Native HW 0x3134: draw call 0x08009338; 15x21 at 0x080113dd.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x21_173", "Mode R panel pointer (landscape)", "Controls", 15, 21, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x080113dd }, { 0x3030, 0x08011405 }, { 0x311c, 0x08013249 } } },
    // Native HW 0x3134: draw call 0x08009314; 15x10 at 0x08011407.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x10_174", "Unit Ohm (landscape)", "Units", 15, 10, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08011407 }, { 0x3030, 0x0801142f }, { 0x311c, 0x08013273 } } },
    // Native HW 0x3134: draw call 0x08009440; 6x12 at 0x0801141b.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_6x12_175", "Decimal separator (landscape)", "Units", 6, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "", { { 0x3134, 0x0801141b }, { 0x3030, 0x08011443 }, { 0x311c, 0x08013287 } } },
    // Native HW 0x3134: draw call 0x08009456; 23x72 at 0x08011427.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_23x72_176", "Mode R panel (landscape, alternate)", "Controls", 23, 72, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011427 }, { 0x3030, 0x0801144f }, { 0x311c, 0x08013293 } } },
    // Native HW 0x3134: draw call 0x080095ca; 15x21 at 0x080114ff.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x21_177", "Mode R panel pointer (landscape)", "Controls", 15, 21, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x080114ff } } },
    // Native HW 0x3134: draw call 0x080095a6; 15x10 at 0x08011529.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x10_178", "Unit Ohm (landscape)", "Units", 15, 10, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08011529 } } },
    // Native HW 0x3134: draw call 0x08008dee; 40x7 at 0x0801153d.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_40x7_179", "Puff panel side edge (landscape)", "Controls", 40, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x0801153d }, { 0x3030, 0x08011565 }, { 0x311c, 0x080133a9 } } },
    // Native HW 0x3134: draw call 0x08008dc0; 18x16 at 0x08011560.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_18x16_180", "Mode panel (landscape)", "Controls", 18, 16, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011560 }, { 0x3030, 0x08011588 }, { 0x311c, 0x080133cc } } },
    // Native HW 0x3134: draw call 0x08008e3e; 18x16 at 0x080117d0.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_18x16_181", "Mode panel fill (landscape)", "Controls", 18, 16, 1, 0x3134, AssetFormat::Rgb565, "", 0, "", { { 0x3134, 0x080117d0 }, { 0x3030, 0x080117f8 }, { 0x311c, 0x0801363c } } },
    // Native HW 0x3134: draw call 0x08008e2a; 18x16 at 0x08011a10.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_18x16_182", "Mode P panel (landscape)", "Controls", 18, 16, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011a10 }, { 0x3030, 0x08011a38 }, { 0x311c, 0x0801387c } } },
    // Native HW 0x3134: draw call 0x08008f92; 23x72 at 0x08011a40.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_23x72_183", "Puff panel (landscape)", "Controls", 23, 72, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011a40 }, { 0x3030, 0x08011a68 }, { 0x311c, 0x080138ac } } },
    // Native HW 0x3134: draw call 0x080090e2; 15x21 at 0x08011b18.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x21_184", "Puff panel pointer (landscape)", "Controls", 15, 21, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011b18 }, { 0x3030, 0x08011b40 }, { 0x311c, 0x08013984 } } },
    // Native HW 0x3134: draw call 0x08008efc; 13x12 at 0x08011b42.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_13x12_185", "Unit S (landscape)", "Units", 13, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08011b42 }, { 0x3030, 0x08011b6a }, { 0x311c, 0x080139ae } } },
    // Native HW 0x3134: draw call 0x080098a4; 23x69 at 0x08011b5a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_23x69_186", "Voltage panel (landscape)", "Controls", 23, 69, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011b5a }, { 0x3030, 0x08011b82 }, { 0x311c, 0x080139c6 } } },
    // Native HW 0x3134: draw call 0x080098b8; 15x21 at 0x08011c29.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x21_187", "Voltage panel pointer (landscape)", "Controls", 15, 21, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011c29 }, { 0x3030, 0x08011c51 }, { 0x311c, 0x08013a95 } } },
    // Native HW 0x3134: draw call 0x080099d6; 14x10 at 0x08011c53.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x10_188", "Unit V (landscape)", "Units", 14, 10, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08011c53 }, { 0x3030, 0x08011c7b }, { 0x311c, 0x08013abf } } },
    // Native HW 0x3134: draw call 0x08009aba; 6x12 at 0x08011c67.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_6x12_189", "Decimal separator (landscape)", "Units", 6, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011c67 }, { 0x3030, 0x08011c8f }, { 0x311c, 0x08013ad3 } } },
    // Native HW 0x3134: draw call 0x08009ae0; 23x69 at 0x08011c73.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_23x69_190", "Voltage panel (landscape, alternate)", "Controls", 23, 69, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011c73 }, { 0x3030, 0x08011c9b }, { 0x311c, 0x08013adf } } },
    // Native HW 0x3134: draw call 0x08009af4; 15x21 at 0x08011d42.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x21_191", "Voltage panel pointer (landscape)", "Controls", 15, 21, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011d42 } } },
    // Native HW 0x3134: draw call 0x08009c12; 14x10 at 0x08011d6c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x10_192", "Unit V (landscape)", "Units", 14, 10, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08011d6c } } },
    // Native HW 0x3134: draw call 0x080096be; 15x21 at 0x08011d80.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x21_193", "Time panel pointer (landscape)", "Controls", 15, 21, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011d80 }, { 0x3030, 0x08011da8 }, { 0x311c, 0x08013bec } } },
    // Native HW 0x3134: draw call 0x08009696; 23x69 at 0x08011daa.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_23x69_194", "Time panel (landscape)", "Controls", 23, 69, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011daa }, { 0x3030, 0x08011dd2 }, { 0x311c, 0x08013c16 } } },
    // Native HW 0x3134: draw call 0x080097de; 14x10 at 0x08011e79.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x10_195", "Unit W (landscape)", "Units", 14, 10, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08011e79 }, { 0x3030, 0x08011ea1 }, { 0x311c, 0x08013ce5 } } },
    // Native HW 0x3134: draw call 0x08008c4e; 40x23 at 0x08011e8d.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_40x23_196", "Mode S panel (landscape)", "Controls", 40, 23, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08011e8d }, { 0x3030, 0x08011eb5 }, { 0x311c, 0x08013cf9 } } },
    // Native HW 0x3134: draw call 0x08005b6a; 58x12 at 0x0801206c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_58x12_197", "Label Theme", "Labels", 58, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x0801206c }, { 0x3030, 0x08012094 }, { 0x311c, 0x08013ed8 } } },
    // Native HW 0x3134: draw call 0x08005c70; 10x42 at 0x080120cc.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x42_198", "Theme panel left edge", "Controls", 10, 42, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "", { { 0x3134, 0x080120cc }, { 0x3030, 0x080120f4 }, { 0x311c, 0x08013f38 } } },
    // Native HW 0x3134: draw call 0x08005c8e; 10x42 at 0x08012120.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x42_199", "Theme panel right edge", "Controls", 10, 42, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "", { { 0x3134, 0x08012120 }, { 0x3030, 0x08012148 }, { 0x311c, 0x08013f8c } } },
    // Native HW 0x3134: draw call 0x08005c10; 94x42 at 0x08012174.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_94x42_200", "Theme IRON panel", "Controls", 94, 42, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3134, 0x08012174 }, { 0x3030, 0x0801219c }, { 0x311c, 0x08013fe0 } } },
    // Native HW 0x3134: draw call 0x08005cb6; 94x42 at 0x0801238c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_94x42_201", "Theme CORE panel", "Controls", 94, 42, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3134, 0x0801238c }, { 0x3030, 0x080123b4 }, { 0x311c, 0x080141f8 } } },
    // Native HW 0x3134: draw call 0x0800bcd0; 90x15 at 0x080125ac.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_90x15_202", "Logo VOOPOO (chip information)", "Logos", 90, 15, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3134, 0x080125ac }, { 0x3030, 0x080125d4 }, { 0x311c, 0x08014418 } } },
    // Native HW 0x3134: draw call 0x0800bd12; 102x87 at 0x08012660.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_102x87_203", "Chip ID panel", "Labels", 102, 87, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3134, 0x08012660 }, { 0x3030, 0x08012688 }, { 0x311c, 0x080144cc } } },
    // Native HW 0x3134: draw call 0x0800bdf8; 102x63 at 0x08012acb.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_102x63_204", "Level Code panel", "Labels", 102, 63, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3134, 0x08012acb }, { 0x3030, 0x08012af3 }, { 0x311c, 0x08014937 } } },
    // Native HW 0x3134: draw call 0x08006542; 30x16 at 0x08012dfe.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_30x16_205", "Mode P", "Labels", 30, 16, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08012dfe }, { 0x3030, 0x08012e26 }, { 0x311c, 0x08014c6a } } },
    // Native HW 0x3134: draw call 0x08006574; 14x4 at 0x08012e3e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x4_206", "Animation segment", "Animations", 14, 4, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x08012e3e }, { 0x3030, 0x08012e66 }, { 0x311c, 0x08014caa } } },
    // Native HW 0x3134: draw call 0x08006514; 15x6 at 0x08012e46.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x6_207", "Animated segment 01", "Animations / Portrait", 15, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012e46 }, { 0x3030, 0x08012e6e }, { 0x311c, 0x08014cb2 } } },
    // Native HW 0x3134: draw call 0x08006514; 15x7 at 0x08012e52.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_208", "Animated segment 02", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012e52 }, { 0x3030, 0x08012e7a }, { 0x311c, 0x08014cbe } } },
    // Native HW 0x3134: draw call 0x08006514; 15x7 at 0x08012e60.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_209", "Animated segment 03", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012e60 }, { 0x3030, 0x08012e88 }, { 0x311c, 0x08014ccc } } },
    // Native HW 0x3134: draw call 0x08006514; 16x8 at 0x08012e6e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_16x8_210", "Animated segment 04", "Animations / Portrait", 16, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012e6e }, { 0x3030, 0x08012e96 }, { 0x311c, 0x08014cda } } },
    // Native HW 0x3134: draw call 0x08006514; 13x8 at 0x08012e7e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_13x8_211", "Animated segment 05", "Animations / Portrait", 13, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012e7e }, { 0x3030, 0x08012ea6 }, { 0x311c, 0x08014cea } } },
    // Native HW 0x3134: draw call 0x08006514; 12x8 at 0x08012e8e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x8_212", "Animated segment 06", "Animations / Portrait", 12, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012e8e }, { 0x3030, 0x08012eb6 }, { 0x311c, 0x08014cfa } } },
    // Native HW 0x3134: draw call 0x08006514; 16x8 at 0x08012e9e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_16x8_213", "Animated segment 07", "Animations / Portrait", 16, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012e9e }, { 0x3030, 0x08012ec6 }, { 0x311c, 0x08014d0a } } },
    // Native HW 0x3134: draw call 0x08006514; 15x7 at 0x08012eae.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_214", "Animated segment 08", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012eae }, { 0x3030, 0x08012ed6 }, { 0x311c, 0x08014d1a } } },
    // Native HW 0x3134: draw call 0x08006514; 15x7 at 0x08012ebc.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_215", "Animated segment 09", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012ebc }, { 0x3030, 0x08012ee4 }, { 0x311c, 0x08014d28 } } },
    // Native HW 0x3134: draw call 0x08006514; 15x6 at 0x08012eca.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x6_216", "Animated segment 10", "Animations / Portrait", 15, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012eca }, { 0x3030, 0x08012ef2 }, { 0x311c, 0x08014d36 } } },
    // Native HW 0x3134: draw call 0x08006514; 15x4 at 0x08012ed6.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x4_217", "Animated segment 11", "Animations / Portrait", 15, 4, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012ed6 }, { 0x3030, 0x08012efe }, { 0x311c, 0x08014d42 } } },
    // Native HW 0x3134: draw call 0x08006514; 3x6 at 0x08012ede.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_3x6_218", "Animated line 01", "Animations / Portrait", 3, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012ede }, { 0x3030, 0x08012f06 }, { 0x311c, 0x08014d4a } } },
    // Native HW 0x3134: draw call 0x08006514; 7x12 at 0x08012ee4.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x12_219", "Animated line 02", "Animations / Portrait", 7, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012ee4 }, { 0x3030, 0x08012f0c }, { 0x311c, 0x08014d50 } } },
    // Native HW 0x3134: draw call 0x08006514; 11x17 at 0x08012ef0.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_11x17_220", "Animated line 03", "Animations / Portrait", 11, 17, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012ef0 }, { 0x3030, 0x08012f18 }, { 0x311c, 0x08014d5c } } },
    // Native HW 0x3134: draw call 0x08006514; 14x20 at 0x08012f12.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x20_221", "Animated line 04", "Animations / Portrait", 14, 20, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012f12 }, { 0x3030, 0x08012f3a }, { 0x311c, 0x08014d7e } } },
    // Native HW 0x3134: draw call 0x08006514; 29x6 at 0x08012f3a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_29x6_222", "Animated line 05", "Animations / Portrait", 29, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012f3a }, { 0x3030, 0x08012f62 }, { 0x311c, 0x08014da6 } } },
    // Native HW 0x3134: draw call 0x08006514; 14x4 at 0x08012f52.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x4_223", "Animated line 06", "Animations / Portrait", 14, 4, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012f52 }, { 0x3030, 0x08012f7a }, { 0x311c, 0x08014dbe } } },
    // Native HW 0x3134: draw call 0x08006514; 11x17 at 0x08012f5a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_11x17_224", "Animated line 07", "Animations / Portrait", 11, 17, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012f5a }, { 0x3030, 0x08012f82 }, { 0x311c, 0x08014dc6 } } },
    // Native HW 0x3134: draw call 0x08006514; 7x12 at 0x08012f7c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x12_225", "Animated line 08", "Animations / Portrait", 7, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012f7c }, { 0x3030, 0x08012fa4 }, { 0x311c, 0x08014de8 } } },
    // Native HW 0x3134: draw call 0x08006514; 3x6 at 0x08012f88.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_3x6_226", "Animated line 09", "Animations / Portrait", 3, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012f88 }, { 0x3030, 0x08012fb0 }, { 0x311c, 0x08014df4 } } },
    // Native HW 0x3134: draw call 0x08006426; 8x3 at 0x08012f8e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_8x3_227", "Animated line 10", "Animations / Portrait", 8, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08012f8e }, { 0x3030, 0x08012fb6 }, { 0x311c, 0x08014dfa } } },
    // Native HW 0x3134: draw call 0x080063f8; 64x15 at 0x08012f91.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_64x15_228", "Logo SUPER", "Logos", 64, 15, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08012f91 }, { 0x3030, 0x08012fb9 }, { 0x311c, 0x08014dfd } } },
    // Native HW 0x3134: draw call 0x080063f8; 8x3 at 0x08013009.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_8x3_229", "Animated line 01", "Animations / Portrait", 8, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08013009 }, { 0x3030, 0x08013031 }, { 0x311c, 0x08014e75 } } },
    // Native HW 0x3134: draw call 0x08006460; 14x4 at 0x0801300c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x4_230", "Animation segment", "Animations", 14, 4, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x0801300c } } },
    // Native HW 0x3134: draw call 0x080063f8; 15x6 at 0x08013014.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x6_231", "Animated segment 01", "Animations / Portrait", 15, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08013014 } } },
    // Native HW 0x3134: draw call 0x080063f8; 15x7 at 0x08013020.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_232", "Animated segment 02", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08013020 } } },
    // Native HW 0x3134: draw call 0x080063f8; 15x7 at 0x0801302e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_233", "Animated segment 03", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801302e } } },
    // Native HW 0x3134: draw call 0x080063f8; 16x8 at 0x0801303c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_16x8_234", "Animated segment 04", "Animations / Portrait", 16, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801303c } } },
    // Native HW 0x3134: draw call 0x080063f8; 13x8 at 0x0801304c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_13x8_235", "Animated segment 05", "Animations / Portrait", 13, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801304c } } },
    // Native HW 0x3134: draw call 0x080063f8; 12x8 at 0x0801305c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x8_236", "Animated segment 06", "Animations / Portrait", 12, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801305c } } },
    // Native HW 0x3134: draw call 0x080063f8; 16x8 at 0x0801306c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_16x8_237", "Animated segment 07", "Animations / Portrait", 16, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801306c } } },
    // Native HW 0x3134: draw call 0x080063f8; 15x7 at 0x0801307c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_238", "Animated segment 08", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801307c } } },
    // Native HW 0x3134: draw call 0x080063f8; 15x7 at 0x0801308a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_239", "Animated segment 09", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801308a } } },
    // Native HW 0x3134: draw call 0x080063f8; 15x6 at 0x08013098.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x6_240", "Animated segment 10", "Animations / Portrait", 15, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08013098 } } },
    // Native HW 0x3134: draw call 0x080063f8; 15x4 at 0x080130a4.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x4_241", "Animated segment 11", "Animations / Portrait", 15, 4, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080130a4 } } },
    // Native HW 0x3134: draw call 0x080063f8; 3x6 at 0x080130ac.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_3x6_242", "Animated line 01", "Animations / Portrait", 3, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080130ac } } },
    // Native HW 0x3134: draw call 0x080063f8; 7x12 at 0x080130b2.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x12_243", "Animated line 02", "Animations / Portrait", 7, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080130b2 } } },
    // Native HW 0x3134: draw call 0x080063f8; 8x14 at 0x080130be.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_8x14_244", "Animated line 16", "Animations / Portrait", 8, 14, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080130be }, { 0x3030, 0x080130e6 }, { 0x311c, 0x08014f2a } } },
    // Native HW 0x3134: draw call 0x080063f8; 3x2 at 0x080130cc.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_3x2_245", "Animated line 17", "Animations / Portrait", 3, 2, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080130cc }, { 0x3030, 0x080130f4 }, { 0x311c, 0x08014f38 } } },
    // Native HW 0x3134: draw call 0x080063f8; 14x5 at 0x080130ce.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x5_246", "Animated line 18", "Animations / Portrait", 14, 5, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080130ce }, { 0x3030, 0x080130f6 }, { 0x311c, 0x08014f3a } } },
    // Native HW 0x3134: draw call 0x080063f8; 29x7 at 0x080130d8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_29x7_247", "Animated line 19", "Animations / Portrait", 29, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080130d8 }, { 0x3030, 0x08013100 }, { 0x311c, 0x08014f44 } } },
    // Native HW 0x3134: draw call 0x080063f8; 14x5 at 0x080130f4.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x5_248", "Animated line 20", "Animations / Portrait", 14, 5, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080130f4 }, { 0x3030, 0x0801311c }, { 0x311c, 0x08014f60 } } },
    // Native HW 0x3134: draw call 0x080063f8; 4x2 at 0x080130fe.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_4x2_249", "Animated line 21", "Animations / Portrait", 4, 2, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080130fe }, { 0x3030, 0x08013126 }, { 0x311c, 0x08014f6a } } },
    // Native HW 0x3134: draw call 0x080063f8; 7x14 at 0x08013100.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x14_250", "Animated line 22", "Animations / Portrait", 7, 14, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08013100 }, { 0x3030, 0x08013128 }, { 0x311c, 0x08014f6c } } },
    // Native HW 0x3134: draw call 0x080063f8; 7x6 at 0x0801310e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x6_251", "Animated line 23", "Animations / Portrait", 7, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801310e }, { 0x3030, 0x08013136 }, { 0x311c, 0x08014f7a } } },
    // Native HW 0x3134: draw call 0x080063f8; 10x3 at 0x08013114.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x3_252", "Animated line 24", "Animations / Portrait", 10, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08013114 }, { 0x3030, 0x0801313c }, { 0x311c, 0x08014f80 } } },
    // Native HW 0x3134: draw call 0x080063f8; 9x3 at 0x0801311a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_9x3_253", "Animated line 25", "Animations / Portrait", 9, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801311a }, { 0x3030, 0x08013142 }, { 0x311c, 0x08014f86 } } },
    // Native HW 0x3134: draw call 0x08008d14; 12x15 at 0x08013120.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x15_254", "Animated block 01", "Animations / Landscape", 12, 15, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x08013120 }, { 0x3030, 0x08013148 }, { 0x311c, 0x08014f8c } } },
    // Native HW 0x3134: draw call 0x08008d14; 14x13 at 0x0801313e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x13_255", "Animated block 02", "Animations / Landscape", 14, 13, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x0801313e }, { 0x3030, 0x08013166 }, { 0x311c, 0x08014faa } } },
    // Native HW 0x3134: draw call 0x08008d14; 14x15 at 0x08013158.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x15_256", "Animated block 03", "Animations / Landscape", 14, 15, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x08013158 }, { 0x3030, 0x08013180 }, { 0x311c, 0x08014fc4 } } },
    // Native HW 0x3134: draw call 0x08008d14; 14x12 at 0x08013176.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x12_257", "Animated block 04", "Animations / Landscape", 14, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x08013176 }, { 0x3030, 0x0801319e }, { 0x311c, 0x08014fe2 } } },
    // Native HW 0x3134: draw call 0x08008d14; 14x13 at 0x0801318e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x13_258", "Animated block 05", "Animations / Landscape", 14, 13, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x0801318e }, { 0x3030, 0x080131b6 }, { 0x311c, 0x08014ffa } } },
    // Native HW 0x3134: draw call 0x08008d14; 12x15 at 0x080131a8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x15_259", "Animated block 06", "Animations / Landscape", 12, 15, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x080131a8 }, { 0x3030, 0x080131d0 }, { 0x311c, 0x08015014 } } },
    // Native HW 0x3134: draw call 0x08008d14; 12x15 at 0x080131c6.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x15_260", "Animated block 07", "Animations / Landscape", 12, 15, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x080131c6 }, { 0x3030, 0x080131ee }, { 0x311c, 0x08015032 } } },
    // Native HW 0x3134: draw call 0x08008d14; 16x15 at 0x080131e4.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_16x15_261", "Animated block 08", "Animations / Landscape", 16, 15, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x080131e4 }, { 0x3030, 0x0801320c }, { 0x311c, 0x08015050 } } },
    // Native HW 0x3134: draw call 0x08008d14; 12x15 at 0x08013202.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x15_262", "Animated block 09", "Animations / Landscape", 12, 15, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x08013202 }, { 0x3030, 0x0801322a }, { 0x311c, 0x0801506e } } },
    // Native HW 0x3134: draw call 0x08008d14; 12x15 at 0x08013220.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x15_263", "Animated block 10", "Animations / Landscape", 12, 15, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x08013220 }, { 0x3030, 0x08013248 }, { 0x311c, 0x0801508c } } },
    // Native HW 0x3134: draw call 0x08008d14; 14x13 at 0x0801323e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x13_264", "Animated block 11", "Animations / Landscape", 14, 13, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x0801323e }, { 0x3030, 0x08013266 }, { 0x311c, 0x080150aa } } },
    // Native HW 0x3134: draw call 0x08008d14; 10x12 at 0x08013258.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x12_265", "Animated block 12", "Animations / Landscape", 10, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x08013258 }, { 0x3030, 0x08013280 }, { 0x311c, 0x080150c4 } } },
    // Native HW 0x3134: draw call 0x08008d14; 10x15 at 0x08013270.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x15_266", "Animated block 13", "Animations / Landscape", 10, 15, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x08013270 }, { 0x3030, 0x08013298 }, { 0x311c, 0x080150dc } } },
    // Native HW 0x3134: draw call 0x08008d14; 14x13 at 0x0801328e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x13_267", "Animated block 14", "Animations / Landscape", 14, 13, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x0801328e }, { 0x3030, 0x080132b6 }, { 0x311c, 0x080150fa } } },
    // Native HW 0x3134: draw call 0x08008d14; 12x15 at 0x080132a8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x15_268", "Animated block 15", "Animations / Landscape", 12, 15, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x080132a8 }, { 0x3030, 0x080132d0 }, { 0x311c, 0x08015114 } } },
    // Native HW 0x3134: draw call 0x08008d14; 12x14 at 0x080132c6.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x14_269", "Animated block 16", "Animations / Landscape", 12, 14, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x080132c6 }, { 0x3030, 0x080132ee }, { 0x311c, 0x08015132 } } },
    // Native HW 0x3134: draw call 0x08008d14; 16x14 at 0x080132e2.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_16x14_270", "Animated block 17", "Animations / Landscape", 16, 14, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x080132e2 }, { 0x3030, 0x0801330a }, { 0x311c, 0x0801514e } } },
    // Native HW 0x3134: draw call 0x08008d14; 12x14 at 0x080132fe.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x14_271", "Animated block 18", "Animations / Landscape", 12, 14, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x080132fe }, { 0x3030, 0x08013326 }, { 0x311c, 0x0801516a } } },
    // Native HW 0x3134: draw call 0x0800501c; 59x29 at 0x080135f2.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_59x29_272", "OFF", "Messages", 59, 29, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x080135f2 }, { 0x3030, 0x0801361a }, { 0x311c, 0x0801545e } } },
    // Native HW 0x3134: draw call 0x08004fa8; 128x28 at 0x080136da.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_128x28_273", "Logo DRAG 3 (boot)", "Logos", 128, 28, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3134, 0x080136da }, { 0x3030, 0x08013702 }, { 0x311c, 0x08015546 } } },
    // Native HW 0x3134: draw call 0x08004fce; 128x50 at 0x0801389a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_128x50_274", "Logo GENE FAN (boot)", "Logos", 128, 50, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x0801389a }, { 0x3030, 0x080138c2 }, { 0x311c, 0x08015706 } } },
    // Native HW 0x3134: draw call 0x0800552a; 61x10 at 0x0801403a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_61x10_275", "Charging", "Messages", 61, 10, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3134, 0x0801403a }, { 0x3030, 0x08014062 }, { 0x311c, 0x08015ea6 } } },
    // Native HW 0x3134: font selector 8; 30x64 at 0x08014698.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_30x64_276", "Font 30x64", "Font 30x64", 30, 64, 10, 0x3134, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3134, 0x08014698 }, { 0x3030, 0x080146c0 }, { 0x311c, 0x08016504 } } },
    // Native HW 0x3134: draw call 0x08005e10; 39x39 at 0x08015098.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_39x39_277", "Icon Time Out", "Messages", 39, 39, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x3134, 0x08015098 }, { 0x3030, 0x080150c0 }, { 0x311c, 0x08016f04 } } },
    // Native HW 0x3134: draw call 0x08005e28; 77x58 at 0x0801515b.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_77x58_278", "Time Out", "Messages", 77, 58, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x0801515b }, { 0x3030, 0x08015183 }, { 0x311c, 0x08016fc7 } } },
    // Native HW 0x3134: draw call 0x08005af2; 30x53 at 0x0801539f.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_30x53_279", "Icon Atomizer", "Messages", 30, 53, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x3134, 0x0801539f }, { 0x3030, 0x080153c7 }, { 0x311c, 0x0801720b } } },
    // Native HW 0x3134: draw call 0x08005b0a; 77x58 at 0x08015473.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_77x58_280", "Max Power", "Messages", 77, 58, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08015473 }, { 0x3030, 0x0801549b }, { 0x311c, 0x080172df } } },
    // Native HW 0x3134: draw call 0x08005480; 62x41 at 0x080156b7.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_62x41_281", "Icon Atomizer Short", "Messages", 62, 41, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x080156b7 }, { 0x3030, 0x080156df }, { 0x311c, 0x08017523 } } },
    // Native HW 0x3134: draw call 0x08005498; 77x58 at 0x080157ff.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_77x58_282", "Atomizer Short", "Messages", 77, 58, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x080157ff }, { 0x3030, 0x08015827 }, { 0x311c, 0x0801766b } } },
    // Native HW 0x3134: draw call 0x08005a24; 28x54 at 0x08015a43.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_28x54_283", "Icon Check Battery", "Messages", 28, 54, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08015a43 }, { 0x3030, 0x08015a6b }, { 0x311c, 0x080178af } } },
    // Native HW 0x3134: draw call 0x08005a3c; 77x58 at 0x08015b1b.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_77x58_284", "Check Battery", "Messages", 77, 58, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08015b1b }, { 0x3030, 0x08015b43 }, { 0x311c, 0x08017987 } } },
    // Native HW 0x3134: draw call 0x08005a8a; 28x53 at 0x08015d5f.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_28x53_285", "Icon Low Power", "Messages", 28, 53, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08015d5f }, { 0x3030, 0x08015d87 }, { 0x311c, 0x08017bcb } } },
    // Native HW 0x3134: draw call 0x08005aa2; 77x58 at 0x08015e33.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_77x58_286", "Low Power", "Messages", 77, 58, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08015e33 }, { 0x3030, 0x08015e5b }, { 0x311c, 0x08017c9f } } },
    // Native HW 0x3134: draw call 0x08005edc; 40x43 at 0x08016077.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_40x43_287", "Locked", "Messages", 40, 43, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08016077 }, { 0x3030, 0x0801609f }, { 0x311c, 0x08017ee3 } } },
    // Native HW 0x3134: draw call 0x08005ef4; 77x58 at 0x0801614e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_77x58_288", "Press Fire & +", "Messages", 77, 58, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x0801614e }, { 0x3030, 0x08016176 }, { 0x311c, 0x08017fba } } },
    // Native HW 0x3134: draw call 0x08005e76; 40x43 at 0x08016392.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_40x43_289", "Unlocked", "Messages", 40, 43, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_lime", { { 0x3134, 0x08016392 }, { 0x3030, 0x080163ba }, { 0x311c, 0x080181fe } } },
    // Native HW 0x3134: draw call 0x0800597a; 27x26 at 0x08016469.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_27x26_290", "Icon Check Atomizer", "Messages", 27, 26, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08016469 }, { 0x3030, 0x08016491 }, { 0x311c, 0x080182d5 } } },
    // Native HW 0x3134: draw call 0x08005992; 27x40 at 0x080164d1.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_27x40_291", "Atomizer silhouette", "Messages", 27, 40, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_cyan", { { 0x3134, 0x080164d1 }, { 0x3030, 0x080164f9 }, { 0x311c, 0x0801833d } } },
    // Native HW 0x3134: draw call 0x080059b8; 77x58 at 0x08016571.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_77x58_292", "Check Atomizer", "Messages", 77, 58, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08016571 }, { 0x3030, 0x08016599 }, { 0x311c, 0x080183dd } } },
    // Native HW 0x3134: draw call 0x08005dac; 43x47 at 0x080167b5.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_43x47_293", "Icon High Temp", "Messages", 43, 47, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x3134, 0x080167b5 }, { 0x3030, 0x080167dd }, { 0x311c, 0x08018621 } } },
    // Native HW 0x3134: draw call 0x08005dc4; 77x58 at 0x080168cf.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_77x58_294", "High Temp", "Messages", 77, 58, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x080168cf }, { 0x3030, 0x080168f7 }, { 0x311c, 0x0801873b } } },
    // Native HW 0x3134: draw call 0x08005d46; 60x31 at 0x08016b13.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_60x31_295", "Icon Puff reset", "Messages", 60, 31, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3134, 0x08016b13 }, { 0x3030, 0x08016b3b }, { 0x311c, 0x0801897f } } },
    // Native HW 0x3134: draw call 0x08005d5e; 77x58 at 0x08016c0b.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_77x58_296", "Puff Cleared", "Messages", 77, 58, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08016c0b }, { 0x3030, 0x08016c33 }, { 0x311c, 0x08018a77 } } },
    // Native HW 0x3134: font selector 7; 11x20 at 0x08016e4f.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_11x20_297", "Font 11x20", "Font 11x20", 11, 20, 10, 0x3134, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3134, 0x08016e4f }, { 0x3030, 0x08016e77 }, { 0x311c, 0x08018cbb } } },
    // Native HW 0x3134: draw call 0x080089e0; 21x23 at 0x08016fdf.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_21x23_298", "Locked", "Controls", 21, 23, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08016fdf }, { 0x3030, 0x08017007 }, { 0x311c, 0x08018e4b } } },
    // Native HW 0x3134: draw call 0x08008a5c; 21x22 at 0x080170ae.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_21x22_299", "Unit W", "Units", 21, 22, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x080170ae }, { 0x3030, 0x080170d6 }, { 0x311c, 0x08018f1a } } },
    // Native HW 0x3134: font selector 2; 6x12 at 0x080171a0.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_6x12_300", "Font 6x12", "Font 6x12", 6, 12, 11, 0x3134, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3134, 0x080171a0 }, { 0x3030, 0x080171c8 }, { 0x311c, 0x0801900c } } },
    // Native HW 0x3134: draw call 0x08006b8e; 10x16 at 0x08017224.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_10x16_301", "Unit S", "Units", 10, 16, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08017224 }, { 0x3030, 0x0801724c }, { 0x311c, 0x08019090 } } },
    // Native HW 0x3134: selector 5; complete 16-glyph decimal/hex bank used by CHIP ID.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_9x18_302", "Font 9x18", "Font 9x18", 9, 18, 16, 0x3134, AssetFormat::Mask1Bit, "0123456789ABCDEF", 0, "digits_wattage", { { 0x3134, 0x08017244 }, { 0x3030, 0x0801726c }, { 0x311c, 0x080190b0 } } },
    // Native HW 0x3134: font selector 9; 13x7 at 0x08017484.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_13x7_303", "Font 13x7 / Landscape", "Font 13x7 / Landscape", 13, 7, 11, 0x3134, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x3134, 0x08017484 }, { 0x3030, 0x080174ac }, { 0x311c, 0x080192f0 } } },
    // Native HW 0x3134: font selector 11; 40x19 at 0x0801751e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_40x19_304", "Font 40x19 / Landscape", "Font 40x19 / Landscape", 40, 19, 10, 0x3134, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x3134, 0x0801751e }, { 0x3030, 0x08017546 }, { 0x311c, 0x0801938a } } },
    // Native HW 0x3134: font selector 10; 14x8 at 0x080178d4.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "font_native_14x8_305", "Font 14x8 / Landscape", "Font 14x8 / Landscape", 14, 8, 11, 0x3134, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x3134, 0x080178d4 }, { 0x3030, 0x080178fc }, { 0x311c, 0x08019740 } } },
    // Native HW 0x3134: font selector 12; 40x10 at 0x08017984.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "symbol_native_40x10_306", "Bar with raised tip", "Special glyphs 40x10 / Landscape", 40, 10, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08017984 }, { 0x3030, 0x080179ac }, { 0x311c, 0x080197f0 } } },
    // Native HW 0x3030: draw call 0x0800966c; 2x3 at 0x08010527.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_307", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08010527 } } },
    // Native HW 0x3030: draw call 0x0800995c; 2x3 at 0x0801052d.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_308", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x0801052d } } },
    // Native HW 0x3030: draw call 0x08009b98; 2x3 at 0x08010530.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_309", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08010530 } } },
    // Native HW 0x3030: draw call 0x08009760; 2x3 at 0x08010533.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_310", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08010533 } } },
    // Native HW 0x3030: draw call 0x08009556; 15x2 at 0x0801053a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x2_311", "Decimal separator (landscape)", "Units", 15, 2, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x0801053a } } },
    // Native HW 0x3030: draw call 0x08009c02; 14x2 at 0x08010542.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x2_312", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08010542 } } },
    // Native HW 0x3030: draw call 0x080097ce; 14x2 at 0x08010546.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x2_313", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08010546 } } },
    // Native HW 0x3030: draw call 0x08009656; 15x21 at 0x08011527.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x21_314", "Mode R panel pointer (landscape)", "Controls", 15, 21, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08011527 } } },
    // Native HW 0x3030: draw call 0x08009632; 15x10 at 0x08011551.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x10_315", "Unit Ohm (landscape)", "Units", 15, 10, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08011551 } } },
    // Native HW 0x3030: draw call 0x08009b80; 15x21 at 0x08011d6a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x21_316", "Voltage panel pointer (landscape)", "Controls", 15, 21, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08011d6a } } },
    // Native HW 0x3030: draw call 0x08009c9e; 14x10 at 0x08011d94.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x10_317", "Unit V (landscape)", "Units", 14, 10, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08011d94 } } },
    // Native HW 0x3030: draw call 0x080064d8; 14x4 at 0x08013034.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x4_318", "Animation segment", "Animations", 14, 4, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08013034 } } },
    // Native HW 0x3030: draw call 0x08006470; 15x6 at 0x0801303c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x6_319", "Animated segment 01", "Animations / Portrait", 15, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x0801303c } } },
    // Native HW 0x3030: draw call 0x08006470; 15x7 at 0x08013048.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_320", "Animated segment 02", "Animations / Portrait", 15, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x08013048 } } },
    // Native HW 0x3030: draw call 0x08006470; 15x7 at 0x08013056.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_321", "Animated segment 03", "Animations / Portrait", 15, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x08013056 } } },
    // Native HW 0x3030: draw call 0x08006470; 16x8 at 0x08013064.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_16x8_322", "Animated segment 04", "Animations / Portrait", 16, 8, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x08013064 } } },
    // Native HW 0x3030: draw call 0x08006470; 13x8 at 0x08013074.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_13x8_323", "Animated segment 05", "Animations / Portrait", 13, 8, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x08013074 } } },
    // Native HW 0x3030: draw call 0x08006470; 12x8 at 0x08013084.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x8_324", "Animated segment 06", "Animations / Portrait", 12, 8, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x08013084 } } },
    // Native HW 0x3030: draw call 0x08006470; 16x8 at 0x08013094.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_16x8_325", "Animated segment 07", "Animations / Portrait", 16, 8, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x08013094 } } },
    // Native HW 0x3030: draw call 0x08006470; 15x7 at 0x080130a4.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_326", "Animated segment 08", "Animations / Portrait", 15, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x080130a4 } } },
    // Native HW 0x3030: draw call 0x08006470; 15x7 at 0x080130b2.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_327", "Animated segment 09", "Animations / Portrait", 15, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x080130b2 } } },
    // Native HW 0x3030: draw call 0x08006470; 15x6 at 0x080130c0.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x6_328", "Animated segment 10", "Animations / Portrait", 15, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x080130c0 } } },
    // Native HW 0x3030: draw call 0x08006470; 15x4 at 0x080130cc.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x4_329", "Animated segment 11", "Animations / Portrait", 15, 4, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x080130cc } } },
    // Native HW 0x3030: draw call 0x08006470; 3x6 at 0x080130d4.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_3x6_330", "Animated line 01", "Animations / Portrait", 3, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x080130d4 } } },
    // Native HW 0x3030: draw call 0x08006470; 7x12 at 0x080130da.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x12_331", "Animated line 02", "Animations / Portrait", 7, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x080130da } } },
    // Native HW 0x311c: draw call 0x0800cac8; 2x3 at 0x0801236b.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_332", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x311c, 0x0801236b } } },
    // Native HW 0x311c: draw call 0x0800cdb8; 2x3 at 0x08012371.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_333", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x311c, 0x08012371 } } },
    // Native HW 0x311c: draw call 0x0800cff4; 2x3 at 0x08012374.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_334", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x311c, 0x08012374 } } },
    // Native HW 0x311c: draw call 0x0800cbbc; 2x3 at 0x08012377.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_2x3_335", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x311c, 0x08012377 } } },
    // Native HW 0x311c: draw call 0x0800c9b2; 15x2 at 0x0801237e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x2_336", "Decimal separator (landscape)", "Units", 15, 2, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x311c, 0x0801237e } } },
    // Native HW 0x311c: draw call 0x0800d05e; 14x2 at 0x08012386.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x2_337", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x311c, 0x08012386 } } },
    // Native HW 0x311c: draw call 0x0800cc2a; 14x2 at 0x0801238a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x2_338", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x311c, 0x0801238a } } },
    // Native HW 0x311c: draw call 0x0800cab2; 15x21 at 0x0801336b.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x21_339", "Mode R panel pointer (landscape)", "Controls", 15, 21, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x311c, 0x0801336b } } },
    // Native HW 0x311c: draw call 0x0800ca8e; 15x10 at 0x08013395.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x10_340", "Unit Ohm (landscape)", "Units", 15, 10, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x311c, 0x08013395 } } },
    // Native HW 0x311c: draw call 0x0800cfdc; 15x21 at 0x08013bae.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x21_341", "Voltage panel pointer (landscape)", "Controls", 15, 21, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x311c, 0x08013bae } } },
    // Native HW 0x311c: draw call 0x0800d0fa; 14x10 at 0x08013bd8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x10_342", "Unit V (landscape)", "Units", 14, 10, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x311c, 0x08013bd8 } } },
    // Native HW 0x311c: draw call 0x08009610; 14x4 at 0x08014e78.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_14x4_343", "Animation segment", "Animations", 14, 4, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x08014e78 } } },
    // Native HW 0x311c: draw call 0x080095a8; 15x6 at 0x08014e80.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x6_344", "Animated segment 01", "Animations / Portrait", 15, 6, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x311c, 0x08014e80 } } },
    // Native HW 0x311c: draw call 0x080095a8; 15x7 at 0x08014e8c.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_345", "Animated segment 02", "Animations / Portrait", 15, 7, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x311c, 0x08014e8c } } },
    // Native HW 0x311c: draw call 0x080095a8; 15x7 at 0x08014e9a.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_346", "Animated segment 03", "Animations / Portrait", 15, 7, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x311c, 0x08014e9a } } },
    // Native HW 0x311c: draw call 0x080095a8; 16x8 at 0x08014ea8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_16x8_347", "Animated segment 04", "Animations / Portrait", 16, 8, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x311c, 0x08014ea8 } } },
    // Native HW 0x311c: draw call 0x080095a8; 13x8 at 0x08014eb8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_13x8_348", "Animated segment 05", "Animations / Portrait", 13, 8, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x311c, 0x08014eb8 } } },
    // Native HW 0x311c: draw call 0x080095a8; 12x8 at 0x08014ec8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_12x8_349", "Animated segment 06", "Animations / Portrait", 12, 8, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x311c, 0x08014ec8 } } },
    // Native HW 0x311c: draw call 0x080095a8; 16x8 at 0x08014ed8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_16x8_350", "Animated segment 07", "Animations / Portrait", 16, 8, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x311c, 0x08014ed8 } } },
    // Native HW 0x311c: draw call 0x080095a8; 15x7 at 0x08014ee8.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_351", "Animated segment 08", "Animations / Portrait", 15, 7, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x311c, 0x08014ee8 } } },
    // Native HW 0x311c: draw call 0x080095a8; 15x7 at 0x08014ef6.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x7_352", "Animated segment 09", "Animations / Portrait", 15, 7, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x311c, 0x08014ef6 } } },
    // Native HW 0x311c: draw call 0x080095a8; 15x6 at 0x08014f04.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x6_353", "Animated segment 10", "Animations / Portrait", 15, 6, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x311c, 0x08014f04 } } },
    // Native HW 0x311c: draw call 0x080095a8; 15x4 at 0x08014f10.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_15x4_354", "Animated segment 11", "Animations / Portrait", 15, 4, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x311c, 0x08014f10 } } },
    // Native HW 0x311c: draw call 0x080095a8; 3x6 at 0x08014f18.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_3x6_355", "Animated line 01", "Animations / Portrait", 3, 6, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x311c, 0x08014f18 } } },
    // Native HW 0x311c: draw call 0x080095a8; 7x12 at 0x08014f1e.
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_7x12_356", "Animated line 02", "Animations / Portrait", 7, 12, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x311c, 0x08014f1e } } },
    // Native HW 0x3151: draw call 0x0800723c; 4x4 at 0x08011a13.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_4x4_357", "Panel corner bottom right", "Controls", 4, 4, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011a13 } } },
    // Native HW 0x3151: draw call 0x08006f7c; 4x6 at 0x08011a17.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_4x6_358", "Panel corner top left", "Controls", 4, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011a17 } } },
    // Native HW 0x3151: draw call 0x08009350; 7x6 at 0x08011a1d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x6_359", "Panel corner top left (landscape)", "Controls", 7, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011a1d } } },
    // Native HW 0x3151: draw call 0x08009174; 7x6 at 0x08011a23.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x6_360", "Panel corner top right (landscape)", "Controls", 7, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011a23 } } },
    // Native HW 0x3151: draw call 0x08009702; 7x6 at 0x08011a29.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x6_361", "Panel corner bottom left (landscape)", "Controls", 7, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011a29 } } },
    // Native HW 0x3151: draw call 0x08009554; 8x6 at 0x08011a2f.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x6_362", "Panel corner bottom right (landscape)", "Controls", 8, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011a2f } } },
    // Native HW 0x3151: draw call 0x0800746e; 5x8 at 0x08011a35.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_5x8_363", "Decimal separator", "Units", 5, 8, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08011a35 } } },
    // Native HW 0x3151: draw call 0x0800965c; 10x4 at 0x08011a3d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x4_364", "Decimal separator (landscape)", "Units", 10, 4, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08011a3d } } },
    // Native HW 0x3151: draw call 0x0800abe8; 5x8 at 0x08011a58.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_5x8_365", "Decimal separator", "Units", 5, 8, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08011a58 } } },
    // Native HW 0x3151: draw call 0x0800ae66; 5x8 at 0x08011a60.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_5x8_366", "Decimal separator", "Units", 5, 8, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08011a60 } } },
    // Native HW 0x3151: draw call 0x0800b172; 5x8 at 0x08011a68.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_5x8_367", "Decimal separator", "Units", 5, 8, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08011a68 } } },
    // Native HW 0x3151: draw call 0x08009faa; 43x9 at 0x08011a88.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_43x9_368", "Header DRAG", "Labels", 43, 9, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08011a88 } } },
    // Native HW 0x3151: draw call 0x0800761a; 2x9 at 0x08011abe.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x9_369", "Battery right edge", "Battery", 2, 9, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08011abe } } },
    // Native HW 0x3151: draw call 0x08008c52; 17x25 at 0x08011ac7.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x25_370", "Locked", "Controls", 17, 25, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011ac7 } } },
    // Native HW 0x3151: draw call 0x08008ca4; 17x15 at 0x08011ba8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x15_371", "Unit W", "Units", 17, 15, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08011ba8 } } },
    // Native HW 0x3151: draw call 0x08006e32; 96x4 at 0x08011bd5.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x4_372", "Puff panel top edge", "Controls", 96, 4, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3151, 0x08011bd5 } } },
    // Native HW 0x3151: draw call 0x08006e4c; 47x10 at 0x08011c05.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_47x10_373", "Label Puff panel", "Labels", 47, 10, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011c05 } } },
    // Native HW 0x3151: draw call 0x08006f9a; 92x6 at 0x08011c41.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_92x6_374", "Puff panel bottom edge", "Controls", 92, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3151, 0x08011c41 } } },
    // Native HW 0x3151: draw call 0x0800703e; 39x2 at 0x08011c89.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_39x2_375", "Puff panel divider", "Controls", 39, 2, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011c89 } } },
    // Native HW 0x3151: draw call 0x08007024; 18x22 at 0x08011c93.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_18x22_376", "Puff panel side edge", "Controls", 18, 22, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011c93 } } },
    // Native HW 0x3151: draw call 0x0800711c; 14x20 at 0x08011cd5.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x20_377", "Unit W panel", "Units", 14, 20, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011cd5 } } },
    // Native HW 0x3151: draw call 0x08007420; 39x2 at 0x08011cfd.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_39x2_378", "Resistance panel top edge", "Controls", 39, 2, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011cfd } } },
    // Native HW 0x3151: draw call 0x0800743a; 17x8 at 0x08011d07.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x8_379", "Unit Ohm panel", "Units", 17, 8, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011d07 } } },
    // Native HW 0x3151: draw call 0x080074f4; 39x4 at 0x08011d1f.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_39x4_380", "Resistance panel bottom edge", "Controls", 39, 4, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011d1f } } },
    // Native HW 0x3151: draw call 0x08007296; 8x10 at 0x08011d33.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x10_381", "Decimal separator", "Units", 8, 10, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08011d33 } } },
    // Native HW 0x3151: draw call 0x08007226; 92x4 at 0x08011d3d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_92x4_382", "Time panel top edge", "Controls", 92, 4, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3151, 0x08011d3d } } },
    // Native HW 0x3151: draw call 0x08007328; 6x10 at 0x08011d6d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_6x10_383", "Unit S", "Units", 6, 10, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08011d6d } } },
    // Native HW 0x3151: draw call 0x08007350; 37x10 at 0x08011d77.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_37x10_384", "Label Time panel", "Labels", 37, 10, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08011d77 } } },
    // Native HW 0x3151: draw call 0x0800736e; 96x4 at 0x08011da9.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x4_385", "Time panel bottom edge", "Controls", 96, 4, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3151, 0x08011da9 } } },
    // Native HW 0x3151: draw call 0x08005e3c; 96x10 at 0x08011f84.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x10_386", "Charging", "Labels", 96, 10, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3151, 0x08011f84 } } },
    // Native HW 0x3151: draw call 0x08006196; 13x16 at 0x08011ffc.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_13x16_387", "Unit %", "Units", 13, 16, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08011ffc }, { 0x3030, 0x08017106 }, { 0x313a, 0x0801710e }, { 0x3134, 0x08017736 }, { 0x311c, 0x080193aa } } },
    // Native HW 0x3151: draw call 0x08008d2e; 17x51 at 0x0801203c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x51_388", "Logo DRAG (landscape, color)", "Logos", 17, 51, 1, 0x3151, AssetFormat::Rgb565, "", 0, "", { { 0x3151, 0x0801203c } } },
    // Native HW 0x3151: draw call 0x08008d1e; 17x51 at 0x08012702.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x51_389", "Logo SUPER (landscape)", "Logos", 17, 51, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08012702 } } },
    // Native HW 0x3151: draw call 0x08009784; 7x51 at 0x0801279b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x51_390", "Header side edge (landscape)", "Controls", 7, 51, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x0801279b } } },
    // Native HW 0x3151: draw call 0x080097ac; 5x5 at 0x080127ce.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_5x5_391", "Decimal separator (landscape)", "Units", 5, 5, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x080127ce } } },
    // Native HW 0x3151: draw call 0x080097c0; 5x5 at 0x080127d3.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_5x5_392", "Decimal separator (landscape)", "Units", 5, 5, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x080127d3 } } },
    // Native HW 0x3151: draw call 0x0800921a; 18x55 at 0x080127d8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_18x55_393", "Mode R panel (landscape)", "Controls", 18, 55, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x080127d8 } } },
    // Native HW 0x3151: draw call 0x08009332; 16x20 at 0x08012922.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x20_394", "Mode R panel pointer (landscape)", "Controls", 16, 20, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08012922 } } },
    // Native HW 0x3151: draw call 0x0800931c; 10x7 at 0x0801294a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x7_395", "Unit Ohm (landscape)", "Units", 10, 7, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x0801294a } } },
    // Native HW 0x3151: draw call 0x08008ef6; 27x22 at 0x08012958.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_27x22_396", "Mode panel (landscape)", "Controls", 27, 22, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08012958 } } },
    // Native HW 0x3151: draw call 0x08008f48; 27x22 at 0x08012e54.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_27x22_397", "Mode panel fill (landscape)", "Controls", 27, 22, 1, 0x3151, AssetFormat::Rgb565, "", 0, "", { { 0x3151, 0x08012e54 } } },
    // Native HW 0x3151: draw call 0x08008f2c; 27x22 at 0x080132f8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_27x22_398", "Mode P panel (landscape)", "Controls", 27, 22, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x080132f8 } } },
    // Native HW 0x3151: draw call 0x08009046; 18x35 at 0x08013350.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_18x35_399", "Puff panel (landscape)", "Controls", 18, 35, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08013350 } } },
    // Native HW 0x3151: draw call 0x08009156; 35x20 at 0x080133b9.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_35x20_400", "Puff panel pointer (landscape)", "Controls", 35, 20, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x080133b9 } } },
    // Native HW 0x3151: draw call 0x08008fd4; 10x10 at 0x0801341d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x10_401", "Unit S (landscape)", "Units", 10, 10, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x0801341d } } },
    // Native HW 0x3151: draw call 0x080095f6; 18x54 at 0x08013431.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_18x54_402", "Voltage panel (landscape)", "Controls", 18, 54, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08013431 } } },
    // Native HW 0x3151: draw call 0x08009610; 16x20 at 0x08013575.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x20_403", "Voltage panel pointer (landscape)", "Controls", 16, 20, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08013575 } } },
    // Native HW 0x3151: draw call 0x080096e4; 10x9 at 0x0801359d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x9_404", "Unit V (landscape)", "Units", 10, 9, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x0801359d } } },
    // Native HW 0x3151: draw call 0x08009446; 18x54 at 0x080135af.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_18x54_405", "Time panel (landscape)", "Controls", 18, 54, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x080135af } } },
    // Native HW 0x3151: draw call 0x08009460; 16x20 at 0x08013651.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x20_406", "Time panel pointer (landscape)", "Controls", 16, 20, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08013651 } } },
    // Native HW 0x3151: draw call 0x08009536; 10x9 at 0x08013679.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x9_407", "Unit W (landscape)", "Units", 10, 9, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08013679 } } },
    // Native HW 0x3151: draw call 0x080093da; 28x49 at 0x0801368b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_28x49_408", "Mode S panel (landscape)", "Controls", 28, 49, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x0801368b } } },
    // Native HW 0x3151: draw call 0x0800a51c; 31x29 at 0x080138a4.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_31x29_409", "Mode NI", "Controls", 31, 29, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x080138a4 } } },
    // Native HW 0x3151: draw call 0x0800a592; 31x29 at 0x08013918.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_31x29_410", "Mode TI", "Controls", 31, 29, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08013918 } } },
    // Native HW 0x3151: draw call 0x0800a608; 31x29 at 0x0801398c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_31x29_411", "Mode SS", "Controls", 31, 29, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x0801398c } } },
    // Native HW 0x3151: draw call 0x0800a6ae; 8x29 at 0x08013a00.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x29_412", "Mode panel top left edge", "Controls", 8, 29, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08013a00 } } },
    // Native HW 0x3151: draw call 0x0800a6c2; 8x29 at 0x08013a1d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x29_413", "Mode panel bottom right edge", "Controls", 8, 29, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08013a1d } } },
    // Native HW 0x3151: draw call 0x0800a72c; 17x41 at 0x08013a3a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x41_414", "Mode panel left edge", "Controls", 17, 41, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08013a3a } } },
    // Native HW 0x3151: draw call 0x0800a754; 17x41 at 0x08013ab5.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x41_415", "Mode panel right edge", "Controls", 17, 41, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08013ab5 } } },
    // Native HW 0x3151: draw call 0x0800a90e; 39x23 at 0x08013cc6.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_39x23_416", "Mode SS (landscape)", "Controls", 39, 23, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08013cc6 } } },
    // Native HW 0x3151: draw call 0x0800a92a; 39x23 at 0x08013d91.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_39x23_417", "Mode M (landscape)", "Controls", 39, 23, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08013d91 } } },
    // Native HW 0x3151: draw call 0x0800a790; 39x23 at 0x08013e04.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_39x23_418", "Unit Ohm panel (landscape)", "Units", 39, 23, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08013e04 } } },
    // Native HW 0x3151: draw call 0x0800aa9e; 2x13 at 0x08013e77.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x13_419", "Decimal separator", "Units", 2, 13, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08013e77 } } },
    // Native HW 0x3151: draw call 0x0800b0b2; 24x22 at 0x08013e84.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_24x22_420", "Locked", "Controls", 24, 22, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08013e84 } } },
    // Native HW 0x3151: draw call 0x0800b094; 24x18 at 0x08013f3e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_24x18_421", "Unit degrees C", "Units", 24, 18, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08013f3e } } },
    // Native HW 0x3151: draw call 0x0800a042; 39x28 at 0x08013fb6.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_39x28_422", "Label Watt / Time panel", "Labels", 39, 28, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08013fb6 } } },
    // Native HW 0x3151: draw call 0x0800a07a; 17x28 at 0x08014042.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x28_423", "Mode NI panel", "Controls", 17, 28, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08014042 } } },
    // Native HW 0x3151: draw call 0x0800a092; 40x28 at 0x0801425e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_40x28_424", "Label Ohms / Puff panel", "Labels", 40, 28, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x0801425e } } },
    // Native HW 0x3151: draw call 0x0800ab4c; 10x11 at 0x080142ea.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x11_425", "Panel corner top left", "Controls", 10, 11, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x080142ea } } },
    // Native HW 0x3151: draw call 0x0800add8; 19x11 at 0x08014300.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_19x11_426", "Panel top divider", "Controls", 19, 11, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08014300 } } },
    // Native HW 0x3151: draw call 0x0800adf0; 11x11 at 0x08014321.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_11x11_427", "Panel corner top right", "Controls", 11, 11, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08014321 } } },
    // Native HW 0x3151: draw call 0x0800b0f4; 9x11 at 0x08014337.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_9x11_428", "Panel corner bottom left", "Controls", 9, 11, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08014337 } } },
    // Native HW 0x3151: draw call 0x0800b10a; 24x11 at 0x0801434d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_24x11_429", "Panel bottom divider", "Controls", 24, 11, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x0801434d } } },
    // Native HW 0x3151: draw call 0x0800ac72; 8x11 at 0x0801436e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x11_430", "Panel corner bottom right", "Controls", 8, 11, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x0801436e } } },
    // Native HW 0x3151: draw call 0x0800a110; 96x6 at 0x08014379.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x6_431", "Panel top edge", "Controls", 96, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08014379 } } },
    // Native HW 0x3151: draw call 0x0800a126; 96x6 at 0x080143c1.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x6_432", "Panel bottom edge", "Controls", 96, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x080143c1 } } },
    // Native HW 0x3151: draw call 0x08006492; 96x12 at 0x08014434.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x12_433", "Label Theme", "Labels", 96, 12, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08014434 } } },
    // Native HW 0x3151: draw call 0x08006556; 15x42 at 0x080144c4.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x42_434", "Theme panel left edge", "Controls", 15, 42, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "", { { 0x3151, 0x080144c4 } } },
    // Native HW 0x3151: draw call 0x08006578; 15x42 at 0x08014518.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x42_435", "Theme panel right edge", "Controls", 15, 42, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "", { { 0x3151, 0x08014518 } } },
    // Native HW 0x3151: draw call 0x0800650a; 66x42 at 0x0801456c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_66x42_436", "Theme IRON panel", "Controls", 66, 42, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x0801456c } } },
    // Native HW 0x3151: draw call 0x08006520; 30x8 at 0x080146e6.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_30x8_437", "Label IRON", "Labels", 30, 8, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x080146e6 }, { 0x3030, 0x080153e8 }, { 0x313a, 0x080153f0 }, { 0x3134, 0x08015a18 }, { 0x311c, 0x0801768c } } },
    // Native HW 0x3151: draw call 0x08006590; 66x42 at 0x08014706.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_66x42_438", "Theme CORE panel", "Controls", 66, 42, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3151, 0x08014706 } } },
    // Native HW 0x3151: draw call 0x080065a6; 34x8 at 0x08014880.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_34x8_439", "Label CORE", "Labels", 34, 8, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08014880 }, { 0x3030, 0x08015600 }, { 0x313a, 0x08015608 }, { 0x3134, 0x08015c30 }, { 0x311c, 0x080178a4 } } },
    // Native HW 0x3151: draw call 0x0800c988; 96x11 at 0x080148a8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x11_440", "Logo VOOPOO (chip information)", "Logos", 96, 11, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3151, 0x080148a8 } } },
    // Native HW 0x3151: draw call 0x0800c9ae; 96x63 at 0x0801492c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x63_441", "Chip ID panel", "Labels", 96, 63, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3151, 0x0801492c } } },
    // Native HW 0x3151: draw call 0x0800ca76; 96x46 at 0x08014c20.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x46_442", "Level Code panel", "Labels", 96, 46, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3151, 0x08014c20 } } },
    // Native HW 0x3151: draw call 0x08006d50; 5x3 at 0x08014e48.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_5x3_443", "Animation dot", "Animations", 5, 3, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014e48 } } },
    // Native HW 0x3151: draw call 0x08006d50; 9x6 at 0x08014e4b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_9x6_444", "Animated segment 01", "Animations / Portrait", 9, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014e4b } } },
    // Native HW 0x3151: draw call 0x08006d50; 13x7 at 0x08014e57.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_13x7_445", "Animated segment 02", "Animations / Portrait", 13, 7, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014e57 } } },
    // Native HW 0x3151: draw call 0x08006d50; 15x9 at 0x08014e65.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x9_446", "Animated segment 03", "Animations / Portrait", 15, 9, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014e65 } } },
    // Native HW 0x3151: draw call 0x08006d50; 17x7 at 0x08014e77.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x7_447", "Animated segment 04", "Animations / Portrait", 17, 7, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014e77 } } },
    // Native HW 0x3151: draw call 0x08006d50; 13x7 at 0x08014e8c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_13x7_448", "Animated segment 05", "Animations / Portrait", 13, 7, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014e8c } } },
    // Native HW 0x3151: draw call 0x08006d50; 15x9 at 0x08014e9a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x9_449", "Animated segment 06", "Animations / Portrait", 15, 9, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014e9a } } },
    // Native HW 0x3151: draw call 0x08006d50; 14x6 at 0x08014eac.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x6_450", "Animated segment 07", "Animations / Portrait", 14, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014eac } } },
    // Native HW 0x3151: draw call 0x08006d50; 5x3 at 0x08014eb8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_5x3_451", "Animated segment 08", "Animations / Portrait", 5, 3, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014eb8 } } },
    // Native HW 0x3151: draw call 0x08006d50; 10x5 at 0x08014ebb.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x5_452", "Animated segment 09", "Animations / Portrait", 10, 5, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014ebb } } },
    // Native HW 0x3151: draw call 0x08006d50; 6x3 at 0x08014ec5.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_6x3_453", "Animated segment 10", "Animations / Portrait", 6, 3, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014ec5 } } },
    // Native HW 0x3151: draw call 0x08006d50; 4x2 at 0x08014ec8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_4x2_454", "Animated segment 11", "Animations / Portrait", 4, 2, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014ec8 } } },
    // Native HW 0x3151: draw call 0x08006d50; 5x6 at 0x08014eca.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_5x6_455", "Animated segment 12", "Animations / Portrait", 5, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014eca } } },
    // Native HW 0x3151: draw call 0x08006d50; 4x9 at 0x08014ed0.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_4x9_456", "Animated segment 13", "Animations / Portrait", 4, 9, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014ed0 } } },
    // Native HW 0x3151: draw call 0x08006d50; 10x14 at 0x08014ed9.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x14_457", "Animated segment 14", "Animations / Portrait", 10, 14, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014ed9 } } },
    // Native HW 0x3151: draw call 0x08006d50; 14x4 at 0x08014ef5.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x4_458", "Animated segment 15", "Animations / Portrait", 14, 4, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014ef5 } } },
    // Native HW 0x3151: draw call 0x08006d50; 30x6 at 0x08014efd.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_30x6_459", "Animated segment 16", "Animations / Portrait", 30, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014efd } } },
    // Native HW 0x3151: draw call 0x08006d50; 13x4 at 0x08014f15.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_13x4_460", "Animated segment 17", "Animations / Portrait", 13, 4, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014f15 } } },
    // Native HW 0x3151: draw call 0x08006d50; 5x15 at 0x08014f1d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_5x15_461", "Animated segment 18", "Animations / Portrait", 5, 15, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014f1d } } },
    // Native HW 0x3151: draw call 0x08006d50; 5x12 at 0x08014f2c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_5x12_462", "Animated segment 19", "Animations / Portrait", 5, 12, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014f2c } } },
    // Native HW 0x3151: draw call 0x08006d50; 4x10 at 0x08014f38.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_4x10_463", "Animated segment 20", "Animations / Portrait", 4, 10, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014f38 } } },
    // Native HW 0x3151: draw call 0x08006d50; 2x7 at 0x08014f42.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x7_464", "Animated segment 21", "Animations / Portrait", 2, 7, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014f42 } } },
    // Native HW 0x3151: draw call 0x08006d50; 4x5 at 0x08014f49.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_4x5_465", "Animated segment 22", "Animations / Portrait", 4, 5, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014f49 } } },
    // Native HW 0x3151: draw call 0x08008e52; 10x6 at 0x08014f4e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x6_466", "Animated block 01", "Animations / Landscape", 10, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014f4e } } },
    // Native HW 0x3151: draw call 0x08008e52; 14x6 at 0x08014f5a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x6_467", "Animated block 02", "Animations / Landscape", 14, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014f5a } } },
    // Native HW 0x3151: draw call 0x08008e52; 14x15 at 0x08014f66.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x15_468", "Animated block 03", "Animations / Landscape", 14, 15, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014f66 } } },
    // Native HW 0x3151: draw call 0x08008e52; 14x12 at 0x08014f84.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x12_469", "Animated block 04", "Animations / Landscape", 14, 12, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014f84 } } },
    // Native HW 0x3151: draw call 0x08008e52; 14x6 at 0x08014f9c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x6_470", "Animated block 05", "Animations / Landscape", 14, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014f9c } } },
    // Native HW 0x3151: draw call 0x08008e52; 8x6 at 0x08014fa8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x6_471", "Animated block 06", "Animations / Landscape", 8, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014fa8 } } },
    // Native HW 0x3151: draw call 0x08008e52; 13x11 at 0x08014fae.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_13x11_472", "Animated block 07", "Animations / Landscape", 13, 11, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014fae } } },
    // Native HW 0x3151: draw call 0x08008e52; 15x11 at 0x08014fc4.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x11_473", "Animated block 08", "Animations / Landscape", 15, 11, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014fc4 } } },
    // Native HW 0x3151: draw call 0x08008e52; 10x6 at 0x08014fda.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x6_474", "Animated block 09", "Animations / Landscape", 10, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014fda } } },
    // Native HW 0x3151: draw call 0x08008e52; 17x6 at 0x08014fe6.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x6_475", "Animated block 10", "Animations / Landscape", 17, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014fe6 } } },
    // Native HW 0x3151: draw call 0x08008e52; 17x8 at 0x08014ff8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x8_476", "Animated block 11", "Animations / Landscape", 17, 8, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08014ff8 } } },
    // Native HW 0x3151: draw call 0x08008e52; 7x7 at 0x08015010.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x7_477", "Animated block 12", "Animations / Landscape", 7, 7, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08015010 } } },
    // Native HW 0x3151: draw call 0x08008e52; 7x3 at 0x08015017.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x3_478", "Animated block 13", "Animations / Landscape", 7, 3, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08015017 } } },
    // Native HW 0x3151: draw call 0x08008e52; 17x9 at 0x0801501a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x9_479", "Animated block 14", "Animations / Landscape", 17, 9, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x0801501a } } },
    // Native HW 0x3151: draw call 0x08008e52; 17x6 at 0x08015035.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x6_480", "Animated block 15", "Animations / Landscape", 17, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08015035 } } },
    // Native HW 0x3151: draw call 0x08008e52; 10x6 at 0x08015047.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x6_481", "Animated block 16", "Animations / Landscape", 10, 6, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08015047 } } },
    // Native HW 0x3151: draw call 0x08008e52; 15x11 at 0x08015053.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x11_482", "Animated block 17", "Animations / Landscape", 15, 11, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08015053 } } },
    // Native HW 0x3151: draw call 0x08008e52; 12x11 at 0x08015069.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x11_483", "Animated block 18", "Animations / Landscape", 12, 11, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_gray", { { 0x3151, 0x08015069 } } },
    // Native HW 0x3151: draw call 0x080053ec; 59x29 at 0x0801525f.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_59x29_484", "OFF", "Messages", 59, 29, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x0801525f } } },
    // Native HW 0x3151: draw call 0x0800536c; 96x20 at 0x08015347.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x20_485", "Logo DRAG 3 (boot)", "Logos", 96, 20, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3151, 0x08015347 } } },
    // Native HW 0x3151: draw call 0x08005392; 96x53 at 0x08015437.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x53_486", "Logo GENE FAN (boot)", "Logos", 96, 53, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08015437 } } },
    // Native HW 0x3151: draw call 0x08006060; 63x24 at 0x08015a73.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_63x24_487", "Battery gauge frame", "Battery", 63, 24, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08015a73 }, { 0x3030, 0x08016ff6 }, { 0x313a, 0x08016ffe }, { 0x3134, 0x08017626 }, { 0x311c, 0x0801929a } } },
    // Native HW 0x3151: font selector 1; 6x10 at 0x08015b33.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_6x10_488", "Font 6x10", "Font 6x10", 6, 10, 11, 0x3151, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3151, 0x08015b33 }, { 0x3030, 0x08017126 }, { 0x313a, 0x0801712e }, { 0x3134, 0x08017756 }, { 0x311c, 0x080193ca } } },
    // Native HW 0x3151: font selector 2; 5x8 at 0x08015ba1.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_5x8_489", "Font 5x8", "Font 5x8", 5, 8, 11, 0x3151, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3151, 0x08015ba1 } } },
    // Native HW 0x3151: font selector 4; 7x13 at 0x08015bf9.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_7x13_490", "Font 7x13", "Font 7x13", 7, 13, 16, 0x3151, AssetFormat::Mask1Bit, "0123456789ABCDEF", 0, "digits_wattage", { { 0x3151, 0x08015bf9 }, { 0x3030, 0x08017194 }, { 0x313a, 0x0801719c }, { 0x3134, 0x080177c4 }, { 0x311c, 0x08019438 } } },
    // Native HW 0x3151: font selector 8; 19x40 at 0x08015cc9.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_19x40_491", "Font 19x40", "Font 19x40", 19, 40, 10, 0x3151, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3151, 0x08015cc9 }, { 0x3030, 0x08017264 }, { 0x313a, 0x0801726c }, { 0x3134, 0x08017894 }, { 0x311c, 0x08019508 } } },
    // Native HW 0x3151: draw call 0x08006720; 96x40 at 0x0801617a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x40_492", "Icon Time Out", "Messages", 96, 40, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x3151, 0x0801617a } } },
    // Native HW 0x3151: draw call 0x08006746; 96x58 at 0x0801635a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x58_493", "Time Out", "Messages", 96, 58, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x0801635a } } },
    // Native HW 0x3151: draw call 0x08006412; 96x54 at 0x08016612.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x54_494", "Icon Atomizer", "Messages", 96, 54, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x3151, 0x08016612 } } },
    // Native HW 0x3151: draw call 0x08006438; 96x58 at 0x0801689a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x58_495", "Max Power", "Messages", 96, 58, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x0801689a } } },
    // Native HW 0x3151: draw call 0x08005d90; 96x41 at 0x08016b52.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x41_496", "Icon Atomizer Short", "Messages", 96, 41, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08016b52 } } },
    // Native HW 0x3151: draw call 0x08005dba; 96x58 at 0x08016d3e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x58_497", "Atomizer Short", "Messages", 96, 58, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08016d3e } } },
    // Native HW 0x3151: draw call 0x08006308; 96x54 at 0x08016ff6.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x54_498", "Icon Check Battery", "Messages", 96, 54, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08016ff6 } } },
    // Native HW 0x3151: draw call 0x08006332; 96x58 at 0x0801727e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x58_499", "Check Battery", "Messages", 96, 58, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x0801727e } } },
    // Native HW 0x3151: draw call 0x0800638e; 96x53 at 0x08017536.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x53_500", "Icon Low Power", "Messages", 96, 53, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08017536 } } },
    // Native HW 0x3151: draw call 0x080063b4; 96x58 at 0x080177b2.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x58_501", "Low Power", "Messages", 96, 58, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x080177b2 } } },
    // Native HW 0x3151: draw call 0x08006824; 96x43 at 0x08017a6a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x43_502", "Locked", "Messages", 96, 43, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x08017a6a } } },
    // Native HW 0x3151: draw call 0x0800684a; 96x58 at 0x08017c6e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x58_503", "Press Fire & +", "Messages", 96, 58, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08017c6e } } },
    // Native HW 0x3151: draw call 0x080067a2; 96x43 at 0x08017f26.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x43_504", "Unlocked", "Messages", 96, 43, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_lime", { { 0x3151, 0x08017f26 } } },
    // Native HW 0x3151: draw call 0x08006252; 96x26 at 0x0801812a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x26_505", "Icon Check Atomizer", "Messages", 96, 26, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3151, 0x0801812a } } },
    // Native HW 0x3151: draw call 0x0800627c; 96x40 at 0x08018262.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x40_506", "Atomizer silhouette", "Messages", 96, 40, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "accent_cyan", { { 0x3151, 0x08018262 } } },
    // Native HW 0x3151: draw call 0x080062a6; 96x58 at 0x08018442.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x58_507", "Check Atomizer", "Messages", 96, 58, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08018442 } } },
    // Native HW 0x3151: draw call 0x080066a0; 96x47 at 0x080186fa.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x47_508", "Icon High Temp", "Messages", 96, 47, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x3151, 0x080186fa } } },
    // Native HW 0x3151: draw call 0x080066c6; 96x58 at 0x0801892e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x58_509", "High Temp", "Messages", 96, 58, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x0801892e } } },
    // Native HW 0x3151: draw call 0x0800661e; 96x31 at 0x08018be6.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x31_510", "Icon Puff reset", "Messages", 96, 31, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3151, 0x08018be6 } } },
    // Native HW 0x3151: draw call 0x08006644; 96x58 at 0x08018d5a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_96x58_511", "Puff Cleared", "Messages", 96, 58, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08018d5a } } },
    // Native HW 0x3151: font selector 5; 8x16 at 0x08019013.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_8x16_512", "Font 8x16", "Font 8x16", 8, 16, 11, 0x3151, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3151, 0x08019013 }, { 0x3030, 0x0801a16c }, { 0x313a, 0x0801a174 }, { 0x3134, 0x0801a79c }, { 0x311c, 0x0801c410 } } },
    // Native HW 0x3151: font selector 15; 10x6 at 0x080190c8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_10x6_513", "Font 10x6 / Landscape", "Font 10x6 / Landscape", 10, 6, 11, 0x3151, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x3151, 0x080190c8 } } },
    // Native HW 0x3151: font selector 19; 30x14 at 0x0801914c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_30x14_514", "Font 30x14 / Landscape", "Font 30x14 / Landscape", 30, 14, 10, 0x3151, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x3151, 0x0801914c } } },
    // Native HW 0x3151: font selector 16; 23x13 at 0x0801937c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_23x13_515", "Font 23x13 / Landscape", "Font 23x13 / Landscape", 23, 13, 10, 0x3151, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x3151, 0x0801937c } } },
    // Native HW 0x3151: font selector 6; 8x13 at 0x08019502.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_8x13_516", "Font 8x13", "Font 8x13", 8, 13, 11, 0x3151, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3151, 0x08019502 } } },
    // Native HW 0x3151: font selector 17; 23x10 at 0x08019591.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "symbol_native_23x10_517", "Bar with raised tip", "Special glyphs 23x10 / Landscape", 23, 10, 1, 0x3151, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3151, 0x08019591 } } },
    // Native HW 0x3030: draw call 0x080096ae; 2x3 at 0x08012708.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_518", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012708 }, { 0x313a, 0x08012710 }, { 0x3134, 0x08012d38 }, { 0x311c, 0x080149ac } } },
    // Native HW 0x3030: draw call 0x0800995a; 2x3 at 0x0801270b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_519", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x0801270b } } },
    // Native HW 0x3030: draw call 0x0800945e; 2x3 at 0x0801270e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_520", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x0801270e }, { 0x313a, 0x08012716 }, { 0x3134, 0x08012d3e }, { 0x311c, 0x080149b2 } } },
    // Native HW 0x3030: draw call 0x08009ce4; 2x3 at 0x08012711.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_521", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012711 } } },
    // Native HW 0x3030: draw call 0x08009f26; 2x3 at 0x08012714.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_522", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012714 } } },
    // Native HW 0x3030: draw call 0x08009ae6; 2x3 at 0x08012717.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_523", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012717 } } },
    // Native HW 0x3030: draw call 0x08009590; 15x2 at 0x0801271d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x2_524", "Decimal separator (landscape)", "Units", 15, 2, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x0801271d }, { 0x313a, 0x08012725 }, { 0x3134, 0x08012d4d }, { 0x311c, 0x080149c1 } } },
    // Native HW 0x3030: draw call 0x08009832; 15x2 at 0x08012721.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x2_525", "Decimal separator (landscape)", "Units", 15, 2, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08012721 } } },
    // Native HW 0x3030: draw call 0x08009d4e; 14x2 at 0x08012725.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x2_526", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08012725 }, { 0x313a, 0x0801272d }, { 0x3134, 0x08012d55 }, { 0x311c, 0x080149c9 } } },
    // Native HW 0x3030: draw call 0x08009f90; 14x2 at 0x08012729.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x2_527", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08012729 } } },
    // Native HW 0x3030: draw call 0x08009b4e; 14x2 at 0x0801272d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x2_528", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x0801272d } } },
    // Native HW 0x3030: draw call 0x08007010; 5x6 at 0x08012737.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_5x6_529", "Panel corner top left", "Controls", 5, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012737 }, { 0x313a, 0x0801273f }, { 0x3134, 0x08012d67 }, { 0x311c, 0x080149db } } },
    // Native HW 0x3030: draw call 0x0800730a; 5x6 at 0x0801273d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_5x6_530", "Panel corner bottom right", "Controls", 5, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x0801273d }, { 0x313a, 0x08012745 }, { 0x3134, 0x08012d6d }, { 0x311c, 0x080149e1 } } },
    // Native HW 0x3030: draw call 0x0800754e; 2x12 at 0x08012774.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x12_531", "Decimal separator", "Units", 2, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08012774 }, { 0x313a, 0x0801277c }, { 0x3134, 0x08012da4 }, { 0x311c, 0x08014a18 } } },
    // Native HW 0x3030: draw call 0x08007388; 2x16 at 0x08012780.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x16_532", "Decimal separator", "Units", 2, 16, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08012780 }, { 0x313a, 0x08012788 }, { 0x3134, 0x08012db0 }, { 0x311c, 0x08014a24 } } },
    // Native HW 0x3030: draw call 0x0800a9f0; 32x10 at 0x080127f3.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_32x10_533", "Header DRAG", "Labels", 32, 10, 1, 0x3030, AssetFormat::Rgb565, "", 0, "", { { 0x3030, 0x080127f3 }, { 0x313a, 0x080127fb }, { 0x3134, 0x08012e23 }, { 0x311c, 0x08014a97 } } },
    // Native HW 0x3030: draw call 0x08007784; 2x10 at 0x08012a73.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x10_534", "Battery right edge", "Battery", 2, 10, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08012a73 }, { 0x313a, 0x08012a7b }, { 0x3134, 0x080130a3 }, { 0x311c, 0x08014d17 } } },
    // Native HW 0x3030: draw call 0x08006ea2; 128x6 at 0x08012a7d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_128x6_535", "Puff panel top edge", "Controls", 128, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x08012a7d }, { 0x313a, 0x08012a85 }, { 0x3134, 0x080130ad }, { 0x311c, 0x08014d21 } } },
    // Native HW 0x3030: draw call 0x08006ebc; 50x16 at 0x08012add.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_50x16_536", "Label Puff panel", "Labels", 50, 16, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012add }, { 0x313a, 0x08012ae5 }, { 0x3134, 0x0801310d }, { 0x311c, 0x08014d81 } } },
    // Native HW 0x3030: draw call 0x0800702e; 123x6 at 0x08012b4d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_123x6_537", "Puff panel bottom edge", "Controls", 123, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x08012b4d }, { 0x313a, 0x08012b55 }, { 0x3134, 0x0801317d }, { 0x311c, 0x08014df1 } } },
    // Native HW 0x3030: draw call 0x080070b8; 63x7 at 0x08012bad.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_63x7_538", "Puff panel divider", "Controls", 63, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012bad }, { 0x313a, 0x08012bb5 }, { 0x3134, 0x080131dd }, { 0x311c, 0x08014e51 } } },
    // Native HW 0x3030: draw call 0x080070d2; 8x12 at 0x08012be5.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x12_539", "Unit V panel", "Units", 8, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012be5 }, { 0x313a, 0x08012bed }, { 0x3134, 0x08013215 }, { 0x311c, 0x08014e89 } } },
    // Native HW 0x3030: draw call 0x080071d6; 7x12 at 0x08012bf1.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x12_540", "Unit Ohm", "Units", 7, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08012bf1 }, { 0x313a, 0x08012bf9 }, { 0x3134, 0x08013221 }, { 0x311c, 0x08014e95 } } },
    // Native HW 0x3030: draw call 0x080071ec; 19x12 at 0x08012bfd.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_19x12_541", "Unit W panel", "Units", 19, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012bfd }, { 0x313a, 0x08012c05 }, { 0x3134, 0x0801322d }, { 0x311c, 0x08014ea1 } } },
    // Native HW 0x3030: draw call 0x08007206; 63x3 at 0x08012c21.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_63x3_542", "Resistance panel top edge", "Controls", 63, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012c21 }, { 0x313a, 0x08012c29 }, { 0x3134, 0x08013251 }, { 0x311c, 0x08014ec5 } } },
    // Native HW 0x3030: draw call 0x08007236; 63x5 at 0x08012c39.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_63x5_543", "Time panel top edge", "Controls", 63, 5, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012c39 }, { 0x313a, 0x08012c41 }, { 0x3134, 0x08013269 }, { 0x311c, 0x08014edd } } },
    // Native HW 0x3030: draw call 0x080074f0; 65x16 at 0x08012c61.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_65x16_544", "Voltage panel top edge", "Controls", 65, 16, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012c61 }, { 0x313a, 0x08012c69 }, { 0x3134, 0x08013291 }, { 0x311c, 0x08014f05 } } },
    // Native HW 0x3030: draw call 0x0800750a; 20x12 at 0x08012cf1.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_20x12_545", "Unit Ohm panel", "Units", 20, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012cf1 }, { 0x313a, 0x08012cf9 }, { 0x3134, 0x08013321 }, { 0x311c, 0x08014f95 } } },
    // Native HW 0x3030: draw call 0x080075ee; 7x12 at 0x08012d15.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x12_546", "Unit V", "Units", 7, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08012d15 }, { 0x313a, 0x08012d1d }, { 0x3134, 0x08013345 }, { 0x311c, 0x08014fb9 } } },
    // Native HW 0x3030: draw call 0x08007614; 8x12 at 0x08012d21.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x12_547", "Decimal separator", "Units", 8, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012d21 }, { 0x313a, 0x08012d29 }, { 0x3134, 0x08013351 }, { 0x311c, 0x08014fc5 } } },
    // Native HW 0x3030: draw call 0x08007632; 65x7 at 0x08012d2d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_65x7_548", "Resistance panel bottom edge", "Controls", 65, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08012d2d }, { 0x313a, 0x08012d35 }, { 0x3134, 0x0801335d }, { 0x311c, 0x08014fd1 } } },
    // Native HW 0x3030: draw call 0x080072f4; 123x6 at 0x08012d6c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_123x6_549", "Voltage panel bottom edge", "Controls", 123, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x08012d6c }, { 0x313a, 0x08012d74 }, { 0x3134, 0x0801339c }, { 0x311c, 0x08015010 } } },
    // Native HW 0x3030: draw call 0x08007470; 128x6 at 0x08012dcc.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_128x6_550", "Time panel bottom edge", "Controls", 128, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3030, 0x08012dcc }, { 0x313a, 0x08012dd4 }, { 0x3134, 0x080133fc }, { 0x311c, 0x08015070 } } },
    // Native HW 0x3030: draw call 0x08008eee; 12x32 at 0x08013084.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x32_551", "Logo DRAG (landscape)", "Logos", 12, 32, 1, 0x3030, AssetFormat::Rgb565, "", 0, "", { { 0x3030, 0x08013084 }, { 0x313a, 0x0801308c }, { 0x3134, 0x080136b4 }, { 0x311c, 0x08015328 } } },
    // Native HW 0x3030: draw call 0x08008eba; 21x79 at 0x08013384.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_21x79_552", "Logo SUPER (landscape)", "Logos", 21, 79, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013384 }, { 0x313a, 0x0801338c }, { 0x3134, 0x080139b4 }, { 0x311c, 0x08015628 } } },
    // Native HW 0x3030: draw call 0x0800a0c4; 10x79 at 0x08013471.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x79_553", "Header side edge (landscape)", "Controls", 10, 79, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013471 }, { 0x313a, 0x08013479 }, { 0x3134, 0x08013aa1 }, { 0x311c, 0x08015715 } } },
    // Native HW 0x3030: draw call 0x0800a0ec; 7x13 at 0x0801350f.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x13_554", "Unit Ohm (landscape)", "Units", 7, 13, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x0801350f }, { 0x313a, 0x08013517 }, { 0x3134, 0x08013b3f }, { 0x311c, 0x080157b3 } } },
    // Native HW 0x3030: draw call 0x0800a110; 7x13 at 0x0801351c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x13_555", "Unit S (landscape)", "Units", 7, 13, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x0801351c }, { 0x313a, 0x08013524 }, { 0x3134, 0x08013b4c }, { 0x311c, 0x080157c0 } } },
    // Native HW 0x3030: draw call 0x08009518; 23x72 at 0x08013529.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_23x72_556", "Mode R panel (landscape)", "Controls", 23, 72, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013529 }, { 0x313a, 0x08013531 }, { 0x3134, 0x08013b59 }, { 0x311c, 0x080157cd } } },
    // Native HW 0x3030: draw call 0x0800968e; 15x21 at 0x08013601.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x21_557", "Mode R panel pointer (landscape)", "Controls", 15, 21, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013601 }, { 0x313a, 0x08013609 }, { 0x3134, 0x08013c31 }, { 0x311c, 0x080158a5 } } },
    // Native HW 0x3030: draw call 0x08009666; 15x10 at 0x0801362b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x10_558", "Unit Ohm (landscape)", "Units", 15, 10, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x0801362b }, { 0x313a, 0x08013633 }, { 0x3134, 0x08013c5b }, { 0x311c, 0x080158cf } } },
    // Native HW 0x3030: draw call 0x0800979e; 6x12 at 0x0801363f.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_6x12_559", "Decimal separator (landscape)", "Units", 6, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x0801363f }, { 0x313a, 0x08013647 }, { 0x311c, 0x080158e3 } } },
    // Native HW 0x3030: draw call 0x080097b4; 23x72 at 0x0801364b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_23x72_560", "Mode R panel (landscape, alternate)", "Controls", 23, 72, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x0801364b }, { 0x313a, 0x08013653 }, { 0x3134, 0x08013c7b }, { 0x311c, 0x080158ef } } },
    // Native HW 0x3030: draw call 0x0800993a; 15x21 at 0x08013723.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x21_561", "Mode R panel pointer (landscape)", "Controls", 15, 21, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013723 } } },
    // Native HW 0x3030: draw call 0x08009912; 15x10 at 0x0801374d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x10_562", "Unit Ohm (landscape)", "Units", 15, 10, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x0801374d } } },
    // Native HW 0x3030: draw call 0x0800914a; 40x7 at 0x08013761.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_40x7_563", "Puff panel side edge (landscape)", "Controls", 40, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013761 }, { 0x313a, 0x08013769 }, { 0x3134, 0x08013d91 }, { 0x311c, 0x08015a05 } } },
    // Native HW 0x3030: draw call 0x08009122; 18x16 at 0x08013784.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_18x16_564", "Mode panel (landscape)", "Controls", 18, 16, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013784 }, { 0x313a, 0x0801378c }, { 0x3134, 0x08013db4 }, { 0x311c, 0x08015a28 } } },
    // Native HW 0x3030: draw call 0x080091a8; 18x16 at 0x080139f4.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_18x16_565", "Mode panel fill (landscape)", "Controls", 18, 16, 1, 0x3030, AssetFormat::Rgb565, "", 0, "", { { 0x3030, 0x080139f4 }, { 0x313a, 0x080139fc }, { 0x3134, 0x08014024 }, { 0x311c, 0x08015c98 } } },
    // Native HW 0x3030: draw call 0x08009194; 18x16 at 0x08013c34.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_18x16_566", "Mode P panel (landscape)", "Controls", 18, 16, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013c34 }, { 0x313a, 0x08013c3c }, { 0x3134, 0x08014264 }, { 0x311c, 0x08015ed8 } } },
    // Native HW 0x3030: draw call 0x080092ec; 23x72 at 0x08013c64.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_23x72_567", "Puff panel (landscape)", "Controls", 23, 72, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013c64 }, { 0x313a, 0x08013c6c }, { 0x3134, 0x08014294 }, { 0x311c, 0x08015f08 } } },
    // Native HW 0x3030: draw call 0x08009440; 15x21 at 0x08013d3c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x21_568", "Puff panel pointer (landscape)", "Controls", 15, 21, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013d3c }, { 0x313a, 0x08013d44 }, { 0x3134, 0x0801436c }, { 0x311c, 0x08015fe0 } } },
    // Native HW 0x3030: draw call 0x0800925c; 13x12 at 0x08013d66.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_13x12_569", "Unit S (landscape)", "Units", 13, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08013d66 }, { 0x313a, 0x08013d6e }, { 0x3134, 0x08014396 }, { 0x311c, 0x0801600a } } },
    // Native HW 0x3030: draw call 0x08009cb2; 23x69 at 0x08013d7e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_23x69_570", "Voltage panel (landscape)", "Controls", 23, 69, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013d7e }, { 0x313a, 0x08013d86 }, { 0x3134, 0x080143ae }, { 0x311c, 0x08016022 } } },
    // Native HW 0x3030: draw call 0x08009ccc; 15x21 at 0x08013e4d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x21_571", "Voltage panel pointer (landscape)", "Controls", 15, 21, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013e4d }, { 0x313a, 0x08013e55 }, { 0x3134, 0x0801447d }, { 0x311c, 0x080160f1 } } },
    // Native HW 0x3030: draw call 0x08009de6; 14x10 at 0x08013e77.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x10_572", "Unit V (landscape)", "Units", 14, 10, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08013e77 }, { 0x313a, 0x08013e7f }, { 0x3134, 0x080144a7 }, { 0x311c, 0x0801611b } } },
    // Native HW 0x3030: draw call 0x08009ecc; 6x12 at 0x08013e8b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_6x12_573", "Decimal separator (landscape)", "Units", 6, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013e8b }, { 0x313a, 0x08013e93 }, { 0x3134, 0x080144bb }, { 0x311c, 0x0801612f } } },
    // Native HW 0x3030: draw call 0x08009ef4; 23x69 at 0x08013e97.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_23x69_574", "Voltage panel (landscape, alternate)", "Controls", 23, 69, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013e97 }, { 0x313a, 0x08013e9f }, { 0x3134, 0x080144c7 }, { 0x311c, 0x0801613b } } },
    // Native HW 0x3030: draw call 0x08009f0e; 15x21 at 0x08013f66.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x21_575", "Voltage panel pointer (landscape)", "Controls", 15, 21, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013f66 } } },
    // Native HW 0x3030: draw call 0x0800a026; 14x10 at 0x08013f90.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x10_576", "Unit V (landscape)", "Units", 14, 10, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08013f90 } } },
    // Native HW 0x3030: draw call 0x08009ace; 15x21 at 0x08013fa4.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x21_577", "Time panel pointer (landscape)", "Controls", 15, 21, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013fa4 }, { 0x313a, 0x08013fac }, { 0x3134, 0x080145d4 }, { 0x311c, 0x08016248 } } },
    // Native HW 0x3030: draw call 0x08009aa4; 23x69 at 0x08013fce.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_23x69_578", "Time panel (landscape)", "Controls", 23, 69, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08013fce }, { 0x313a, 0x08013fd6 }, { 0x3134, 0x080145fe }, { 0x311c, 0x08016272 } } },
    // Native HW 0x3030: draw call 0x08009be4; 14x10 at 0x0801409d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x10_579", "Unit W (landscape)", "Units", 14, 10, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x0801409d }, { 0x313a, 0x080140a5 }, { 0x3134, 0x080146cd }, { 0x311c, 0x08016341 } } },
    // Native HW 0x3030: draw call 0x080099f8; 40x23 at 0x080140b1.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_40x23_580", "Mode S panel (landscape)", "Controls", 40, 23, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x080140b1 }, { 0x313a, 0x080140b9 }, { 0x3134, 0x080146e1 }, { 0x311c, 0x08016355 } } },
    // Native HW 0x3030: draw call 0x0800af84; 40x26 at 0x08014290.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_40x26_581", "Mode NI", "Controls", 40, 26, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014290 }, { 0x313a, 0x08014298 }, { 0x3134, 0x080148c0 }, { 0x311c, 0x08016534 } } },
    // Native HW 0x3030: draw call 0x0800b02a; 40x26 at 0x08014312.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_40x26_582", "Mode TI", "Controls", 40, 26, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014312 }, { 0x313a, 0x0801431a }, { 0x3134, 0x08014942 }, { 0x311c, 0x080165b6 } } },
    // Native HW 0x3030: draw call 0x0800b0ca; 40x26 at 0x08014394.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_40x26_583", "Mode SS", "Controls", 40, 26, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014394 }, { 0x313a, 0x0801439c }, { 0x3134, 0x080149c4 }, { 0x311c, 0x08016638 } } },
    // Native HW 0x3030: draw call 0x0800b1d0; 11x32 at 0x08014416.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_11x32_584", "Mode panel top left edge", "Controls", 11, 32, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014416 }, { 0x313a, 0x0801441e }, { 0x3134, 0x08014a46 }, { 0x311c, 0x080166ba } } },
    // Native HW 0x3030: draw call 0x0800b204; 11x32 at 0x08014456.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_11x32_585", "Mode panel bottom right edge", "Controls", 11, 32, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014456 }, { 0x313a, 0x0801445e }, { 0x3134, 0x08014a86 }, { 0x311c, 0x080166fa } } },
    // Native HW 0x3030: draw call 0x0800b276; 17x54 at 0x08014496.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x54_586", "Mode panel left edge", "Controls", 17, 54, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014496 }, { 0x313a, 0x0801449e }, { 0x3134, 0x08014ac6 }, { 0x311c, 0x0801673a } } },
    // Native HW 0x3030: draw call 0x0800b29e; 17x54 at 0x08014538.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_17x54_587", "Mode panel right edge", "Controls", 17, 54, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014538 }, { 0x313a, 0x08014540 }, { 0x3134, 0x08014b68 }, { 0x311c, 0x080167dc } } },
    // Native HW 0x3030: draw call 0x0800b470; 56x29 at 0x08014770.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_56x29_588", "Mode SS (landscape)", "Controls", 56, 29, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014770 }, { 0x313a, 0x08014778 }, { 0x3134, 0x08014da0 }, { 0x311c, 0x08016a14 } } },
    // Native HW 0x3030: draw call 0x0800b48c; 56x29 at 0x0801483b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_56x29_589", "Mode M (landscape)", "Controls", 56, 29, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x0801483b }, { 0x313a, 0x08014843 }, { 0x3134, 0x08014e6b }, { 0x311c, 0x08016adf } } },
    // Native HW 0x3030: draw call 0x0800b2dc; 56x29 at 0x08014906.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_56x29_590", "Unit Ohm panel (landscape)", "Units", 56, 29, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014906 }, { 0x313a, 0x0801490e }, { 0x311c, 0x08016baa } } },
    // Native HW 0x3030: draw call 0x0800b66a; 2x20 at 0x080149d1.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x20_591", "Decimal separator", "Units", 2, 20, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x080149d1 }, { 0x313a, 0x080149d9 }, { 0x3134, 0x08015001 }, { 0x311c, 0x08016c75 } } },
    // Native HW 0x3030: draw call 0x0800bd06; 32x34 at 0x080149e5.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_32x34_592", "Locked", "Controls", 32, 34, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x080149e5 }, { 0x313a, 0x080149ed }, { 0x3134, 0x08015015 }, { 0x311c, 0x08016c89 } } },
    // Native HW 0x3030: draw call 0x0800bce8; 32x30 at 0x08014ae5.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_32x30_593", "Unit degrees C", "Units", 32, 30, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08014ae5 }, { 0x313a, 0x08014aed }, { 0x3134, 0x08015115 }, { 0x311c, 0x08016d89 } } },
    // Native HW 0x3030: draw call 0x0800aa9c; 46x36 at 0x08014b5d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_46x36_594", "Label Watt / Time panel", "Labels", 46, 36, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014b5d }, { 0x313a, 0x08014b65 }, { 0x3134, 0x0801518d }, { 0x311c, 0x08016e01 } } },
    // Native HW 0x3030: draw call 0x0800aad4; 36x36 at 0x08014c35.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_36x36_595", "Mode NI panel", "Controls", 36, 36, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014c35 }, { 0x313a, 0x08014c3d }, { 0x3134, 0x08015265 }, { 0x311c, 0x08016ed9 } } },
    // Native HW 0x3030: draw call 0x0800aaea; 46x36 at 0x08014e51.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_46x36_596", "Label Ohms / Puff panel", "Labels", 46, 36, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014e51 }, { 0x313a, 0x08014e59 }, { 0x3134, 0x08015481 }, { 0x311c, 0x080170f5 } } },
    // Native HW 0x3030: draw call 0x0800b6e4; 12x18 at 0x08014f29.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x18_597", "Panel corner top left", "Controls", 12, 18, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014f29 }, { 0x313a, 0x08014f31 }, { 0x3134, 0x08015559 }, { 0x311c, 0x080171cd } } },
    // Native HW 0x3030: draw call 0x0800b790; 2x14 at 0x08014f4d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x14_598", "Decimal separator", "Units", 2, 14, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08014f4d }, { 0x313a, 0x08014f55 }, { 0x3134, 0x0801557d }, { 0x311c, 0x080171f1 } } },
    // Native HW 0x3030: draw call 0x0800ba08; 12x18 at 0x08014f5b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x18_599", "Panel corner top right", "Controls", 12, 18, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014f5b }, { 0x313a, 0x08014f63 }, { 0x311c, 0x080171ff } } },
    // Native HW 0x3030: draw call 0x0800ba96; 2x14 at 0x08014f7f.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x14_600", "Decimal separator", "Units", 2, 14, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08014f7f } } },
    // Native HW 0x3030: draw call 0x0800bd46; 10x18 at 0x08014f8d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x18_601", "Panel corner bottom left", "Controls", 10, 18, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014f8d }, { 0x313a, 0x08014f95 }, { 0x3134, 0x080155bd }, { 0x311c, 0x08017231 } } },
    // Native HW 0x3030: draw call 0x0800bdb2; 2x14 at 0x08014fb1.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x14_602", "Decimal separator", "Units", 2, 14, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08014fb1 } } },
    // Native HW 0x3030: draw call 0x0800be50; 9x14 at 0x08014fbf.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_9x14_603", "Unit S", "Units", 9, 14, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08014fbf }, { 0x313a, 0x08014fc7 }, { 0x3134, 0x080155ef }, { 0x311c, 0x08017263 } } },
    // Native HW 0x3030: draw call 0x0800b862; 8x18 at 0x08014fdb.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x18_604", "Panel corner bottom right", "Controls", 8, 18, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014fdb }, { 0x313a, 0x08014fe3 }, { 0x3134, 0x0801560b }, { 0x311c, 0x0801727f } } },
    // Native HW 0x3030: draw call 0x0800ab6c; 128x7 at 0x08014fee.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_128x7_605", "Panel top edge", "Controls", 128, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08014fee }, { 0x313a, 0x08014ff6 }, { 0x3134, 0x0801561e }, { 0x311c, 0x08017292 } } },
    // Native HW 0x3030: draw call 0x0800ab82; 128x8 at 0x0801505e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_128x8_606", "Panel bottom edge", "Controls", 128, 8, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x0801505e }, { 0x313a, 0x08015066 }, { 0x3134, 0x0801568e }, { 0x311c, 0x08017302 } } },
    // Native HW 0x3030: draw call 0x08006420; 58x12 at 0x080150e8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_58x12_607", "Label Theme", "Labels", 58, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x080150e8 }, { 0x313a, 0x080150f0 }, { 0x3134, 0x08015718 }, { 0x311c, 0x0801738c } } },
    // Native HW 0x3030: draw call 0x0800652a; 10x42 at 0x08015148.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x42_608", "Theme panel left edge", "Controls", 10, 42, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "", { { 0x3030, 0x08015148 }, { 0x313a, 0x08015150 }, { 0x3134, 0x08015778 }, { 0x311c, 0x080173ec } } },
    // Native HW 0x3030: draw call 0x0800654c; 10x42 at 0x0801519c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x42_609", "Theme panel right edge", "Controls", 10, 42, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "", { { 0x3030, 0x0801519c }, { 0x313a, 0x080151a4 }, { 0x3134, 0x080157cc }, { 0x311c, 0x08017440 } } },
    // Native HW 0x3030: draw call 0x080064ce; 94x42 at 0x080151f0.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_94x42_610", "Theme IRON panel", "Controls", 94, 42, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x080151f0 }, { 0x313a, 0x080151f8 }, { 0x3134, 0x08015820 }, { 0x311c, 0x08017494 } } },
    // Native HW 0x3030: draw call 0x08006574; 94x42 at 0x08015408.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_94x42_611", "Theme CORE panel", "Controls", 94, 42, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3030, 0x08015408 }, { 0x313a, 0x08015410 }, { 0x3134, 0x08015a38 }, { 0x311c, 0x080176ac } } },
    // Native HW 0x3030: draw call 0x0800d60e; 90x15 at 0x08015628.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_90x15_612", "Logo VOOPOO (chip information)", "Logos", 90, 15, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3030, 0x08015628 }, { 0x313a, 0x08015630 }, { 0x3134, 0x08015c58 }, { 0x311c, 0x080178cc } } },
    // Native HW 0x3030: draw call 0x0800d658; 102x87 at 0x080156dc.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_102x87_613", "Chip ID panel", "Labels", 102, 87, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3030, 0x080156dc }, { 0x313a, 0x080156e4 }, { 0x3134, 0x08015d0c }, { 0x311c, 0x08017980 } } },
    // Native HW 0x3030: draw call 0x0800d74e; 102x63 at 0x08015b47.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_102x63_614", "Level Code panel", "Labels", 102, 63, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3030, 0x08015b47 }, { 0x313a, 0x08015b4f }, { 0x3134, 0x08016177 }, { 0x311c, 0x08017deb } } },
    // Native HW 0x3030: draw call 0x08006daa; 14x4 at 0x08015eba.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x4_615", "Animation segment", "Animations", 14, 4, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015eba }, { 0x313a, 0x08015ec2 }, { 0x3134, 0x080164ea }, { 0x311c, 0x0801815e } } },
    // Native HW 0x3030: draw call 0x08006daa; 15x6 at 0x08015ec2.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x6_616", "Animated segment 01", "Animations / Portrait", 15, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015ec2 }, { 0x313a, 0x08015eca }, { 0x311c, 0x08018166 } } },
    // Native HW 0x3030: draw call 0x08006daa; 15x7 at 0x08015ece.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_617", "Animated segment 02", "Animations / Portrait", 15, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015ece }, { 0x313a, 0x08015ed6 }, { 0x311c, 0x08018172 } } },
    // Native HW 0x3030: draw call 0x08006daa; 15x7 at 0x08015edc.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_618", "Animated segment 03", "Animations / Portrait", 15, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015edc }, { 0x313a, 0x08015ee4 }, { 0x311c, 0x08018180 } } },
    // Native HW 0x3030: draw call 0x08006daa; 16x8 at 0x08015eea.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x8_619", "Animated segment 04", "Animations / Portrait", 16, 8, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015eea }, { 0x313a, 0x08015ef2 }, { 0x311c, 0x0801818e } } },
    // Native HW 0x3030: draw call 0x08006daa; 13x8 at 0x08015efa.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_13x8_620", "Animated segment 05", "Animations / Portrait", 13, 8, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015efa }, { 0x313a, 0x08015f02 }, { 0x311c, 0x0801819e } } },
    // Native HW 0x3030: draw call 0x08006daa; 12x8 at 0x08015f0a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x8_621", "Animated segment 06", "Animations / Portrait", 12, 8, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015f0a }, { 0x313a, 0x08015f12 }, { 0x311c, 0x080181ae } } },
    // Native HW 0x3030: draw call 0x08006daa; 16x8 at 0x08015f1a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x8_622", "Animated segment 07", "Animations / Portrait", 16, 8, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015f1a }, { 0x313a, 0x08015f22 }, { 0x311c, 0x080181be } } },
    // Native HW 0x3030: draw call 0x08006daa; 15x7 at 0x08015f2a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_623", "Animated segment 08", "Animations / Portrait", 15, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015f2a }, { 0x313a, 0x08015f32 }, { 0x311c, 0x080181ce } } },
    // Native HW 0x3030: draw call 0x08006daa; 15x7 at 0x08015f38.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_624", "Animated segment 09", "Animations / Portrait", 15, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015f38 }, { 0x313a, 0x08015f40 }, { 0x311c, 0x080181dc } } },
    // Native HW 0x3030: draw call 0x08006daa; 15x6 at 0x08015f46.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x6_625", "Animated segment 10", "Animations / Portrait", 15, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015f46 }, { 0x313a, 0x08015f4e }, { 0x311c, 0x080181ea } } },
    // Native HW 0x3030: draw call 0x08006daa; 15x4 at 0x08015f52.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x4_626", "Animated segment 11", "Animations / Portrait", 15, 4, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015f52 }, { 0x313a, 0x08015f5a }, { 0x311c, 0x080181f6 } } },
    // Native HW 0x3030: draw call 0x08006daa; 3x6 at 0x08015f5a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_3x6_627", "Animated line 01", "Animations / Portrait", 3, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015f5a }, { 0x313a, 0x08015f62 }, { 0x311c, 0x080181fe } } },
    // Native HW 0x3030: draw call 0x08006daa; 7x12 at 0x08015f60.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x12_628", "Animated line 02", "Animations / Portrait", 7, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015f60 }, { 0x313a, 0x08015f68 }, { 0x311c, 0x08018204 } } },
    // Native HW 0x3030: draw call 0x08006daa; 11x17 at 0x08015f6c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_11x17_629", "Animated line 03", "Animations / Portrait", 11, 17, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015f6c }, { 0x313a, 0x08015f74 }, { 0x311c, 0x08018210 } } },
    // Native HW 0x3030: draw call 0x08006daa; 14x20 at 0x08015f8e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x20_630", "Animated line 04", "Animations / Portrait", 14, 20, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015f8e }, { 0x313a, 0x08015f96 }, { 0x311c, 0x08018232 } } },
    // Native HW 0x3030: draw call 0x08006daa; 29x6 at 0x08015fb6.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_29x6_631", "Animated line 05", "Animations / Portrait", 29, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015fb6 }, { 0x313a, 0x08015fbe }, { 0x311c, 0x0801825a } } },
    // Native HW 0x3030: draw call 0x08006daa; 14x4 at 0x08015fce.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x4_632", "Animated line 06", "Animations / Portrait", 14, 4, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015fce }, { 0x313a, 0x08015fd6 }, { 0x311c, 0x08018272 } } },
    // Native HW 0x3030: draw call 0x08006daa; 11x17 at 0x08015fd6.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_11x17_633", "Animated line 07", "Animations / Portrait", 11, 17, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015fd6 }, { 0x313a, 0x08015fde }, { 0x311c, 0x0801827a } } },
    // Native HW 0x3030: draw call 0x08006daa; 7x12 at 0x08015ff8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x12_634", "Animated line 08", "Animations / Portrait", 7, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08015ff8 }, { 0x313a, 0x08016000 }, { 0x311c, 0x0801829c } } },
    // Native HW 0x3030: draw call 0x08006daa; 3x6 at 0x08016004.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_3x6_635", "Animated line 09", "Animations / Portrait", 3, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016004 }, { 0x313a, 0x0801600c }, { 0x311c, 0x080182a8 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 8x3 at 0x0801600a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x3_636", "Animated line 10", "Animations / Portrait", 8, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801600a }, { 0x313a, 0x08016012 }, { 0x311c, 0x080182ae } } },
    // Native HW 0x3030: draw call 0x08006c8e; 64x15 at 0x0801600d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_64x15_637", "Logo SUPER", "Logos", 64, 15, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801600d }, { 0x313a, 0x08016015 }, { 0x311c, 0x080182b1 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 8x3 at 0x08016085.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x3_638", "Animated line 01", "Animations / Portrait", 8, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016085 }, { 0x313a, 0x0801608d }, { 0x311c, 0x08018329 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 14x4 at 0x08016088.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x4_639", "Animation segment", "Animations", 14, 4, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016088 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 15x6 at 0x08016090.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x6_640", "Animated segment 01", "Animations / Portrait", 15, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016090 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 15x7 at 0x0801609c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_641", "Animated segment 02", "Animations / Portrait", 15, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801609c } } },
    // Native HW 0x3030: draw call 0x08006c8e; 15x7 at 0x080160aa.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_642", "Animated segment 03", "Animations / Portrait", 15, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x080160aa } } },
    // Native HW 0x3030: draw call 0x08006c8e; 16x8 at 0x080160b8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x8_643", "Animated segment 04", "Animations / Portrait", 16, 8, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x080160b8 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 13x8 at 0x080160c8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_13x8_644", "Animated segment 05", "Animations / Portrait", 13, 8, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x080160c8 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 12x8 at 0x080160d8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x8_645", "Animated segment 06", "Animations / Portrait", 12, 8, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x080160d8 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 16x8 at 0x080160e8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x8_646", "Animated segment 07", "Animations / Portrait", 16, 8, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x080160e8 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 15x7 at 0x080160f8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_647", "Animated segment 08", "Animations / Portrait", 15, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x080160f8 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 15x7 at 0x08016106.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_648", "Animated segment 09", "Animations / Portrait", 15, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016106 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 15x6 at 0x08016114.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x6_649", "Animated segment 10", "Animations / Portrait", 15, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016114 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 15x4 at 0x08016120.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x4_650", "Animated segment 11", "Animations / Portrait", 15, 4, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016120 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 3x6 at 0x08016128.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_3x6_651", "Animated line 01", "Animations / Portrait", 3, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016128 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 7x12 at 0x0801612e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x12_652", "Animated line 02", "Animations / Portrait", 7, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801612e } } },
    // Native HW 0x3030: draw call 0x08006c8e; 8x14 at 0x0801613a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x14_653", "Animated line 16", "Animations / Portrait", 8, 14, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801613a }, { 0x313a, 0x08016142 }, { 0x311c, 0x080183de } } },
    // Native HW 0x3030: draw call 0x08006c8e; 3x2 at 0x08016148.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_3x2_654", "Animated line 17", "Animations / Portrait", 3, 2, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016148 }, { 0x313a, 0x08016150 }, { 0x311c, 0x080183ec } } },
    // Native HW 0x3030: draw call 0x08006c8e; 14x5 at 0x0801614a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x5_655", "Animated line 18", "Animations / Portrait", 14, 5, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801614a }, { 0x313a, 0x08016152 }, { 0x311c, 0x080183ee } } },
    // Native HW 0x3030: draw call 0x08006c8e; 29x7 at 0x08016154.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_29x7_656", "Animated line 19", "Animations / Portrait", 29, 7, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016154 }, { 0x313a, 0x0801615c }, { 0x311c, 0x080183f8 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 14x5 at 0x08016170.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x5_657", "Animated line 20", "Animations / Portrait", 14, 5, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016170 }, { 0x313a, 0x08016178 }, { 0x311c, 0x08018414 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 4x2 at 0x0801617a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_4x2_658", "Animated line 21", "Animations / Portrait", 4, 2, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801617a }, { 0x313a, 0x08016182 }, { 0x311c, 0x0801841e } } },
    // Native HW 0x3030: draw call 0x08006c8e; 7x14 at 0x0801617c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x14_659", "Animated line 22", "Animations / Portrait", 7, 14, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801617c }, { 0x313a, 0x08016184 }, { 0x311c, 0x08018420 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 7x6 at 0x0801618a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x6_660", "Animated line 23", "Animations / Portrait", 7, 6, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801618a }, { 0x313a, 0x08016192 }, { 0x311c, 0x0801842e } } },
    // Native HW 0x3030: draw call 0x08006c8e; 10x3 at 0x08016190.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x3_661", "Animated line 24", "Animations / Portrait", 10, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016190 }, { 0x313a, 0x08016198 }, { 0x311c, 0x08018434 } } },
    // Native HW 0x3030: draw call 0x08006c8e; 9x3 at 0x08016196.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_9x3_662", "Animated line 25", "Animations / Portrait", 9, 3, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016196 }, { 0x313a, 0x0801619e }, { 0x311c, 0x0801843a } } },
    // Native HW 0x3030: draw call 0x08009078; 12x15 at 0x0801619c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x15_663", "Animated block 01", "Animations / Landscape", 12, 15, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801619c }, { 0x313a, 0x080161a4 }, { 0x3134, 0x080167cc }, { 0x311c, 0x08018440 } } },
    // Native HW 0x3030: draw call 0x08009078; 14x13 at 0x080161ba.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x13_664", "Animated block 02", "Animations / Landscape", 14, 13, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x080161ba }, { 0x313a, 0x080161c2 }, { 0x3134, 0x080167ea }, { 0x311c, 0x0801845e } } },
    // Native HW 0x3030: draw call 0x08009078; 14x15 at 0x080161d4.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x15_665", "Animated block 03", "Animations / Landscape", 14, 15, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x080161d4 }, { 0x313a, 0x080161dc }, { 0x3134, 0x08016804 }, { 0x311c, 0x08018478 } } },
    // Native HW 0x3030: draw call 0x08009078; 14x12 at 0x080161f2.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x12_666", "Animated block 04", "Animations / Landscape", 14, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x080161f2 }, { 0x313a, 0x080161fa }, { 0x3134, 0x08016822 }, { 0x311c, 0x08018496 } } },
    // Native HW 0x3030: draw call 0x08009078; 14x13 at 0x0801620a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x13_667", "Animated block 05", "Animations / Landscape", 14, 13, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801620a }, { 0x313a, 0x08016212 }, { 0x3134, 0x0801683a }, { 0x311c, 0x080184ae } } },
    // Native HW 0x3030: draw call 0x08009078; 12x15 at 0x08016224.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x15_668", "Animated block 06", "Animations / Landscape", 12, 15, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016224 }, { 0x313a, 0x0801622c }, { 0x3134, 0x08016854 }, { 0x311c, 0x080184c8 } } },
    // Native HW 0x3030: draw call 0x08009078; 12x15 at 0x08016242.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x15_669", "Animated block 07", "Animations / Landscape", 12, 15, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016242 }, { 0x313a, 0x0801624a }, { 0x3134, 0x08016872 }, { 0x311c, 0x080184e6 } } },
    // Native HW 0x3030: draw call 0x08009078; 16x15 at 0x08016260.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x15_670", "Animated block 08", "Animations / Landscape", 16, 15, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016260 }, { 0x313a, 0x08016268 }, { 0x3134, 0x08016890 }, { 0x311c, 0x08018504 } } },
    // Native HW 0x3030: draw call 0x08009078; 12x15 at 0x0801627e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x15_671", "Animated block 09", "Animations / Landscape", 12, 15, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801627e }, { 0x313a, 0x08016286 }, { 0x3134, 0x080168ae }, { 0x311c, 0x08018522 } } },
    // Native HW 0x3030: draw call 0x08009078; 12x15 at 0x0801629c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x15_672", "Animated block 10", "Animations / Landscape", 12, 15, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801629c }, { 0x313a, 0x080162a4 }, { 0x3134, 0x080168cc }, { 0x311c, 0x08018540 } } },
    // Native HW 0x3030: draw call 0x08009078; 14x13 at 0x080162ba.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x13_673", "Animated block 11", "Animations / Landscape", 14, 13, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x080162ba }, { 0x313a, 0x080162c2 }, { 0x3134, 0x080168ea }, { 0x311c, 0x0801855e } } },
    // Native HW 0x3030: draw call 0x08009078; 10x12 at 0x080162d4.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x12_674", "Animated block 12", "Animations / Landscape", 10, 12, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x080162d4 }, { 0x313a, 0x080162dc }, { 0x3134, 0x08016904 }, { 0x311c, 0x08018578 } } },
    // Native HW 0x3030: draw call 0x08009078; 10x15 at 0x080162ec.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x15_675", "Animated block 13", "Animations / Landscape", 10, 15, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x080162ec }, { 0x313a, 0x080162f4 }, { 0x3134, 0x0801691c }, { 0x311c, 0x08018590 } } },
    // Native HW 0x3030: draw call 0x08009078; 14x13 at 0x0801630a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x13_676", "Animated block 14", "Animations / Landscape", 14, 13, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801630a }, { 0x313a, 0x08016312 }, { 0x3134, 0x0801693a }, { 0x311c, 0x080185ae } } },
    // Native HW 0x3030: draw call 0x08009078; 12x15 at 0x08016324.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x15_677", "Animated block 15", "Animations / Landscape", 12, 15, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016324 }, { 0x313a, 0x0801632c }, { 0x3134, 0x08016954 }, { 0x311c, 0x080185c8 } } },
    // Native HW 0x3030: draw call 0x08009078; 12x14 at 0x08016342.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x14_678", "Animated block 16", "Animations / Landscape", 12, 14, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x08016342 }, { 0x313a, 0x0801634a }, { 0x3134, 0x08016972 }, { 0x311c, 0x080185e6 } } },
    // Native HW 0x3030: draw call 0x08009078; 16x14 at 0x0801635e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x14_679", "Animated block 17", "Animations / Landscape", 16, 14, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801635e }, { 0x313a, 0x08016366 }, { 0x3134, 0x0801698e }, { 0x311c, 0x08018602 } } },
    // Native HW 0x3030: draw call 0x08009078; 12x14 at 0x0801637a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x14_680", "Animated block 18", "Animations / Landscape", 12, 14, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3030, 0x0801637a }, { 0x313a, 0x08016382 }, { 0x3134, 0x080169aa }, { 0x311c, 0x0801861e } } },
    // Native HW 0x3030: draw call 0x080053d0; 59x29 at 0x0801666e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_59x29_681", "OFF", "Messages", 59, 29, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x0801666e }, { 0x313a, 0x08016676 }, { 0x3134, 0x08016c9e }, { 0x311c, 0x08018912 } } },
    // Native HW 0x3030: draw call 0x0800534c; 128x28 at 0x08016756.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_128x28_682", "Logo DRAG 3 (boot)", "Logos", 128, 28, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3030, 0x08016756 }, { 0x313a, 0x0801675e }, { 0x3134, 0x08016d86 }, { 0x311c, 0x080189fa } } },
    // Native HW 0x3030: draw call 0x08005376; 128x50 at 0x08016916.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_128x50_683", "Logo GENE FAN (boot)", "Logos", 128, 50, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08016916 }, { 0x313a, 0x0801691e }, { 0x3134, 0x08016f46 }, { 0x311c, 0x08018bba } } },
    // Native HW 0x3030: draw call 0x08005e1c; 61x10 at 0x080170b6.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_61x10_684", "Charging", "Messages", 61, 10, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3030, 0x080170b6 }, { 0x313a, 0x080170be }, { 0x3134, 0x080176e6 }, { 0x311c, 0x0801935a } } },
    // Native HW 0x3030: font selector 9; 30x64 at 0x08017714.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_30x64_685", "Font 30x64", "Font 30x64", 30, 64, 10, 0x3030, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3030, 0x08017714 }, { 0x313a, 0x0801771c }, { 0x3134, 0x08017d44 }, { 0x311c, 0x080199b8 } } },
    // Native HW 0x3030: draw call 0x080066cc; 39x39 at 0x08018114.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_39x39_686", "Icon Time Out", "Messages", 39, 39, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x3030, 0x08018114 }, { 0x313a, 0x0801811c }, { 0x3134, 0x08018744 }, { 0x311c, 0x0801a3b8 } } },
    // Native HW 0x3030: draw call 0x080066e4; 77x58 at 0x080181d7.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_77x58_687", "Time Out", "Messages", 77, 58, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x080181d7 }, { 0x313a, 0x080181df }, { 0x3134, 0x08018807 }, { 0x311c, 0x0801a47b } } },
    // Native HW 0x3030: draw call 0x080063a6; 30x53 at 0x0801841b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_30x53_688", "Icon Atomizer", "Messages", 30, 53, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x3030, 0x0801841b }, { 0x313a, 0x08018423 }, { 0x3134, 0x08018a4b }, { 0x311c, 0x0801a6bf } } },
    // Native HW 0x3030: draw call 0x080063be; 77x58 at 0x080184ef.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_77x58_689", "Max Power", "Messages", 77, 58, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x080184ef }, { 0x313a, 0x080184f7 }, { 0x3134, 0x08018b1f }, { 0x311c, 0x0801a793 } } },
    // Native HW 0x3030: draw call 0x08005d78; 62x41 at 0x08018733.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_62x41_690", "Icon Atomizer Short", "Messages", 62, 41, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08018733 }, { 0x313a, 0x0801873b }, { 0x3134, 0x08018d63 }, { 0x311c, 0x0801a9d7 } } },
    // Native HW 0x3030: draw call 0x08005d90; 77x58 at 0x0801887b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_77x58_691", "Atomizer Short", "Messages", 77, 58, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x0801887b }, { 0x313a, 0x08018883 }, { 0x3134, 0x08018eab }, { 0x311c, 0x0801ab1f } } },
    // Native HW 0x3030: draw call 0x080062d8; 28x54 at 0x08018abf.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_28x54_692", "Icon Check Battery", "Messages", 28, 54, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08018abf }, { 0x313a, 0x08018ac7 }, { 0x3134, 0x080190ef }, { 0x311c, 0x0801ad63 } } },
    // Native HW 0x3030: draw call 0x080062f0; 77x58 at 0x08018b97.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_77x58_693", "Check Battery", "Messages", 77, 58, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08018b97 }, { 0x313a, 0x08018b9f }, { 0x3134, 0x080191c7 }, { 0x311c, 0x0801ae3b } } },
    // Native HW 0x3030: draw call 0x0800633e; 28x53 at 0x08018ddb.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_28x53_694", "Icon Low Power", "Messages", 28, 53, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x08018ddb }, { 0x313a, 0x08018de3 }, { 0x3134, 0x0801940b }, { 0x311c, 0x0801b07f } } },
    // Native HW 0x3030: draw call 0x08006356; 77x58 at 0x08018eaf.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_77x58_695", "Low Power", "Messages", 77, 58, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08018eaf }, { 0x313a, 0x08018eb7 }, { 0x3134, 0x080194df }, { 0x311c, 0x0801b153 } } },
    // Native HW 0x3030: draw call 0x08006798; 40x43 at 0x080190f3.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_40x43_696", "Locked", "Messages", 40, 43, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x080190f3 }, { 0x313a, 0x080190fb }, { 0x3134, 0x08019723 }, { 0x311c, 0x0801b397 } } },
    // Native HW 0x3030: draw call 0x080067b0; 77x58 at 0x080191ca.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_77x58_697", "Press Fire & +", "Messages", 77, 58, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x080191ca }, { 0x313a, 0x080191d2 }, { 0x3134, 0x080197fa }, { 0x311c, 0x0801b46e } } },
    // Native HW 0x3030: draw call 0x08006732; 40x43 at 0x0801940e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_40x43_698", "Unlocked", "Messages", 40, 43, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_lime", { { 0x3030, 0x0801940e }, { 0x313a, 0x08019416 }, { 0x3134, 0x08019a3e }, { 0x311c, 0x0801b6b2 } } },
    // Native HW 0x3030: draw call 0x0800622e; 27x26 at 0x080194e5.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_27x26_699", "Icon Check Atomizer", "Messages", 27, 26, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x080194e5 }, { 0x313a, 0x080194ed }, { 0x3134, 0x08019b15 }, { 0x311c, 0x0801b789 } } },
    // Native HW 0x3030: draw call 0x08006246; 27x40 at 0x0801954d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_27x40_700", "Atomizer silhouette", "Messages", 27, 40, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_cyan", { { 0x3030, 0x0801954d }, { 0x313a, 0x08019555 }, { 0x3134, 0x08019b7d }, { 0x311c, 0x0801b7f1 } } },
    // Native HW 0x3030: draw call 0x0800626c; 77x58 at 0x080195ed.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_77x58_701", "Check Atomizer", "Messages", 77, 58, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x080195ed }, { 0x313a, 0x080195f5 }, { 0x3134, 0x08019c1d }, { 0x311c, 0x0801b891 } } },
    // Native HW 0x3030: draw call 0x08006668; 43x47 at 0x08019831.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_43x47_702", "Icon High Temp", "Messages", 43, 47, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x3030, 0x08019831 }, { 0x313a, 0x08019839 }, { 0x3134, 0x08019e61 }, { 0x311c, 0x0801bad5 } } },
    // Native HW 0x3030: draw call 0x08006680; 77x58 at 0x0801994b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_77x58_703", "High Temp", "Messages", 77, 58, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x0801994b }, { 0x313a, 0x08019953 }, { 0x3134, 0x08019f7b }, { 0x311c, 0x0801bbef } } },
    // Native HW 0x3030: draw call 0x08006602; 60x31 at 0x08019b8f.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_60x31_704", "Icon Puff reset", "Messages", 60, 31, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3030, 0x08019b8f }, { 0x313a, 0x08019b97 }, { 0x3134, 0x0801a1bf }, { 0x311c, 0x0801be33 } } },
    // Native HW 0x3030: draw call 0x0800661a; 77x58 at 0x08019c87.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_77x58_705", "Puff Cleared", "Messages", 77, 58, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x08019c87 }, { 0x313a, 0x08019c8f }, { 0x3134, 0x0801a2b7 }, { 0x311c, 0x0801bf2b } } },
    // Native HW 0x3030: font selector 8; 11x20 at 0x08019ecb.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_11x20_706", "Font 11x20", "Font 11x20", 11, 20, 10, 0x3030, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3030, 0x08019ecb }, { 0x313a, 0x08019ed3 }, { 0x3134, 0x0801a4fb }, { 0x311c, 0x0801c16f } } },
    // Native HW 0x3030: draw call 0x08008da8; 21x23 at 0x0801a05b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_21x23_707", "Locked", "Controls", 21, 23, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3030, 0x0801a05b }, { 0x313a, 0x0801a063 }, { 0x3134, 0x0801a68b }, { 0x311c, 0x0801c2ff } } },
    // Native HW 0x3030: draw call 0x08008e28; 21x22 at 0x0801a12a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_21x22_708", "Unit W", "Units", 21, 22, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x0801a12a }, { 0x313a, 0x0801a132 }, { 0x3134, 0x0801a75a }, { 0x311c, 0x0801c3ce } } },
    // Native HW 0x3030: font selector 2; 6x12 at 0x0801a21c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_6x12_709", "Font 6x12", "Font 6x12", 6, 12, 11, 0x3030, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3030, 0x0801a21c }, { 0x313a, 0x0801a224 }, { 0x3134, 0x0801a84c }, { 0x311c, 0x0801c4c0 } } },
    // Native HW 0x3030: draw call 0x0800742a; 10x16 at 0x0801a2a0.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x16_710", "Unit S", "Units", 10, 16, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x0801a2a0 }, { 0x313a, 0x0801a2a8 }, { 0x3134, 0x0801a8d0 }, { 0x311c, 0x0801c544 } } },
    // Native HW 0x3030: font selector 6; 9x18 at 0x0801a2c0.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_9x18_711", "Font 9x18", "Font 9x18", 9, 18, 10, 0x3030, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3030, 0x0801a2c0 }, { 0x313a, 0x0801a2c8 }, { 0x3134, 0x0801a8f0 }, { 0x311c, 0x0801c564 } } },
    // Native HW 0x3030: font selector 10; 13x7 at 0x0801a500.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_13x7_712", "Font 13x7 / Landscape", "Font 13x7 / Landscape", 13, 7, 11, 0x3030, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x3030, 0x0801a500 }, { 0x313a, 0x0801a508 }, { 0x3134, 0x0801ab30 }, { 0x311c, 0x0801c7a4 } } },
    // Native HW 0x3030: font selector 12; 40x19 at 0x0801a59a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_40x19_713", "Font 40x19 / Landscape", "Font 40x19 / Landscape", 40, 19, 10, 0x3030, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x3030, 0x0801a59a }, { 0x313a, 0x0801a5a2 }, { 0x3134, 0x0801abca }, { 0x311c, 0x0801c83e } } },
    // Native HW 0x3030: font selector 11; 14x8 at 0x0801a950.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_14x8_714", "Font 14x8 / Landscape", "Font 14x8 / Landscape", 14, 8, 11, 0x3030, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x3030, 0x0801a950 }, { 0x313a, 0x0801a958 }, { 0x3134, 0x0801af80 }, { 0x311c, 0x0801cbf4 } } },
    // Native HW 0x3030: font selector 4; 8x14 at 0x0801aa00.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "font_native_8x14_715", "Font 8x14", "Font 8x14", 8, 14, 11, 0x3030, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3030, 0x0801aa00 }, { 0x313a, 0x0801aa08 }, { 0x3134, 0x0801b030 }, { 0x311c, 0x0801cca4 } } },
    // Native HW 0x3030: font selector 13; 40x10 at 0x0801aa9a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "symbol_native_40x10_716", "Bar with raised tip", "Special glyphs 40x10 / Landscape", 40, 10, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3030, 0x0801aa9a }, { 0x313a, 0x0801aaa2 }, { 0x3134, 0x0801b0ca }, { 0x311c, 0x0801cd3e } } },
    // Native HW 0x313a: draw call 0x0800995a; 2x3 at 0x08012713.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_717", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x313a, 0x08012713 } } },
    // Native HW 0x313a: draw call 0x08009ce4; 2x3 at 0x08012719.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_718", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x313a, 0x08012719 } } },
    // Native HW 0x313a: draw call 0x08009f26; 2x3 at 0x0801271c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_719", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x313a, 0x0801271c } } },
    // Native HW 0x313a: draw call 0x08009ae6; 2x3 at 0x0801271f.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_720", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x313a, 0x0801271f } } },
    // Native HW 0x313a: draw call 0x08009832; 15x2 at 0x08012729.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x2_721", "Decimal separator (landscape)", "Units", 15, 2, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x313a, 0x08012729 } } },
    // Native HW 0x313a: draw call 0x08009f90; 14x2 at 0x08012731.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x2_722", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x313a, 0x08012731 } } },
    // Native HW 0x313a: draw call 0x08009b4e; 14x2 at 0x08012735.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x2_723", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x313a, 0x08012735 } } },
    // Native HW 0x313a: draw call 0x0800993a; 15x21 at 0x0801372b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x21_724", "Mode R panel pointer (landscape)", "Controls", 15, 21, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x313a, 0x0801372b } } },
    // Native HW 0x313a: draw call 0x08009912; 15x10 at 0x08013755.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x10_725", "Unit Ohm (landscape)", "Units", 15, 10, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x313a, 0x08013755 } } },
    // Native HW 0x313a: draw call 0x08009f0e; 15x21 at 0x08013f6e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x21_726", "Voltage panel pointer (landscape)", "Controls", 15, 21, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x313a, 0x08013f6e } } },
    // Native HW 0x313a: draw call 0x0800a026; 14x10 at 0x08013f98.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x10_727", "Unit V (landscape)", "Units", 14, 10, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x313a, 0x08013f98 } } },
    // Native HW 0x313a: draw call 0x0800ba96; 2x14 at 0x08014f87.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x14_728", "Decimal separator", "Units", 2, 14, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x313a, 0x08014f87 } } },
    // Native HW 0x313a: draw call 0x0800bdb2; 2x14 at 0x08014fb9.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x14_729", "Decimal separator", "Units", 2, 14, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x313a, 0x08014fb9 } } },
    // Native HW 0x313a: draw call 0x08006c8e; 14x4 at 0x08016090.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x4_730", "Animation segment", "Animations", 14, 4, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x08016090 } } },
    // Native HW 0x313a: draw call 0x08006c8e; 15x6 at 0x08016098.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x6_731", "Animated segment 01", "Animations / Portrait", 15, 6, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x08016098 } } },
    // Native HW 0x313a: draw call 0x08006c8e; 15x7 at 0x080160a4.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_732", "Animated segment 02", "Animations / Portrait", 15, 7, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x080160a4 } } },
    // Native HW 0x313a: draw call 0x08006c8e; 15x7 at 0x080160b2.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_733", "Animated segment 03", "Animations / Portrait", 15, 7, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x080160b2 } } },
    // Native HW 0x313a: draw call 0x08006c8e; 16x8 at 0x080160c0.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x8_734", "Animated segment 04", "Animations / Portrait", 16, 8, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x080160c0 } } },
    // Native HW 0x313a: draw call 0x08006c8e; 13x8 at 0x080160d0.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_13x8_735", "Animated segment 05", "Animations / Portrait", 13, 8, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x080160d0 } } },
    // Native HW 0x313a: draw call 0x08006c8e; 12x8 at 0x080160e0.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x8_736", "Animated segment 06", "Animations / Portrait", 12, 8, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x080160e0 } } },
    // Native HW 0x313a: draw call 0x08006c8e; 16x8 at 0x080160f0.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x8_737", "Animated segment 07", "Animations / Portrait", 16, 8, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x080160f0 } } },
    // Native HW 0x313a: draw call 0x08006c8e; 15x7 at 0x08016100.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_738", "Animated segment 08", "Animations / Portrait", 15, 7, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x08016100 } } },
    // Native HW 0x313a: draw call 0x08006c8e; 15x7 at 0x0801610e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_739", "Animated segment 09", "Animations / Portrait", 15, 7, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x0801610e } } },
    // Native HW 0x313a: draw call 0x08006c8e; 15x6 at 0x0801611c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x6_740", "Animated segment 10", "Animations / Portrait", 15, 6, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x0801611c } } },
    // Native HW 0x313a: draw call 0x08006c8e; 15x4 at 0x08016128.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x4_741", "Animated segment 11", "Animations / Portrait", 15, 4, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x08016128 } } },
    // Native HW 0x313a: draw call 0x08006c8e; 3x6 at 0x08016130.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_3x6_742", "Animated line 01", "Animations / Portrait", 3, 6, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x08016130 } } },
    // Native HW 0x313a: draw call 0x08006c8e; 7x12 at 0x08016136.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x12_743", "Animated line 02", "Animations / Portrait", 7, 12, 1, 0x313a, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x313a, 0x08016136 } } },
    // Native HW 0x3134: draw call 0x0800a0d0; 2x3 at 0x08012d3b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_744", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08012d3b } } },
    // Native HW 0x3134: draw call 0x0800a3c0; 2x3 at 0x08012d41.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_745", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08012d41 } } },
    // Native HW 0x3134: draw call 0x0800a5fc; 2x3 at 0x08012d44.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_746", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08012d44 } } },
    // Native HW 0x3134: draw call 0x0800a1c4; 2x3 at 0x08012d47.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_747", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08012d47 } } },
    // Native HW 0x3134: draw call 0x08009fba; 15x2 at 0x08012d51.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x2_748", "Decimal separator (landscape)", "Units", 15, 2, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08012d51 } } },
    // Native HW 0x3134: draw call 0x0800a666; 14x2 at 0x08012d59.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x2_749", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08012d59 } } },
    // Native HW 0x3134: draw call 0x0800a232; 14x2 at 0x08012d5d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x2_750", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08012d5d } } },
    // Native HW 0x3134: draw call 0x08009f30; 6x12 at 0x08013c6f.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_6x12_751", "Decimal separator (landscape)", "Units", 6, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "", { { 0x3134, 0x08013c6f } } },
    // Native HW 0x3134: draw call 0x0800a0ba; 15x21 at 0x08013d53.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x21_752", "Mode R panel pointer (landscape)", "Controls", 15, 21, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08013d53 } } },
    // Native HW 0x3134: draw call 0x0800a096; 15x10 at 0x08013d7d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x10_753", "Unit Ohm (landscape)", "Units", 15, 10, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x08013d7d } } },
    // Native HW 0x3134: draw call 0x0800a5e4; 15x21 at 0x08014596.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x21_754", "Voltage panel pointer (landscape)", "Controls", 15, 21, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x08014596 } } },
    // Native HW 0x3134: draw call 0x0800a702; 14x10 at 0x080145c0.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x10_755", "Unit V (landscape)", "Units", 14, 10, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x080145c0 } } },
    // Native HW 0x3134: draw call 0x0800b97a; 56x29 at 0x08014f36.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_56x29_756", "Unit Ohm panel (landscape)", "Units", 56, 29, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "", { { 0x3134, 0x08014f36 } } },
    // Native HW 0x3134: draw call 0x0800c094; 12x18 at 0x0801558b.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x18_757", "Panel corner top right", "Controls", 12, 18, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "", { { 0x3134, 0x0801558b } } },
    // Native HW 0x3134: draw call 0x0800c112; 2x14 at 0x080155af.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x14_758", "Decimal separator", "Units", 2, 14, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x080155af } } },
    // Native HW 0x3134: draw call 0x0800c428; 2x14 at 0x080155e1.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x14_759", "Decimal separator", "Units", 2, 14, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3134, 0x080155e1 } } },
    // Native HW 0x3134: draw call 0x080070be; 30x16 at 0x080164aa.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_30x16_760", "Mode P", "Labels", 30, 16, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x080164aa } } },
    // Native HW 0x3134: draw call 0x08007090; 15x6 at 0x080164f2.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x6_761", "Animated segment 01", "Animations / Portrait", 15, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080164f2 } } },
    // Native HW 0x3134: draw call 0x08007090; 15x7 at 0x080164fe.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_762", "Animated segment 02", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080164fe } } },
    // Native HW 0x3134: draw call 0x08007090; 15x7 at 0x0801650c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_763", "Animated segment 03", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801650c } } },
    // Native HW 0x3134: draw call 0x08007090; 16x8 at 0x0801651a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x8_764", "Animated segment 04", "Animations / Portrait", 16, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801651a } } },
    // Native HW 0x3134: draw call 0x08007090; 13x8 at 0x0801652a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_13x8_765", "Animated segment 05", "Animations / Portrait", 13, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801652a } } },
    // Native HW 0x3134: draw call 0x08007090; 12x8 at 0x0801653a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x8_766", "Animated segment 06", "Animations / Portrait", 12, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801653a } } },
    // Native HW 0x3134: draw call 0x08007090; 16x8 at 0x0801654a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x8_767", "Animated segment 07", "Animations / Portrait", 16, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801654a } } },
    // Native HW 0x3134: draw call 0x08007090; 15x7 at 0x0801655a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_768", "Animated segment 08", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801655a } } },
    // Native HW 0x3134: draw call 0x08007090; 15x7 at 0x08016568.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_769", "Animated segment 09", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016568 } } },
    // Native HW 0x3134: draw call 0x08007090; 15x6 at 0x08016576.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x6_770", "Animated segment 10", "Animations / Portrait", 15, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016576 } } },
    // Native HW 0x3134: draw call 0x08007090; 15x4 at 0x08016582.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x4_771", "Animated segment 11", "Animations / Portrait", 15, 4, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016582 } } },
    // Native HW 0x3134: draw call 0x08007090; 3x6 at 0x0801658a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_3x6_772", "Animated line 01", "Animations / Portrait", 3, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801658a } } },
    // Native HW 0x3134: draw call 0x08007090; 7x12 at 0x08016590.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x12_773", "Animated line 02", "Animations / Portrait", 7, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016590 } } },
    // Native HW 0x3134: draw call 0x08007090; 11x17 at 0x0801659c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_11x17_774", "Animated line 03", "Animations / Portrait", 11, 17, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801659c } } },
    // Native HW 0x3134: draw call 0x08007090; 14x20 at 0x080165be.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x20_775", "Animated line 04", "Animations / Portrait", 14, 20, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080165be } } },
    // Native HW 0x3134: draw call 0x08007090; 29x6 at 0x080165e6.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_29x6_776", "Animated line 05", "Animations / Portrait", 29, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080165e6 } } },
    // Native HW 0x3134: draw call 0x08007090; 14x4 at 0x080165fe.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x4_777", "Animated line 06", "Animations / Portrait", 14, 4, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080165fe } } },
    // Native HW 0x3134: draw call 0x08007090; 11x17 at 0x08016606.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_11x17_778", "Animated line 07", "Animations / Portrait", 11, 17, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016606 } } },
    // Native HW 0x3134: draw call 0x08007090; 7x12 at 0x08016628.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x12_779", "Animated line 08", "Animations / Portrait", 7, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016628 } } },
    // Native HW 0x3134: draw call 0x08007090; 3x6 at 0x08016634.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_3x6_780", "Animated line 09", "Animations / Portrait", 3, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016634 } } },
    // Native HW 0x3134: draw call 0x08006fa2; 8x3 at 0x0801663a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x3_781", "Animated line 10", "Animations / Portrait", 8, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3134, 0x0801663a } } },
    // Native HW 0x3134: draw call 0x08006f74; 64x15 at 0x0801663d.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_64x15_782", "Logo SUPER", "Logos", 64, 15, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801663d } } },
    // Native HW 0x3134: draw call 0x08006f74; 8x3 at 0x080166b5.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x3_783", "Animated line 01", "Animations / Portrait", 8, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080166b5 } } },
    // Native HW 0x3134: draw call 0x08006fdc; 14x4 at 0x080166b8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x4_784", "Animation segment", "Animations", 14, 4, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3134, 0x080166b8 } } },
    // Native HW 0x3134: draw call 0x08006f74; 15x6 at 0x080166c0.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x6_785", "Animated segment 01", "Animations / Portrait", 15, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080166c0 } } },
    // Native HW 0x3134: draw call 0x08006f74; 15x7 at 0x080166cc.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_786", "Animated segment 02", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080166cc } } },
    // Native HW 0x3134: draw call 0x08006f74; 15x7 at 0x080166da.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_787", "Animated segment 03", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080166da } } },
    // Native HW 0x3134: draw call 0x08006f74; 16x8 at 0x080166e8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x8_788", "Animated segment 04", "Animations / Portrait", 16, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080166e8 } } },
    // Native HW 0x3134: draw call 0x08006f74; 13x8 at 0x080166f8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_13x8_789", "Animated segment 05", "Animations / Portrait", 13, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080166f8 } } },
    // Native HW 0x3134: draw call 0x08006f74; 12x8 at 0x08016708.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x8_790", "Animated segment 06", "Animations / Portrait", 12, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016708 } } },
    // Native HW 0x3134: draw call 0x08006f74; 16x8 at 0x08016718.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x8_791", "Animated segment 07", "Animations / Portrait", 16, 8, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016718 } } },
    // Native HW 0x3134: draw call 0x08006f74; 15x7 at 0x08016728.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_792", "Animated segment 08", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016728 } } },
    // Native HW 0x3134: draw call 0x08006f74; 15x7 at 0x08016736.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_793", "Animated segment 09", "Animations / Portrait", 15, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016736 } } },
    // Native HW 0x3134: draw call 0x08006f74; 15x6 at 0x08016744.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x6_794", "Animated segment 10", "Animations / Portrait", 15, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016744 } } },
    // Native HW 0x3134: draw call 0x08006f74; 15x4 at 0x08016750.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x4_795", "Animated segment 11", "Animations / Portrait", 15, 4, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016750 } } },
    // Native HW 0x3134: draw call 0x08006f74; 3x6 at 0x08016758.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_3x6_796", "Animated line 01", "Animations / Portrait", 3, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016758 } } },
    // Native HW 0x3134: draw call 0x08006f74; 7x12 at 0x0801675e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x12_797", "Animated line 02", "Animations / Portrait", 7, 12, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801675e } } },
    // Native HW 0x3134: draw call 0x08006f74; 8x14 at 0x0801676a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_8x14_798", "Animated line 16", "Animations / Portrait", 8, 14, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801676a } } },
    // Native HW 0x3134: draw call 0x08006f74; 3x2 at 0x08016778.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_3x2_799", "Animated line 17", "Animations / Portrait", 3, 2, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016778 } } },
    // Native HW 0x3134: draw call 0x08006f74; 14x5 at 0x0801677a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x5_800", "Animated line 18", "Animations / Portrait", 14, 5, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x0801677a } } },
    // Native HW 0x3134: draw call 0x08006f74; 29x7 at 0x08016784.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_29x7_801", "Animated line 19", "Animations / Portrait", 29, 7, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x08016784 } } },
    // Native HW 0x3134: draw call 0x08006f74; 14x5 at 0x080167a0.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x5_802", "Animated line 20", "Animations / Portrait", 14, 5, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080167a0 } } },
    // Native HW 0x3134: draw call 0x08006f74; 4x2 at 0x080167aa.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_4x2_803", "Animated line 21", "Animations / Portrait", 4, 2, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080167aa } } },
    // Native HW 0x3134: draw call 0x08006f74; 7x14 at 0x080167ac.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x14_804", "Animated line 22", "Animations / Portrait", 7, 14, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080167ac } } },
    // Native HW 0x3134: draw call 0x08006f74; 7x6 at 0x080167ba.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x6_805", "Animated line 23", "Animations / Portrait", 7, 6, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080167ba } } },
    // Native HW 0x3134: draw call 0x08006f74; 10x3 at 0x080167c0.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_10x3_806", "Animated line 24", "Animations / Portrait", 10, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080167c0 } } },
    // Native HW 0x3134: draw call 0x08006f74; 9x3 at 0x080167c6.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_9x3_807", "Animated line 25", "Animations / Portrait", 9, 3, 1, 0x3134, AssetFormat::Mask1Bit, "", 0, "native_muted_red", { { 0x3134, 0x080167c6 } } },
    // Native HW 0x311c: draw call 0x0800d226; 2x3 at 0x080149af.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_808", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x311c, 0x080149af } } },
    // Native HW 0x311c: draw call 0x0800d5b0; 2x3 at 0x080149b5.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_809", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x311c, 0x080149b5 } } },
    // Native HW 0x311c: draw call 0x0800d7f2; 2x3 at 0x080149b8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_810", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x311c, 0x080149b8 } } },
    // Native HW 0x311c: draw call 0x0800d3b2; 2x3 at 0x080149bb.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x3_811", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x311c, 0x080149bb } } },
    // Native HW 0x311c: draw call 0x0800d0fe; 15x2 at 0x080149c5.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x2_812", "Decimal separator (landscape)", "Units", 15, 2, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x311c, 0x080149c5 } } },
    // Native HW 0x311c: draw call 0x0800d85c; 14x2 at 0x080149cd.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x2_813", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x311c, 0x080149cd } } },
    // Native HW 0x311c: draw call 0x0800d41a; 14x2 at 0x080149d1.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x2_814", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x311c, 0x080149d1 } } },
    // Native HW 0x311c: draw call 0x0800d206; 15x21 at 0x080159c7.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x21_815", "Mode R panel pointer (landscape)", "Controls", 15, 21, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x311c, 0x080159c7 } } },
    // Native HW 0x311c: draw call 0x0800d1de; 15x10 at 0x080159f1.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x10_816", "Unit Ohm (landscape)", "Units", 15, 10, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x311c, 0x080159f1 } } },
    // Native HW 0x311c: draw call 0x0800d7da; 15x21 at 0x0801620a.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x21_817", "Voltage panel pointer (landscape)", "Controls", 15, 21, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x311c, 0x0801620a } } },
    // Native HW 0x311c: draw call 0x0800d8f2; 14x10 at 0x08016234.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x10_818", "Unit V (landscape)", "Units", 14, 10, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x311c, 0x08016234 } } },
    // Native HW 0x311c: draw call 0x0800fb0e; 2x14 at 0x08017223.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x14_819", "Decimal separator", "Units", 2, 14, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x311c, 0x08017223 } } },
    // Native HW 0x311c: draw call 0x0800fe32; 2x14 at 0x08017255.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_2x14_820", "Decimal separator", "Units", 2, 14, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x311c, 0x08017255 } } },
    // Native HW 0x311c: draw call 0x0800a096; 14x4 at 0x0801832c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_14x4_821", "Animation segment", "Animations", 14, 4, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x0801832c } } },
    // Native HW 0x311c: draw call 0x0800a096; 15x6 at 0x08018334.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x6_822", "Animated segment 01", "Animations / Portrait", 15, 6, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x08018334 } } },
    // Native HW 0x311c: draw call 0x0800a096; 15x7 at 0x08018340.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_823", "Animated segment 02", "Animations / Portrait", 15, 7, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x08018340 } } },
    // Native HW 0x311c: draw call 0x0800a096; 15x7 at 0x0801834e.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_824", "Animated segment 03", "Animations / Portrait", 15, 7, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x0801834e } } },
    // Native HW 0x311c: draw call 0x0800a096; 16x8 at 0x0801835c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x8_825", "Animated segment 04", "Animations / Portrait", 16, 8, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x0801835c } } },
    // Native HW 0x311c: draw call 0x0800a096; 13x8 at 0x0801836c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_13x8_826", "Animated segment 05", "Animations / Portrait", 13, 8, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x0801836c } } },
    // Native HW 0x311c: draw call 0x0800a096; 12x8 at 0x0801837c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_12x8_827", "Animated segment 06", "Animations / Portrait", 12, 8, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x0801837c } } },
    // Native HW 0x311c: draw call 0x0800a096; 16x8 at 0x0801838c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_16x8_828", "Animated segment 07", "Animations / Portrait", 16, 8, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x0801838c } } },
    // Native HW 0x311c: draw call 0x0800a096; 15x7 at 0x0801839c.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_829", "Animated segment 08", "Animations / Portrait", 15, 7, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x0801839c } } },
    // Native HW 0x311c: draw call 0x0800a096; 15x7 at 0x080183aa.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x7_830", "Animated segment 09", "Animations / Portrait", 15, 7, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x080183aa } } },
    // Native HW 0x311c: draw call 0x0800a096; 15x6 at 0x080183b8.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x6_831", "Animated segment 10", "Animations / Portrait", 15, 6, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x080183b8 } } },
    // Native HW 0x311c: draw call 0x0800a096; 15x4 at 0x080183c4.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_15x4_832", "Animated segment 11", "Animations / Portrait", 15, 4, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x080183c4 } } },
    // Native HW 0x311c: draw call 0x0800a096; 3x6 at 0x080183cc.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_3x6_833", "Animated line 01", "Animations / Portrait", 3, 6, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x080183cc } } },
    // Native HW 0x311c: draw call 0x0800a096; 7x12 at 0x080183d2.
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_7x12_834", "Animated line 02", "Animations / Portrait", 7, 12, 1, 0x311c, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x311c, 0x080183d2 } } },
    // Native HW 0x3021: draw call 0x08008e2e; 2x3 at 0x0800f4e4.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_2x3_835", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800f4e4 } } },
    // Native HW 0x3021: draw call 0x08008c0a; 2x3 at 0x0800f4e7.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_2x3_836", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800f4e7 } } },
    // Native HW 0x3021: draw call 0x080090be; 2x3 at 0x0800f4ea.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_2x3_837", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800f4ea } } },
    // Native HW 0x3021: draw call 0x08008eec; 2x3 at 0x0800f4ed.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_2x3_838", "Decimal separator (landscape)", "Units", 2, 3, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800f4ed } } },
    // Native HW 0x3021: draw call 0x08008d18; 15x2 at 0x0800f4f0.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_15x2_839", "Decimal separator (landscape)", "Units", 15, 2, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x0800f4f0 } } },
    // Native HW 0x3021: draw call 0x08009128; 14x2 at 0x0800f4f4.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_14x2_840", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x0800f4f4 } } },
    // Native HW 0x3021: draw call 0x08008f5a; 14x2 at 0x0800f4f8.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_14x2_841", "Decimal separator (landscape)", "Units", 14, 2, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x0800f4f8 } } },
    // Native HW 0x3021: draw call 0x080065f4; 5x6 at 0x0800f50e.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_5x6_842", "Panel corner top left", "Controls", 5, 6, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800f50e } } },
    // Native HW 0x3021: draw call 0x080068a2; 5x6 at 0x0800f514.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_5x6_843", "Panel corner bottom right", "Controls", 5, 6, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800f514 } } },
    // Native HW 0x3021: draw call 0x08006ab8; 2x12 at 0x0800f534.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_2x12_844", "Decimal separator", "Units", 2, 12, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x0800f534 } } },
    // Native HW 0x3021: draw call 0x08006916; 2x16 at 0x0800f540.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_2x16_845", "Decimal separator", "Units", 2, 16, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x0800f540 } } },
    // Native HW 0x3021: draw call 0x08006206; 32x10 at 0x0800f5b3.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_32x10_846", "Header DRAG", "Labels", 32, 10, 1, 0x3021, AssetFormat::Rgb565, "", 0, "", { { 0x3021, 0x0800f5b3 } } },
    // Native HW 0x3021: draw call 0x08006cd2; 2x10 at 0x0800f833.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_2x10_847", "Battery right edge", "Battery", 2, 10, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x0800f833 } } },
    // Native HW 0x3021: draw call 0x08006498; 128x6 at 0x0800f83d.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_128x6_848", "Puff panel top edge", "Controls", 128, 6, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x0800f83d } } },
    // Native HW 0x3021: draw call 0x080064b2; 50x16 at 0x0800f89d.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_50x16_849", "Label Puff panel", "Labels", 50, 16, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800f89d } } },
    // Native HW 0x3021: draw call 0x0800660a; 123x6 at 0x0800f90d.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_123x6_850", "Puff panel bottom edge", "Controls", 123, 6, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x0800f90d } } },
    // Native HW 0x3021: draw call 0x0800668e; 63x7 at 0x0800f96d.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_63x7_851", "Puff panel divider", "Controls", 63, 7, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800f96d } } },
    // Native HW 0x3021: draw call 0x080066a8; 8x12 at 0x0800f9a5.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_8x12_852", "Unit V panel", "Units", 8, 12, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800f9a5 } } },
    // Native HW 0x3021: draw call 0x0800679c; 7x12 at 0x0800f9b1.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_7x12_853", "Unit Ohm", "Units", 7, 12, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x0800f9b1 } } },
    // Native HW 0x3021: draw call 0x080067b2; 19x12 at 0x0800f9bd.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_19x12_854", "Unit W panel", "Units", 19, 12, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800f9bd } } },
    // Native HW 0x3021: draw call 0x080067d6; 63x3 at 0x0800f9e1.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_63x3_855", "Resistance panel top edge", "Controls", 63, 3, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800f9e1 } } },
    // Native HW 0x3021: draw call 0x080067fa; 63x5 at 0x0800f9f9.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_63x5_856", "Time panel top edge", "Controls", 63, 5, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800f9f9 } } },
    // Native HW 0x3021: draw call 0x08006a5c; 65x16 at 0x0800fa21.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_65x16_857", "Voltage panel top edge", "Controls", 65, 16, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800fa21 } } },
    // Native HW 0x3021: draw call 0x08006a74; 20x12 at 0x0800fab1.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_20x12_858", "Unit Ohm panel", "Units", 20, 12, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800fab1 } } },
    // Native HW 0x3021: draw call 0x08006b4e; 7x12 at 0x0800fad5.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_7x12_859", "Unit V", "Units", 7, 12, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x0800fad5 } } },
    // Native HW 0x3021: draw call 0x08006b70; 8x12 at 0x0800fae1.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_8x12_860", "Decimal separator", "Units", 8, 12, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800fae1 } } },
    // Native HW 0x3021: draw call 0x08006b8e; 65x7 at 0x0800faed.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_65x7_861", "Resistance panel bottom edge", "Controls", 65, 7, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0800faed } } },
    // Native HW 0x3021: draw call 0x0800688c; 123x6 at 0x0800fb2c.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_123x6_862", "Voltage panel bottom edge", "Controls", 123, 6, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x0800fb2c } } },
    // Native HW 0x3021: draw call 0x080069ee; 128x6 at 0x0800fb8c.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_128x6_863", "Time panel bottom edge", "Controls", 128, 6, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x0800fb8c } } },
    // Native HW 0x3021: draw call 0x080086d0; 12x32 at 0x0800fd04.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_12x32_864", "Logo DRAG (landscape)", "Logos", 12, 32, 1, 0x3021, AssetFormat::Rgb565, "", 0, "", { { 0x3021, 0x0800fd04 } } },
    // Native HW 0x3021: draw call 0x0800923a; 10x79 at 0x08010004.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_10x79_865", "Header side edge (landscape)", "Controls", 10, 79, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x08010004 } } },
    // Native HW 0x3021: draw call 0x0800925e; 7x13 at 0x080100a2.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_7x13_866", "Unit Ohm (landscape)", "Units", 7, 13, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080100a2 } } },
    // Native HW 0x3021: draw call 0x08009280; 7x13 at 0x080100af.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_7x13_867", "Unit S (landscape)", "Units", 7, 13, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080100af } } },
    // Native HW 0x3021: draw call 0x08008ca4; 23x72 at 0x080100bc.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_23x72_868", "Mode R panel (landscape)", "Controls", 23, 72, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080100bc } } },
    // Native HW 0x3021: draw call 0x08008e18; 15x21 at 0x08010194.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_15x21_869", "Mode R panel pointer (landscape)", "Controls", 15, 21, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x08010194 } } },
    // Native HW 0x3021: draw call 0x08008df4; 15x10 at 0x080101be.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_15x10_870", "Unit Ohm (landscape)", "Units", 15, 10, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x080101be } } },
    // Native HW 0x3021: draw call 0x0800892e; 40x7 at 0x080101d2.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_40x7_871", "Puff panel side edge (landscape)", "Controls", 40, 7, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080101d2 } } },
    // Native HW 0x3021: draw call 0x08008900; 18x16 at 0x080101f5.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_18x16_872", "Mode panel (landscape)", "Controls", 18, 16, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080101f5 } } },
    // Native HW 0x3021: draw call 0x08008952; 18x16 at 0x08010465.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_18x16_873", "Mode panel fill (landscape)", "Controls", 18, 16, 1, 0x3021, AssetFormat::Rgb565, "", 0, "", { { 0x3021, 0x08010465 } } },
    // Native HW 0x3021: draw call 0x08008aa4; 23x72 at 0x080106a5.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_23x72_874", "Puff panel (landscape)", "Controls", 23, 72, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080106a5 } } },
    // Native HW 0x3021: draw call 0x08008bf4; 15x21 at 0x0801077d.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_15x21_875", "Puff panel pointer (landscape)", "Controls", 15, 21, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0801077d } } },
    // Native HW 0x3021: draw call 0x08008a34; 13x12 at 0x080107a7.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_13x12_876", "Unit S (landscape)", "Units", 13, 12, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x080107a7 } } },
    // Native HW 0x3021: draw call 0x08009092; 23x69 at 0x080107bf.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_23x69_877", "Voltage panel (landscape)", "Controls", 23, 69, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080107bf } } },
    // Native HW 0x3021: draw call 0x080090a6; 15x21 at 0x0801088e.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_15x21_878", "Voltage panel pointer (landscape)", "Controls", 15, 21, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0801088e } } },
    // Native HW 0x3021: draw call 0x080091c4; 14x10 at 0x080108b8.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_14x10_879", "Unit V (landscape)", "Units", 14, 10, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x080108b8 } } },
    // Native HW 0x3021: draw call 0x08008ed6; 15x21 at 0x080108cc.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_15x21_880", "Time panel pointer (landscape)", "Controls", 15, 21, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080108cc } } },
    // Native HW 0x3021: draw call 0x08008eae; 23x69 at 0x080108f6.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_23x69_881", "Time panel (landscape)", "Controls", 23, 69, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080108f6 } } },
    // Native HW 0x3021: draw call 0x08008ff6; 14x10 at 0x080109c5.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_14x10_882", "Unit W (landscape)", "Units", 14, 10, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x080109c5 } } },
    // Native HW 0x3021: draw call 0x080087be; 40x23 at 0x080109d9.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_40x23_883", "Mode S panel (landscape)", "Controls", 40, 23, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080109d9 } } },
    // Native HW 0x3021: draw call 0x08005b6a; 58x12 at 0x08010bb8.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_58x12_884", "Label Theme", "Labels", 58, 12, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x08010bb8 } } },
    // Native HW 0x3021: draw call 0x08005c70; 10x42 at 0x08010c18.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_10x42_885", "Theme panel left edge", "Controls", 10, 42, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "", { { 0x3021, 0x08010c18 } } },
    // Native HW 0x3021: draw call 0x08005c8e; 10x42 at 0x08010c6c.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_10x42_886", "Theme panel right edge", "Controls", 10, 42, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "", { { 0x3021, 0x08010c6c } } },
    // Native HW 0x3021: draw call 0x08005c10; 94x42 at 0x08010cc0.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_94x42_887", "Theme IRON panel", "Controls", 94, 42, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3021, 0x08010cc0 } } },
    // Native HW 0x3021: draw call 0x08005c28; 30x8 at 0x08010eb8.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_30x8_888", "Label IRON", "Labels", 30, 8, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x08010eb8 } } },
    // Native HW 0x3021: draw call 0x08005cb6; 94x42 at 0x08010ed8.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_94x42_889", "Theme CORE panel", "Controls", 94, 42, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_green", { { 0x3021, 0x08010ed8 } } },
    // Native HW 0x3021: draw call 0x08005cce; 34x8 at 0x080110d0.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_34x8_890", "Label CORE", "Labels", 34, 8, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x080110d0 } } },
    // Native HW 0x3021: draw call 0x0800af08; 90x15 at 0x080110f8.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_90x15_891", "Logo VOOPOO (chip information)", "Logos", 90, 15, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080110f8 } } },
    // Native HW 0x3021: draw call 0x0800af4a; 102x87 at 0x080111ac.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_102x87_892", "Chip ID panel", "Labels", 102, 87, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080111ac } } },
    // Native HW 0x3021: draw call 0x0800b030; 102x63 at 0x08011617.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_102x63_893", "Level Code panel", "Labels", 102, 63, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x08011617 } } },
    // Native HW 0x3021: draw call 0x080063e6; 30x16 at 0x0801194a.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_30x16_894", "Mode P", "Labels", 30, 16, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x0801194a } } },
    // Native HW 0x3021: draw call 0x08006418; 14x4 at 0x0801198a.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_14x4_895", "Animation segment", "Animations", 14, 4, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x0801198a } } },
    // Native HW 0x3021: draw call 0x080063b8; 15x6 at 0x08011992.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_15x6_896", "Animated segment 01", "Animations / Portrait", 15, 6, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x08011992 } } },
    // Native HW 0x3021: draw call 0x080063b8; 15x7 at 0x0801199e.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_15x7_897", "Animated segment 02", "Animations / Portrait", 15, 7, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x0801199e } } },
    // Native HW 0x3021: draw call 0x080063b8; 15x7 at 0x080119ac.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_15x7_898", "Animated segment 03", "Animations / Portrait", 15, 7, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x080119ac } } },
    // Native HW 0x3021: draw call 0x080063b8; 16x8 at 0x080119ba.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_16x8_899", "Animated segment 04", "Animations / Portrait", 16, 8, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x080119ba } } },
    // Native HW 0x3021: draw call 0x080063b8; 13x8 at 0x080119ca.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_13x8_900", "Animated segment 05", "Animations / Portrait", 13, 8, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x080119ca } } },
    // Native HW 0x3021: draw call 0x080063b8; 12x8 at 0x080119da.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_12x8_901", "Animated segment 06", "Animations / Portrait", 12, 8, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x080119da } } },
    // Native HW 0x3021: draw call 0x080063b8; 16x8 at 0x080119ea.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_16x8_902", "Animated segment 07", "Animations / Portrait", 16, 8, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x080119ea } } },
    // Native HW 0x3021: draw call 0x080063b8; 15x7 at 0x080119fa.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_15x7_903", "Animated segment 08", "Animations / Portrait", 15, 7, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x080119fa } } },
    // Native HW 0x3021: draw call 0x080063b8; 15x7 at 0x08011a08.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_15x7_904", "Animated segment 09", "Animations / Portrait", 15, 7, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x08011a08 } } },
    // Native HW 0x3021: draw call 0x080063b8; 15x6 at 0x08011a16.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_15x6_905", "Animated segment 10", "Animations / Portrait", 15, 6, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x08011a16 } } },
    // Native HW 0x3021: draw call 0x080063b8; 15x4 at 0x08011a22.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_15x4_906", "Animated segment 11", "Animations / Portrait", 15, 4, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x08011a22 } } },
    // Native HW 0x3021: draw call 0x080063b8; 3x6 at 0x08011a2a.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_3x6_907", "Animated line 01", "Animations / Portrait", 3, 6, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x08011a2a } } },
    // Native HW 0x3021: draw call 0x080063b8; 7x12 at 0x08011a30.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_7x12_908", "Animated line 02", "Animations / Portrait", 7, 12, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x08011a30 } } },
    // Native HW 0x3021: draw call 0x080063b8; 11x17 at 0x08011a3c.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_11x17_909", "Animated line 03", "Animations / Portrait", 11, 17, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x08011a3c } } },
    // Native HW 0x3021: draw call 0x080063b8; 14x20 at 0x08011a5e.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_14x20_910", "Animated line 04", "Animations / Portrait", 14, 20, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x08011a5e } } },
    // Native HW 0x3021: draw call 0x080063b8; 29x6 at 0x08011a86.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_29x6_911", "Animated line 05", "Animations / Portrait", 29, 6, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x08011a86 } } },
    // Native HW 0x3021: draw call 0x080063b8; 14x4 at 0x08011a9e.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_14x4_912", "Animated line 06", "Animations / Portrait", 14, 4, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x08011a9e } } },
    // Native HW 0x3021: draw call 0x080063b8; 11x17 at 0x08011aa6.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_11x17_913", "Animated line 07", "Animations / Portrait", 11, 17, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x08011aa6 } } },
    // Native HW 0x3021: draw call 0x080063b8; 7x12 at 0x08011ac8.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_7x12_914", "Animated line 08", "Animations / Portrait", 7, 12, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x08011ac8 } } },
    // Native HW 0x3021: draw call 0x080063b8; 3x6 at 0x08011ad4.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_3x6_915", "Animated line 09", "Animations / Portrait", 3, 6, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_max_accent", { { 0x3021, 0x08011ad4 } } },
    // Native HW 0x3021: draw call 0x08008870; 12x15 at 0x08011ada.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_12x15_916", "Animated block 01", "Animations / Landscape", 12, 15, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011ada } } },
    // Native HW 0x3021: draw call 0x08008870; 14x13 at 0x08011af8.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_14x13_917", "Animated block 02", "Animations / Landscape", 14, 13, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011af8 } } },
    // Native HW 0x3021: draw call 0x08008870; 14x15 at 0x08011b12.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_14x15_918", "Animated block 03", "Animations / Landscape", 14, 15, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011b12 } } },
    // Native HW 0x3021: draw call 0x08008870; 14x12 at 0x08011b30.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_14x12_919", "Animated block 04", "Animations / Landscape", 14, 12, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011b30 } } },
    // Native HW 0x3021: draw call 0x08008870; 14x13 at 0x08011b48.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_14x13_920", "Animated block 05", "Animations / Landscape", 14, 13, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011b48 } } },
    // Native HW 0x3021: draw call 0x08008870; 12x15 at 0x08011b62.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_12x15_921", "Animated block 06", "Animations / Landscape", 12, 15, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011b62 } } },
    // Native HW 0x3021: draw call 0x08008870; 12x15 at 0x08011b80.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_12x15_922", "Animated block 07", "Animations / Landscape", 12, 15, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011b80 } } },
    // Native HW 0x3021: draw call 0x08008870; 16x15 at 0x08011b9e.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_16x15_923", "Animated block 08", "Animations / Landscape", 16, 15, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011b9e } } },
    // Native HW 0x3021: draw call 0x08008870; 12x15 at 0x08011bbc.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_12x15_924", "Animated block 09", "Animations / Landscape", 12, 15, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011bbc } } },
    // Native HW 0x3021: draw call 0x08008870; 12x15 at 0x08011bda.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_12x15_925", "Animated block 10", "Animations / Landscape", 12, 15, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011bda } } },
    // Native HW 0x3021: draw call 0x08008870; 14x13 at 0x08011bf8.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_14x13_926", "Animated block 11", "Animations / Landscape", 14, 13, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011bf8 } } },
    // Native HW 0x3021: draw call 0x08008870; 10x12 at 0x08011c12.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_10x12_927", "Animated block 12", "Animations / Landscape", 10, 12, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011c12 } } },
    // Native HW 0x3021: draw call 0x08008870; 10x15 at 0x08011c2a.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_10x15_928", "Animated block 13", "Animations / Landscape", 10, 15, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011c2a } } },
    // Native HW 0x3021: draw call 0x08008870; 14x13 at 0x08011c48.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_14x13_929", "Animated block 14", "Animations / Landscape", 14, 13, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011c48 } } },
    // Native HW 0x3021: draw call 0x08008870; 12x15 at 0x08011c62.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_12x15_930", "Animated block 15", "Animations / Landscape", 12, 15, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011c62 } } },
    // Native HW 0x3021: draw call 0x08008870; 12x14 at 0x08011c80.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_12x14_931", "Animated block 16", "Animations / Landscape", 12, 14, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011c80 } } },
    // Native HW 0x3021: draw call 0x08008870; 16x14 at 0x08011c9c.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_16x14_932", "Animated block 17", "Animations / Landscape", 16, 14, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011c9c } } },
    // Native HW 0x3021: draw call 0x08008870; 12x14 at 0x08011cb8.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_12x14_933", "Animated block 18", "Animations / Landscape", 12, 14, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_silver", { { 0x3021, 0x08011cb8 } } },
    // Native HW 0x3021: draw call 0x08005024; 59x29 at 0x08011fac.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_59x29_934", "OFF", "Messages", 59, 29, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x08011fac } } },
    // Native HW 0x3021: draw call 0x08004fcc; 117x20 at 0x08012094.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_117x20_935", "Logo DRAG MAX (boot)", "Logos", 117, 20, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x08012094 } } },
    // Native HW 0x3021: draw call 0x08004fe4; 111x50 at 0x080121c0.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_111x50_936", "Logo GENE FAN (boot)", "Logos", 111, 50, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x080121c0 } } },
    // Native HW 0x3021: draw call 0x0800574a; 63x24 at 0x0801283c.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_63x24_937", "Battery gauge frame", "Battery", 63, 24, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x0801283c } } },
    // Native HW 0x3021: draw call 0x0800552a; 61x10 at 0x080128fc.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_61x10_938", "Charging", "Messages", 61, 10, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080128fc } } },
    // Native HW 0x3021: draw call 0x08005876; 13x16 at 0x0801294c.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_13x16_939", "Unit %", "Units", 13, 16, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x0801294c } } },
    // Native HW 0x3021: font selector 1; 6x10 at 0x0801296c.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "font_native_6x10_940", "Font 6x10", "Font 6x10", 6, 10, 11, 0x3021, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3021, 0x0801296c } } },
    // Native HW 0x3021: font selector 3; 7x13 at 0x080129da.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "font_native_7x13_941", "Font 7x13", "Font 7x13", 7, 13, 16, 0x3021, AssetFormat::Mask1Bit, "0123456789ABCDEF", 0, "digits_wattage", { { 0x3021, 0x080129da } } },
    // Native HW 0x3021: font selector 6; 19x40 at 0x08012aaa.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "font_native_19x40_942", "Font 19x40", "Font 19x40", 19, 40, 10, 0x3021, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3021, 0x08012aaa } } },
    // Native HW 0x3021: font selector 8; 30x64 at 0x08012f5a.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "font_native_30x64_943", "Font 30x64", "Font 30x64", 30, 64, 10, 0x3021, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3021, 0x08012f5a } } },
    // Native HW 0x3021: draw call 0x08005e10; 39x39 at 0x0801395a.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_39x39_944", "Icon Time Out", "Messages", 39, 39, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x3021, 0x0801395a } } },
    // Native HW 0x3021: draw call 0x08005e28; 77x58 at 0x08013a1d.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_77x58_945", "Time Out", "Messages", 77, 58, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x08013a1d } } },
    // Native HW 0x3021: draw call 0x08005af2; 30x53 at 0x08013c61.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_30x53_946", "Icon Atomizer", "Messages", 30, 53, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x3021, 0x08013c61 } } },
    // Native HW 0x3021: draw call 0x08005b0a; 77x58 at 0x08013d35.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_77x58_947", "Max Power", "Messages", 77, 58, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x08013d35 } } },
    // Native HW 0x3021: draw call 0x08005480; 62x41 at 0x08013f79.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_62x41_948", "Icon Atomizer Short", "Messages", 62, 41, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3021, 0x08013f79 } } },
    // Native HW 0x3021: draw call 0x08005498; 77x58 at 0x080140c1.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_77x58_949", "Atomizer Short", "Messages", 77, 58, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x080140c1 } } },
    // Native HW 0x3021: draw call 0x08005a24; 28x54 at 0x08014305.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_28x54_950", "Icon Check Battery", "Messages", 28, 54, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3021, 0x08014305 } } },
    // Native HW 0x3021: draw call 0x08005a3c; 77x58 at 0x080143dd.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_77x58_951", "Check Battery", "Messages", 77, 58, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x080143dd } } },
    // Native HW 0x3021: draw call 0x08005a8a; 28x53 at 0x08014621.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_28x53_952", "Icon Low Power", "Messages", 28, 53, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3021, 0x08014621 } } },
    // Native HW 0x3021: draw call 0x08005aa2; 77x58 at 0x080146f5.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_77x58_953", "Low Power", "Messages", 77, 58, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x080146f5 } } },
    // Native HW 0x3021: draw call 0x08005edc; 40x43 at 0x08014939.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_40x43_954", "Locked", "Messages", 40, 43, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3021, 0x08014939 } } },
    // Native HW 0x3021: draw call 0x08005ef4; 77x58 at 0x08014a10.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_77x58_955", "Press Fire & +", "Messages", 77, 58, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x08014a10 } } },
    // Native HW 0x3021: draw call 0x08005e76; 40x43 at 0x08014c54.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_40x43_956", "Unlocked", "Messages", 40, 43, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_lime", { { 0x3021, 0x08014c54 } } },
    // Native HW 0x3021: draw call 0x0800597a; 27x26 at 0x08014d2b.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_27x26_957", "Icon Check Atomizer", "Messages", 27, 26, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3021, 0x08014d2b } } },
    // Native HW 0x3021: draw call 0x08005992; 27x40 at 0x08014d93.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_27x40_958", "Atomizer silhouette", "Messages", 27, 40, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "accent_cyan", { { 0x3021, 0x08014d93 } } },
    // Native HW 0x3021: draw call 0x080059b8; 77x58 at 0x08014e33.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_77x58_959", "Check Atomizer", "Messages", 77, 58, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x08014e33 } } },
    // Native HW 0x3021: draw call 0x08005dac; 43x47 at 0x08015077.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_43x47_960", "Icon High Temp", "Messages", 43, 47, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "native_orange", { { 0x3021, 0x08015077 } } },
    // Native HW 0x3021: draw call 0x08005dc4; 77x58 at 0x08015191.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_77x58_961", "High Temp", "Messages", 77, 58, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x08015191 } } },
    // Native HW 0x3021: draw call 0x08005d46; 60x31 at 0x080153d5.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_60x31_962", "Icon Puff reset", "Messages", 60, 31, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "highlight_yellow", { { 0x3021, 0x080153d5 } } },
    // Native HW 0x3021: draw call 0x08005d5e; 77x58 at 0x080154cd.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_77x58_963", "Puff Cleared", "Messages", 77, 58, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x080154cd } } },
    // Native HW 0x3021: font selector 7; 11x20 at 0x08015711.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "font_native_11x20_964", "Font 11x20", "Font 11x20", 11, 20, 10, 0x3021, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3021, 0x08015711 } } },
    // Native HW 0x3021: draw call 0x08008626; 21x23 at 0x080158a1.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_21x23_965", "Locked", "Controls", 21, 23, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "accent_red", { { 0x3021, 0x080158a1 } } },
    // Native HW 0x3021: draw call 0x08008662; 21x22 at 0x08015970.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_21x22_966", "Unit W", "Units", 21, 22, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x08015970 } } },
    // Native HW 0x3021: font selector 4; 8x16 at 0x080159b2.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "font_native_8x16_967", "Font 8x16", "Font 8x16", 8, 16, 11, 0x3021, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3021, 0x080159b2 } } },
    // Native HW 0x3021: font selector 2; 6x12 at 0x08015a62.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "font_native_6x12_968", "Font 6x12", "Font 6x12", 6, 12, 11, 0x3021, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3021, 0x08015a62 } } },
    // Native HW 0x3021: draw call 0x080069aa; 10x16 at 0x08015ae6.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "bitmap_native_10x16_969", "Unit S", "Units", 10, 16, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x08015ae6 } } },
    // Native HW 0x3021: font selector 5; 9x18 at 0x08015b06.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "font_native_9x18_970", "Font 9x18", "Font 9x18", 9, 18, 10, 0x3021, AssetFormat::Mask1Bit, "0123456789", 0, "digits_wattage", { { 0x3021, 0x08015b06 } } },
    // Native HW 0x3021: font selector 9; 13x7 at 0x08015d46.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "font_native_13x7_971", "Font 13x7 / Landscape", "Font 13x7 / Landscape", 13, 7, 11, 0x3021, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x3021, 0x08015d46 } } },
    // Native HW 0x3021: font selector 11; 40x19 at 0x08015de0.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "font_native_40x19_972", "Font 40x19 / Landscape", "Font 40x19 / Landscape", 40, 19, 10, 0x3021, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x3021, 0x08015de0 } } },
    // Native HW 0x3021: font selector 10; 14x8 at 0x08016196.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "font_native_14x8_973", "Font 14x8 / Landscape", "Font 14x8 / Landscape", 14, 8, 11, 0x3021, AssetFormat::Mask1Bit, "0123456789", 3, "digits_wattage", { { 0x3021, 0x08016196 } } },
    // Native HW 0x3021: font selector 12; 40x10 at 0x08016246.
    { FirmwareEdition::Standard, FirmwareModel::DragMax, "symbol_native_40x10_974", "Bar with raised tip", "Special glyphs 40x10 / Landscape", 40, 10, 1, 0x3021, AssetFormat::Mask1Bit, "", 0, "digits_wattage", { { 0x3021, 0x08016246 } } },
    { FirmwareEdition::Factory, FirmwareModel::Drag3, "bitmap_native_30x182_975", "Logo VOOPOO (animated boot)", "Logos", 30, 182, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "", { { 0x3030, 0x08013342 } }, true },
    { FirmwareEdition::Tc, FirmwareModel::Drag3, "bitmap_native_30x182_976", "Logo VOOPOO (animated boot)", "Logos", 30, 182, 1, 0x3030, AssetFormat::Mask1Bit, "", 0, "", { { 0x3030, 0x08016396 } }, true },
};
static constexpr ColorSite kColorSites[] = {
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x1f16, 0xffff, 5, 0, 0x1f64, 0xfab8f002, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "highlight_yellow", 0x1fe4, 0xffe0, 2, 0, 0x1ff8, 0xfa6ef002, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x200a, 0xffff, 2, 0, 0x201e, 0xfa5bf002, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x2058, 0xffff, 5, 0, 0x206c, 0xfa34f002, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "accent_red", 0x24cc, 0xe800, 2, 0, 0x24e0, 0xfffaf001, 0x4268f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x24f2, 0xffff, 2, 0, 0x2506, 0xffe7f001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "highlight_yellow", 0x2578, 0xffe0, 3, 0, 0x258c, 0xffa4f001, 0x73e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x25b6, 0xffff, 5, 0, 0x25cc, 0xff84f001, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "accent_red", 0x29d2, 0xe800, 2, 0, 0x29e6, 0xfd77f001, 0x4268f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "accent_cyan", 0x29f8, 0x075f, 2, 0, 0x2a0c, 0xfd64f001, 0x725ff240 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x2a1e, 0xffff, 2, 0, 0x2a32, 0xfd51f001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "accent_red", 0x2a7c, 0xe800, 2, 0, 0x2a90, 0xfd22f001, 0x4268f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x2aa2, 0xffff, 2, 0, 0x2ab6, 0xfd0ff001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "accent_red", 0x2afe, 0xe800, 2, 0, 0x2b12, 0xfce1f001, 0x4268f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x2b24, 0xffff, 2, 0, 0x2b38, 0xfccef001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "native_orange", 0x2b82, 0xeb00, 2, 0, 0x2b96, 0xfc9ff001, 0x426bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x2ba8, 0xffff, 2, 0, 0x2bbc, 0xfc8cf001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x2bee, 0xffff, 7, 0, 0x2c14, 0xfc60f001, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "native_green", 0x2c70, 0x07e0, 11, 0, 0x2c86, 0xfc27f001, 0x6bfcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "highlight_yellow", 0x2d82, 0xffe0, 2, 0, 0x2d96, 0xfb9ff001, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x2da8, 0xffff, 2, 0, 0x2dbc, 0xfb8cf001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "native_orange", 0x2e04, 0xeb00, 2, 0, 0x2e18, 0xfb5ef001, 0x426bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x2e2a, 0xffff, 2, 0, 0x2e3e, 0xfb4bf001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "native_orange", 0x2e84, 0xeb00, 2, 0, 0x2e98, 0xfb1ef001, 0x426bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x2eaa, 0xffff, 2, 0, 0x2ebe, 0xfb0bf001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "native_lime", 0x2f06, 0x5fe0, 2, 0, 0x2f1a, 0xfaddf001, 0x72e0f645 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x2f2c, 0xffff, 2, 0, 0x2f40, 0xfacaf001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "accent_red", 0x2f88, 0xe800, 2, 0, 0x2f9c, 0xfa9cf001, 0x4268f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x2fae, 0xffff, 2, 0, 0x2fc2, 0xfa89f001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x32c6, 0xffff, 1, 0, 0x32dc, 0xf8fcf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "native_gray", 0x351c, 0x6308, 2, 0, 0x3534, 0xffd0f000, 0x3208f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x3610, 0xffff, 6, 0, 0x3620, 0xfe0af000, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x380e, 0xffff, 7, 0, 0x381e, 0xfd0bf000, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x3a16, 0xffff, 6, 0, 0x3a28, 0xfc06f000, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x3bde, 0xffff, 9, 0, 0x3bf0, 0xfb22f000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x3ce2, 0xffff, 8, 0, 0x3d06, 0xfbe7f000, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x55f0, 0xffff, 0, 0, 0x5622, 0xfe09f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x5668, 0xffff, 1, 0, 0x5698, 0xfdcef7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x56d6, 0xffff, 1, 0, 0x5706, 0xfd97f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x5760, 0xffff, 1, 0, 0x5790, 0xfd52f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x57ce, 0xffff, 1, 0, 0x57fe, 0xfd1bf7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "accent_red", 0x5838, 0xe800, 0, 0, 0x5870, 0xfe32f7fe, 0x4068f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x588c, 0xffff, 0, 0, 0x58c0, 0xfe0af7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "accent_red", 0x5926, 0xe800, 3, 0, 0x5938, 0xfdcef7fe, 0x4368f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x59d4, 0xffff, 7, 0, 0x5a12, 0xfd61f7fe, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "native_gray", 0x5b0c, 0x6308, 2, 0, 0x5b20, 0xfcdaf7fe, 0x3208f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x5cc0, 0xffff, 6, 0, 0x5cd0, 0xfab2f7fe, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x5ebe, 0xffff, 10, 0, 0x5ecc, 0xf9b4f7fe, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x6074, 0xffff, 6, 0, 0x608c, 0xf8d4f7fe, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x6224, 0xffff, 6, 0, 0x623a, 0xfffdf7fd, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "native_gray", 0x650e, 0x6308, 0, 0, 0x651c, 0xfac8f7fe, 0x3008f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "native_gray", 0x6544, 0x6308, 0, 0, 0x6552, 0xfaadf7fe, 0x3008f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "native_gray", 0x6580, 0x6308, 8, 0, 0x6590, 0xfa8ef7fe, 0x3808f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "native_gray", 0x65ce, 0x6308, 7, 0, 0x65dc, 0xfa68f7fe, 0x3708f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "digits_wattage", 0x666c, 0xffff, 7, 0, 0x66fa, 0xfd9df7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x315c, "highlight_yellow", 0x81d8, 0xffe0, 11, 0, 0x81ec, 0xf974f7fc, 0x7be0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x1f32, 0xffff, 5, 0, 0x1f80, 0xfaacf002, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "highlight_yellow", 0x2000, 0xffe0, 2, 0, 0x2014, 0xfa62f002, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x2026, 0xffff, 2, 0, 0x203a, 0xfa4ff002, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x2074, 0xffff, 5, 0, 0x2088, 0xfa28f002, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "accent_red", 0x24e4, 0xe800, 2, 0, 0x24f8, 0xfff0f001, 0x4268f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x250a, 0xffff, 2, 0, 0x251e, 0xffddf001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "highlight_yellow", 0x2590, 0xffe0, 3, 0, 0x25a4, 0xff9af001, 0x73e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x25ce, 0xffff, 5, 0, 0x25e4, 0xff7af001, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "accent_red", 0x29ea, 0xe800, 2, 0, 0x29fe, 0xfd6df001, 0x4268f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "accent_cyan", 0x2a10, 0x075f, 2, 0, 0x2a24, 0xfd5af001, 0x725ff240 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x2a36, 0xffff, 2, 0, 0x2a4a, 0xfd47f001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "accent_red", 0x2a94, 0xe800, 2, 0, 0x2aa8, 0xfd18f001, 0x4268f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x2aba, 0xffff, 2, 0, 0x2ace, 0xfd05f001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "accent_red", 0x2b16, 0xe800, 2, 0, 0x2b2a, 0xfcd7f001, 0x4268f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x2b3c, 0xffff, 2, 0, 0x2b50, 0xfcc4f001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "native_orange", 0x2b9a, 0xeb00, 2, 0, 0x2bae, 0xfc95f001, 0x426bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x2bc0, 0xffff, 2, 0, 0x2bd4, 0xfc82f001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x2c06, 0xffff, 7, 0, 0x2c2c, 0xfc56f001, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "native_green", 0x2c88, 0x07e0, 11, 0, 0x2c9e, 0xfc1df001, 0x6bfcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "highlight_yellow", 0x2d9a, 0xffe0, 2, 0, 0x2dae, 0xfb95f001, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x2dc0, 0xffff, 2, 0, 0x2dd4, 0xfb82f001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "native_orange", 0x2e1c, 0xeb00, 2, 0, 0x2e30, 0xfb54f001, 0x426bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x2e42, 0xffff, 2, 0, 0x2e56, 0xfb41f001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "native_orange", 0x2e9c, 0xeb00, 2, 0, 0x2eb0, 0xfb14f001, 0x426bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x2ec2, 0xffff, 2, 0, 0x2ed6, 0xfb01f001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "native_lime", 0x2f1e, 0x5fe0, 2, 0, 0x2f32, 0xfad3f001, 0x72e0f645 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x2f44, 0xffff, 2, 0, 0x2f58, 0xfac0f001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "accent_red", 0x2fa0, 0xe800, 2, 0, 0x2fb4, 0xfa92f001, 0x4268f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x2fc6, 0xffff, 2, 0, 0x2fda, 0xfa7ff001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x32ca, 0xffff, 1, 0, 0x32e0, 0xf8fcf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "native_gray", 0x3520, 0x6308, 2, 0, 0x3538, 0xffd0f000, 0x3208f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x3614, 0xffff, 6, 0, 0x3624, 0xfe0af000, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x3812, 0xffff, 7, 0, 0x3822, 0xfd0bf000, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x3a1a, 0xffff, 6, 0, 0x3a2c, 0xfc06f000, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x3be2, 0xffff, 9, 0, 0x3bf4, 0xfb22f000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x3ce6, 0xffff, 8, 0, 0x3d0a, 0xfbe7f000, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x55fc, 0xffff, 0, 0, 0x562e, 0xfe05f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x5674, 0xffff, 1, 0, 0x56a4, 0xfdcaf7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x56e2, 0xffff, 1, 0, 0x5712, 0xfd93f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x576c, 0xffff, 1, 0, 0x579c, 0xfd4ef7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x57da, 0xffff, 1, 0, 0x580a, 0xfd17f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "accent_red", 0x5844, 0xe800, 0, 0, 0x587c, 0xfe2ef7fe, 0x4068f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x5898, 0xffff, 0, 0, 0x58cc, 0xfe06f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "accent_red", 0x5932, 0xe800, 3, 0, 0x5944, 0xfdcaf7fe, 0x4368f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x59e0, 0xffff, 7, 0, 0x5a1e, 0xfd5df7fe, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "native_gray", 0x5b18, 0x6308, 2, 0, 0x5b2c, 0xfcd6f7fe, 0x3208f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x5ccc, 0xffff, 6, 0, 0x5cdc, 0xfaaef7fe, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x5eca, 0xffff, 10, 0, 0x5ed8, 0xf9b0f7fe, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x6080, 0xffff, 6, 0, 0x6098, 0xf8d0f7fe, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x6230, 0xffff, 6, 0, 0x6246, 0xfff9f7fd, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "native_gray", 0x651a, 0x6308, 0, 0, 0x6528, 0xfac4f7fe, 0x3008f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "native_gray", 0x6550, 0x6308, 0, 0, 0x655e, 0xfaa9f7fe, 0x3008f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "native_gray", 0x658c, 0x6308, 8, 0, 0x659c, 0xfa8af7fe, 0x3808f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "native_gray", 0x65da, 0x6308, 7, 0, 0x65e8, 0xfa64f7fe, 0x3708f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "digits_wattage", 0x6678, 0xffff, 7, 0, 0x6706, 0xfd99f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3151, "highlight_yellow", 0x818c, 0xffe0, 11, 0, 0x81a0, 0xf99cf7fc, 0x7be0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "highlight_yellow", 0x1f94, 0xffe0, 2, 0, 0x1fa8, 0xfb0ef002, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x1fba, 0xffff, 2, 0, 0x1fce, 0xfafbf002, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x2008, 0xffff, 5, 0, 0x201c, 0xfad4f002, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "accent_red", 0x246c, 0xe800, 1, 0, 0x2480, 0xf8a2f002, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x2484, 0xffff, 1, 0, 0x2498, 0xf896f002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "highlight_yellow", 0x2516, 0xffe0, 2, 0, 0x252a, 0xf84df002, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x2562, 0xffff, 7, 0, 0x2578, 0xf826f002, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "accent_red", 0x2966, 0xe800, 1, 0, 0x297a, 0xfe25f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "accent_cyan", 0x297e, 0x075f, 1, 0, 0x2992, 0xfe19f001, 0x715ff240 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x29a4, 0xffff, 1, 0, 0x29b8, 0xfe06f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "accent_red", 0x2a10, 0xe800, 1, 0, 0x2a24, 0xfdd0f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x2a28, 0xffff, 1, 0, 0x2a3c, 0xfdc4f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "accent_red", 0x2a76, 0xe800, 1, 0, 0x2a8a, 0xfd9df001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x2a8e, 0xffff, 1, 0, 0x2aa2, 0xfd91f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "native_orange", 0x2ade, 0xeb00, 1, 0, 0x2af2, 0xfd69f001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x2af6, 0xffff, 1, 0, 0x2b0a, 0xfd5df001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x2b2e, 0xffff, 8, 0, 0x2b6a, 0xfd2df001, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "native_green", 0x2bfa, 0x07e0, 11, 0, 0x2c10, 0xfcdaf001, 0x6bfcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "highlight_yellow", 0x2d32, 0xffe0, 1, 0, 0x2d46, 0xfc3ff001, 0x71e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x2d4a, 0xffff, 1, 0, 0x2d5e, 0xfc33f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "native_orange", 0x2d98, 0xeb00, 1, 0, 0x2dac, 0xfc0cf001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x2db0, 0xffff, 1, 0, 0x2dc4, 0xfc00f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "native_orange", 0x2dfc, 0xeb00, 1, 0, 0x2e10, 0xfbdaf001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x2e14, 0xffff, 1, 0, 0x2e28, 0xfbcef001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "native_lime", 0x2e62, 0x5fe0, 1, 0, 0x2e76, 0xfba7f001, 0x71e0f645 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x2e7a, 0xffff, 1, 0, 0x2e8e, 0xfb9bf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "accent_red", 0x2ec8, 0xe800, 1, 0, 0x2edc, 0xfb74f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x2ee0, 0xffff, 1, 0, 0x2ef4, 0xfb68f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "native_silver", 0x3448, 0x8410, 2, 0, 0x3460, 0xf8b2f001, 0x4210f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "native_silver", 0x3560, 0x8410, 2, 0, 0x3574, 0xf828f001, 0x4210f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x3666, 0xffff, 6, 0, 0x3676, 0xfe8ff000, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x3874, 0xffff, 6, 0, 0x3882, 0xfd89f000, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x3ac0, 0xffff, 10, 0, 0x3ad2, 0xfc61f000, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x3c7a, 0xffff, 9, 0, 0x3c8c, 0xfb84f000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x3ea8, 0xffff, 10, 0, 0x3ece, 0xfb7bf000, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x563e, 0xffff, 0, 0, 0x5670, 0xfe92f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x56ae, 0xffff, 1, 0, 0x56de, 0xfe5bf7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "accent_red", 0x5714, 0xe800, 1, 0, 0x574a, 0xff3df7fe, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x578e, 0xffff, 0, 0, 0x57c4, 0xff00f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x5854, 0xffff, 0, 0, 0x5886, 0xfd87f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x58d0, 0xffff, 1, 0, 0x5900, 0xfd4af7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x593e, 0xffff, 1, 0, 0x596e, 0xfd13f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "accent_red", 0x59aa, 0xe800, 1, 0, 0x59e0, 0xfdf2f7fe, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x5a26, 0xffff, 0, 0, 0x5a5c, 0xfdb4f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "native_silver", 0x5d00, 0x8410, 2, 0, 0x5d14, 0xfc58f7fe, 0x4210f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x5ebe, 0xffff, 0, 0, 0x5efc, 0xfb64f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x5fd6, 0xffff, 7, 0, 0x5fe6, 0xf9d7f7fe, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x61f2, 0xffff, 7, 0, 0x6202, 0xf8c9f7fe, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x6484, 0xffff, 7, 0, 0x6494, 0xff80f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x670a, 0xffff, 7, 0, 0x671e, 0xfe3bf7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x6900, 0xffff, 7, 0, 0x6916, 0xfd3ff7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x6b3c, 0xffff, 7, 0, 0x6b52, 0xfc21f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "native_silver", 0x6e3a, 0x8410, 0, 0, 0x6e48, 0xfea6f7fd, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "native_silver", 0x6e70, 0x8410, 0, 0, 0x6e7e, 0xfe8bf7fd, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "native_silver", 0x6eac, 0x8410, 8, 0, 0x6ebc, 0xfe6cf7fd, 0x4810f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "native_silver", 0x6ef8, 0x8410, 7, 0, 0x6f04, 0xfe48f7fd, 0x4710f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x6f88, 0xffff, 0, 0, 0x6fb8, 0xf9eef7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x6ff6, 0xffff, 0, 0, 0x7026, 0xf9b7f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x7094, 0xffff, 0, 0, 0x70c6, 0xf967f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x7110, 0xffff, 0, 0, 0x7140, 0xf92af7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "digits_wattage", 0x717e, 0xffff, 0, 0, 0x71ae, 0xf8f3f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3134, "highlight_yellow", 0x8cbc, 0xffe0, 10, 0, 0x8cd0, 0xfc7af7fb, 0x7ae0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "highlight_yellow", 0x1ff0, 0xffe0, 2, 0, 0x2004, 0xfb1cf002, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x2016, 0xffff, 2, 0, 0x202a, 0xfb09f002, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x2064, 0xffff, 5, 0, 0x2078, 0xfae2f002, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "accent_red", 0x24d0, 0xe800, 1, 0, 0x24e4, 0xf8acf002, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x24e8, 0xffff, 1, 0, 0x24fc, 0xf8a0f002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "highlight_yellow", 0x257a, 0xffe0, 2, 0, 0x258e, 0xf857f002, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x25c6, 0xffff, 7, 0, 0x25dc, 0xf830f002, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "accent_red", 0x29ca, 0xe800, 1, 0, 0x29de, 0xfe2ff001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "accent_cyan", 0x29e2, 0x075f, 1, 0, 0x29f6, 0xfe23f001, 0x715ff240 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x2a08, 0xffff, 1, 0, 0x2a1c, 0xfe10f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "accent_red", 0x2a74, 0xe800, 1, 0, 0x2a88, 0xfddaf001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x2a8c, 0xffff, 1, 0, 0x2aa0, 0xfdcef001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "accent_red", 0x2ada, 0xe800, 1, 0, 0x2aee, 0xfda7f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x2af2, 0xffff, 1, 0, 0x2b06, 0xfd9bf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "native_orange", 0x2b42, 0xeb00, 1, 0, 0x2b56, 0xfd73f001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x2b5a, 0xffff, 1, 0, 0x2b6e, 0xfd67f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x2b92, 0xffff, 8, 0, 0x2bce, 0xfd37f001, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "native_green", 0x2c5e, 0x07e0, 11, 0, 0x2c74, 0xfce4f001, 0x6bfcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "highlight_yellow", 0x2d96, 0xffe0, 1, 0, 0x2daa, 0xfc49f001, 0x71e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x2dae, 0xffff, 1, 0, 0x2dc2, 0xfc3df001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "native_orange", 0x2dfc, 0xeb00, 1, 0, 0x2e10, 0xfc16f001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x2e14, 0xffff, 1, 0, 0x2e28, 0xfc0af001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "native_orange", 0x2e60, 0xeb00, 1, 0, 0x2e74, 0xfbe4f001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x2e78, 0xffff, 1, 0, 0x2e8c, 0xfbd8f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "native_lime", 0x2ec6, 0x5fe0, 1, 0, 0x2eda, 0xfbb1f001, 0x71e0f645 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x2ede, 0xffff, 1, 0, 0x2ef2, 0xfba5f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "accent_red", 0x2f2c, 0xe800, 1, 0, 0x2f40, 0xfb7ef001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x2f44, 0xffff, 1, 0, 0x2f58, 0xfb72f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "native_silver", 0x34c0, 0x8410, 2, 0, 0x34d8, 0xf8b2f001, 0x4210f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "native_silver", 0x35d8, 0x8410, 2, 0, 0x35ec, 0xf828f001, 0x4210f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x36de, 0xffff, 6, 0, 0x36ee, 0xfe8ff000, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x38ec, 0xffff, 6, 0, 0x38fa, 0xfd89f000, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x3b38, 0xffff, 10, 0, 0x3b4a, 0xfc61f000, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x3cf2, 0xffff, 9, 0, 0x3d04, 0xfb84f000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x3f20, 0xffff, 10, 0, 0x3f46, 0xfb7bf000, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x56ca, 0xffff, 0, 0, 0x56fc, 0xfe88f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x573a, 0xffff, 1, 0, 0x576a, 0xfe51f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "accent_red", 0x57a0, 0xe800, 1, 0, 0x57d6, 0xff33f7fe, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x581a, 0xffff, 0, 0, 0x5850, 0xfef6f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x58e0, 0xffff, 0, 0, 0x5912, 0xfd7df7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x595c, 0xffff, 1, 0, 0x598c, 0xfd40f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x59ca, 0xffff, 1, 0, 0x59fa, 0xfd09f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "accent_red", 0x5a36, 0xe800, 1, 0, 0x5a6c, 0xfde8f7fe, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x5ab2, 0xffff, 0, 0, 0x5ae8, 0xfdaaf7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "native_silver", 0x5d8c, 0x8410, 2, 0, 0x5da0, 0xfc4ef7fe, 0x4210f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x5f4a, 0xffff, 0, 0, 0x5f88, 0xfb5af7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x6062, 0xffff, 7, 0, 0x6072, 0xf9cdf7fe, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x627e, 0xffff, 7, 0, 0x628e, 0xf8bff7fe, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x6510, 0xffff, 7, 0, 0x6520, 0xff76f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x6796, 0xffff, 7, 0, 0x67aa, 0xfe31f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x698c, 0xffff, 7, 0, 0x69a2, 0xfd35f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x6bc8, 0xffff, 7, 0, 0x6bde, 0xfc17f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "native_silver", 0x6ec6, 0x8410, 0, 0, 0x6ed4, 0xfe9cf7fd, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "native_silver", 0x6efc, 0x8410, 0, 0, 0x6f0a, 0xfe81f7fd, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "native_silver", 0x6f38, 0x8410, 8, 0, 0x6f48, 0xfe62f7fd, 0x4810f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "native_silver", 0x6f84, 0x8410, 7, 0, 0x6f90, 0xfe3ef7fd, 0x4710f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x7014, 0xffff, 0, 0, 0x7044, 0xf9e4f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x7082, 0xffff, 0, 0, 0x70b2, 0xf9adf7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x7120, 0xffff, 0, 0, 0x7152, 0xf95df7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x719c, 0xffff, 0, 0, 0x71cc, 0xf920f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "digits_wattage", 0x720a, 0xffff, 0, 0, 0x723a, 0xf8e9f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x3030, "highlight_yellow", 0x8d20, 0xffe0, 10, 0, 0x8d34, 0xfc84f7fb, 0x7ae0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "highlight_yellow", 0x25e8, 0xffe0, 2, 0, 0x25fc, 0xfe28f002, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x260e, 0xffff, 2, 0, 0x2622, 0xfe15f002, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x265c, 0xffff, 5, 0, 0x2670, 0xfdeef002, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "accent_red", 0x2acc, 0xe800, 1, 0, 0x2ae0, 0xfbb6f002, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x2ae4, 0xffff, 1, 0, 0x2af8, 0xfbaaf002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "highlight_yellow", 0x2b76, 0xffe0, 2, 0, 0x2b8a, 0xfb61f002, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x2bc2, 0xffff, 7, 0, 0x2bd8, 0xfb3af002, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "accent_red", 0x2fc6, 0xe800, 1, 0, 0x2fda, 0xf939f002, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "accent_cyan", 0x2fde, 0x075f, 1, 0, 0x2ff2, 0xf92df002, 0x715ff240 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x3004, 0xffff, 1, 0, 0x3018, 0xf91af002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "accent_red", 0x3070, 0xe800, 1, 0, 0x3084, 0xf8e4f002, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x3088, 0xffff, 1, 0, 0x309c, 0xf8d8f002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "accent_red", 0x30d6, 0xe800, 1, 0, 0x30ea, 0xf8b1f002, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x30ee, 0xffff, 1, 0, 0x3102, 0xf8a5f002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "native_orange", 0x313e, 0xeb00, 1, 0, 0x3152, 0xf87df002, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x3156, 0xffff, 1, 0, 0x316a, 0xf871f002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x318e, 0xffff, 8, 0, 0x31ca, 0xf841f002, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "native_green", 0x325a, 0x07e0, 11, 0, 0x3270, 0xffeef001, 0x6bfcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "highlight_yellow", 0x3392, 0xffe0, 1, 0, 0x33a6, 0xff53f001, 0x71e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x33aa, 0xffff, 1, 0, 0x33be, 0xff47f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "native_orange", 0x33f8, 0xeb00, 1, 0, 0x340c, 0xff20f001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x3410, 0xffff, 1, 0, 0x3424, 0xff14f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "native_orange", 0x345c, 0xeb00, 1, 0, 0x3470, 0xfeeef001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x3474, 0xffff, 1, 0, 0x3488, 0xfee2f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "native_lime", 0x34c2, 0x5fe0, 1, 0, 0x34d6, 0xfebbf001, 0x71e0f645 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x34da, 0xffff, 1, 0, 0x34ee, 0xfeaff001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "accent_red", 0x3528, 0xe800, 1, 0, 0x353c, 0xfe88f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x3540, 0xffff, 1, 0, 0x3554, 0xfe7cf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "native_silver", 0x3df8, 0x8410, 2, 0, 0x3e10, 0xfa1ef001, 0x4210f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "native_silver", 0x3f10, 0x8410, 2, 0, 0x3f24, 0xf994f001, 0x4210f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x4012, 0xffff, 6, 0, 0x4022, 0xfffbf000, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x4220, 0xffff, 6, 0, 0x422e, 0xfef5f000, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x446c, 0xffff, 10, 0, 0x447e, 0xfdcdf000, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x4626, 0xffff, 9, 0, 0x4638, 0xfcf0f000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x4854, 0xffff, 10, 0, 0x487a, 0xfce9f000, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x631a, 0xffff, 0, 0, 0x634c, 0xfe66f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x638a, 0xffff, 1, 0, 0x63ba, 0xfe2ff7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "accent_red", 0x63f0, 0xe800, 1, 0, 0x6426, 0xff13f7fe, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x646a, 0xffff, 0, 0, 0x64a0, 0xfed6f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x6530, 0xffff, 0, 0, 0x6562, 0xfd5bf7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x65ac, 0xffff, 1, 0, 0x65dc, 0xfd1ef7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x661a, 0xffff, 1, 0, 0x664a, 0xfce7f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "accent_red", 0x6686, 0xe800, 1, 0, 0x66bc, 0xfdc8f7fe, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x6702, 0xffff, 0, 0, 0x6738, 0xfd8af7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "native_silver", 0x69ec, 0x8410, 2, 0, 0x6a00, 0xfc26f7fe, 0x4210f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x6baa, 0xffff, 0, 0, 0x6be8, 0xfb32f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x6cbe, 0xffff, 7, 0, 0x6cce, 0xf9a5f7fe, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x6eda, 0xffff, 7, 0, 0x6eea, 0xf897f7fe, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x716c, 0xffff, 7, 0, 0x717c, 0xff4ef7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x73f2, 0xffff, 7, 0, 0x7406, 0xfe09f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x75e8, 0xffff, 7, 0, 0x75fe, 0xfd0df7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x7824, 0xffff, 7, 0, 0x783a, 0xfbeff7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "native_silver", 0x7b22, 0x8410, 0, 0, 0x7b30, 0xfe7af7fd, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "native_silver", 0x7b58, 0x8410, 0, 0, 0x7b66, 0xfe5ff7fd, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "native_silver", 0x7b94, 0x8410, 8, 0, 0x7ba4, 0xfe40f7fd, 0x4810f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "native_silver", 0x7be0, 0x8410, 7, 0, 0x7bec, 0xfe1cf7fd, 0x4710f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x7c70, 0xffff, 0, 0, 0x7ca0, 0xf9bcf7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x7cde, 0xffff, 0, 0, 0x7d0e, 0xf985f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x7d7c, 0xffff, 0, 0, 0x7dae, 0xf935f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x7df8, 0xffff, 0, 0, 0x7e28, 0xf8f8f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "digits_wattage", 0x7e66, 0xffff, 0, 0, 0x7e96, 0xf8c1f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Factory, 0x311c, "highlight_yellow", 0xa670, 0xffe0, 10, 0, 0xa684, 0xfde4f7fa, 0x7ae0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "highlight_yellow", 0x2358, 0xffe0, 2, 0, 0x236c, 0xfd09f002, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x237e, 0xffff, 2, 0, 0x2392, 0xfcf6f002, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x23d8, 0xffff, 5, 0, 0x23ec, 0xfcc9f002, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "accent_red", 0x2d78, 0xe800, 1, 0, 0x2d90, 0xfff7f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x2da2, 0xffff, 1, 0, 0x2dba, 0xffe2f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "highlight_yellow", 0x2e2a, 0xffe0, 3, 0, 0x2e3c, 0xffa1f001, 0x73e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x2e74, 0xffff, 9, 0, 0x2e8c, 0xff79f001, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "accent_red", 0x323a, 0xe800, 1, 0, 0x3252, 0xfd96f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "accent_cyan", 0x3264, 0x075f, 1, 0, 0x327c, 0xfd81f001, 0x715ff240 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x328e, 0xffff, 1, 0, 0x32a6, 0xfd6cf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "accent_red", 0x32f0, 0xe800, 1, 0, 0x3308, 0xfd3bf001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x331a, 0xffff, 1, 0, 0x3332, 0xfd26f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "accent_red", 0x337a, 0xe800, 2, 0, 0x338e, 0xfcf8f001, 0x4268f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x33a0, 0xffff, 2, 0, 0x33b4, 0xfce5f001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_orange", 0x33fe, 0xeb00, 2, 0, 0x3412, 0xfcb6f001, 0x426bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x3424, 0xffff, 2, 0, 0x3438, 0xfca3f001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x347e, 0xffff, 6, 0, 0x3492, 0xfc76f001, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_green", 0x34f4, 0x07e0, 9, 0, 0x350a, 0xfc3af001, 0x69fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "highlight_yellow", 0x360a, 0xffe0, 2, 0, 0x361e, 0xfbb0f001, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x3630, 0xffff, 2, 0, 0x3644, 0xfb9df001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_orange", 0x368c, 0xeb00, 2, 0, 0x36a0, 0xfb6ff001, 0x426bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x36b2, 0xffff, 2, 0, 0x36c6, 0xfb5cf001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_orange", 0x370c, 0xeb00, 2, 0, 0x3720, 0xfb2ff001, 0x426bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x3732, 0xffff, 2, 0, 0x3746, 0xfb1cf001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_lime", 0x378e, 0x5fe0, 2, 0, 0x37a2, 0xfaeef001, 0x72e0f645 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x37b4, 0xffff, 2, 0, 0x37c8, 0xfadbf001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "accent_red", 0x3810, 0xe800, 2, 0, 0x3824, 0xfaadf001, 0x4268f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x3836, 0xffff, 2, 0, 0x384a, 0xfa9af001, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x3b0e, 0xffff, 1, 0, 0x3b24, 0xf92df001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_gray", 0x3d38, 0x6308, 1, 0, 0x3d50, 0xf817f001, 0x3108f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x3e58, 0xffff, 9, 0, 0x3e68, 0xfe58f000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x4058, 0xffff, 9, 0, 0x4068, 0xfd58f000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x426e, 0xffff, 9, 0, 0x4280, 0xfc4cf000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x4446, 0xffff, 8, 0, 0x4458, 0xfb60f000, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x4552, 0xffff, 11, 0, 0x4576, 0xfc04f000, 0x7bfff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x59ba, 0xffff, 0, 0, 0x59ec, 0xf896f7ff, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x5a36, 0xffff, 1, 0, 0x5a66, 0xf859f7ff, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x5aaa, 0xffff, 0, 0, 0x5adc, 0xf81ef7ff, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x5b3c, 0xffff, 1, 0, 0x5b6c, 0xffd6f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x5bb0, 0xffff, 0, 0, 0x5be2, 0xff9bf7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "accent_red", 0x5c18, 0xe800, 0, 0, 0x5c52, 0xf896f7ff, 0x4068f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x5c70, 0xffff, 0, 0, 0x5ca4, 0xf86df7ff, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "accent_red", 0x5d0e, 0xe800, 3, 0, 0x5d1e, 0xf830f7ff, 0x4368f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_gray", 0x5e3e, 0x6308, 1, 0, 0x5e52, 0xff96f7fe, 0x3108f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x5f88, 0xffff, 4, 0, 0x5fd4, 0xfed5f7fe, 0x74fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x6066, 0xffff, 8, 0, 0x6076, 0xfd51f7fe, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x625a, 0xffff, 9, 0, 0x6268, 0xfc58f7fe, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x6480, 0xffff, 8, 0, 0x6498, 0xfb40f7fe, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x6630, 0xffff, 8, 0, 0x6646, 0xfa69f7fe, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_gray", 0x68f8, 0x6308, 0, 0, 0x6906, 0xfcd0f7fe, 0x3008f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_gray", 0x6938, 0x6308, 0, 0, 0x6946, 0xfcb0f7fe, 0x3008f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_gray", 0x6970, 0x6308, 0, 0, 0x697e, 0xfc94f7fe, 0x3008f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_gray", 0x69ae, 0x6308, 7, 0, 0x69bc, 0xfc75f7fe, 0x3708f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_gray", 0x69fa, 0x6308, 6, 0, 0x6a08, 0xfc4ff7fe, 0x3608f246 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x6a80, 0xffff, 6, 0, 0x6b14, 0xf802f7fe, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x6f92, 0xffff, 0, 0, 0x6faa, 0xfeeaf7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_green", 0x702a, 0x07e0, 8, 0, 0x7042, 0xfe9ef7fd, 0x68fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_green", 0x70fa, 0x07e0, 7, 0, 0x7110, 0xfe37f7fd, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_green", 0x74f4, 0x07e0, 8, 0, 0x751c, 0xfc31f7fd, 0x68fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_green", 0x771a, 0x07e0, 0, 0, 0x772c, 0xfb29f7fd, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_green", 0x7742, 0x07e0, 0, 0, 0x7754, 0xfb15f7fd, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_green", 0x776e, 0x07e0, 1, 0, 0x7790, 0xfaf7f7fd, 0x61fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_green", 0x77c0, 0x07e0, 0, 0, 0x77ce, 0xfd6cf7fd, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x77e6, 0xffff, 8, 0, 0x7812, 0xf983f7fd, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_green", 0x7b32, 0x07e0, 7, 0, 0x7b4c, 0xf919f7fd, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x7b7c, 0xffff, 7, 0, 0x7b90, 0xffc4f7fc, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_green", 0x7c5c, 0x07e0, 7, 0, 0x7c72, 0xf886f7fd, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x7ca2, 0xffff, 7, 0, 0x7cb4, 0xff32f7fc, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_green", 0x7dc0, 0x07e0, 8, 0, 0x7dd8, 0xffd3f7fc, 0x68fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x7e30, 0xffff, 8, 0, 0x7e40, 0xfe6cf7fc, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x7fca, 0xffff, 9, 0, 0x7fdc, 0xfd9ef7fc, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "accent_red", 0x809e, 0xe800, 1, 0, 0x80b2, 0xfe66f7fc, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "native_green", 0x80da, 0x07e0, 7, 0, 0x80f4, 0xfe45f7fc, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "digits_wattage", 0x813a, 0xffff, 7, 0, 0x814e, 0xfce5f7fc, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3151, "highlight_yellow", 0x9974, 0xffe0, 11, 0, 0x9988, 0xf9fbf7fb, 0x7be0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "highlight_yellow", 0x2334, 0xffe0, 1, 0, 0x234c, 0xfd7df002, 0x71e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x235e, 0xffff, 1, 0, 0x2376, 0xfd68f002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x23bc, 0xffff, 5, 0, 0x23d0, 0xfd3bf002, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "accent_red", 0x2d64, 0xe800, 1, 0, 0x2d78, 0xf867f002, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x2d7c, 0xffff, 1, 0, 0x2d90, 0xf85bf002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "highlight_yellow", 0x2e08, 0xffe0, 2, 0, 0x2e1c, 0xf815f002, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x2e5c, 0xffff, 8, 0, 0x2e74, 0xffe9f001, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "accent_red", 0x321a, 0xe800, 1, 0, 0x322e, 0xfe0cf001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "accent_cyan", 0x3232, 0x075f, 1, 0, 0x3246, 0xfe00f001, 0x715ff240 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x3258, 0xffff, 1, 0, 0x326c, 0xfdedf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "accent_red", 0x32c4, 0xe800, 1, 0, 0x32d8, 0xfdb7f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x32dc, 0xffff, 1, 0, 0x32f0, 0xfdabf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "accent_red", 0x332a, 0xe800, 1, 0, 0x333e, 0xfd84f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x3342, 0xffff, 1, 0, 0x3356, 0xfd78f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_orange", 0x3392, 0xeb00, 1, 0, 0x33a6, 0xfd50f001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x33aa, 0xffff, 1, 0, 0x33be, 0xfd44f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x340c, 0xffff, 7, 0, 0x3420, 0xfd13f001, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_green", 0x34b8, 0x07e0, 11, 0, 0x34ce, 0xfcbcf001, 0x6bfcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "highlight_yellow", 0x35ee, 0xffe0, 1, 0, 0x3602, 0xfc22f001, 0x71e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x3606, 0xffff, 1, 0, 0x361a, 0xfc16f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_orange", 0x3654, 0xeb00, 1, 0, 0x3668, 0xfbeff001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x366c, 0xffff, 1, 0, 0x3680, 0xfbe3f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_orange", 0x36b8, 0xeb00, 1, 0, 0x36cc, 0xfbbdf001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x36d0, 0xffff, 1, 0, 0x36e4, 0xfbb1f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_lime", 0x371e, 0x5fe0, 1, 0, 0x3732, 0xfb8af001, 0x71e0f645 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x3736, 0xffff, 1, 0, 0x374a, 0xfb7ef001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "accent_red", 0x3784, 0xe800, 1, 0, 0x3798, 0xfb57f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x379c, 0xffff, 1, 0, 0x37b0, 0xfb4bf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_silver", 0x3c76, 0x8410, 1, 0, 0x3c8e, 0xf8dcf001, 0x4110f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_silver", 0x3d96, 0x8410, 1, 0, 0x3daa, 0xf84ef001, 0x4110f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x3eda, 0xffff, 9, 0, 0x3eea, 0xfea9f000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x40ec, 0xffff, 9, 0, 0x40fc, 0xfda0f000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x434e, 0xffff, 8, 0, 0x4360, 0xfc6ef000, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x4516, 0xffff, 8, 0, 0x4528, 0xfb8af000, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x475c, 0xffff, 10, 0, 0x4784, 0xfb61f000, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x59f0, 0xffff, 0, 0, 0x5a22, 0xf90df7ff, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x5a64, 0xffff, 1, 0, 0x5a94, 0xf8d4f7ff, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "accent_red", 0x5acc, 0xe800, 1, 0, 0x5b04, 0xf9a1f7ff, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x5b4c, 0xffff, 0, 0, 0x5b84, 0xf961f7ff, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x5c16, 0xffff, 0, 0, 0x5c48, 0xfffaf7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x5c96, 0xffff, 1, 0, 0x5cc6, 0xffbbf7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x5d08, 0xffff, 1, 0, 0x5d38, 0xff82f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "accent_red", 0x5d70, 0xe800, 1, 0, 0x5da8, 0xf84ff7ff, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x5df0, 0xffff, 0, 0, 0x5e28, 0xf80ff7ff, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_silver", 0x6064, 0x8410, 1, 0, 0x6078, 0xfee7f7fe, 0x4110f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x6224, 0xffff, 0, 0, 0x625c, 0xfdf5f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x6332, 0xffff, 9, 0, 0x6342, 0xfc7df7fe, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x6546, 0xffff, 10, 0, 0x6556, 0xfb73f7fe, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x67e4, 0xffff, 10, 0, 0x67f4, 0xfa24f7fe, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x6b16, 0xffff, 7, 0, 0x6b2a, 0xf889f7fe, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x6d14, 0xffff, 8, 0, 0x6d28, 0xff8af7fd, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x6f56, 0xffff, 7, 0, 0x6f6c, 0xfe68f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_silver", 0x7236, 0x8410, 0, 0, 0x7244, 0xf893f7fe, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_silver", 0x7272, 0x8410, 0, 0, 0x7280, 0xf875f7fe, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_silver", 0x72aa, 0x8410, 0, 0, 0x72b8, 0xf859f7fe, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_silver", 0x72e8, 0x8410, 7, 0, 0x72f6, 0xf83af7fe, 0x4710f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_silver", 0x7338, 0x8410, 6, 0, 0x7344, 0xf813f7fe, 0x4610f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x73b4, 0xffff, 0, 0, 0x73e8, 0xfc2af7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x7428, 0xffff, 0, 0, 0x7458, 0xfbf2f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x74cc, 0xffff, 0, 0, 0x7502, 0xfb9df7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x7550, 0xffff, 0, 0, 0x7584, 0xfb5cf7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x75c4, 0xffff, 0, 0, 0x75f4, 0xfb24f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_green", 0x7a86, 0x07e0, 6, 0, 0x7a9c, 0xf9d5f7fd, 0x66fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_green", 0x7b56, 0x07e0, 7, 0, 0x7b6c, 0xf96df7fd, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_green", 0x7f40, 0x07e0, 8, 0, 0x7f84, 0xff61f7fc, 0x68fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_green", 0x8264, 0x07e0, 0, 0, 0x8276, 0xfde8f7fc, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_green", 0x828c, 0x07e0, 0, 0, 0x829e, 0xfdd4f7fc, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_green", 0x82b8, 0x07e0, 1, 0, 0x82dc, 0xfdb5f7fc, 0x61fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_green", 0x830c, 0x07e0, 0, 0, 0x831a, 0xf828f7fd, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x8332, 0xffff, 8, 0, 0x835e, 0xfc6ff7fc, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_green", 0x86ca, 0x07e0, 7, 0, 0x86e4, 0xfbb1f7fc, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x8714, 0xffff, 7, 0, 0x8728, 0xfa8af7fc, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_green", 0x881a, 0x07e0, 7, 0, 0x882a, 0xfda0f7fc, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x88a6, 0xffff, 7, 0, 0x88b6, 0xf9c3f7fc, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_green", 0x89bc, 0x07e0, 8, 0, 0x89cc, 0xfccff7fc, 0x68fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x8a60, 0xffff, 8, 0, 0x8a70, 0xf8e6f7fc, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x8c1e, 0xffff, 9, 0, 0x8c30, 0xf806f7fc, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "accent_red", 0x8cf2, 0xe800, 1, 0, 0x8d06, 0xf8a0f7fc, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "native_green", 0x8d2e, 0x07e0, 7, 0, 0x8d46, 0xf880f7fc, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "digits_wattage", 0x8d76, 0xffff, 7, 0, 0x8d8a, 0xff59f7fb, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "highlight_yellow", 0xa5fa, 0xffe0, 4, 0, 0xa60e, 0xfc1cf7fa, 0x74e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3030, "highlight_yellow", 0xa73a, 0xffe0, 0, 0, 0xa74e, 0xfb7cf7fa, 0x70e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "highlight_yellow", 0x2334, 0xffe0, 1, 0, 0x234c, 0xfd7df002, 0x71e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x235e, 0xffff, 1, 0, 0x2376, 0xfd68f002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x23bc, 0xffff, 5, 0, 0x23d0, 0xfd3bf002, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "accent_red", 0x2d64, 0xe800, 1, 0, 0x2d78, 0xf867f002, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x2d7c, 0xffff, 1, 0, 0x2d90, 0xf85bf002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "highlight_yellow", 0x2e08, 0xffe0, 2, 0, 0x2e1c, 0xf815f002, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x2e5c, 0xffff, 8, 0, 0x2e74, 0xffe9f001, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "accent_red", 0x321a, 0xe800, 1, 0, 0x322e, 0xfe0cf001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "accent_cyan", 0x3232, 0x075f, 1, 0, 0x3246, 0xfe00f001, 0x715ff240 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x3258, 0xffff, 1, 0, 0x326c, 0xfdedf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "accent_red", 0x32c4, 0xe800, 1, 0, 0x32d8, 0xfdb7f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x32dc, 0xffff, 1, 0, 0x32f0, 0xfdabf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "accent_red", 0x332a, 0xe800, 1, 0, 0x333e, 0xfd84f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x3342, 0xffff, 1, 0, 0x3356, 0xfd78f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_orange", 0x3392, 0xeb00, 1, 0, 0x33a6, 0xfd50f001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x33aa, 0xffff, 1, 0, 0x33be, 0xfd44f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x340c, 0xffff, 7, 0, 0x3420, 0xfd13f001, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_green", 0x34b8, 0x07e0, 11, 0, 0x34ce, 0xfcbcf001, 0x6bfcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "highlight_yellow", 0x35ee, 0xffe0, 1, 0, 0x3602, 0xfc22f001, 0x71e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x3606, 0xffff, 1, 0, 0x361a, 0xfc16f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_orange", 0x3654, 0xeb00, 1, 0, 0x3668, 0xfbeff001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x366c, 0xffff, 1, 0, 0x3680, 0xfbe3f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_orange", 0x36b8, 0xeb00, 1, 0, 0x36cc, 0xfbbdf001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x36d0, 0xffff, 1, 0, 0x36e4, 0xfbb1f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_lime", 0x371e, 0x5fe0, 1, 0, 0x3732, 0xfb8af001, 0x71e0f645 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x3736, 0xffff, 1, 0, 0x374a, 0xfb7ef001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "accent_red", 0x3784, 0xe800, 1, 0, 0x3798, 0xfb57f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x379c, 0xffff, 1, 0, 0x37b0, 0xfb4bf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_silver", 0x3c76, 0x8410, 1, 0, 0x3c8e, 0xf8dcf001, 0x4110f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_silver", 0x3d96, 0x8410, 1, 0, 0x3daa, 0xf84ef001, 0x4110f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x3eda, 0xffff, 9, 0, 0x3eea, 0xfea9f000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x40ec, 0xffff, 9, 0, 0x40fc, 0xfda0f000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x434e, 0xffff, 8, 0, 0x4360, 0xfc6ef000, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x4516, 0xffff, 8, 0, 0x4528, 0xfb8af000, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x475c, 0xffff, 10, 0, 0x4784, 0xfb61f000, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x59f0, 0xffff, 0, 0, 0x5a22, 0xf90df7ff, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x5a64, 0xffff, 1, 0, 0x5a94, 0xf8d4f7ff, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "accent_red", 0x5acc, 0xe800, 1, 0, 0x5b04, 0xf9a1f7ff, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x5b4c, 0xffff, 0, 0, 0x5b84, 0xf961f7ff, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x5c16, 0xffff, 0, 0, 0x5c48, 0xfffaf7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x5c96, 0xffff, 1, 0, 0x5cc6, 0xffbbf7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x5d08, 0xffff, 1, 0, 0x5d38, 0xff82f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "accent_red", 0x5d70, 0xe800, 1, 0, 0x5da8, 0xf84ff7ff, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x5df0, 0xffff, 0, 0, 0x5e28, 0xf80ff7ff, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_silver", 0x6064, 0x8410, 1, 0, 0x6078, 0xfee7f7fe, 0x4110f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x6224, 0xffff, 0, 0, 0x625c, 0xfdf5f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x6332, 0xffff, 9, 0, 0x6342, 0xfc7df7fe, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x6546, 0xffff, 10, 0, 0x6556, 0xfb73f7fe, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x67e4, 0xffff, 10, 0, 0x67f4, 0xfa24f7fe, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x6b16, 0xffff, 7, 0, 0x6b2a, 0xf889f7fe, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x6d14, 0xffff, 8, 0, 0x6d28, 0xff8af7fd, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x6f56, 0xffff, 7, 0, 0x6f6c, 0xfe68f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_silver", 0x7236, 0x8410, 0, 0, 0x7244, 0xf893f7fe, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_silver", 0x7272, 0x8410, 0, 0, 0x7280, 0xf875f7fe, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_silver", 0x72aa, 0x8410, 0, 0, 0x72b8, 0xf859f7fe, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_silver", 0x72e8, 0x8410, 7, 0, 0x72f6, 0xf83af7fe, 0x4710f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_silver", 0x7338, 0x8410, 6, 0, 0x7344, 0xf813f7fe, 0x4610f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x73b4, 0xffff, 0, 0, 0x73e8, 0xfc2af7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x7428, 0xffff, 0, 0, 0x7458, 0xfbf2f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x74cc, 0xffff, 0, 0, 0x7502, 0xfb9df7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x7550, 0xffff, 0, 0, 0x7584, 0xfb5cf7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x75c4, 0xffff, 0, 0, 0x75f4, 0xfb24f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_green", 0x7a86, 0x07e0, 6, 0, 0x7a9c, 0xf9d5f7fd, 0x66fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_green", 0x7b56, 0x07e0, 7, 0, 0x7b6c, 0xf96df7fd, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_green", 0x7f40, 0x07e0, 8, 0, 0x7f84, 0xff61f7fc, 0x68fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_green", 0x8264, 0x07e0, 0, 0, 0x8276, 0xfde8f7fc, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_green", 0x828c, 0x07e0, 0, 0, 0x829e, 0xfdd4f7fc, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_green", 0x82b8, 0x07e0, 1, 0, 0x82dc, 0xfdb5f7fc, 0x61fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_green", 0x830c, 0x07e0, 0, 0, 0x831a, 0xf828f7fd, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x8332, 0xffff, 8, 0, 0x835e, 0xfc6ff7fc, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_green", 0x86ca, 0x07e0, 7, 0, 0x86e4, 0xfbb1f7fc, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x8714, 0xffff, 7, 0, 0x8728, 0xfa8af7fc, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_green", 0x881a, 0x07e0, 7, 0, 0x882a, 0xfda0f7fc, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x88a6, 0xffff, 7, 0, 0x88b6, 0xf9c3f7fc, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_green", 0x89bc, 0x07e0, 8, 0, 0x89cc, 0xfccff7fc, 0x68fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x8a60, 0xffff, 8, 0, 0x8a70, 0xf8e6f7fc, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x8c1e, 0xffff, 9, 0, 0x8c30, 0xf806f7fc, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "accent_red", 0x8cf2, 0xe800, 1, 0, 0x8d06, 0xf8a0f7fc, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "native_green", 0x8d2e, 0x07e0, 7, 0, 0x8d46, 0xf880f7fc, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "digits_wattage", 0x8d76, 0xffff, 7, 0, 0x8d8a, 0xff59f7fb, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "highlight_yellow", 0xa5fa, 0xffe0, 4, 0, 0xa60e, 0xfc1cf7fa, 0x74e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x313a, "highlight_yellow", 0xa73a, 0xffe0, 0, 0, 0xa74e, 0xfb7cf7fa, 0x70e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "highlight_yellow", 0x25f4, 0xffe0, 2, 0, 0x2608, 0xfd56f002, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x261a, 0xffff, 2, 0, 0x262e, 0xfd43f002, 0x72fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x2668, 0xffff, 5, 0, 0x267c, 0xfd1cf002, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "accent_red", 0x3010, 0xe800, 1, 0, 0x3024, 0xf848f002, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x3028, 0xffff, 1, 0, 0x303c, 0xf83cf002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "highlight_yellow", 0x30ba, 0xffe0, 2, 0, 0x30ce, 0xfff3f001, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x3106, 0xffff, 7, 0, 0x311c, 0xffccf001, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "accent_red", 0x3522, 0xe800, 1, 0, 0x3536, 0xfdbff001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "accent_cyan", 0x353a, 0x075f, 1, 0, 0x354e, 0xfdb3f001, 0x715ff240 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x3560, 0xffff, 1, 0, 0x3574, 0xfda0f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "accent_red", 0x35cc, 0xe800, 1, 0, 0x35e0, 0xfd6af001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x35e4, 0xffff, 1, 0, 0x35f8, 0xfd5ef001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "accent_red", 0x3632, 0xe800, 1, 0, 0x3646, 0xfd37f001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x364a, 0xffff, 1, 0, 0x365e, 0xfd2bf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_orange", 0x369a, 0xeb00, 1, 0, 0x36ae, 0xfd03f001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x36b2, 0xffff, 1, 0, 0x36c6, 0xfcf7f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x36ea, 0xffff, 8, 0, 0x3726, 0xfcc7f001, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_green", 0x37b6, 0x07e0, 11, 0, 0x37cc, 0xfc74f001, 0x6bfcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "highlight_yellow", 0x38ee, 0xffe0, 1, 0, 0x3902, 0xfbd9f001, 0x71e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x3906, 0xffff, 1, 0, 0x391a, 0xfbcdf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_orange", 0x3954, 0xeb00, 1, 0, 0x3968, 0xfba6f001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x396c, 0xffff, 1, 0, 0x3980, 0xfb9af001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_orange", 0x39b8, 0xeb00, 1, 0, 0x39cc, 0xfb74f001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x39d0, 0xffff, 1, 0, 0x39e4, 0xfb68f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_lime", 0x3a1e, 0x5fe0, 1, 0, 0x3a32, 0xfb41f001, 0x71e0f645 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x3a36, 0xffff, 1, 0, 0x3a4a, 0xfb35f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "accent_red", 0x3a84, 0xe800, 1, 0, 0x3a98, 0xfb0ef001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x3a9c, 0xffff, 1, 0, 0x3ab0, 0xfb02f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_silver", 0x3fc4, 0x8410, 2, 0, 0x3fdc, 0xf86cf001, 0x4210f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_silver", 0x40dc, 0x8410, 2, 0, 0x40f0, 0xffe2f000, 0x4210f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x41e2, 0xffff, 6, 0, 0x41f2, 0xfe3df000, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x43f0, 0xffff, 6, 0, 0x43fe, 0xfd37f000, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x463c, 0xffff, 10, 0, 0x464e, 0xfc0ff000, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x47f6, 0xffff, 9, 0, 0x4808, 0xfb32f000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x49b8, 0xffff, 10, 0, 0x49de, 0xfb6bf000, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x612e, 0xffff, 0, 0, 0x6160, 0xfe86f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x619e, 0xffff, 1, 0, 0x61ce, 0xfe4ff7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "accent_red", 0x6204, 0xe800, 1, 0, 0x623a, 0xff3df7fe, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x627e, 0xffff, 0, 0, 0x62b4, 0xff00f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x6344, 0xffff, 0, 0, 0x6376, 0xfd7bf7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x63c0, 0xffff, 1, 0, 0x63f0, 0xfd3ef7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x642e, 0xffff, 1, 0, 0x645e, 0xfd07f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "accent_red", 0x649a, 0xe800, 1, 0, 0x64d0, 0xfdf2f7fe, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x6516, 0xffff, 0, 0, 0x654c, 0xfdb4f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_silver", 0x67f0, 0x8410, 2, 0, 0x6804, 0xfc58f7fe, 0x4210f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x69ae, 0xffff, 0, 0, 0x69ec, 0xfb64f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x6ac6, 0xffff, 7, 0, 0x6ad6, 0xf9cbf7fe, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x6ce2, 0xffff, 7, 0, 0x6cf2, 0xf8bdf7fe, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x6f74, 0xffff, 7, 0, 0x6f84, 0xff74f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x71fa, 0xffff, 7, 0, 0x720e, 0xfe2ff7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x73f0, 0xffff, 7, 0, 0x7406, 0xfd33f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x762c, 0xffff, 7, 0, 0x7642, 0xfc15f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_silver", 0x792a, 0x8410, 0, 0, 0x7938, 0xfea6f7fd, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_silver", 0x7960, 0x8410, 0, 0, 0x796e, 0xfe8bf7fd, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_silver", 0x799c, 0x8410, 8, 0, 0x79ac, 0xfe6cf7fd, 0x4810f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_silver", 0x79e8, 0x8410, 7, 0, 0x79f4, 0xfe48f7fd, 0x4710f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x7a78, 0xffff, 0, 0, 0x7aa8, 0xf9e2f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x7ae6, 0xffff, 0, 0, 0x7b16, 0xf9abf7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x7b84, 0xffff, 0, 0, 0x7bb6, 0xf95bf7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x7c00, 0xffff, 0, 0, 0x7c30, 0xf91ef7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x7c6e, 0xffff, 0, 0, 0x7c9e, 0xf8e7f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_green", 0x812e, 0x07e0, 9, 0, 0x8142, 0xffb9f7fc, 0x69fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_green", 0x81f2, 0x07e0, 8, 0, 0x820a, 0xff55f7fc, 0x68fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_green", 0x85de, 0x07e0, 6, 0, 0x8620, 0xfd4af7fc, 0x66fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_green", 0x8900, 0x07e0, 2, 0, 0x8910, 0xfbd2f7fc, 0x62fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_green", 0x8924, 0x07e0, 2, 0, 0x8934, 0xfbc0f7fc, 0x62fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_green", 0x894e, 0x07e0, 0, 0, 0x89b8, 0xfb7ef7fc, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_green", 0x89ea, 0x07e0, 0, 0, 0x89f8, 0xfe46f7fc, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x8a14, 0xffff, 7, 0, 0x8a40, 0xfa16f7fc, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_green", 0x8d6c, 0x07e0, 7, 0, 0x8d7e, 0xf99bf7fc, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x8daa, 0xffff, 7, 0, 0x8dbe, 0xf857f7fc, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_green", 0x8ea0, 0x07e0, 7, 0, 0x8eb2, 0xfbe9f7fc, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x8f2c, 0xffff, 7, 0, 0x8f3c, 0xff98f7fb, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_green", 0x9048, 0x07e0, 7, 0, 0x9058, 0xfb16f7fc, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x90de, 0xffff, 10, 0, 0x90ec, 0xfec0f7fb, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x9298, 0xffff, 6, 0, 0x92aa, 0xfde1f7fb, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "accent_red", 0x936c, 0xe800, 1, 0, 0x9380, 0xfe9af7fb, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "native_green", 0x93aa, 0x07e0, 6, 0, 0x93c2, 0xfe79f7fb, 0x66fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "digits_wattage", 0x93f0, 0xffff, 6, 0, 0x9404, 0xfd34f7fb, 0x76fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x3134, "highlight_yellow", 0xac74, 0xffe0, 10, 0, 0xac88, 0xfa16f7fa, 0x7ae0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "highlight_yellow", 0x2bec, 0xffe0, 1, 0, 0x2c04, 0xf877f003, 0x71e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x2c16, 0xffff, 1, 0, 0x2c2e, 0xf862f003, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x2c74, 0xffff, 5, 0, 0x2c88, 0xf835f003, 0x75fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "accent_red", 0x3624, 0xe800, 1, 0, 0x3638, 0xfb5df002, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x363c, 0xffff, 1, 0, 0x3650, 0xfb51f002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "highlight_yellow", 0x36c8, 0xffe0, 2, 0, 0x36dc, 0xfb0bf002, 0x72e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x371c, 0xffff, 8, 0, 0x3734, 0xfadff002, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "accent_red", 0x3ada, 0xe800, 1, 0, 0x3aee, 0xf902f002, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "accent_cyan", 0x3af2, 0x075f, 1, 0, 0x3b06, 0xf8f6f002, 0x715ff240 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x3b18, 0xffff, 1, 0, 0x3b2c, 0xf8e3f002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "accent_red", 0x3b84, 0xe800, 1, 0, 0x3b98, 0xf8adf002, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x3b9c, 0xffff, 1, 0, 0x3bb0, 0xf8a1f002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "accent_red", 0x3bea, 0xe800, 1, 0, 0x3bfe, 0xf87af002, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x3c02, 0xffff, 1, 0, 0x3c16, 0xf86ef002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_orange", 0x3c52, 0xeb00, 1, 0, 0x3c66, 0xf846f002, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x3c6a, 0xffff, 1, 0, 0x3c7e, 0xf83af002, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x3ccc, 0xffff, 7, 0, 0x3ce0, 0xf809f002, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_green", 0x3d78, 0x07e0, 11, 0, 0x3d8e, 0xffb2f001, 0x6bfcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "highlight_yellow", 0x3eae, 0xffe0, 1, 0, 0x3ec2, 0xff18f001, 0x71e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x3ec6, 0xffff, 1, 0, 0x3eda, 0xff0cf001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_orange", 0x3f14, 0xeb00, 1, 0, 0x3f28, 0xfee5f001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x3f2c, 0xffff, 1, 0, 0x3f40, 0xfed9f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_orange", 0x3f78, 0xeb00, 1, 0, 0x3f8c, 0xfeb3f001, 0x416bf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x3f90, 0xffff, 1, 0, 0x3fa4, 0xfea7f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_lime", 0x3fde, 0x5fe0, 1, 0, 0x3ff2, 0xfe80f001, 0x71e0f645 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x3ff6, 0xffff, 1, 0, 0x400a, 0xfe74f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "accent_red", 0x4044, 0xe800, 1, 0, 0x4058, 0xfe4df001, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x405c, 0xffff, 1, 0, 0x4070, 0xfe41f001, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_silver", 0x487e, 0x8410, 1, 0, 0x4896, 0xfa2ef001, 0x4110f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_silver", 0x499e, 0x8410, 1, 0, 0x49b2, 0xf9a0f001, 0x4110f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x4ade, 0xffff, 9, 0, 0x4aee, 0xfffbf000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x4cf0, 0xffff, 9, 0, 0x4d00, 0xfef2f000, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x4f52, 0xffff, 8, 0, 0x4f64, 0xfdc0f000, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x511a, 0xffff, 8, 0, 0x512c, 0xfcdcf000, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x5364, 0xffff, 10, 0, 0x538c, 0xfcb3f000, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x6aa8, 0xffff, 0, 0, 0x6ada, 0xf805f7ff, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x6b1c, 0xffff, 1, 0, 0x6b4c, 0xffccf7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "accent_red", 0x6b84, 0xe800, 1, 0, 0x6bbc, 0xf89bf7ff, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x6c04, 0xffff, 0, 0, 0x6c3c, 0xf85bf7ff, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x6cce, 0xffff, 0, 0, 0x6d00, 0xfef2f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x6d4e, 0xffff, 1, 0, 0x6d7e, 0xfeb3f7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x6dc0, 0xffff, 1, 0, 0x6df0, 0xfe7af7fe, 0x71fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "accent_red", 0x6e28, 0xe800, 1, 0, 0x6e60, 0xff49f7fe, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x6ea8, 0xffff, 0, 0, 0x6ee0, 0xff09f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_silver", 0x7134, 0x8410, 1, 0, 0x7148, 0xfdd5f7fe, 0x4110f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x72f4, 0xffff, 0, 0, 0x732c, 0xfce3f7fe, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x73fe, 0xffff, 9, 0, 0x740e, 0xfb6bf7fe, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x7612, 0xffff, 10, 0, 0x7622, 0xfa61f7fe, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x78b0, 0xffff, 10, 0, 0x78c0, 0xf912f7fe, 0x7afff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x7be2, 0xffff, 7, 0, 0x7bf6, 0xff77f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x7de0, 0xffff, 8, 0, 0x7df4, 0xfe78f7fd, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x8022, 0xffff, 7, 0, 0x8038, 0xfd56f7fd, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_silver", 0x8302, 0x8410, 0, 0, 0x8310, 0xff85f7fd, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_silver", 0x833e, 0x8410, 0, 0, 0x834c, 0xff67f7fd, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_silver", 0x8376, 0x8410, 0, 0, 0x8384, 0xff4bf7fd, 0x4010f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_silver", 0x83b4, 0x8410, 7, 0, 0x83c2, 0xff2cf7fd, 0x4710f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_silver", 0x8404, 0x8410, 6, 0, 0x8410, 0xff05f7fd, 0x4610f248 },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x8480, 0xffff, 0, 0, 0x84b4, 0xfb18f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x84f4, 0xffff, 0, 0, 0x8524, 0xfae0f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x8598, 0xffff, 0, 0, 0x85ce, 0xfa8bf7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x861c, 0xffff, 0, 0, 0x8650, 0xfa4af7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x8690, 0xffff, 0, 0, 0x86c0, 0xfa12f7fd, 0x70fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_green", 0x92ee, 0x07e0, 6, 0, 0x9306, 0xfcf6f7fc, 0x66fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_green", 0x93ca, 0x07e0, 7, 0, 0x93e0, 0xfc89f7fc, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_green", 0x97b4, 0x07e0, 8, 0, 0x97f8, 0xfa7df7fc, 0x68fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_green", 0x9ad8, 0x07e0, 0, 0, 0x9aea, 0xf904f7fc, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_green", 0x9b00, 0x07e0, 0, 0, 0x9b12, 0xf8f0f7fc, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_green", 0x9b2c, 0x07e0, 1, 0, 0x9b50, 0xf8d1f7fc, 0x61fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_green", 0x9b80, 0x07e0, 0, 0, 0x9b8e, 0xfb46f7fc, 0x60fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x9ba6, 0xffff, 8, 0, 0x9bd2, 0xff89f7fb, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_green", 0x9f3e, 0x07e0, 7, 0, 0x9f58, 0xfecdf7fb, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0x9f88, 0xffff, 7, 0, 0x9f9c, 0xfda4f7fb, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_green", 0xa08a, 0x07e0, 7, 0, 0xa09a, 0xf8c0f7fc, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0xa116, 0xffff, 7, 0, 0xa126, 0xfcdff7fb, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_green", 0xa234, 0x07e0, 8, 0, 0xa244, 0xffebf7fb, 0x68fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0xa2d8, 0xffff, 8, 0, 0xa2e8, 0xfbfef7fb, 0x78fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0xa49e, 0xffff, 9, 0, 0xa4b0, 0xfb1af7fb, 0x79fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "accent_red", 0xa572, 0xe800, 1, 0, 0xa586, 0xfbb6f7fb, 0x4168f44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "native_green", 0xa5ae, 0x07e0, 7, 0, 0xa5c6, 0xfb96f7fb, 0x67fcf44f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "digits_wattage", 0xa5f6, 0xffff, 7, 0, 0xa60a, 0xfa6df7fb, 0x77fff64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "highlight_yellow", 0xc41e, 0xffe0, 4, 0, 0xc432, 0xfc60f7f9, 0x74e0f64f },
    { FirmwareModel::Drag3, FirmwareEdition::Tc, 0x311c, "highlight_yellow", 0xc55e, 0xffe0, 0, 0, 0xc572, 0xfbc0f7f9, 0x70e0f64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "highlight_yellow", 0x1fb8, 0xffe0, 1, 0, 0x1fcc, 0xf9fef002, 0x71e0f64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x1fd0, 0xffff, 1, 0, 0x1fe4, 0xf9f2f002, 0x71fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x2010, 0xffff, 5, 0, 0x2024, 0xf9d2f002, 0x75fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "accent_red", 0x246c, 0xe800, 1, 0, 0x2480, 0xffa4f001, 0x4168f44f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x2484, 0xffff, 1, 0, 0x2498, 0xff98f001, 0x71fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "highlight_yellow", 0x2516, 0xffe0, 2, 0, 0x252a, 0xff4ff001, 0x72e0f64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x2562, 0xffff, 7, 0, 0x2578, 0xff28f001, 0x77fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "accent_red", 0x2966, 0xe800, 1, 0, 0x297a, 0xfd27f001, 0x4168f44f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "accent_cyan", 0x297e, 0x075f, 1, 0, 0x2992, 0xfd1bf001, 0x715ff240 },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x29a4, 0xffff, 1, 0, 0x29b8, 0xfd08f001, 0x71fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "accent_red", 0x2a10, 0xe800, 1, 0, 0x2a24, 0xfcd2f001, 0x4168f44f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x2a28, 0xffff, 1, 0, 0x2a3c, 0xfcc6f001, 0x71fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "accent_red", 0x2a76, 0xe800, 1, 0, 0x2a8a, 0xfc9ff001, 0x4168f44f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x2a8e, 0xffff, 1, 0, 0x2aa2, 0xfc93f001, 0x71fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "native_orange", 0x2ade, 0xeb00, 1, 0, 0x2af2, 0xfc6bf001, 0x416bf44f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x2af6, 0xffff, 1, 0, 0x2b0a, 0xfc5ff001, 0x71fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x2b2e, 0xffff, 8, 0, 0x2b6a, 0xfc2ff001, 0x78fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "native_green", 0x2bfa, 0x07e0, 11, 0, 0x2c10, 0xfbdcf001, 0x6bfcf44f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "highlight_yellow", 0x2d32, 0xffe0, 1, 0, 0x2d46, 0xfb41f001, 0x71e0f64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x2d4a, 0xffff, 1, 0, 0x2d5e, 0xfb35f001, 0x71fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "native_orange", 0x2d98, 0xeb00, 1, 0, 0x2dac, 0xfb0ef001, 0x416bf44f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x2db0, 0xffff, 1, 0, 0x2dc4, 0xfb02f001, 0x71fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "native_orange", 0x2dfc, 0xeb00, 1, 0, 0x2e10, 0xfadcf001, 0x416bf44f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x2e14, 0xffff, 1, 0, 0x2e28, 0xfad0f001, 0x71fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "native_lime", 0x2e62, 0x5fe0, 1, 0, 0x2e76, 0xfaa9f001, 0x71e0f645 },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x2e7a, 0xffff, 1, 0, 0x2e8e, 0xfa9df001, 0x71fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "accent_red", 0x2ec8, 0xe800, 1, 0, 0x2edc, 0xfa76f001, 0x4168f44f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x2ee0, 0xffff, 1, 0, 0x2ef4, 0xfa6af001, 0x71fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "native_silver", 0x3404, 0x8410, 2, 0, 0x3418, 0xffd8f000, 0x4210f248 },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x34cc, 0xffff, 6, 0, 0x34dc, 0xfe5ef000, 0x76fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x36c2, 0xffff, 6, 0, 0x36d0, 0xfd64f000, 0x76fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x38e0, 0xffff, 7, 0, 0x38f2, 0xfc53f000, 0x77fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x3a80, 0xffff, 9, 0, 0x3a92, 0xfb83f000, 0x79fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x3cac, 0xffff, 10, 0, 0x3cd2, 0xfb7bf000, 0x7afff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x5434, 0xffff, 8, 0, 0x5442, 0xfeabf7fe, 0x78fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x555a, 0xffff, 7, 0, 0x556e, 0xfe15f7fe, 0x77fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "native_silver", 0x585c, 0x8410, 2, 0, 0x5870, 0xfdacf7fe, 0x4210f248 },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x5984, 0xffff, 8, 0, 0x59ce, 0xfbe5f7fe, 0x78fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x5ae8, 0xffff, 7, 0, 0x5af8, 0xfb50f7fe, 0x77fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x5cd2, 0xffff, 7, 0, 0x5ce2, 0xfa5bf7fe, 0x77fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x5f22, 0xffff, 7, 0, 0x5f36, 0xf931f7fe, 0x77fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x60ee, 0xffff, 7, 0, 0x6104, 0xf84af7fe, 0x77fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "native_silver", 0x63ba, 0x8410, 0, 0, 0x63c8, 0xfae8f7fe, 0x4010f248 },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "native_silver", 0x63f0, 0x8410, 0, 0, 0x63fe, 0xfacdf7fe, 0x4010f248 },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "native_silver", 0x642c, 0x8410, 8, 0, 0x643c, 0xfaaef7fe, 0x4810f248 },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "native_silver", 0x6478, 0x8410, 7, 0, 0x6484, 0xfa8af7fe, 0x4710f248 },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "digits_wattage", 0x64e4, 0xffff, 7, 0, 0x64f4, 0xfe52f7fd, 0x77fff64f },
    { FirmwareModel::DragMax, FirmwareEdition::Standard, 0x3021, "highlight_yellow", 0x7ef4, 0xffe0, 10, 0, 0x7f08, 0xfa60f7fc, 0x7ae0f64f },
};

uint16_t Code(const HardwareRecord& record) {
    return static_cast<uint16_t>((record.device_code[0] << 8) | record.device_code[1]);
}
uint32_t Word(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
const RecordSpec* Identity(const HardwareRecord& record) {
    for (const auto& spec : kRecords) {
        if (Code(record) == spec.code && record.image.size() == spec.size &&
            record.linked_base == spec.base && record.sp == spec.sp && record.reset == spec.reset &&
            record.image.size() >= spec.renderer + sizeof(spec.guard) &&
            std::memcmp(record.image.data() + spec.renderer, spec.guard, sizeof(spec.guard)) == 0)
            return &spec;
    }
    return nullptr;
}
const Resource* ResourceForAsset(const AssetSpec& asset) {
    const size_t last = asset.id.rfind('_');
    if (last == std::string::npos || last == 0) return nullptr;
    const size_t first = asset.id.rfind('_', last - 1);
    if (first == std::string::npos) return nullptr;
    size_t index = 0;
    for (size_t i = first + 1; i < last; ++i) {
        if (asset.id[i] < '0' || asset.id[i] > '9') return nullptr;
        index = index * 10 + static_cast<size_t>(asset.id[i] - '0');
        if (index >= sizeof(kResources) / sizeof(kResources[0])) return nullptr;
    }
    const auto& resource = kResources[index];
    return asset.id.compare(0, last, resource.id) == 0 ? &resource : nullptr;
}
const char* RoleLabel(const char* role) {
    if (std::strcmp(role, "digits_wattage") == 0) return "Native white text / digits";
    if (std::strcmp(role, "accent_red") == 0) return "Native red / warnings";
    if (std::strcmp(role, "accent_cyan") == 0) return "Native cyan";
    if (std::strcmp(role, "highlight_yellow") == 0) return "Native yellow / SMART";
    if (std::strcmp(role, "native_orange") == 0) return "Native orange";
    if (std::strcmp(role, "native_green") == 0) return "Native green / temperature control";
    if (std::strcmp(role, "native_lime") == 0) return "Native lime";
    if (std::strcmp(role, "native_muted_red") == 0) return "Native muted red";
    if (std::strcmp(role, "native_max_accent") == 0) return "MAX native muted yellow";
    if (std::strcmp(role, "native_gray") == 0) return "Native dark gray";
    return "Native silver";
}
bool ReadColor(const HardwareRecord& record, const ColorSite& site, uint16_t& color) {
    if (site.offset > record.image.size() || record.image.size() - site.offset < 4 ||
        site.guardOffset > record.image.size() || record.image.size() - site.guardOffset < 4) return false;
    uint8_t context[4];
    std::memcpy(context, record.image.data() + site.guardOffset, sizeof(context));
    if (!FirmwareLayout::NormalizeThreeMaxLayoutContext(record, record.linked_base + site.guardOffset,
        context, sizeof(context)) || Word(context) != site.guard) return false;
    const uint8_t* p = record.image.data() + site.offset;
    if (site.kind == 1) {
        const uint32_t value = Word(p);
        if (value > 0xffff) return false;
        color = static_cast<uint16_t>(value);
        return true;
    }
    if (DecodeMovwInstruction(p, site.rd, color)) return true;
    // Some stock constants use Thumb's modified-immediate MOV.W encoding.
    // A color edit replaces it with an equally-sized, flag-preserving MOVW.
    if (Word(p) != site.stockInstruction) return false;
    color = site.stock;
    return true;
}
}

bool Detect(const FirmwarePackage& fw, FirmwareModel& model, FirmwareEdition& edition) {
    if (fw.records.empty()) return false;
    const RecordSpec* first = Identity(fw.records.front());
    if (!first) return false;
    size_t expected = 0;
    for (const auto& spec : kRecords)
        if (spec.model == first->model && spec.edition == first->edition) ++expected;
    if (expected != fw.records.size()) return false;
    for (size_t i = 0; i < fw.records.size(); ++i) {
        const auto* spec = Identity(fw.records[i]);
        if (!spec || spec->model != first->model || spec->edition != first->edition) return false;
        for (size_t j = 0; j < i; ++j)
            if (Code(fw.records[j]) == spec->code) return false;
    }
    model = first->model;
    edition = first->edition;
    return true;
}

std::vector<AssetSpec> Assets(FirmwareModel model, FirmwareEdition edition) {
    std::vector<AssetSpec> assets;
    for (const auto& resource : kResources) {
        if (resource.model != model || resource.edition != edition) continue;
        const size_t characterCount = std::strlen(resource.characters);
        for (size_t i = 0; i < resource.count; ++i) {
            AssetSpec asset;
            char suffix[16];
            std::snprintf(suffix, sizeof(suffix), "_%02u", static_cast<unsigned>(i));
            asset.id = std::string(resource.id) + suffix;
            if (i < characterCount) {
                const char character = resource.characters[i];
                asset.title = character >= '0' && character <= '9' ? "Digit " : "Glyph ";
                asset.title += character;
            } else if (characterCount) {
                asset.title = "Unavailable digit dash";
            } else {
                asset.title = resource.title;
            }
            asset.group = resource.group;
            asset.width = resource.width;
            asset.height = resource.height;
            asset.ordinal = i;
            asset.format = resource.format;
            asset.sourceCode = resource.source;
            for (const auto& location : resource.addresses) {
                if (!location.first) continue;
                const uint32_t address = location.second + static_cast<uint32_t>(i * asset.byte_len());
                asset.recordAddresses.emplace_back(location.first, address);
                if (location.first == asset.sourceCode) asset.address = address;
            }
            assets.push_back(std::move(asset));
        }
    }
    return assets;
}

bool RecordPaletteColor(const FirmwarePackage& fw, uint16_t code, const char* role, Rgb8& color) {
    if (!role) return false;
    for (const auto& record : fw.records) {
        if (Code(record) != code) continue;
        const auto* identity = Identity(record);
        if (!identity) return false;
        for (const auto& site : kColorSites) {
            if (site.code != code || site.model != identity->model || site.edition != identity->edition ||
                std::strcmp(site.role, role) != 0) continue;
            uint16_t value = 0;
            if (!ReadColor(record, site, value)) return false;
            color = Rgb8::FromRgb565(value);
            return true;
        }
    }
    return false;
}

bool DetectColors(const FirmwarePackage& fw, std::vector<PaletteState>& palette, Rgb8& background) {
    FirmwareModel model;
    FirmwareEdition edition;
    if (!Detect(fw, model, edition)) return false;
    std::vector<PaletteState> result;
    for (const auto& site : kColorSites) {
        if (site.model != model || site.edition != edition) continue;
        const auto existing = std::find_if(result.begin(), result.end(), [&](const PaletteState& state) {
            return std::strcmp(state.spec.id, site.role) == 0;
        });
        if (existing != result.end()) continue;
        Rgb8 current;
        if (!RecordPaletteColor(fw, site.code, site.role, current)) return false;
        const Rgb8 original = Rgb8::FromRgb565(site.stock);
        result.push_back({{site.role, RoleLabel(site.role), "Native RGB565 foreground; background remains black", original}, current});
    }
    // Reject a damaged guard at any site, not just the representative color.
    for (const auto& record : fw.records) {
        for (const auto& site : kColorSites) {
            if (site.model != model || site.edition != edition || site.code != Code(record)) continue;
            uint16_t value = 0;
            if (!ReadColor(record, site, value)) return false;
        }
    }
    palette = std::move(result);
    background = {0, 0, 0};
    return true;
}

bool BuildColorPatches(const FirmwarePackage& fw, const std::vector<PaletteState>& palette,
    Rgb8 background, std::vector<CodePatch>& patches, std::string& error) {
    FirmwareModel model;
    FirmwareEdition edition;
    if (!Detect(fw, model, edition)) { error = "Unrecognized DRAG 3 / MAX native profile"; return false; }
    if (background != Rgb8(0, 0, 0)) { error = "This firmware uses a fixed black background"; return false; }
    std::vector<CodePatch> result;
    for (size_t index = 0; index < fw.records.size(); ++index) {
        const auto& record = fw.records[index];
        for (const auto& site : kColorSites) {
            if (site.model != model || site.edition != edition || site.code != Code(record)) continue;
            const auto state = std::find_if(palette.begin(), palette.end(), [&](const PaletteState& item) {
                return std::strcmp(item.spec.id, site.role) == 0;
            });
            uint16_t current = 0;
            if (!ReadColor(record, site, current)) { error = "Native color instruction guard mismatch"; return false; }
            if (state == palette.end()) { error = std::string("Missing native palette role: ") + site.role; return false; }
            const uint16_t target = state->current.ToRgb565();
            if (target == current) continue;
            CodePatch patch;
            patch.id = std::string("palette_") + site.role;
            patch.original.assign(record.image.begin() + site.offset, record.image.begin() + site.offset + 4);
            if (site.kind == 0 && target != site.stock) patch.replacement = MovwInstruction(site.rd, target);
            else {
                const uint32_t word = target == site.stock ? site.stockInstruction : target;
                patch.replacement = {uint8_t(word), uint8_t(word >> 8), uint8_t(word >> 16), uint8_t(word >> 24)};
            }
            patch.locations.resize(fw.records.size());
            patch.locations[index].push_back(site.offset);
            result.push_back(std::move(patch));
        }
    }
    patches = std::move(result);
    return true;
}

const char* PaletteId(const AssetSpec& asset, bool) {
    const auto* resource = ResourceForAsset(asset);
    return resource && resource->role[0] ? resource->role : nullptr;
}
bool UsesModeColor(const AssetSpec&) { return false; }
Rgb8 Foreground(const FirmwarePackage&, const AssetSpec& asset,
    const std::vector<PaletteState>& palette, bool cyanMode) {
    const auto* resource = ResourceForAsset(asset);
    if (resource && resource->fixedWhite) return Rgb8(255, 255, 255);
    const char* role = PaletteId(asset, cyanMode);
    if (role) {
        for (const auto& state : palette)
            if (std::strcmp(state.spec.id, role) == 0) return state.current.ScreenColor();
    }
    return role ? Rgb8(255, 255, 255) : Rgb8(0, 0, 0);
}
Rgb8 Background(const FirmwarePackage&, const AssetSpec&, const std::vector<PaletteState>&, bool) {
    return {0, 0, 0};
}
std::vector<Preset> Presets(FirmwareModel model, FirmwareEdition edition) {
    std::vector<Preset> presets;
    for (const auto& resource : kResources) {
        if (resource.model != model || resource.edition != edition || !resource.characters[0]) continue;
        Preset preset;
        preset.name = resource.title;
        preset.description = std::string("Characters: ") + resource.characters +
            (resource.rotation ? "; landscape (stored counterclockwise)" : "; portrait");
        preset.width = resource.width;
        preset.height = resource.height;
        preset.rotationQuarterTurns = resource.rotation;
        for (size_t i = 0; resource.characters[i]; ++i) {
            char suffix[20];
            std::snprintf(suffix, sizeof(suffix), "_%02u.png", static_cast<unsigned>(i));
            preset.glyphs.push_back({resource.characters[i], std::string(resource.id) + suffix});
        }
        presets.push_back(std::move(preset));
    }
    return presets;
}
} // namespace ThreeMaxFirmware
