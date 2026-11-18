#include "firmware_menu_patch.h"
#include "embedded_firmware.h"
#include "firmware_logo_patch.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <utility>

namespace FirmwareMenu {
namespace {
constexpr uint32_t SettingsBase = 0x0801f000;
constexpr size_t StateBytes = 56, HeaderBytes = 64, FooterBytes = 24;
static_assert(StateBytes <= 56);
constexpr std::array<uint8_t, 8> Magic{'V','F','S','M','E','N','U','1'};
constexpr std::array<uint8_t, 8> EndMagic{'V','F','S','M','E','N','D','1'};
#include "firmware_menu_executor.inc"
struct Layout {
    const uint8_t* executor;
    size_t executorBytes, descriptorBytes;
    uint32_t modules;
    size_t queue, main, voltage, save, offSave;
    std::array<size_t, 3> starts;
    size_t hud, paint;
    constexpr size_t Descriptor() const { return HeaderBytes + executorBytes; }
    constexpr size_t MainTrampoline() const { return Descriptor() + descriptorBytes; }
    constexpr size_t VoltageTrampoline() const { return MainTrampoline() + 8; }
    constexpr size_t SaveTrampoline() const { return MainTrampoline() + 8 + (HasVoltage() ? 8 : 0); }
    constexpr size_t OffSaveTrampoline() const { return SaveTrampoline() + 16; }
    constexpr size_t HudTrampoline() const { return OffSaveTrampoline() + 16; }
    // Score entry stub20 + native PUSH trampoline8; Puff call stub24 + exact-LR
    // native BL trampoline12. Only Score records allocate these paint guards.
    constexpr size_t PaintStub(size_t index) const { return HudTrampoline() + 8 + index * 28; }
    constexpr size_t PaintTrampoline(size_t index) const { return HudTrampoline() + 8 + (index ? 52 : 20); }
    constexpr size_t ExtensionBytes(size_t paintGuards = 0) const {
        return HudTrampoline() + (hud ? 8 : 0) + (paintGuards ? 64 : 0) + FooterBytes;
    }
    constexpr bool HasVoltage() const { return (modules & VoltageMask) != 0; }
};
#define MENU_LAYOUT(ns,bytes,mask,hudEntry) Layout{ns::Executor, sizeof(ns::Executor), bytes, mask, \
    ns::QueueOffset, ns::MainOffset, ns::VoltageOffset, ns::SaveOffset, ns::OffsaveOffset, \
    {ns::Start0Offset, ns::Start1Offset, ns::Start2Offset}, hudEntry, ns::PaintOffset}
constexpr size_t DescriptorBytes(uint32_t modules, uint32_t fields) {
    const bool info = (modules & InformationMask) != 0, voltage = (modules & VoltageMask) != 0;
    return 132 + 4 * (voltage || (info && (fields & InfoBatteryMask))) +
        4 * (voltage || (info && (fields & InfoResistanceMask))) +
        4 * (info && (fields & InfoDurationMask)) + 12 * voltage +
        36 * ((modules & (PuffEditorMask | ScoreEditorMask | RankSelectorMask)) != 0) +
        8 * ((modules & PuffEditorMask) && (modules & (ScoreEditorMask | RankSelectorMask)));
}
#define CURRENT_LAYOUT(ns,mask,fields) MENU_LAYOUT(ns,DescriptorBytes(mask,fields),mask,ns::HudOffset)
#define INFO_LAYOUTS(prefix,mask) std::array<Layout,8>{ \
    CURRENT_LAYOUT(prefix##Base,mask,0), CURRENT_LAYOUT(prefix##Info1,mask|InformationMask,1), \
    CURRENT_LAYOUT(prefix##Info2,mask|InformationMask,2), CURRENT_LAYOUT(prefix##Info3,mask|InformationMask,3), \
    CURRENT_LAYOUT(prefix##Info4,mask|InformationMask,4), CURRENT_LAYOUT(prefix##Info5,mask|InformationMask,5), \
    CURRENT_LAYOUT(prefix##Info6,mask|InformationMask,6), CURRENT_LAYOUT(prefix##Info,mask|InformationMask,7)}
#define BOTH_LAYOUTS(prefix,mask) std::array<Layout,8>{ \
    CURRENT_LAYOUT(prefix##Voltage,mask|VoltageMask,0), CURRENT_LAYOUT(prefix##All1,mask|DefaultMask,1), \
    CURRENT_LAYOUT(prefix##All2,mask|DefaultMask,2), CURRENT_LAYOUT(prefix##All3,mask|DefaultMask,3), \
    CURRENT_LAYOUT(prefix##All4,mask|DefaultMask,4), CURRENT_LAYOUT(prefix##All5,mask|DefaultMask,5), \
    CURRENT_LAYOUT(prefix##All6,mask|DefaultMask,6), CURRENT_LAYOUT(prefix##All,mask|DefaultMask,7)}
constexpr auto InfoLayouts = INFO_LAYOUTS(,0);
constexpr auto BothLayouts = BOTH_LAYOUTS(,0);
constexpr std::array<std::array<Layout,8>,8> EditorInfoLayouts{
    InfoLayouts, INFO_LAYOUTS(Puff,4), INFO_LAYOUTS(Score,8), INFO_LAYOUTS(PuffScore,12),
    INFO_LAYOUTS(Rank,16), INFO_LAYOUTS(PuffRank,20), INFO_LAYOUTS(ScoreRank,24), INFO_LAYOUTS(PuffScoreRank,28)};
constexpr std::array<std::array<Layout,8>,8> EditorBothLayouts{
    BothLayouts, BOTH_LAYOUTS(Puff,4), BOTH_LAYOUTS(Score,8), BOTH_LAYOUTS(PuffScore,12),
    BOTH_LAYOUTS(Rank,16), BOTH_LAYOUTS(PuffRank,20), BOTH_LAYOUTS(ScoreRank,24), BOTH_LAYOUTS(PuffScoreRank,28)};
constexpr Layout AllLayout = BothLayouts[7], BaseLayout = InfoLayouts[0];
#undef INFO_LAYOUTS
#undef BOTH_LAYOUTS
#undef CURRENT_LAYOUT
#undef MENU_LAYOUT
static_assert(All::DescriptorOffset == sizeof(All::Executor));
const Layout& CurrentLayout(uint32_t modules, uint32_t fields = AllInfoFields) {
    const auto& info = EditorInfoLayouts[(modules >> 2) & 7];
    const auto& both = EditorBothLayouts[(modules >> 2) & 7];
    return (modules & VoltageMask) ? both[(modules & InformationMask) ? fields : 0] :
        info[(modules & InformationMask) ? fields : 0];
}

// Both native assembly and this direct host raster use the same source bytes.
#define MENU_INFO 1
#define MENU_VOLTAGE 1
#define MENU_INFO_FIELDS 7
#define MENU_PUFF_EDITOR 1
#define MENU_SCORE_EDITOR 1
#define MENU_RANK_SELECTOR 1
#define MENU_TEXT(name,value) constexpr char PreviewText_##name[] = value;
#include "firmware_menu_visuals.inc"
#undef MENU_TEXT
constexpr uint8_t PreviewFont[] = {
#define MENU_GLYPH(...) __VA_ARGS__,
#include "firmware_menu_visuals.inc"
#undef MENU_GLYPH
};
#undef MENU_INFO_FIELDS
#undef MENU_PUFF_EDITOR
#undef MENU_SCORE_EDITOR
#undef MENU_RANK_SELECTOR
#undef MENU_VOLTAGE
#undef MENU_INFO
static_assert(sizeof(PreviewFont) == 38 * 5);
void PreviewText(std::array<uint32_t,80*160>& pixels, unsigned x, unsigned y, const char* text) {
    for (; *text; ++text,x+=6) {
        const unsigned c = static_cast<unsigned char>(*text);
        const unsigned glyph = c >= '0' && c <= '9' ? c-'0' :
            c >= 'A' && c <= 'Z' ? c-'A'+10 : c == '>' ? 36 : 37;
        for (unsigned column = 0; column < 5 && x+column < 80; ++column) {
            const uint8_t bits = PreviewFont[glyph*5+column];
            for (unsigned row = 0; row < 7 && y+row < 160; ++row)
                if (bits & (1u << row)) pixels[(y+row)*80+x+column] = 0xffffffffu;
        }
    }
}
void PreviewNumber(std::array<uint32_t,80*160>& pixels, unsigned y, uint32_t value) {
    char digits[11];
    char* end = digits+sizeof(digits)-1;
    *end = '\0';
    do {
        *--end = char('0'+value%10);
        value /= 10;
    } while (value);
    PreviewText(pixels,34,y,end);
}
constexpr uint32_t ItemMask(Item item) {
    return item == Item::Exit ? 0u : 1u << (uint32_t(item) > 2 ? uint32_t(item)-1 : uint32_t(item));
}

// D: explicit fixed RTX TCB-pool reservation, NOT unclaimed SRAM or a stack
// shrink. Each stock box is header12 + 7*56 = 0x194 bytes. Its _init_box length
// is changed to 0x15c, leaving six boxes and excluding the FINAL 56-byte box.
// Raw, halfword-aligned full-image audits of all sixteen records found exactly
// one bootstrap B.W to task-create and one/two odd-pointer constants serving
// four guarded handle slots. Duplicate startup/UI creation paths are serialized
// in the initial application task. The initial task plus four service tasks
// need at most five TCBs; the native idle TCB is separate. No USB task creator
// exists outside this closure. _init_box links through end-block_size and null
// terminates: reinitialization cannot include the reserved final box. Free-list
// release is reachable only for allocated boxes, never the excluded box.
// The menu initializes lazily after scatterload; reset naturally clears it.
struct Profile {
    unsigned edition;
    uint16_t code;
    uint32_t poolLength, state, main, voltage, fill, queue;
    std::array<uint32_t, 2> consumers;
    std::array<uint32_t, 3> starts;
    uint32_t settings, sensors, page, activity, activityTimer;
    bool history, combined;
};
constexpr Profile Profiles[] = {
    {0,0x3013,0x08010188,0x2000100c,0x08005740,0x08008f0c,0x08007870,0x08009998,{0x080042e0,0x08004392},{0x08004408,0x0800467e},0x20001530,0x20001660,0x2000005c,0x20000098,0x200000a2,true,false},
    {0,0x3014,0x08010198,0x2000100c,0x08005740,0x08008f14,0x08007870,0x080099a0,{0x080042e0,0x08004392},{0x08004408,0x0800467e},0x20001530,0x20001660,0x2000005c,0x20000098,0x200000a2,true,false},
    {0,0x3029,0x0800f188,0x20000ffc,0x080056e4,0x080088d4,0x08007248,0x080093b4,{0x08004390,0x08004442},{0x08004070,0x080044b8,0x0800472e},0x20001520,0x20001624,0x2000005c,0x2000008c,0x20000090,false,false},
    {0,0x302a,0x0800f198,0x20000ffc,0x080056e4,0x080088dc,0x08007248,0x080093bc,{0x08004390,0x08004442},{0x08004070,0x080044b8,0x0800472e},0x20001520,0x20001624,0x2000005c,0x2000008c,0x20000090,false,false},
    {0,0x3103,0x08010258,0x20001344,0x08007e94,0x0800ba80,0x0800a0d8,0x0800d390,{0x080061aa},{0x08006224,0x08006478},0x200018a8,0x200019d0,0x2000008c,0x20000092,0x20000096,false,false},
    {0,0x3105,0x0801039c,0x20001344,0x08007e94,0x0800ba80,0x0800a0d8,0x0800d390,{0x080061aa},{0x08006224,0x08006478},0x200018a8,0x200019d0,0x2000008c,0x20000092,0x20000096,false,false},
    {0,0x314d,0x080101d0,0x20001344,0x08007e94,0x0800ba58,0x0800a090,0x0800d368,{0x080061aa},{0x08006224,0x08006478},0x200018a8,0x200019d0,0x2000008c,0x20000092,0x20000096,false,false},
    {1,0x3013,0x0800efe0,0x20001004,0x08005038,0x0800886c,0x080070fc,0x080091d8,{0x08003bcc,0x08003c7e},{0x08003cf4,0x08003f6a},0x20001528,0x20001654,0x2000008e,0x20000094,0x2000009e,true,false},
    {1,0x3014,0x0800eff0,0x20001004,0x08005038,0x08008874,0x080070fc,0x080091e0,{0x08003bcc,0x08003c7e},{0x08003cf4,0x08003f6a},0x20001528,0x20001654,0x2000008e,0x20000094,0x2000009e,true,false},
    {1,0x3029,0x0800da7c,0x20000ff4,0x08004fb0,0x08007ad8,0x08007110,0x08008790,{0x08003c7c,0x08003d2e},{0x0800395c,0x08003da4,0x0800401a},0x20001518,0x20001618,0x20000084,0x20000088,0x2000008c,false,true},
    {1,0x3034,0x0800dd08,0x20000ff4,0x08004fb0,0x08007ad8,0x08007110,0x08008790,{0x08003c7c,0x08003d2e},{0x0800395c,0x08003da4,0x0800401a},0x20001518,0x20001618,0x20000084,0x20000088,0x2000008c,false,true},
    {1,0x3148,0x0800db48,0x20000ff4,0x08004fb0,0x08007b68,0x080070f0,0x08008820,{0x08003c7c,0x08003d2e},{0x0800395c,0x08003da4,0x0800401a},0x20001518,0x20001618,0x20000084,0x20000088,0x2000008c,false,true},
    {1,0x3103,0x0800fe4c,0x20000f3c,0x08007edc,0x0800b05c,0x0800a6fe,0x0800cfd4,{0x08006242},{0x08005eb8,0x080062ba,0x080064fc},0x20001498,0x200015bc,0x2000008c,0x20000090,0x20000094,false,true},
    {1,0x3104,0x08010af0,0x20000f3c,0x08007d2c,0x0800b168,0x0800a54c,0x0800d224,{0x080062aa},{0x08005ebc,0x08006320,0x08006590},0x20001498,0x200015bc,0x2000008c,0x20000090,0x20000094,false,true},
    {1,0x3105,0x08011134,0x20000f3c,0x08007d2c,0x0800b15c,0x0800a54c,0x0800d218,{0x080062aa},{0x08005ebc,0x08006320,0x08006590},0x20001498,0x200015bc,0x2000008c,0x20000090,0x20000094,false,true},
    {1,0x314d,0x0801082c,0x20000f3c,0x08007d58,0x0800b174,0x0800a568,0x0800d244,{0x080062b0},{0x08005ebe,0x08006328,0x08006598},0x20001498,0x200015bc,0x2000008c,0x20000090,0x20000094,false,true},
};
constexpr size_t PaintGuardCount(const Profile& p) { return p.edition == 0 ? 2 : 0; }
#include "firmware_menu_profiles.inc"
uint16_t Code(const HardwareRecord& r) { return uint16_t(r.device_code[0]) << 8 | r.device_code[1]; }
uint32_t Read32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
void Set32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) b[at + i] = uint8_t(v >> (i * 8));
}
const FirmwarePackage& Stock(unsigned edition) {
    static const auto stock = [] {
        std::array<FirmwarePackage, 2> p;
        std::string error;
        LoadEmbeddedFirmware(p[0], error, FirmwareModel::DragS, FirmwareEdition::Score);
        LoadEmbeddedFirmware(p[1], error, FirmwareModel::DragS, FirmwareEdition::Factory);
        return p;
    }();
    return stock[edition];
}
const HardwareRecord* Baseline(const Profile& p) {
    const auto& stock = Stock(p.edition);
    const auto r = std::find_if(stock.records.begin(), stock.records.end(), [&](const HardwareRecord& r) { return Code(r) == p.code; });
    return r == stock.records.end() ? nullptr : &*r;
}
const Profile* Find(const HardwareRecord& r) {
    for (const auto& p : Profiles) {
        if (p.code != Code(r)) continue;
        const auto* b = Baseline(p);
        if (b && r.linked_base == b->linked_base && r.sp == b->sp && r.reset == b->reset && r.marker == b->marker) return &p;
    }
    return nullptr;
}
std::array<uint8_t, 4> Branch(uint32_t from, uint32_t to, bool link) {
    const uint32_t delta = to - from - 4;
    const uint32_t s = delta >> 24 & 1, i1 = delta >> 23 & 1, i2 = delta >> 22 & 1;
    const uint16_t a = uint16_t(0xf000 | s << 10 | (delta >> 12 & 0x3ff));
    const uint16_t b = uint16_t((link ? 0xd000 : 0x9000) | ((~i1 ^ s) & 1) << 13 | ((~i2 ^ s) & 1) << 11 | (delta >> 1 & 0x7ff));
    return {uint8_t(a), uint8_t(a >> 8), uint8_t(b), uint8_t(b >> 8)};
}
struct StartTransaction {
    uint32_t resume = 0, mask = 0, commit = 0;
    size_t prefixBytes = 0, storeBytes = 0;
};
StartTransaction Transaction(const HardwareRecord& stock, uint32_t site) {
    if (!site) return {};
    const size_t at = site - stock.linked_base;
    if (at > stock.image.size() || stock.image.size() - at < 12 ||
        Read32(stock.image.data() + at) != 0x0001f040) return {};
    const uint32_t next = Read32(stock.image.data() + at + 4);
    StartTransaction t;
    t.commit = site + 4;
    if (next == 0x7080f040 || next == 0x0000f440) {
        t.mask = next == 0x7080f040 ? 0x01000000 : 0x00800000;
        t.commit += 4;
    } else {
        const uint32_t a = next & 0xffff, b = next >> 16;
        if ((a & 0xf800) == 0xf000 && (b & 0xd000) == 0x9000) {
            const uint32_t s = a >> 10 & 1;
            const uint32_t bits = s << 24 | ((~((b >> 13 & 1) ^ s)) & 1) << 23 |
                ((~((b >> 11 & 1) ^ s)) & 1) << 22 | (a & 0x3ff) << 12 | (b & 0x7ff) << 1;
            const int32_t delta = static_cast<int32_t>(bits << 7) >> 7;
            t.commit = site + 8 + delta;
        }
    }
    const size_t commit = t.commit - stock.linked_base;
    if (commit > stock.image.size() || stock.image.size() - commit < 4) return {};
    const uint32_t store = Read32(stock.image.data() + commit);
    t.storeBytes = (store & 0xffff) == 0x6020 ? 2 : store == 0x0000f8cb ? 4 : 0;
    if (!t.storeBytes) return {};
    t.prefixBytes = t.commit == site + 4 ? 4 + t.storeBytes :
        t.commit == site + 8 && t.mask ? 8 + t.storeBytes : 8;
    t.resume = (t.commit + uint32_t(t.storeBytes)) | 1;
    return t;
}
struct Hook { uint32_t address; std::array<uint8_t, 4> bytes; size_t size = 4; };
struct HookList {
    std::array<Hook, 13> items{};
    size_t count = 0;
    void Add(Hook h) { items[count++] = h; }
    const Hook* begin() const { return items.data(); }
    const Hook* end() const { return items.data() + count; }
    size_t size() const { return count; }
};
constexpr std::array<uint32_t,16> PeriodicSavers{
    0x08009050,0x08009058,0x08008a24,0x08008a2c,0x0800bc44,0x0800bc44,0x0800bc1c,
    0x080089b0,0x080089b8,0x08007f14,0x08007f14,0x08007fa4,0x0800b838,0x0800b934,0x0800b928,0x0800b944};
#include "firmware_menu_puff_profiles.inc"
constexpr std::array<uint32_t,16> OffSavers{
    0x08009194,0x0800919c,0x08008b64,0x08008b6c,0x0800bda0,0x0800bda0,0x0800bd78,
    0x08008af4,0x08008afc,0x08008060,0x08008060,0x080080f0,0x0800b994,0x0800ba94,0x0800ba88,0x0800baa8};
// Mapped paint-only battery HUD closure, including its trailing literals.
// Every native store is stack-local; callees are LCD primitives (plus a pure
// RGB565 arithmetic helper on older builds). Native callers retain sensor,
// percentage, eligibility, mutex and cache side effects outside this closure.
struct HudPainter { uint32_t entry, end, prologue; };
constexpr std::array<HudPainter,16> HudPainters{{
    {0x0800620c,0x080062d4,0x43f0e92d}, {0x0800620c,0x080062d4,0x43f0e92d},
    {0x08006008,0x080060d0,0x43f0e92d}, {0x08006008,0x080060d0,0x43f0e92d},
    {0x080087ec,0x080088b4,0x43f0e92d}, {0x080087ec,0x080088b4,0x43f0e92d},
    {0x080087ec,0x080088cc,0x4ff0e92d},
    {0x08005a88,0x08005b50,0x43f0e92d}, {0x08005a88,0x08005b50,0x43f0e92d},
    {0x08005eb0,0x08005f58,0x43f0e92d}, {0x08005eb0,0x08005f58,0x43f0e92d},
    {0x08005eb0,0x08005f94,0x4ff0e92d}, {0x08008eb0,0x08008f58,0x43f0e92d},
    {0x08008d54,0x08008dfc,0x43f0e92d}, {0x08008d54,0x08008dfc,0x43f0e92d},
    {0x08008d94,0x08008e50,0x43f0e92d}
}};
// Native menu-independent refresh callers also paint Score/Puff. Score accepts
// best by value and is paint-only. Puff is NOT: 3013/3014 normalize the backing
// halfword before painting. Hook its first LCD fill only, after that prefix,
// and use its original ADD SP / POP.W exit. Shared four/five-digit tails and
// trailing literals belong to the guarded closure, not to the next renderer.
// Source intervals: firmware_layout_sx.cpp Instructions0..5. Native execution
// on 3029 (051cc/051da), 3013 and 3103 confirms LCD/RGB-only paint callees and
// stack-only paint stores; all accumulation/settings/safety/IRQ callers remain.
struct ScoreHudPainters {
    HudPainter score, puff;
    uint32_t puffPaint, puffExit;
    uint16_t stackWords;
};
constexpr std::array<ScoreHudPainters,7> ScorePainters{{
    {{0x08008898,0x08008a34,0x43f0e92d},{0x080083f8,0x0800876c,0x4ff0e92d},0x08008452,0x0800874a,5},
    {{0x080088a0,0x08008a3c,0x43f0e92d},{0x08008400,0x08008774,0x4ff0e92d},0x0800845a,0x08008752,5},
    {{0x08008260,0x080083fc,0x43f0e92d},{0x08007dac,0x08008134,0x4ff0e92d},0x08007e00,0x0800810a,3},
    {{0x08008268,0x08008404,0x43f0e92d},{0x08007db4,0x0800813c,0x4ff0e92d},0x08007e08,0x08008112,3},
    {{0x0800b42c,0x0800b5cc,0x43f0e92d},{0x0800aef8,0x0800b2f4,0x4ff0e92d},0x0800af42,0x0800b2cc,5},
    {{0x0800b42c,0x0800b5cc,0x43f0e92d},{0x0800aef8,0x0800b2f4,0x4ff0e92d},0x0800af42,0x0800b2cc,5},
    {{0x0800b404,0x0800b5a4,0x43f0e92d},{0x0800aed0,0x0800b2cc,0x4ff0e92d},0x0800af1a,0x0800b2a4,5}
}};
bool SaveLoad(const HardwareRecord& stock, uint32_t entry, uint32_t& pointer, uint16_t& load) {
    const size_t at = entry-stock.linked_base;
    if (at > stock.image.size() || stock.image.size()-at < 4) return false;
    const uint32_t prefix = Read32(stock.image.data()+at);
    if ((prefix & 0xff00) != 0x4800) return false;
    load = uint16_t(prefix >> 16);
    if (load != 0x6800 && load != 0x7800) return false;
    const size_t literal = (((entry+4)&~3u)+(prefix&255)*4)-stock.linked_base;
    if (literal > stock.image.size() || stock.image.size()-literal < 4) return false;
    pointer = Read32(stock.image.data()+literal);
    return true;
}
HookList Hooks(const HardwareRecord& r, const Profile& p, size_t start, const Layout& layout = AllLayout) {
    const uint32_t code = r.linked_base + uint32_t(start + HeaderBytes);
    HookList hooks;
    hooks.Add({p.main, Branch(p.main, code + uint32_t(layout.main), false)});
    if (layout.HasVoltage()) hooks.Add({p.voltage, Branch(p.voltage, code + uint32_t(layout.voltage), false)});
    const size_t index = size_t(&p-Profiles);
    hooks.Add({PeriodicSavers[index], Branch(PeriodicSavers[index], code + uint32_t(layout.save), false)});
    hooks.Add({OffSavers[index], Branch(OffSavers[index], code + uint32_t(layout.offSave), false)});
    if (layout.hud) {
        const auto at = HudPainters[index].entry;
        hooks.Add({at, Branch(at, code + uint32_t(layout.hud), false)});
    }
    if (PaintGuardCount(p)) {
        const auto& painters = ScorePainters[index];
        for (const auto pair : {std::pair{painters.score.entry,size_t(0)},std::pair{painters.puffPaint,size_t(1)}})
            hooks.Add({pair.first,Branch(pair.first,r.linked_base+uint32_t(start+layout.PaintStub(pair.second)),false)});
    }
    for (auto at : p.consumers) if (at) hooks.Add({at, Branch(at, code + uint32_t(layout.queue), true)});
    for (size_t i = 0; i < p.starts.size(); ++i) if (p.starts[i])
        hooks.Add({p.starts[i], Branch(p.starts[i], code + uint32_t(layout.starts[i]), false)});
    hooks.Add({p.poolLength, {0x5c,0x01,0,0}, 2});
    return hooks;
}
constexpr std::array<uint32_t,16> Creators{
    0x0800d730,0x0800d740,0x0800d0dc,0x0800d0ec,0x0800f980,0x0800fab8,0x0800f8ec,
    0x0800c548,0x0800c558,0x0800ba18,0x0800bb94,0x0800b9b0,0x0800f530,0x08010108,0x08010760,0x0800fe80};
struct ResourceExclusions { std::vector<uint8_t> bytes; bool valid = false; };
const ResourceExclusions& Resources(const Profile& profile) {
    static const auto cache = [] {
        std::array<ResourceExclusions,16> result;
        for (size_t i = 0; i < std::size(Profiles); ++i) {
            const auto& p = Profiles[i];
            const auto& package = Stock(p.edition);
            const auto* record = Baseline(p);
            if (!record) continue;
            auto& entry = result[i];
            entry.bytes.resize(record->image.size());
            const size_t index = size_t(record-package.records.data());
            entry.valid = true;
            for (const auto& asset : GetAllAssets(FirmwareModel::DragS,
                p.edition ? FirmwareEdition::Factory : FirmwareEdition::Score)) {
                size_t offset = 0;
                if (!asset.recordAddresses.empty()) {
                    const auto found = std::find_if(asset.recordAddresses.begin(),asset.recordAddresses.end(),
                        [&](const auto& address) { return address.first == p.code; });
                    if (found == asset.recordAddresses.end()) continue;
                    offset = found->second-record->linked_base;
                } else {
                    const uint16_t source = asset.sourceCode ? asset.sourceCode : FirmwareCanonicalCode(FirmwareModel::DragS);
                    const auto found = std::find_if(package.records.begin(),package.records.end(),
                        [source](const HardwareRecord& r) { return Code(r) == source; });
                    const auto* bytes = found == package.records.end() ? nullptr :
                        package.BytesAt(size_t(found-package.records.begin()),asset.address,asset.byte_len());
                    if (!bytes || !package.FindUnique(index,bytes,asset.byte_len(),offset)) { entry.valid = false; break; }
                }
                if (offset > entry.bytes.size() || asset.byte_len() > entry.bytes.size()-offset) { entry.valid = false; break; }
                std::fill_n(entry.bytes.begin()+offset,asset.byte_len(),uint8_t(1));
            }
        }
        return result;
    }();
    return cache[size_t(&profile-Profiles)];
}
bool CreatorReference(const uint8_t* bytes, uint32_t pc, uint32_t creator) {
    const uint32_t word = Read32(bytes);
    if (word == (creator | 1)) return true;
    const uint32_t a = word & 0xffff, b = word >> 16;
    if ((a & 0xf800) != 0xf000 || ((b & 0xd000) != 0x9000 && (b & 0xd000) != 0xd000)) return false;
    const uint32_t s = a >> 10 & 1;
    const uint32_t bits = s << 24 | ((~((b >> 13 & 1) ^ s)) & 1) << 23 |
        ((~((b >> 11 & 1) ^ s)) & 1) << 22 | (a & 0x3ff) << 12 | (b & 0x7ff) << 1;
    return pc + 4 + (static_cast<int32_t>(bits << 7) >> 7) == creator;
}
bool NativeMatches(const HardwareRecord& r, const Profile& p, std::string& error,
    bool editor = false) {
    const auto* b = Baseline(p);
    if (!b || r.image.size() < b->image.size() || uint64_t(r.linked_base) + r.image.size() >= SettingsBase) {
        error = "Native menu image exceeds protected Flash bounds or lacks its stock baseline"; return false;
    }
    for (const auto& h : Hooks(r, p, 0, AllLayout)) {
        const size_t at = h.address - r.linked_base;
        if (at > b->image.size() || h.size > b->image.size() - at ||
            !std::equal(b->image.begin() + at, b->image.begin() + at + h.size, r.image.begin() + at)) {
            error = "Native menu requires an unchanged native hook or fixed TCB pool configuration"; return false;
        }
        if ((h.address == p.main || h.address == p.voltage) &&
            (r.image[at] != 0x2d || r.image[at + 1] != 0xe9 ||
             !(r.image[at + 3] & 0x40) || (r.image[at + 3] & 0xa0))) {
            error = "Native menu renderer trampoline requires a complete PUSH.W saving LR, not a literal or partial instruction"; return false;
        }
    }
    {
        const auto& painter = HudPainters[size_t(&p-Profiles)];
        const size_t at = painter.entry-r.linked_base, bytes = painter.end-painter.entry;
        if (at > b->image.size() || bytes > b->image.size()-at ||
            Read32(b->image.data()+at) != painter.prologue ||
            !std::equal(b->image.begin()+at,b->image.begin()+at+bytes,r.image.begin()+at)) {
            error = "Native menu requires its exact mapped paint-only battery HUD and complete PUSH.W entry"; return false;
        }
    }
    if (PaintGuardCount(p)) {
        const auto& painters = ScorePainters[size_t(&p-Profiles)];
        for (const auto& painter : {painters.score,painters.puff}) {
            const size_t at = painter.entry-r.linked_base, bytes = painter.end-painter.entry;
            if (at > b->image.size() || bytes > b->image.size()-at ||
                Read32(b->image.data()+at) != painter.prologue ||
                !std::equal(b->image.begin()+at,b->image.begin()+at+bytes,r.image.begin()+at)) {
                error = "Native menu requires its exact mapped Score/Puff paint closures and normalization prefix"; return false;
            }
        }
        const size_t paint = painters.puffPaint-r.linked_base, exit = painters.puffExit-r.linked_base;
        const auto fill = Branch(painters.puffPaint,p.fill,true);
        if (painters.puffPaint < painters.puff.entry || painters.puffExit+6 > painters.puff.end ||
            !std::equal(fill.begin(),fill.end(),b->image.begin()+paint) ||
            (Read32(b->image.data()+exit)&0xffff) != (0xb000u|painters.stackWords) ||
            Read32(b->image.data()+exit+2) != 0x8ff0e8bd) {
            error = "Native menu Puff paint guard requires its exact native fill call and complete stack-restoring exit"; return false;
        }
    }
    for (auto site : p.starts) {
        if (!site) continue;
        const auto t = Transaction(*b, site);
        const size_t at = site - r.linked_base, commit = t.commit - r.linked_base;
        if (!t.resume || !std::equal(b->image.begin() + at, b->image.begin() + at + t.prefixBytes, r.image.begin() + at) ||
            !std::equal(b->image.begin() + commit, b->image.begin() + commit + t.storeBytes, r.image.begin() + commit)) {
            error = "Native menu requires the complete unchanged START transaction and shared commit"; return false;
        }
    }
    const size_t reload = p.poolLength - r.linked_base - 16;
    if (Read32(r.image.data() + reload) != (r.linked_base == 0x08003000 ? 95999u : 71999u)) {
        error = "Native menu short-code timing requires its original one-millisecond RTX tick"; return false;
    }
    const size_t length = p.poolLength - r.linked_base;
    if (b->image[length] != 0x94 || b->image[length + 1] != 0x01) {
        error = "Native menu TCB pool budget does not match the independently mapped seven-box pool"; return false;
    }
    for (const auto& range : ImmutableRanges[size_t(&p - Profiles)]) {
        if (!range.end) break;
        const size_t at = range.begin - r.linked_base, length = range.end - range.begin;
        if (at > b->image.size() || length > b->image.size() - at ||
            !std::equal(b->image.begin() + at, b->image.begin() + at + length, r.image.begin() + at)) {
            error = "Native menu requires the exact native RTX allocation, task-creation and control closure"; return false;
        }
    }
    const size_t profileIndex = size_t(&p-Profiles);
    const size_t saveBegin = PeriodicSavers[profileIndex]-r.linked_base;
    const size_t saveEnd = OffSavers[profileIndex]-r.linked_base+0x100;
    uint32_t pointer = 0; uint16_t load = 0;
    if (saveEnd > b->image.size() || !SaveLoad(*b,PeriodicSavers[profileIndex],pointer,load) ||
        !SaveLoad(*b,OffSavers[profileIndex],pointer,load) ||
        !std::equal(b->image.begin()+saveBegin,b->image.begin()+saveEnd,r.image.begin()+saveBegin)) {
        error = "Native menu requires its exact native periodic/OFF save protocol and eligibility checks"; return false;
    }
    if (editor) {
        const auto& puff = PuffProfiles[profileIndex];
        const bool low = puff.format != 2;
        // Guard windows include complete functions and their trailing literals,
        // not just callsite opcodes. CRC table bytes are separately immutable.
        const auto unchanged = [&](uint32_t begin, size_t bytes) {
            const size_t at = begin-r.linked_base;
            return at <= b->image.size() && bytes <= b->image.size()-at &&
                std::equal(b->image.begin()+at,b->image.begin()+at+bytes,r.image.begin()+at);
        };
        const uint32_t physicalKeys = low ? (p.edition == 0 ? 0x08003fbcu : 0x080038a8u) : 0x08005dfcu;
        if (Read32(b->image.data()+puff.transaction-r.linked_base) != 0x5ff0e92d ||
            !unchanged(puff.crc,0x80) || !unchanged(PuffCrcTables[profileIndex],512) ||
            !unchanged(puff.writer,low ? 0xcc : 0x58) ||
            !unchanged(puff.flashBegin-(low ? 0 : 0x8c),puff.flashEnd-puff.flashBegin+(low ? 0 : 0x10c)) ||
            !unchanged(physicalKeys,low ? 0x28 : 0x34) ||
            !unchanged(puff.increment-16,36) || !unchanged(puff.display-8,24) ||
            (!low && !unchanged(PuffEraseWrappers[profileIndex],0x30))) {
            error = "Puff editor requires the exact mapped native counter, CRC, supply eligibility and Flash transaction closure";
            return false;
        }
    }
    if (editor && p.edition == 0) {
        // ROM-proven total address/load/store/normalization and record-load closure.
        struct ScoreGuard { uint32_t literal, transfer, best, increment, normalize, maximum, load; };
        constexpr ScoreGuard guards[]{
            {0x08005944,0x080058da,0x08005d2a,0x08005dbe,0x08008ab6,0x08008e44,0x08006de0},
            {0x08005944,0x080058da,0x08005d2a,0x08005dbe,0x08008abe,0x08008e4c,0x08006de0},
            {0x080058ac,0x0800584a,0x08005b2e,0x08005bc2,0x0800847e,0x0800880c,0x080067d4},
            {0x080058ac,0x0800584a,0x08005b2e,0x08005bc2,0x08008486,0x08008814,0x080067d4},
            {0x08008004,0x08007fa4,0x08008240,0x080082f6,0x0800b652,0x0800b9dc,0x0800954c},
            {0x08008004,0x08007fa4,0x08008240,0x080082f6,0x0800b652,0x0800b9dc,0x0800954c},
            {0x08008004,0x08007fa4,0x08008240,0x080082f6,0x0800b62a,0x0800b9b4,0x08009564}};
        const auto& g = guards[profileIndex];
        const bool low = r.linked_base == 0x08003000;
        if (Read32(b->image.data()+g.literal-r.linked_base) != p.settings) {
            error = "Achievement total has no proven native settings base"; return false;
        }
        for (const auto span : {std::pair{g.literal,4u}, {g.transfer,low ? 14u : 10u},
            {g.best,low ? 8u : 12u}, {g.increment,12u}, {g.normalize,low ? 34u : 26u},
            {g.maximum,4u}, {g.load,0xb0u}}) {
            const size_t at = span.first-r.linked_base;
            if (at > b->image.size() || span.second > b->image.size()-at ||
                !std::equal(b->image.begin()+at,b->image.begin()+at+span.second,r.image.begin()+at)) {
                error = "Achievement editor requires its exact native total and Flash load closure"; return false;
            }
        }
    }
    // An unchanged set of OLD callsites is insufficient: also reject a NEW
    // BL/B.W/Thumb function pointer anywhere outside genuine catalog assets.
    const auto& resources = Resources(p);
    if (!resources.valid) { error = "Native menu cannot establish immutable resource exclusions"; return false; }
    const uint32_t creator = Creators[size_t(&p-Profiles)];
    for (size_t at = 0; at + 4 <= b->image.size(); at += 2) {
        if (std::memcmp(b->image.data()+at,r.image.data()+at,4) == 0 ||
            (resources.bytes[at] && resources.bytes[at+1] && resources.bytes[at+2] && resources.bytes[at+3])) continue;
        if (CreatorReference(b->image.data()+at,r.linked_base+uint32_t(at),creator) !=
            CreatorReference(r.image.data()+at,r.linked_base+uint32_t(at),creator)) {
            error = "Native menu task-creation reference closure differs from the mapped stock image"; return false;
        }
    }
    if (r.image.size() > b->image.size()) {
        auto expected = b->image;
        if (!FirmwareLogo::NormalizeForGuard(expected,r,error)) return false;
    }
    return true;
}
std::array<uint8_t, 32> Digest(const std::vector<uint8_t>& image) {
    const auto hex = FirmwarePackage::ComputeSha256(image.data(), image.size());
    std::array<uint8_t, 32> result{};
    auto nibble = [](char c) { return c >= '0' && c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; };
    for (size_t i = 0; i < result.size(); ++i) result[i] = uint8_t(nibble(hex[i * 2]) << 4 | nibble(hex[i * 2 + 1]));
    return result;
}
bool Encode(const HardwareRecord& inner, const Profile& p, Settings settings,
    std::vector<uint8_t>& image, std::string& error) {
    if (!ValidSettings(settings) || !settings.enabled) { error = "Invalid native menu settings"; return false; }
    const uint32_t effectiveModules = p.edition == 0 ? settings.enabledItems :
        settings.enabledItems & ~(ScoreEditorMask | RankSelectorMask);
    if (!NativeMatches(inner,p,error,(effectiveModules & (PuffEditorMask | ScoreEditorMask | RankSelectorMask)) != 0)) return false;
    const auto& layout = CurrentLayout(effectiveModules,settings.infoFields);
    const size_t extensionBytes = layout.ExtensionBytes(PaintGuardCount(p));
    const size_t start = (inner.image.size() + 3) & ~size_t(3), total = start + extensionBytes;
    if (uint64_t(inner.linked_base) + total >= SettingsBase) {
        error = "Native menu and custom screens exceed safe Flash capacity below protected settings at 0x0801F000"; return false;
    }
    image = inner.image;
    image.resize(total, 0);
    std::fill(image.begin() + inner.image.size(), image.begin() + start, 0xff);
    std::copy(Magic.begin(), Magic.end(), image.begin() + start);
    Set32(image,start+8,1); Set32(image,start+12,uint32_t(inner.image.size()));
    Set32(image,start+16,uint32_t(extensionBytes)); Set32(image,start+20,p.code);
    uint32_t flags = 1u | (settings.batteryVoltage ? 2u : 0u);
    flags |= (settings.reverseNavigation ? 4u : 0u) | (settings.enabledItems << 3);
    unsigned orderIndex = 0;
    for (auto item : settings.order) flags |= uint32_t(item) << (8 + orderIndex++*3);
    flags |= settings.infoFields << 26;
    Set32(image,start+24,flags); Set32(image,start+28,p.state);
    const auto digest = Digest(inner.image);
    std::copy(digest.begin(), digest.end(), image.begin() + start + 32);
    std::copy_n(layout.executor,layout.executorBytes,image.begin()+start+HeaderBytes);
    const size_t d = start + layout.Descriptor();
    const bool low = inner.linked_base == 0x08003000, score = p.edition == 0;
    const uint32_t width = low ? 2 : 4;
    const uint32_t power = low ? (score ? 0x12 : p.history ? 0x0e : 0x0a) : score ? 0x20 : 0x14;
    const uint32_t coil = low ? (score ? (p.history ? 0x24 : 0x20) : p.history ? 0x20 : 0x18) : score ? 0x3c : 0x34;
    const uint32_t duration = low ? (score ? (p.history ? 0x28 : 0x24) : p.history ? 0x24 : 0x1c) : score ? 0x44 : 0x3c;
    const uint32_t scanner = p.settings + (low ? (score ? (p.history ? 0x60 : 0x34) : p.history ? 0x5c : 0x30) : score ? 0x58 : 0x54);
    const uint32_t queueBase = low ? (score ? 0x20000064 : 0x2000005b) : score || p.code != 0x3103 ? 0x2000005f : 0x2000005e;
    const uint32_t tick = low ? (score ? (p.history ? 0x2000029c : 0x2000028c) :
        p.history ? 0x20000298 : 0x20000288) : score ? 0x200001d8 : 0x200001d4;
    const auto* baseline = Baseline(p);
    const std::array<StartTransaction, 3> transactions{
        Transaction(*baseline,p.starts[0]), Transaction(*baseline,p.starts[1]), Transaction(*baseline,p.starts[2])};
    {
        uint32_t count = 0;
        std::array<uint8_t,8> items{};
        for (auto item : settings.order) {
            const uint32_t id = uint32_t(item);
            if (item != Item::Exit && !(effectiveModules & ItemMask(item))) continue;
            items[count++] = uint8_t(id);
        }
        const std::array<uint32_t, 31> values{
            p.state, low ? 0x20000054u : 0x20000058u,
            low ? 0x48000000u : 0x40010800u, low ? 0x48000400u : 0x40010c00u,
            p.page, p.settings+power, p.settings+power+width,
            p.settings+width*2, p.settings+power+width*3, p.settings+coil+width,
            p.fill|1, p.queue|1, (inner.linked_base+uint32_t(start+layout.MainTrampoline()))|1,
            width, low ? 0x40014834u : 0x40000840u,
            transactions[0].resume, transactions[1].resume, transactions[2].resume,
            p.activity, p.activityTimer, scanner, queueBase,
            transactions[0].mask, transactions[1].mask, transactions[2].mask, tick,
            (inner.linked_base+uint32_t(start+layout.SaveTrampoline()))|1,
            (inner.linked_base+uint32_t(start+layout.OffSaveTrampoline()))|1,
            settings.reverseNavigation ? 1u : 0u, count, 0};
        for (size_t i = 0; i < values.size(); ++i) Set32(image,d+i*4,values[i]);
        std::copy(items.begin(),items.end(),image.begin()+d+120);
        size_t optional = d+128;
        const bool info = (settings.enabledItems & InformationMask) != 0;
        const uint32_t fields = info ? settings.infoFields : 0;
        if (layout.HasVoltage() || (fields & InfoBatteryMask)) {
            Set32(image,optional,p.sensors+8); optional += 4;
        }
        if (layout.HasVoltage() || (fields & InfoResistanceMask)) {
            Set32(image,optional,p.settings+coil); optional += 4;
        }
        if (info && (fields & InfoDurationMask)) {
            Set32(image,optional,p.settings+duration); optional += 4;
        }
        if (layout.HasVoltage()) {
            Set32(image,optional,settings.batteryVoltage ? 1u : 0u);
            Set32(image,optional+4,p.combined ? 1u : 0u);
            Set32(image,optional+8,(inner.linked_base+uint32_t(start+layout.VoltageTrampoline()))|1);
        }
    }
    if (layout.hud) {
        const bool editor = (effectiveModules & (PuffEditorMask | ScoreEditorMask | RankSelectorMask)) != 0;
        const bool both = (effectiveModules & PuffEditorMask) && (effectiveModules & (ScoreEditorMask | RankSelectorMask));
        const size_t hud = d+layout.descriptorBytes-4-(editor ? 36 : 0)-(both ? 8 : 0);
        Set32(image,hud,(inner.linked_base+uint32_t(start+layout.HudTrampoline()))|1);
        if (editor) {
            const auto& puff = PuffProfiles[size_t(&p-Profiles)];
            const std::array<uint32_t,9> metadata{
                p.settings+((effectiveModules & PuffEditorMask) ? puff.offset : low ? 0x0c : 0x18),
                (effectiveModules & PuffEditorMask) ? puff.format : low ? 1u : 2u,
                (effectiveModules & PuffEditorMask) ? (std::min)(puff.limit,PuffEditorMaximum) : PuffEditorMaximum,puff.transaction|1,
                low ? 0x0801f800u : 0x0801f000u,p.settings,puff.recordBytes,
                p.sensors+8,puff.guardMask};
            for (size_t i = 0; i < metadata.size(); ++i) Set32(image,hud+4+i*4,metadata[i]);
            if (both) {
                Set32(image,hud+40,p.settings+(low ? 0x0c : 0x18));
                Set32(image,hud+44,low ? 1u : 2u);
            }
        }
    }
    for (const auto pair : {std::pair{p.main,layout.MainTrampoline()},std::pair{p.voltage,layout.VoltageTrampoline()},
        std::pair{HudPainters[size_t(&p-Profiles)].entry,layout.HudTrampoline()}}) {
        if ((pair.first == p.voltage && !layout.HasVoltage()) ||
            (pair.second == layout.HudTrampoline() && !layout.hud)) continue;
        std::copy_n(inner.image.begin() + pair.first - inner.linked_base, 4, image.begin() + start + pair.second);
        const auto jump = Branch(inner.linked_base + uint32_t(start + pair.second + 4), pair.first + 4, false);
        std::copy(jump.begin(),jump.end(),image.begin()+start+pair.second+4);
    }
    if (PaintGuardCount(p)) {
        const auto& painters = ScorePainters[size_t(&p-Profiles)];
        // SUB.W SP,#4; PUSH {r0,r1}; LDR r0,[PC,#8]. Score passes exit=0;
        // Puff loads the original native epilogue. All instructions preserve APSR.
        constexpr std::array<uint8_t,16> scoreStub{
            0xad,0xf1,0x04,0x0d,0x03,0xb4,0x02,0x48,0x4f,0xf0,0x00,0x01,0,0,0,0};
        constexpr std::array<uint8_t,16> puffStub{
            0xad,0xf1,0x04,0x0d,0x03,0xb4,0x02,0x48,0x02,0x49,0,0,0,0,0x00,0xbf};
        for (size_t i = 0; i < PaintGuardCount(p); ++i) {
            const size_t stub = start+layout.PaintStub(i), trampoline = start+layout.PaintTrampoline(i);
            const auto& prefix = i ? puffStub : scoreStub;
            std::copy(prefix.begin(),prefix.end(),image.begin()+stub);
            const size_t jumpAt = stub+(i ? 10 : 12);
            const auto guard = Branch(inner.linked_base+uint32_t(jumpAt),
                inner.linked_base+uint32_t(start+HeaderBytes+layout.paint),false);
            std::copy(guard.begin(),guard.end(),image.begin()+jumpAt);
            Set32(image,stub+16,(inner.linked_base+uint32_t(trampoline))|1);
            if (!i) {
                std::copy_n(inner.image.begin()+painters.score.entry-inner.linked_base,4,image.begin()+trampoline);
                const auto resume = Branch(inner.linked_base+uint32_t(trampoline+4),painters.score.entry+4,false);
                std::copy(resume.begin(),resume.end(),image.begin()+trampoline+4);
            } else {
                Set32(image,stub+20,painters.puffExit|1);
                // Relocate BL without changing the LR seen by the native fill:
                // LDR.W LR,[PC,#4]; B.W fill; original odd return address.
                Set32(image,trampoline,0xe004f8df);
                const auto fill = Branch(inner.linked_base+uint32_t(trampoline+4),p.fill,false);
                std::copy(fill.begin(),fill.end(),image.begin()+trampoline+4);
                Set32(image,trampoline+8,(painters.puffPaint+4)|1);
            }
        }
    }
    // Relocate the original PC-relative load rather than silently executing it
    // at a new PC. The second original LDR/LDRB and every eligibility/IRQ check
    // run unchanged. These thunks are called only by original native callers.
    const size_t index = size_t(&p-Profiles);
    for (const auto pair : {std::pair{PeriodicSavers[index],layout.SaveTrampoline()},std::pair{OffSavers[index],layout.OffSaveTrampoline()}}) {
        uint32_t pointer = 0; uint16_t load = 0;
        if (!SaveLoad(inner,pair.first,pointer,load)) { error = "Unsupported native save entry load ABI"; return false; }
        const size_t at = start+pair.second;
        image[at]=1; image[at+1]=0x48;
        image[at+2]=uint8_t(load); image[at+3]=uint8_t(load>>8);
        const auto jump = Branch(inner.linked_base+uint32_t(at+4),pair.first+4,false);
        std::copy(jump.begin(),jump.end(),image.begin()+at+4);
        Set32(image,at+8,pointer);
    }
    const size_t footer = total - FooterBytes;
    std::copy(EndMagic.begin(),EndMagic.end(),image.begin()+footer);
    Set32(image,footer+8,uint32_t(start)); Set32(image,footer+12,uint32_t(extensionBytes));
    Set32(image,footer+16,FirmwarePackage::Crc16Ccitt(image.data()+start,footer-start)); Set32(image,footer+20,1);
    for (const auto& h : Hooks(inner,p,start,layout)) std::copy_n(h.bytes.begin(),h.size,image.begin()+h.address-inner.linked_base);
    return true;
}
bool HasEnvelope(const HardwareRecord& r) {
    return r.image.size() >= FooterBytes && std::equal(EndMagic.begin(),EndMagic.end(),r.image.end()-FooterBytes);
}
bool LooksDamaged(const HardwareRecord& r, const Profile& p) {
    const auto* b = Baseline(p);
    if (!b || r.image.size() <= b->image.size()) return false;
    return std::search(r.image.begin()+b->image.size(),r.image.end(),Magic.begin(),Magic.end()) != r.image.end() ||
        std::search(r.image.begin()+b->image.size(),r.image.end(),EndMagic.begin(),EndMagic.end()) != r.image.end();
}
bool Decode(HardwareRecord& r, const Profile& p, Settings& settings, std::string& error) {
    if (!HasEnvelope(r)) { error = "Native menu footer is missing or damaged"; return false; }
    const auto* footer = r.image.data()+r.image.size()-FooterBytes;
    const uint32_t version = Read32(footer+20);
    if (version != 1) { error = "Unsupported native menu envelope version"; return false; }
    // The minimum header bounds are established before reading module flags.
    const size_t envelopeBytes = Read32(footer+12);
    const size_t start = Read32(footer+8);
    const auto* baseline = Baseline(p);
    if (!baseline || start < baseline->image.size() || start > r.image.size() ||
        r.image.size()-start != envelopeBytes || envelopeBytes < HeaderBytes+FooterBytes ||
        Read32(footer+16) != FirmwarePackage::Crc16Ccitt(r.image.data()+start,envelopeBytes-FooterBytes)) {
        error = "Invalid native menu envelope bounds, version, or CRC"; return false;
    }
    const auto* header = r.image.data()+start;
    const size_t innerSize = Read32(header+12);
    const uint32_t flags = Read32(header+24);
    settings = {true, (flags & 2) != 0, (flags & 4) != 0};
    settings.enabledItems = (flags >> 3) & FunctionalMask;
    unsigned seen = 0;
    for (unsigned i = 0; i < ItemCount; ++i) {
        const auto item = Item((flags >> (8+i*3)) & 7);
        const unsigned id = unsigned(item);
        if (seen & (1u << id)) {
            error = "Invalid native menu item order"; return false;
        }
        seen |= 1u << id;
        settings.order[i] = item;
    }
    settings.infoFields = (flags >> 26) & AllInfoFields;
    if (!ValidSettings(settings)) { error = "Invalid native menu module selection or item order"; return false; }
    const uint32_t effectiveModules = p.edition == 0 ? settings.enabledItems :
        settings.enabledItems & ~(ScoreEditorMask | RankSelectorMask);
    const auto& layout = CurrentLayout(effectiveModules,settings.infoFields);
    if (!std::equal(Magic.begin(),Magic.end(),header) || Read32(header+8) != version ||
        r.image.size()-start != layout.ExtensionBytes(PaintGuardCount(p)) ||
        Read32(header+16) != layout.ExtensionBytes(PaintGuardCount(p)) ||
        Read32(header+20) != p.code || Read32(header+28) != p.state ||
        !(flags & 1) || (flags & ~0x1fffffffu) ||
        innerSize < baseline->image.size() || innerSize > start || start-innerSize > 3 ||
        ((innerSize+3)&~size_t(3)) != start || uint64_t(r.linked_base)+r.image.size() >= SettingsBase) {
        error = "Invalid native menu identity, settings, state reservation, or alignment"; return false;
    }
    HardwareRecord inner = r;
    for (const auto& h : Hooks(r,p,start,layout)) {
        const size_t at = h.address-r.linked_base;
        if (at > innerSize || h.size > innerSize-at || !std::equal(h.bytes.begin(),h.bytes.begin()+h.size,r.image.begin()+at)) {
            error = "Damaged native menu hook or fixed TCB pool bound"; return false;
        }
        std::copy_n(baseline->image.begin()+at,h.size,inner.image.begin()+at);
    }
    inner.image.resize(innerSize);
    const auto digest = Digest(inner.image);
    if (!std::equal(digest.begin(),digest.end(),header+32)) { error = "Native menu inner-image SHA-256 mismatch"; return false; }
    std::vector<uint8_t> expected;
    if (!Encode(inner,p,settings,expected,error)) return false;
    if (expected != r.image) { error = "Unrecognized native menu executor, descriptors, trampoline, or padding"; return false; }
    r = std::move(inner);
    return true;
}
} // namespace

bool ValidSettings(const Settings& settings) {
    if ((settings.enabledItems & ~FunctionalMask) || (settings.enabled && !settings.enabledItems) ||
        (settings.infoFields & ~AllInfoFields) ||
        ((settings.enabledItems & InformationMask) && !settings.infoFields)) return false;
    uint32_t seen = 0;
    for (auto item : settings.order) {
        const uint32_t id = uint32_t(item);
        if (id >= ItemCount || (seen & (1u << id))) return false;
        seen |= 1u << id;
    }
    return seen == (1u << ItemCount)-1;
}

bool AchievementSupported(const HardwareRecord& record) {
    const auto* p = Find(record);
    return p && p->edition == 0;
}
bool Supported(const FirmwarePackage& package) {
    if (package.records.empty()) return false;
    for (const auto& r : package.records) {
        const auto* p = Find(r);
        if (!p) return false;
        std::string error;
        if (HasEnvelope(r)) { auto inner = r; Settings settings; if (!Decode(inner,*p,settings,error)) return false; }
        else if (LooksDamaged(r,*p) || !NativeMatches(r,*p,error)) return false;
    }
    return true;
}
bool Extract(FirmwarePackage& package, Settings& settings, std::string& error) {
    error.clear(); settings = {};
    // Validate every record before changing any of them; mixed settings/envelope
    // presence cannot silently narrow a requested package-wide native feature.
    FirmwarePackage normalized = package;
    bool initialized = false, enabled = false, unsupported = false;
    for (auto& r : normalized.records) {
        const auto* p = Find(r);
        if (!p) {
            if (HasEnvelope(r)) { error = "Native menu hardware identity is unsupported"; return false; }
            unsupported = true;
            continue;
        }
        const bool present = HasEnvelope(r);
        if (!present && LooksDamaged(r,*p)) { error = "Native menu outer envelope is damaged"; return false; }
        Settings current;
        if (present && !Decode(r,*p,current,error)) return false;
        if (initialized && (present != enabled || current != settings)) { error = "Native menu settings differ across hardware records"; return false; }
        settings = current; enabled = present; initialized = true;
    }
    if (enabled && unsupported) { error = "Native menu cannot mix supported patched and unsupported unpatched hardware records"; return false; }
    package = std::move(normalized);
    return true;
}
bool Apply(std::vector<uint8_t>& packedFirmware, Settings settings, BuildReport& report, std::string& error) {
    error.clear();
    if (!ValidSettings(settings)) { error = "Invalid native menu settings"; return false; }
    if (!settings.enabled) return true;
    FirmwarePackage package;
    if (!FirmwarePackage::Parse(packedFirmware,package,error)) return false;
    if (!Supported(package)) { error = "Native menu requires recognized DRAG S Score or Factory native hooks"; return false; }
    size_t ranges = 0, bytes = 0;
    for (auto& r : package.records) {
        const auto& p = *Find(r);
        if (HasEnvelope(r)) { error = "Native menu must be extracted before rebuilding its outer envelope"; return false; }
        const auto hooks = Hooks(r,p,(r.image.size()+3)&~size_t(3),CurrentLayout(settings.enabledItems));
        ranges += hooks.size()+1;
        for (const auto& h : hooks) bytes += h.size;
        std::vector<uint8_t> image;
        if (!Encode(r,p,settings,image,error)) return false;
        r.image = std::move(image);
    }
    std::vector<uint8_t> output;
    BuildReport packed;
    if (!package.PackWith({}, {}, output, packed, error)) return false;
    report.output_sha256 = std::move(packed.output_sha256);
    report.output_size = packed.output_size;
    report.records = packed.records;
    report.hardware_codes = std::move(packed.hardware_codes);
    report.changed_assets += package.records.size();
    report.patched_ranges += ranges;
    report.unchanged_payload_bytes_verified = report.unchanged_payload_bytes_verified > bytes ? report.unchanged_payload_bytes_verified-bytes : 0;
    packedFirmware = std::move(output);
    return true;
}
bool NormalizeForGuard(const HardwareRecord& stock, HardwareRecord& candidate, std::string& error) {
    error.clear();
    const auto* p = Find(candidate);
    if (!p) { if (HasEnvelope(candidate)) { error = "Unsupported native menu hardware identity"; return false; } return true; }
    if (!HasEnvelope(candidate)) {
        if (LooksDamaged(candidate,*p)) { error = "Damaged native menu outer envelope"; return false; }
        return true;
    }
    if (Find(stock) != p || stock.image.size() != Baseline(*p)->image.size()) { error = "Native menu guard requires its exact stock hardware record"; return false; }
    Settings settings;
    return Decode(candidate,*p,settings,error);
}
FlashBreakdown FlashCosts(const HardwareRecord& record, Settings settings, size_t innerImageBytes) {
    if (!settings.enabled || !ValidSettings(settings)) return {};
    const auto* profile = Find(record);
    if (!profile) return {};
    if (!AchievementSupported(record)) settings.enabledItems &= ~(ScoreEditorMask | RankSelectorMask);
    const size_t size = innerImageBytes ? innerImageBytes : record.image.size();
    FlashBreakdown cost;
    cost.common = BaseLayout.ExtensionBytes(PaintGuardCount(*profile)) + ((4-size%4)%4);
    const bool info = (settings.enabledItems & InformationMask) != 0;
    const auto& information = info ? CurrentLayout(InformationMask,settings.infoFields) : BaseLayout;
    cost.information = information.ExtensionBytes() - BaseLayout.ExtensionBytes();
    uint32_t modules = settings.enabledItems & DefaultMask;
    size_t previous = CurrentLayout(modules,settings.infoFields).ExtensionBytes();
    cost.voltage = previous - information.ExtensionBytes();
    for (const auto entry : {std::pair{PuffEditorMask,&cost.puffEditor},
        std::pair{ScoreEditorMask,&cost.scoreEditor},std::pair{RankSelectorMask,&cost.rankSelector}}) {
        modules |= settings.enabledItems & entry.first;
        const size_t next = CurrentLayout(modules,settings.infoFields).ExtensionBytes();
        *entry.second = next-previous;
        previous = next;
    }
    cost.total = cost.common + cost.information + cost.voltage + cost.puffEditor + cost.scoreEditor + cost.rankSelector;
    return cost;
}
size_t FlashBytes(const HardwareRecord& record, Settings settings, size_t innerImageBytes) {
    return FlashCosts(record,settings,innerImageBytes).total;
}
uint32_t PuffLimit(const HardwareRecord& record) {
    const auto* p = Find(record);
    return p ? (std::min)(PuffProfiles[size_t(p-Profiles)].limit,PuffEditorMaximum) : 0;
}
bool RenderPreview(Settings settings, PreviewState state, PreviewValues values,
    std::array<uint32_t,80*160>& pixels) {
    if (!ValidSettings(settings)) return false;
    if (!settings.enabled) { pixels.fill(0xff000000u); return true; }
    std::array<Item,ItemCount> items{};
    unsigned count = 0;
    for (auto item : settings.order)
        if (item == Item::Exit || (settings.enabledItems & ItemMask(item)))
            items[count++] = item;
    if (state.selection >= count || (state.information && !(settings.enabledItems & InformationMask))) return false;
    pixels.fill(0xff000000u);
    PreviewText(pixels,16,10,state.selectingRank ? PreviewText_set_rank : PreviewText_settings);
    if (state.selectingRank) {
        if (!(settings.enabledItems & RankSelectorMask) || state.rankSelection >= RankRowCount) return false;
        for (unsigned row=0;row<RankRowCount;++row)
            PreviewText(pixels,7,RankRowsY+RankRowPitch*row,row<RankCount ? RankNames[row] : PreviewText_rank_back);
        PreviewText(pixels,0,RankRowsY+RankRowPitch*state.rankSelection,PreviewText_arrow);
        if (state.puffStatus == 1) PreviewText(pixels,4,150,PreviewText_save_error);
    } else if (state.editingPuffs || state.editingScore) {
        if (!(settings.enabledItems & (state.editingScore ? ScoreEditorMask : PuffEditorMask)) ||
            state.puffDigit >= PuffDigitCount) return false;
        PreviewText(pixels,state.editingScore ? 22 : 10,35,state.editingScore ? PreviewText_points : PreviewText_puff_count);
        if (state.puffStatus == 2 || state.puffValue > PuffEditorMaximum) {
            PreviewText(pixels,4,55,PreviewText_range_error);
            return true;
        }
        constexpr uint32_t places[]{10000,1000,100,10,1};
        for (unsigned digit = 0; digit < PuffDigitCount; ++digit) {
            const unsigned glyph = (state.puffValue / places[digit]) % 10;
            for (unsigned column = 0; column < 5; ++column)
                for (unsigned row = 0; row < 7; ++row)
                    if (PreviewFont[glyph*5+column] & (1u << row))
                        for (unsigned dy = 0; dy < 2; ++dy)
                            for (unsigned dx = 0; dx < 2; ++dx)
                                pixels[(55+row*2+dy)*80+PuffDigitsX+digit*PuffDigitPitch+column*2+dx] = 0xffffffffu;
        }
        for (unsigned y = 72; y < 74; ++y)
            for (unsigned x = PuffDigitsX+state.puffDigit*PuffDigitPitch;
                x < PuffDigitsX+state.puffDigit*PuffDigitPitch+PuffDigitWidth; ++x)
                pixels[y*80+x] = 0xffffffffu;
        PreviewText(pixels,4,90,PreviewText_limit);
        PreviewNumber(pixels,90,state.editingScore ? PuffEditorMaximum : (std::min)(values.maximumPuffs,PuffEditorMaximum));
        if (state.puffStatus == 1) PreviewText(pixels,4,110,PreviewText_save_error);
    } else if (state.information) {
        unsigned y = 35;
        const auto field = [&](uint32_t mask, const char* label, uint32_t value) {
            if (!(settings.infoFields & mask)) return;
            PreviewText(pixels,4,y,label);
            PreviewNumber(pixels,y,value);
            y += 20;
        };
        field(InfoBatteryMask,PreviewText_bat,values.batteryMillivolts);
        field(InfoResistanceMask,PreviewText_coil,values.resistanceMilliOhms);
        field(InfoDurationMask,PreviewText_puff,values.durationCentiseconds);
    } else {
        for (unsigned row = 0; row < count; ++row) {
            const char* label = items[row] == Item::Information ? PreviewText_info :
                items[row] == Item::Voltage ? (state.batteryVoltage ? PreviewText_battery : PreviewText_output) :
                items[row] == Item::PuffEditor ? PreviewText_puff_count :
                items[row] == Item::ScoreEditor ? PreviewText_points :
                items[row] == Item::RankSelector ? PreviewText_set_rank : PreviewText_exit;
            PreviewText(pixels,7,35+20*row,label);
        }
        PreviewText(pixels,0,35+20*state.selection,PreviewText_arrow);
    }
    return true;
}
uint32_t StateAddress(const HardwareRecord& record) {
    const auto* p = Find(record);
    if (!p || !HasEnvelope(record)) return 0;
    HardwareRecord inner = record;
    Settings settings;
    std::string error;
    return Decode(inner,*p,settings,error) ? p->state : 0;
}
} // namespace FirmwareMenu
