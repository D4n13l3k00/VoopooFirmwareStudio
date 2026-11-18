#pragma once

// Factory-only Thumb render-site profiles. Offsets are image-relative; records
// are selected by code, image size and linked base, never by package order.
// Native MOV.W constants may be replaced by equal-width MOVW instructions.
// Stock bytes are retained per site because Factory combines three UI generations.
namespace {
enum class FactoryColorEncoding : uint8_t { Rgb, SplitRgb, Immediate565, Literal565 };
struct FactoryColorSite {
    uint32_t offset;
    uint64_t stock;
    uint32_t before, after, before_mask, after_mask;
    uint32_t reference_offset, reference;
    uint8_t role, reg;
    FactoryColorEncoding encoding;
};
struct FactoryColorHardware {
    ColorHardwareIdentity identity;
    const FactoryColorSite* sites;
    size_t site_count;
};
static constexpr const char* kFactoryColorRoleIds[] = { "accent_red", "accent_cyan", "digits_wattage", "digits_stats", "highlight_yellow", "service_green" };
static constexpr uint16_t kFactoryColorBaselines[] = { 0xe800, 0x07ff, 0xffff, 0xffff, 0xffe0, 0x77e0 };

static constexpr FactoryColorSite kFactoryS3013ColorSites[] = {
    { 0x2088, 0x200021db22ffULL, 0x2803bf11, 0x0000bf1e, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x2090, 0x20f321012233ULL, 0xbf1e0000, 0xfd15f002, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x212a, 0x7afff64fULL, 0xf101fb17, 0xa500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 10, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x29a0, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2f66, 0x77fff64fULL, 0x02fff011, 0xe9cdd007, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 7, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x31e0, 0x78fff64fULL, 0xbf142a00, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x31e4, 0x79fff64fULL, 0x00000000, 0xbf882d17, 0x00000000, 0xffffffff, 0x0, 0x00000000, 2, 9, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x3202, 0x77fff64fULL, 0x2600f101, 0x7600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 7, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x4b14, 0x20f321012233ULL, 0x82abf000, 0xffd3f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x506a, 0x200021db22ffULL, 0x8ff0e8bd, 0xfd28f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x5094, 0x20f321012233ULL, 0xd06f2803, 0xfd13f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x50d0, 0x207621ff2200ULL, 0xd0580f00, 0xfcf5f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 5, 0, FactoryColorEncoding::Rgb }, // service_green
    { 0x5174, 0x200021db22ffULL, 0xe7ff87f0, 0xfca3f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x5182, 0x20f321012233ULL, 0xe78e4605, 0xfc9cf7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x51d0, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x523a, 0x20f321012233ULL, 0x81e5f000, 0xfc40f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x52f2, 0x78fff64fULL, 0x1106fb01, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5478, 0x71fff64fULL, 0xb2caf1f8, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x54aa, 0x71fff64fULL, 0xb2caf1f8, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x54de, 0x71fff64fULL, 0x3207f3c1, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5518, 0x71fff64fULL, 0x08ca2101, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5546, 0x71fff64fULL, 0x0241eb06, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5604, 0x200021db22ffULL, 0x8ff0e8bd, 0xfa5bf7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x565e, 0x76fff64fULL, 0x1105fb01, 0x6400e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x571a, 0x200021db22ffULL, 0x2803bf11, 0x0000bf1e, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x5722, 0x20f321012233ULL, 0xbf1e0000, 0xf9ccf7ff, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x5772, 0x200021db22ffULL, 0x2803bf11, 0x0000bf1e, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x577a, 0x20f321012233ULL, 0xbf1e0000, 0xf9a0f7ff, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x57ae, 0x76fff64fULL, 0x1104fb01, 0x6500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x589c, 0x76fff64fULL, 0x1104fb01, 0x6500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x595c, 0x200021db22ffULL, 0x2803bf11, 0x0000bf1e, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x5964, 0x20f321012233ULL, 0xbf1e0000, 0xf8abf7ff, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x7034, 0x20f321dd2201ULL, 0x240bf865, 0xfd43f7fd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x7076, 0x20f321dd2201ULL, 0x2403f844, 0xfd22f7fd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x70e6, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x7160, 0x20f321dd2201ULL, 0x2503ffcf, 0xfcadf7fd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
};

static constexpr FactoryColorSite kFactoryS3014ColorSites[] = {
    { 0x2088, 0x200021db22ffULL, 0x2803bf11, 0x0000bf1e, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x2090, 0x20f321012233ULL, 0xbf1e0000, 0xfd19f002, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x212a, 0x7afff64fULL, 0xf101fb17, 0xa500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 10, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x29a0, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2f66, 0x77fff64fULL, 0x02fff011, 0xe9cdd007, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 7, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x31e0, 0x78fff64fULL, 0xbf142a00, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x31e4, 0x79fff64fULL, 0x00000000, 0xbf882d17, 0x00000000, 0xffffffff, 0x0, 0x00000000, 2, 9, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x3202, 0x77fff64fULL, 0x2600f101, 0x7600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 7, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x4b1c, 0x20f321012233ULL, 0x82abf000, 0xffd3f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x5072, 0x200021db22ffULL, 0x8ff0e8bd, 0xfd28f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x509c, 0x20f321012233ULL, 0xd06f2803, 0xfd13f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x50d8, 0x207621ff2200ULL, 0xd0580f00, 0xfcf5f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 5, 0, FactoryColorEncoding::Rgb }, // service_green
    { 0x517c, 0x200021db22ffULL, 0xe7ff87f0, 0xfca3f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x518a, 0x20f321012233ULL, 0xe78e4605, 0xfc9cf7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x51d8, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x5242, 0x20f321012233ULL, 0x81e5f000, 0xfc40f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x52fa, 0x78fff64fULL, 0x1106fb01, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5480, 0x71fff64fULL, 0xb2caf1f8, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x54b2, 0x71fff64fULL, 0xb2caf1f8, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x54e6, 0x71fff64fULL, 0x3207f3c1, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5520, 0x71fff64fULL, 0x08ca2101, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x554e, 0x71fff64fULL, 0x0241eb06, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x560c, 0x200021db22ffULL, 0x8ff0e8bd, 0xfa5bf7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x5666, 0x76fff64fULL, 0x1105fb01, 0x6400e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5722, 0x200021db22ffULL, 0x2803bf11, 0x0000bf1e, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x572a, 0x20f321012233ULL, 0xbf1e0000, 0xf9ccf7ff, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x577a, 0x200021db22ffULL, 0x2803bf11, 0x0000bf1e, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x5782, 0x20f321012233ULL, 0xbf1e0000, 0xf9a0f7ff, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x57b6, 0x76fff64fULL, 0x1104fb01, 0x6500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x58a4, 0x76fff64fULL, 0x1104fb01, 0x6500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5964, 0x200021db22ffULL, 0x2803bf11, 0x0000bf1e, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x596c, 0x20f321012233ULL, 0xbf1e0000, 0xf8abf7ff, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x703c, 0x20f321dd2201ULL, 0x240bf861, 0xfd43f7fd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x707e, 0x20f321dd2201ULL, 0x2403f840, 0xfd22f7fd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x70ee, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x7168, 0x20f321dd2201ULL, 0x2503ffcb, 0xfcadf7fd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
};

static constexpr FactoryColorSite kFactoryS3029ColorSites[] = {
    { 0x1fce, 0x79fff64fULL, 0x484480b7, 0x9000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 9, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x211c, 0x66dff240ULL, 0x2803bf16, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x2120, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x25aa, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2dc8, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2ecc, 0x4568f44fULL, 0x2a00bf04, 0xf64f2037, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4bae, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x4df6, 0x4568f44fULL, 0x0600f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4e0c, 0x65dff240ULL, 0x26022903, 0xbf182601, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x4e14, 0x4568f44fULL, 0xbf182601, 0x093bf04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4ec4, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x65f4, 0x20f321dd2201ULL, 0x240bfd8f, 0xfbebf7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x6636, 0x20f321dd2201ULL, 0x2403fd6e, 0xfbcaf7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x66a6, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x6720, 0x20f321dd2201ULL, 0x2503fcf9, 0xfb55f7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    // Literal tables abut editable decimal bitmaps: guard the non-asset prefix
    // and the verified pointer reference, never the following bitmap pixels.
    { 0xaae2, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x4dd0, 0x0800dae2, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xaae4, 0x6dfULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x4dd0, 0x0800dae2, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryS3034ColorSites[] = {
    { 0x1fce, 0x79fff64fULL, 0x484480b7, 0x9000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 9, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x211c, 0x66dff240ULL, 0x2803bf16, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x2120, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x25aa, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2dc8, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2ecc, 0x4568f44fULL, 0x2a00bf04, 0xf64f2037, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4bae, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x4df6, 0x4568f44fULL, 0x0600f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4e0c, 0x65dff240ULL, 0x26022903, 0xbf182601, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x4e14, 0x4568f44fULL, 0xbf182601, 0x093bf04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4ec4, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x65f4, 0x20f321dd2201ULL, 0x240bfd8f, 0xfbebf7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x6636, 0x20f321dd2201ULL, 0x2403fd6e, 0xfbcaf7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x66a6, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x6720, 0x20f321dd2201ULL, 0x2503fcf9, 0xfb55f7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0xad6e, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x4dd0, 0x0800dd6e, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xad70, 0x6dfULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x4dd0, 0x0800dd6e, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryS3103ColorSites[] = {
    // This generation's independent header is explicitly grouped with the
    // large readout, matching the shared constant in the other Factory layouts.
    { 0x26fa, 0x71fff64fULL, 0x482e8008, 0x1800e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 1, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x27f6, 0x74fff240ULL, 0x2803bf16, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 4, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x27fa, 0x4468f44fULL, 0x00000000, 0x0f06f1b9, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 4, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x2ce0, 0x79fff64fULL, 0x1107fb01, 0x9800e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x352e, 0x77fff64fULL, 0x0040eb01, 0xe9cd9700, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 7, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x36cc, 0x4568f44fULL, 0x2a00bf04, 0xf64f2037, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x592c, 0x7afff64fULL, 0x0d800005, 0xa900e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 10, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x59ac, 0x71fff64fULL, 0xb2ca71e1, 0x1900e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x59ee, 0x75fff64fULL, 0x0141eb05, 0x5900e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 5, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5e8a, 0x4568f44fULL, 0x4614b083, 0x0c32f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5ea6, 0x75fff240ULL, 0x2903bf16, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x5eaa, 0x4568f44fULL, 0x00000000, 0x0800f04f, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5f6c, 0x78fff64fULL, 0xf101fb14, 0x8600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x83a0, 0x7ae0f64fULL, 0x4883fdb0, 0xa000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 10, FactoryColorEncoding::Immediate565 }, // highlight_yellow
    { 0x8464, 0x78fff64fULL, 0x25007120, 0x2d08e005, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0xa6b2, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x5b68, 0x0800feb2, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xa6b4, 0x7ffULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x5b68, 0x0800feb2, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryS3104ColorSites[] = {
    { 0x254a, 0x79fff64fULL, 0x484560b7, 0x9000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 9, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x269c, 0x76fff240ULL, 0x2803bf16, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x26a0, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x2b2a, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x33cc, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x3570, 0x4568f44fULL, 0x2a00bf04, 0xf64f2037, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5a3e, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5f9e, 0x4568f44fULL, 0x0600f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5fb4, 0x75fff240ULL, 0x26022903, 0xbf182601, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x5fbc, 0x4568f44fULL, 0xbf182601, 0x093bf04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x606c, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x8714, 0x74e0f64fULL, 0x487afb1d, 0xe9cd9400, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 4, FactoryColorEncoding::Immediate565 }, // highlight_yellow
    { 0x87b4, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x882c, 0x71e0f64fULL, 0xfa90f7fc, 0x91004a35, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 1, FactoryColorEncoding::Immediate565 }, // highlight_yellow
    { 0xb356, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x5c60, 0x08010b56, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xb358, 0x7ffULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x5c60, 0x08010b56, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryS3105ColorSites[] = {
    { 0x254a, 0x79fff64fULL, 0x484560b7, 0x9000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 9, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x269c, 0x76fff240ULL, 0x2803bf16, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x26a0, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x2b2a, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x33cc, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x3570, 0x4568f44fULL, 0x2a00bf04, 0xf64f2037, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5a32, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5f92, 0x4568f44fULL, 0x0600f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5fa8, 0x75fff240ULL, 0x26022903, 0xbf182601, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x5fb0, 0x4568f44fULL, 0xbf182601, 0x093bf04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x6060, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x8708, 0x74e0f64fULL, 0x487afb23, 0xe9cd9400, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 4, FactoryColorEncoding::Immediate565 }, // highlight_yellow
    { 0x87a8, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x8820, 0x71e0f64fULL, 0xfa96f7fc, 0x91004a35, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 1, FactoryColorEncoding::Immediate565 }, // highlight_yellow
    { 0xb99a, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x5c54, 0x0801119a, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xb99c, 0x7ffULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x5c54, 0x0801119a, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryS3148ColorSites[] = {
    { 0x1fce, 0x79fff64fULL, 0x484480b7, 0x9000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 9, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x211c, 0x66dff240ULL, 0x2803bf16, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x2120, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x25aa, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2dc8, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2f02, 0x4768f44fULL, 0x0f00f1b8, 0xf64f224c, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 7, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4c3e, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x4e86, 0x4568f44fULL, 0x0600f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4e9c, 0x65dff240ULL, 0x26022903, 0xbf182601, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x4ea4, 0x4568f44fULL, 0xbf182601, 0x093bf04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4f54, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x6684, 0x20f321dd2201ULL, 0x240bfd37, 0xfbebf7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x66c6, 0x20f321dd2201ULL, 0x2403fd16, 0xfbcaf7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x6736, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x67b0, 0x20f321dd2201ULL, 0x2503fca1, 0xfb55f7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0xabae, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x4e60, 0x0800dbae, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xabb0, 0x6dfULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x4e60, 0x0800dbae, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryS314DColorSites[] = {
    { 0x2576, 0x79fff64fULL, 0x484660b7, 0x9000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 9, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x26cc, 0x76fff240ULL, 0x2803bf16, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x26d0, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x2b5a, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x340c, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x35b0, 0x4668f44fULL, 0x2a00bf04, 0x0037f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5a4a, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5faa, 0x4568f44fULL, 0x0600f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5fc0, 0x75fff240ULL, 0x26022903, 0xbf182601, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x5fc8, 0x4568f44fULL, 0xbf182601, 0x093bf04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x607c, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x873c, 0x74e0f64fULL, 0x487afb17, 0xe9cd9400, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 4, FactoryColorEncoding::Immediate565 }, // highlight_yellow
    { 0x87dc, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x8854, 0x71e0f64fULL, 0xfa8af7fc, 0x91004a35, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 1, FactoryColorEncoding::Immediate565 }, // highlight_yellow
    { 0xb092, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x5c6c, 0x08010892, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xb094, 0x7ffULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x5c6c, 0x08010892, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryX3011ColorSites[] = {
    { 0x1fe8, 0x200021db22ffULL, 0xbf032801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x1fee, 0x20f32101bf1c2233ULL, 0x00000000, 0xfcf9f002, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::SplitRgb }, // accent_red
    { 0x2086, 0x7afff64fULL, 0xf101fb17, 0xa500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 10, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x28e8, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2eae, 0x77fff64fULL, 0x02fff011, 0xe9cdd007, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 7, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x3128, 0x78fff64fULL, 0xbf142a00, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x312c, 0x79fff64fULL, 0x00000000, 0xbf882d17, 0x00000000, 0xffffffff, 0x0, 0x00000000, 2, 9, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x314a, 0x77fff64fULL, 0x2600f101, 0x7600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 7, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x4a38, 0x20f321012233ULL, 0x82abf000, 0xffd5f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x4f8e, 0x200021db22ffULL, 0x8ff0e8bd, 0xfd2af7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x4fb2, 0x200021db22ffULL, 0xbf032801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x4fb8, 0x20f32101bf1c2233ULL, 0x00000000, 0xfd14f7ff, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::SplitRgb }, // accent_red
    { 0x4ff0, 0x207621ff2200ULL, 0xd0410f00, 0xfcf9f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 5, 0, FactoryColorEncoding::Rgb }, // service_green
    { 0x5074, 0x20f321012233ULL, 0x81f0e8bd, 0xfcb7f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x50c0, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x5126, 0x20f321012233ULL, 0x81e5f000, 0xfc5ef7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x51de, 0x78fff64fULL, 0x1106fb01, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5364, 0x71fff64fULL, 0xb2caf1f8, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5396, 0x71fff64fULL, 0xb2caf1f8, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x53ca, 0x71fff64fULL, 0x3207f3c1, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5404, 0x71fff64fULL, 0x08ca2101, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5432, 0x71fff64fULL, 0x0241eb06, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x54f0, 0x200021db22ffULL, 0x8ff0e8bd, 0xfa79f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x554a, 0x76fff64fULL, 0x1105fb01, 0x6400e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5604, 0x200021db22ffULL, 0xbf032801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x560a, 0x20f32101bf1c2233ULL, 0x00000000, 0xf9ebf7ff, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::SplitRgb }, // accent_red
    { 0x565c, 0x200021db22ffULL, 0xbf032801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x5662, 0x20f32101bf1c2233ULL, 0x00000000, 0xf9bff7ff, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::SplitRgb }, // accent_red
    { 0x5698, 0x76fff64fULL, 0x1104fb01, 0x6500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5788, 0x76fff64fULL, 0x1104fb01, 0x6500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5846, 0x200021db22ffULL, 0xbf032801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x584c, 0x20f32101bf1c2233ULL, 0x00000000, 0xf8caf7ff, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::SplitRgb }, // accent_red
    { 0x6f18, 0x20f321dd2201ULL, 0x240bf897, 0xfd65f7fd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x6f5a, 0x20f321dd2201ULL, 0x2403f876, 0xfd44f7fd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x6fca, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x7046, 0x20f321dd2201ULL, 0x2403f800, 0xfccef7fd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
};

static constexpr FactoryColorSite kFactoryX3012ColorSites[] = {
    { 0x1fe8, 0x200021db22ffULL, 0xbf032801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x1fee, 0x20f32101bf1c2233ULL, 0x00000000, 0xfcfdf002, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::SplitRgb }, // accent_red
    { 0x2086, 0x7afff64fULL, 0xf101fb17, 0xa500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 10, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x28e8, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2eae, 0x77fff64fULL, 0x02fff011, 0xe9cdd007, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 7, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x3128, 0x78fff64fULL, 0xbf142a00, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x312c, 0x79fff64fULL, 0x00000000, 0xbf882d17, 0x00000000, 0xffffffff, 0x0, 0x00000000, 2, 9, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x314a, 0x77fff64fULL, 0x2600f101, 0x7600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 7, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x4a40, 0x20f321012233ULL, 0x82abf000, 0xffd5f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x4f96, 0x200021db22ffULL, 0x8ff0e8bd, 0xfd2af7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x4fba, 0x200021db22ffULL, 0xbf032801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x4fc0, 0x20f32101bf1c2233ULL, 0x00000000, 0xfd14f7ff, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::SplitRgb }, // accent_red
    { 0x4ff8, 0x207621ff2200ULL, 0xd0410f00, 0xfcf9f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 5, 0, FactoryColorEncoding::Rgb }, // service_green
    { 0x507c, 0x20f321012233ULL, 0x81f0e8bd, 0xfcb7f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x50c8, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x512e, 0x20f321012233ULL, 0x81e5f000, 0xfc5ef7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::Rgb }, // accent_red
    { 0x51e6, 0x78fff64fULL, 0x1106fb01, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x536c, 0x71fff64fULL, 0xb2caf1f8, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x539e, 0x71fff64fULL, 0xb2caf1f8, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x53d2, 0x71fff64fULL, 0x3207f3c1, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x540c, 0x71fff64fULL, 0x08ca2101, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x543a, 0x71fff64fULL, 0x0241eb06, 0x1500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 1, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x54f8, 0x200021db22ffULL, 0x8ff0e8bd, 0xfa79f7ff, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x5552, 0x76fff64fULL, 0x1105fb01, 0x6400e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x560c, 0x200021db22ffULL, 0xbf032801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x5612, 0x20f32101bf1c2233ULL, 0x00000000, 0xf9ebf7ff, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::SplitRgb }, // accent_red
    { 0x5664, 0x200021db22ffULL, 0xbf032801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x566a, 0x20f32101bf1c2233ULL, 0x00000000, 0xf9bff7ff, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::SplitRgb }, // accent_red
    { 0x56a0, 0x76fff64fULL, 0x1104fb01, 0x6500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5790, 0x76fff64fULL, 0x1104fb01, 0x6500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x584e, 0x200021db22ffULL, 0xbf032801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 0, FactoryColorEncoding::Rgb }, // accent_cyan
    { 0x5854, 0x20f32101bf1c2233ULL, 0x00000000, 0xf8caf7ff, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 0, FactoryColorEncoding::SplitRgb }, // accent_red
    { 0x6f20, 0x20f321dd2201ULL, 0x240bf893, 0xfd65f7fd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x6f62, 0x20f321dd2201ULL, 0x2403f872, 0xfd44f7fd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x6fd2, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x704e, 0x20f321dd2201ULL, 0x2403fffc, 0xfccef7fd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
};

static constexpr FactoryColorSite kFactoryX302BColorSites[] = {
    { 0x1f32, 0x78fff64fULL, 0x484380ae, 0x8000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x207a, 0x66dff240ULL, 0xbf0c2801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x207e, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x2506, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2d10, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2e14, 0x4568f44fULL, 0x2a00bf04, 0xf64f2037, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4ab4, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x4cfe, 0x4568f44fULL, 0x0700f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4d12, 0x65dff240ULL, 0x2702bf07, 0x00002701, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x4d18, 0x4568f44fULL, 0x27010000, 0x083bf04f, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4dc8, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x64f4, 0x20f321dd2201ULL, 0x240bfdb1, 0xfbeff7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x6536, 0x20f321dd2201ULL, 0x2403fd90, 0xfbcef7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x65a8, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x6624, 0x20f321dd2201ULL, 0x2503fd19, 0xfb57f7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0xa9ee, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x4cd8, 0x0800d9ee, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xa9f0, 0x6dfULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x4cd8, 0x0800d9ee, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryX302CColorSites[] = {
    { 0x1f32, 0x78fff64fULL, 0x484380ae, 0x8000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x207a, 0x66dff240ULL, 0xbf0c2801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x207e, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x2506, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2d10, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2e14, 0x4568f44fULL, 0x2a00bf04, 0xf64f2037, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4abc, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x4d06, 0x4568f44fULL, 0x0700f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4d1a, 0x65dff240ULL, 0x2702bf07, 0x00002701, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x4d20, 0x4568f44fULL, 0x27010000, 0x083bf04f, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4dd0, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x64f8, 0x20f321dd2201ULL, 0x240bfdaf, 0xfbf1f7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x653a, 0x20f321dd2201ULL, 0x2403fd8e, 0xfbd0f7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x65ac, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x6628, 0x20f321dd2201ULL, 0x2503fd17, 0xfb59f7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0xaa22, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x4ce0, 0x0800da22, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xaa24, 0x6dfULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x4ce0, 0x0800da22, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryX3033ColorSites[] = {
    { 0x1f32, 0x78fff64fULL, 0x484380ae, 0x8000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x207a, 0x66dff240ULL, 0xbf0c2801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x207e, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x2506, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2d10, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2e14, 0x4568f44fULL, 0x2a00bf04, 0xf64f2037, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4ab4, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x4cfe, 0x4568f44fULL, 0x0700f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4d12, 0x65dff240ULL, 0x2702bf07, 0x00002701, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x4d18, 0x4568f44fULL, 0x27010000, 0x083bf04f, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4dc8, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x64f4, 0x20f321dd2201ULL, 0x240bfdb1, 0xfbeff7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x6536, 0x20f321dd2201ULL, 0x2403fd90, 0xfbcef7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x65a8, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x6624, 0x20f321dd2201ULL, 0x2503fd19, 0xfb57f7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0xac5a, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x4cd8, 0x0800dc5a, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xac5c, 0x6dfULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x4cd8, 0x0800dc5a, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryX3106ColorSites[] = {
    { 0x2522, 0x78fff64fULL, 0x484460ae, 0x8000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x266e, 0x76fff240ULL, 0xbf0c2801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x2672, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x2afa, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x3388, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x352c, 0x4568f44fULL, 0x2a00bf04, 0xf64f2037, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x59cc, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5f2e, 0x4568f44fULL, 0x0700f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5f42, 0x75fff240ULL, 0x2702bf07, 0x00002701, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x5f48, 0x4568f44fULL, 0x27010000, 0x083bf04f, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5ff8, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x869c, 0x79e0f64fULL, 0x487cfb35, 0x9000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 9, FactoryColorEncoding::Immediate565 }, // highlight_yellow
    { 0x873c, 0x76fff64fULL, 0x78637821, 0x78a34419, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0xb2c2, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x5bf0, 0x08010ac2, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xb2c4, 0x7ffULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x5bf0, 0x08010ac2, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryX3107ColorSites[] = {
    { 0x2522, 0x78fff64fULL, 0x484460ae, 0x8000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x266e, 0x76fff240ULL, 0xbf0c2801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x2672, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x2afa, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x3388, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x352c, 0x4568f44fULL, 0x2a00bf04, 0xf64f2037, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x59d8, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5f3a, 0x4568f44fULL, 0x0700f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5f4e, 0x75fff240ULL, 0x2702bf07, 0x00002701, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x5f54, 0x4568f44fULL, 0x27010000, 0x083bf04f, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x6004, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x86a8, 0x79e0f64fULL, 0x487cfb2f, 0x9000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 9, FactoryColorEncoding::Immediate565 }, // highlight_yellow
    { 0x8748, 0x76fff64fULL, 0x78637821, 0x78a34419, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0xb2de, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x5bfc, 0x08010ade, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xb2e0, 0x7ffULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x5bfc, 0x08010ade, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryX3108ColorSites[] = {
    { 0x2522, 0x78fff64fULL, 0x484460ae, 0x8000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x266e, 0x76fff240ULL, 0xbf0c2801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x2672, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x2afa, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x3388, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x352c, 0x4568f44fULL, 0x2a00bf04, 0xf64f2037, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x59cc, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5f2e, 0x4568f44fULL, 0x0700f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5f42, 0x75fff240ULL, 0x2702bf07, 0x00002701, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x5f48, 0x4568f44fULL, 0x27010000, 0x083bf04f, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5ff8, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x869c, 0x79e0f64fULL, 0x487cfb35, 0x9000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 9, FactoryColorEncoding::Immediate565 }, // highlight_yellow
    { 0x873c, 0x76fff64fULL, 0x78637821, 0x78a34419, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0xb926, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x5bf0, 0x08011126, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xb928, 0x7ffULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x5bf0, 0x08011126, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryX3149ColorSites[] = {
    { 0x1f32, 0x78fff64fULL, 0x484380ae, 0x8000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x207a, 0x66dff240ULL, 0xbf0c2801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x207e, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x2506, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2d10, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x2e4a, 0x4768f44fULL, 0x0f00f1b8, 0xf64f224c, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 7, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4b44, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x4d8e, 0x4568f44fULL, 0x0700f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4da2, 0x65dff240ULL, 0x2702bf07, 0x00002701, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x4da8, 0x4568f44fULL, 0x27010000, 0x083bf04f, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x4e58, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x6584, 0x20f321dd2201ULL, 0x240bfd59, 0xfbeff7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x65c6, 0x20f321dd2201ULL, 0x2403fd38, 0xfbcef7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0x6638, 0x76fff64fULL, 0x78637820, 0x78a34418, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x66b4, 0x20f321dd2201ULL, 0x2503fcc1, 0xfb57f7fe, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 0, FactoryColorEncoding::Rgb }, // highlight_yellow
    { 0xaa96, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x4d68, 0x0800da96, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xaa98, 0x6dfULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x4d68, 0x0800da96, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorSite kFactoryX314EColorSites[] = {
    { 0x253a, 0x78fff64fULL, 0x484460ae, 0x8000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x2686, 0x76fff240ULL, 0xbf0c2801, 0x00000000, 0xffffffff, 0x00000000, 0x0, 0x00000000, 1, 6, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x268a, 0x4668f44fULL, 0x00000000, 0x0f06f1b8, 0x00000000, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x2b12, 0x78fff64fULL, 0x110bfb01, 0x8700e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 8, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x33a4, 0x76fff64fULL, 0x0040eb01, 0xe9cd9600, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x3548, 0x4668f44fULL, 0x2a00bf04, 0x0037f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 6, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x59c0, 0x79fff64fULL, 0x0d800004, 0x9600e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 9, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0x5f22, 0x4568f44fULL, 0x0700f04f, 0x0032f04f, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5f36, 0x75fff240ULL, 0x2702bf07, 0x00002701, 0xffffffff, 0x0000ffff, 0x0, 0x00000000, 1, 5, FactoryColorEncoding::Immediate565 }, // accent_cyan
    { 0x5f3c, 0x4568f44fULL, 0x27010000, 0x083bf04f, 0xffff0000, 0xffffffff, 0x0, 0x00000000, 0, 5, FactoryColorEncoding::Immediate565 }, // accent_red
    { 0x5fec, 0x78fff64fULL, 0xf101fb14, 0x8500e9cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 2, 8, FactoryColorEncoding::Immediate565 }, // digits_wattage
    { 0x86a4, 0x79e0f64fULL, 0x487cfb2d, 0x9000f8cd, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 4, 9, FactoryColorEncoding::Immediate565 }, // highlight_yellow
    { 0x8744, 0x76fff64fULL, 0x78637821, 0x78a34419, 0xffffffff, 0xffffffff, 0x0, 0x00000000, 3, 6, FactoryColorEncoding::Immediate565 }, // digits_stats
    { 0xb032, 0xe800ULL, 0x00083401, 0x00000000, 0xffffffff, 0x00000000, 0x5be4, 0x08010832, 0, 0, FactoryColorEncoding::Literal565 }, // accent_red
    { 0xb034, 0x7ffULL, 0x00000008, 0x00000000, 0x0000ffff, 0x00000000, 0x5be4, 0x08010832, 1, 0, FactoryColorEncoding::Literal565 }, // accent_cyan
};

static constexpr FactoryColorHardware kFactoryDragSColorHardware[] = {
    { { 0x3013, 73440, 0x08003000 }, kFactoryS3013ColorSites, sizeof(kFactoryS3013ColorSites) / sizeof(kFactoryS3013ColorSites[0]) },
    { { 0x3014, 73452, 0x08003000 }, kFactoryS3014ColorSites, sizeof(kFactoryS3014ColorSites) / sizeof(kFactoryS3014ColorSites[0]) },
    { { 0x3029, 62388, 0x08003000 }, kFactoryS3029ColorSites, sizeof(kFactoryS3029ColorSites) / sizeof(kFactoryS3029ColorSites[0]) },
    { { 0x3034, 63040, 0x08003000 }, kFactoryS3034ColorSites, sizeof(kFactoryS3034ColorSites) / sizeof(kFactoryS3034ColorSites[0]) },
    { { 0x3103, 61340, 0x08005800 }, kFactoryS3103ColorSites, sizeof(kFactoryS3103ColorSites) / sizeof(kFactoryS3103ColorSites[0]) },
    { { 0x3104, 64572, 0x08005800 }, kFactoryS3104ColorSites, sizeof(kFactoryS3104ColorSites) / sizeof(kFactoryS3104ColorSites[0]) },
    { { 0x3105, 66176, 0x08005800 }, kFactoryS3105ColorSites, sizeof(kFactoryS3105ColorSites) / sizeof(kFactoryS3105ColorSites[0]) },
    { { 0x3148, 62592, 0x08003000 }, kFactoryS3148ColorSites, sizeof(kFactoryS3148ColorSites) / sizeof(kFactoryS3148ColorSites[0]) },
    { { 0x314d, 63864, 0x08005800 }, kFactoryS314DColorSites, sizeof(kFactoryS314DColorSites) / sizeof(kFactoryS314DColorSites[0]) },
};

static constexpr FactoryColorHardware kFactoryDragXColorHardware[] = {
    { { 0x3011, 73052, 0x08003000 }, kFactoryX3011ColorSites, sizeof(kFactoryX3011ColorSites) / sizeof(kFactoryX3011ColorSites[0]) },
    { { 0x3012, 73060, 0x08003000 }, kFactoryX3012ColorSites, sizeof(kFactoryX3012ColorSites) / sizeof(kFactoryX3012ColorSites[0]) },
    { { 0x302b, 62144, 0x08003000 }, kFactoryX302BColorSites, sizeof(kFactoryX302BColorSites) / sizeof(kFactoryX302BColorSites[0]) },
    { { 0x302c, 62196, 0x08003000 }, kFactoryX302CColorSites, sizeof(kFactoryX302CColorSites) / sizeof(kFactoryX302CColorSites[0]) },
    { { 0x3033, 62764, 0x08003000 }, kFactoryX3033ColorSites, sizeof(kFactoryX3033ColorSites) / sizeof(kFactoryX3033ColorSites[0]) },
    { { 0x3106, 64424, 0x08005800 }, kFactoryX3106ColorSites, sizeof(kFactoryX3106ColorSites) / sizeof(kFactoryX3106ColorSites[0]) },
    { { 0x3107, 64452, 0x08005800 }, kFactoryX3107ColorSites, sizeof(kFactoryX3107ColorSites) / sizeof(kFactoryX3107ColorSites[0]) },
    { { 0x3108, 66060, 0x08005800 }, kFactoryX3108ColorSites, sizeof(kFactoryX3108ColorSites) / sizeof(kFactoryX3108ColorSites[0]) },
    { { 0x3149, 62312, 0x08003000 }, kFactoryX3149ColorSites, sizeof(kFactoryX3149ColorSites) / sizeof(kFactoryX3149ColorSites[0]) },
    { { 0x314e, 63768, 0x08005800 }, kFactoryX314EColorSites, sizeof(kFactoryX314EColorSites) / sizeof(kFactoryX314EColorSites[0]) },
};

} // namespace
