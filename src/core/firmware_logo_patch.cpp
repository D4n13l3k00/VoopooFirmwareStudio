#include "firmware_logo_patch.h"
#include "embedded_firmware.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>

namespace FirmwareLogo {
namespace {
// Each released image independently references the 128-KiB Flash end 08020000
// and settings at 0801f000/0801f800. Never consume either settings page or the
// loader below linked_base. Every final extension is checked against 0801f000.
// Low-base releases match GD32F3x0 SystemInit, including IRC48M /2 *4 = 96 MHz
// (not STM32F3 despite the old preview naming). High-base releases request
// 72 MHz in their own clock-selection path. Every reset path enables CP10/11.
// GD32F3x0 User Manual Rev2.8 pp24-25 documents M4/FPU/DWT/ITM; the exact clock
// sequence is system_clock_96m_irc48m in GigaDevice's CMSIS source:
// https://raw.githubusercontent.com/CommunityGD32Cores/gd32-pio-spl-package/main/gd32/cmsis/variants/gd32f3x0/system_gd32f3x0.c
// https://www.gd32mcu.com/data/documents/userManual/GD32F3x0_User_Manual_Rev2.8.pdf
// N32G43x User Manual, memory map and Debug Support, documents E0001000 DWT:
// https://www.nsing.com.sg/uploads/MCUProducts/N32G435/Chip_Documentation/User_Manual/EN_UM_N32G43x_Series_User_Manual.pdf
struct Profile {
    unsigned edition;
    uint16_t code;
    size_t size;
    uint32_t stack, boot, rgb, fill, flag, watchdog, init, cyclesPerMs, info;
    uint32_t firstCall;
    std::array<uint32_t, 6> mainCalls;
    uint32_t main, servicePage, subview = 0, historyCall = 0, history = 0;
    uint32_t timeoutCall = 0, backlight = 0;
};
constexpr Profile Profiles[] = {
    {0, 0x314d, 63488, 0x20003820, 0x08008024, 0x08009bc8, 0x0800a090, 0x2000008c, 0x08009640, 0x0800c1e4, 72000, 0x0800df60, 0x08008388, {0x0800f0fc}, 0x08007e94, 4},
    {0, 0x3014, 79400, 0x200025b8, 0x08005958, 0x08007328, 0x08007870, 0x2000005c, 0x0800ae58, 0x08009244, 96000, 0x0800a7dc, 0x08005e52, {0x08005a80, 0x08005c1e, 0x08005ef4, 0x08005f46, 0x08005fa2, 0x0800cf40}, 0x08005740, 5, 0x2000009e, 0x08005abe, 0x080064cc, 0x08005f8a, 0x0800e068},
    {0, 0x302a, 69428, 0x20002558, 0x080058c8, 0x08006d00, 0x08007248, 0x2000005c, 0x0800a854, 0x08008c1c, 96000, 0x0800a200, 0x08005c56, {0x080059c0, 0x08005cf0, 0x08005d42, 0x08005d9e, 0x0800c8ec}, 0x080056e4, 4},
    {0, 0x3013, 79384, 0x200025b8, 0x08005958, 0x08007328, 0x08007870, 0x2000005c, 0x0800ae50, 0x0800923c, 96000, 0x0800a7d4, 0x08005e52, {0x08005a80, 0x08005c1e, 0x08005ef4, 0x08005f46, 0x08005fa2, 0x0800cf30}, 0x08005740, 5, 0x2000009e, 0x08005abe, 0x080064cc, 0x08005f8a, 0x0800e058},
    {0, 0x3029, 69412, 0x20002558, 0x080058c8, 0x08006d00, 0x08007248, 0x2000005c, 0x0800a84c, 0x08008c14, 96000, 0x0800a1f8, 0x08005c56, {0x080059c0, 0x08005cf0, 0x08005d42, 0x08005d9e, 0x0800c8dc}, 0x080056e4, 4},
    {0, 0x3103, 63612, 0x20003820, 0x08008024, 0x08009bb0, 0x0800a0d8, 0x2000008c, 0x08009628, 0x0800c20c, 72000, 0x0800df88, 0x08008388, {0x0800f190}, 0x08007e94, 4},
    {0, 0x3105, 63936, 0x20003820, 0x08008024, 0x08009bb0, 0x0800a0d8, 0x2000008c, 0x08009628, 0x0800c20c, 72000, 0x0800df88, 0x08008388, {0x0800f2c8}, 0x08007e94, 4},
    {1, 0x3148, 62592, 0x20002550, 0x08005830, 0x08006c04, 0x080070f0, 0x20000084, 0x08009cbc, 0x080081a0, 96000, 0x08009664, 0x08005b02, {0x08004f6a, 0x08005b9c, 0x08005bee, 0x08005c4a, 0x0800b1b0}, 0x08004fb0, 7},
    {1, 0x3014, 73452, 0x200025b0, 0x08005210, 0x08006bb4, 0x080070fc, 0x2000008e, 0x0800a6ac, 0x08008ba4, 96000, 0x0800a01c, 0x080056d2, {0x08005338, 0x080054de, 0x08005774, 0x080057c6, 0x08005822, 0x0800bd58}, 0x08005038, 5, 0x2000009a, 0x08005376, 0x08005d4c, 0x0800580a, 0x0800ce7c},
    {1, 0x3104, 64572, 0x20003410, 0x080085b8, 0x08009ff4, 0x0800a54c, 0x2000008c, 0x08009b18, 0x0800be1c, 72000, 0x0800def8, 0x08008888, {0x08007ce6, 0x08008930, 0x08008980, 0x080089da, 0x0800f908}, 0x08007d2c, 7},
    {1, 0x3013, 73440, 0x200025b0, 0x08005210, 0x08006bb4, 0x080070fc, 0x2000008e, 0x0800a6a4, 0x08008b9c, 96000, 0x0800a014, 0x080056d2, {0x08005338, 0x080054de, 0x08005774, 0x080057c6, 0x08005822, 0x0800bd48}, 0x08005038, 5, 0x2000009a, 0x08005376, 0x08005d4c, 0x0800580a, 0x0800ce6c},
    {1, 0x3029, 62388, 0x20002550, 0x08005830, 0x08006bc8, 0x08007110, 0x20000084, 0x08009c2c, 0x08008110, 96000, 0x080095d4, 0x08005b02, {0x08004f6a, 0x08005b9c, 0x08005bee, 0x08005c4a, 0x0800b218}, 0x08004fb0, 7},
    {1, 0x3103, 61340, 0x20003410, 0x0800877c, 0x0800a1a4, 0x0800a6fe, 0x2000008c, 0x08009c74, 0x0800be00, 72000, 0x0800db84, 0x08008a4c, {0x0800ed40}, 0x08007edc, 7},
    {1, 0x3034, 63040, 0x20002550, 0x08005830, 0x08006bc8, 0x08007110, 0x20000084, 0x08009c2c, 0x08008110, 96000, 0x080095d4, 0x08005b02, {0x08004f6a, 0x08005b9c, 0x08005bee, 0x08005c4a, 0x0800b394}, 0x08004fb0, 7},
    {1, 0x3105, 66176, 0x20003410, 0x080085b8, 0x08009ff4, 0x0800a54c, 0x2000008c, 0x08009b18, 0x0800be10, 72000, 0x0800deec, 0x08008888, {0x08007ce6, 0x08008930, 0x08008980, 0x080089da, 0x0800ff60}, 0x08007d2c, 7},
    {1, 0x314d, 63864, 0x20003410, 0x080085e8, 0x0800a070, 0x0800a568, 0x2000008c, 0x08009b94, 0x0800be30, 72000, 0x0800df20, 0x080088c4, {0x08007d12, 0x0800896c, 0x080089be, 0x08008a18, 0x0800f67c}, 0x08007d58, 7},
};
constexpr size_t HeaderBytes = 112, DescriptorBytes = 44, PixelBytes = Width * Height * 2;
constexpr size_t HookCount = 4, FirstHook = 3;
constexpr size_t CloseDescriptorBytes = 32;
constexpr uint32_t SettingsBase = 0x0801f000, FlashEnd = 0x08020000;
constexpr std::array<uint8_t, 8> Magic = {'V','F','S','L','O','G','O','1'};
// Regenerate the relocation-free .text with the clang command in the assembly;
// llvm-objcopy -O binary --only-section=.text, truncate at firmware_logo_descriptor.
#include "firmware_logo_executor.inc"
constexpr size_t FixedBytes = HeaderBytes + sizeof(Executor) + HookCount * DescriptorBytes + CloseDescriptorBytes;
uint32_t Read32(const uint8_t* bytes) {
    return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 | uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
}
uint16_t Code(const HardwareRecord& r) { return uint16_t(r.device_code[0]) << 8 | r.device_code[1]; }
uint32_t Base(const Profile& p) { return p.stack > 0x20003000 ? 0x08005800 : 0x08003000; }
const FirmwarePackage& Stock(unsigned edition) {
    static const auto packages = [] {
        std::array<FirmwarePackage, 2> result;
        std::string error;
        LoadEmbeddedFirmware(result[0], error, FirmwareModel::DragS, FirmwareEdition::Score);
        LoadEmbeddedFirmware(result[1], error, FirmwareModel::DragS, FirmwareEdition::Factory);
        return result;
    }();
    return packages[edition];
}
const HardwareRecord* Baseline(const Profile& p) {
    const auto& package = Stock(p.edition);
    const auto found = std::find_if(package.records.begin(), package.records.end(),
        [&](const HardwareRecord& r) { return Code(r) == p.code; });
    return found == package.records.end() ? nullptr : &*found;
}
bool EqualRange(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, size_t offset, size_t count) {
    return offset <= a.size() && count <= a.size() - offset && offset <= b.size() && count <= b.size() - offset &&
        std::equal(a.begin() + offset, a.begin() + offset + count, b.begin() + offset);
}
bool ContainsWord(const std::vector<uint8_t>& bytes, uint32_t word) {
    for (size_t i = 0; i + 4 <= bytes.size(); i += 2) if (Read32(bytes.data() + i) == word) return true;
    return false;
}
const Profile* Find(const HardwareRecord& record) {
    const auto found = std::find_if(std::begin(Profiles), std::end(Profiles), [&](const Profile& p) {
        return p.code == Code(record) && p.stack == record.sp && Base(p) == record.linked_base;
    });
    return found == std::end(Profiles) ? nullptr : &*found;
}
bool Identity(const HardwareRecord& record, const Profile& p) {
    const auto* baseline = Baseline(p);
    return baseline && baseline->image.size() == p.size && record.reset == baseline->reset &&
        record.marker == baseline->marker && record.sp == p.stack && record.linked_base == Base(p) &&
        Base(p) + p.size + FixedBytes < SettingsBase && SettingsBase + 0x1000 == FlashEnd &&
        ContainsWord(baseline->image, FlashEnd) && ContainsWord(baseline->image, SettingsBase) &&
        ContainsWord(baseline->image, SettingsBase + 0x800) &&
        // The first animation has no RAM flag side effect. Its only callers
        // immediately BL the named-logo entry; preserve that entire sequence.
        EqualRange(record.image, baseline->image, p.firstCall - Base(p) - 24, 56) &&
        EqualRange(record.image, baseline->image, p.boot + 0xb4 - Base(p),
            Base(p) == 0x08003000 ? 48 : 32) &&
        EqualRange(record.image, baseline->image, p.init - Base(p), Base(p) == 0x08003000 ? 272 : 0x2e0) &&
        EqualRange(record.image, baseline->image, p.rgb - Base(p), 72) &&
        EqualRange(record.image, baseline->image, p.fill - Base(p), 64) &&
        EqualRange(record.image, baseline->image, p.watchdog - Base(p), 16);
}
uint16_t Read16(const uint8_t* bytes) { return uint16_t(bytes[0]) | uint16_t(bytes[1]) << 8; }
void Put16(std::vector<uint8_t>& bytes, uint16_t value) {
    bytes.push_back(static_cast<uint8_t>(value)); bytes.push_back(static_cast<uint8_t>(value >> 8));
}
void Set32(std::vector<uint8_t>& bytes, size_t at, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = static_cast<uint8_t>(value >> (8 * i));
}
// First startup is boot+0xb0 in all 16 releases, with the complete four-byte
// push/sub prologue replaced. The adjacent shutdown routine is boot+0x60.
// Only second startup clears the completion byte; first startup has no flag.
uint32_t Entry(const Profile& p, size_t hook) {
    return hook == FirstHook ? p.boot + 0xb0 : hook == 0 ? p.boot : hook == 1 ? p.boot + 0x60 : p.info;
}
bool HookEnabled(const Patches& settings, size_t hook) {
    if (hook == FirstHook)
        return settings[0].enabled && (static_cast<uint32_t>(settings[0].bootStages) & 1);
    if (hook == 0)
        return settings[0].enabled && (static_cast<uint32_t>(settings[0].bootStages) & 2);
    return settings[hook].enabled;
}
bool Valid(const Settings& s, size_t target, std::string& error) {
    if (s.pixels.size() != PixelBytes) { error = "Custom screen must contain an encoded 80 x 160 RGB565 image"; return false; }
    if (static_cast<uint32_t>(s.animation) >= static_cast<uint32_t>(Animation::Count) ||
        static_cast<uint32_t>(s.exitAnimation) >= static_cast<uint32_t>(Animation::Count)) {
        error = "Unknown native screen transition animation"; return false;
    }
    const auto stages = static_cast<uint32_t>(s.bootStages);
    if (stages < 1 || stages > 3 || (target != 0 && s.bootStages != BootStages::Second)) {
        error = "Startup stage selection applies only to the boot screen"; return false;
    }
    if (s.durationMs < MinimumDurationMs || s.durationMs > MaximumDurationMs ||
        s.exitDurationMs < MinimumDurationMs || s.exitDurationMs > MaximumDurationMs || s.holdMs > 10000) {
        error = "Native transitions must be 200..2000 milliseconds and hold 0..10000 milliseconds"; return false;
    }
    return true;
}
std::array<uint8_t, 4> Branch(uint32_t source, uint32_t destination, bool link = false) {
    const uint32_t delta = destination - source - 4;
    const uint16_t first = static_cast<uint16_t>(0xf000 | ((delta >> 12) & 0x3ff));
    const uint16_t second = static_cast<uint16_t>((link ? 0xd000 : 0x9000) |
        (((~(delta >> 23)) & 1) << 13) | (((~(delta >> 22)) & 1) << 11) | ((delta >> 1) & 0x7ff));
    return {uint8_t(first), uint8_t(first >> 8), uint8_t(second), uint8_t(second >> 8)};
}
std::array<uint8_t, 4> Trampoline(const Profile& p, size_t target) {
    // Replace complete prologue instructions, never a partial Thumb encoding.
    return Branch(Entry(p, target), Base(p) + static_cast<uint32_t>(p.size + HeaderBytes + 8 * target));
}
std::array<uint32_t, 8> CloseSites(const Profile& p) {
    return {p.mainCalls[0], p.mainCalls[1], p.mainCalls[2], p.mainCalls[3],
        p.mainCalls[4], p.mainCalls[5], p.historyCall, p.timeoutCall};
}
std::array<uint8_t, 4> CloseHook(const Profile& p, size_t hook) {
    const size_t entry = hook < 6 ? InfoCloseOffset : hook == 6 ? InfoHistoryOffset : InfoTimeoutOffset;
    const uint32_t source = hook < 6 ? p.mainCalls[hook] : hook == 6 ? p.historyCall : p.timeoutCall;
    return Branch(source, Base(p) + static_cast<uint32_t>(p.size + HeaderBytes + entry), true);
}
bool CloseHooksMatch(const HardwareRecord& record, const Profile& p, bool installed) {
    const auto* baseline = Baseline(p);
    const auto sites = CloseSites(p);
    for (size_t hook = 0; hook < sites.size(); ++hook) {
        if (!sites[hook]) continue;
        const size_t offset = sites[hook] - Base(p);
        if (installed) {
            const auto jump = CloseHook(p, hook);
            if (offset > record.image.size() || jump.size() > record.image.size() - offset ||
                !std::equal(jump.begin(), jump.end(), record.image.begin() + offset)) return false;
        } else if (!EqualRange(record.image, baseline->image, offset, 4)) return false;
    }
    return true;
}
std::array<uint8_t, 4> HookCode(const Profile& p, const Patches& settings, size_t hook) {
    // Both startup stages form one replacement: omit VOOPOO and execute the
    // custom sequence once at the named-logo entry, preserving its completion flag.
    if (hook == FirstHook && settings[0].bootStages == BootStages::Both)
        return {0x70, 0x47, 0x00, 0xbf}; // BX LR; NOP, before any stack adjustment.
    return Trampoline(p, hook);
}
// Lossless after RGB565 conversion: each row independently selects raw pixels,
// RGB565 runs, or exact 1/2/4/8-bit palette indices. Repeated rows share bytes.
// Comparing whole encodings accounts for palette overhead, even for tiny art.
std::vector<uint8_t> EncodeRow(const uint8_t* source,
    const std::map<uint16_t, uint16_t>& palette, unsigned bits) {
    std::vector<uint8_t> row;
    Put16(row, 0);
    row.insert(row.end(), source, source + Width * 2);
    std::vector<uint8_t> runs(2);
    uint16_t count = 0;
    for (size_t x = 0; x < Width;) {
        const uint16_t color = Read16(source + x * 2);
        size_t end = x + 1;
        while (end < Width && Read16(source + end * 2) == color) ++end;
        Put16(runs, static_cast<uint16_t>(end - x)); Put16(runs, color);
        ++count; x = end;
    }
    runs[0] = static_cast<uint8_t>(count);
    if (runs.size() < row.size()) row = std::move(runs);
    if (bits && 2 + Width * bits / 8 < row.size()) {
        row.assign(2 + Width * bits / 8, 0);
        row[1] = 0x80;
        for (size_t x = 0; x < Width; ++x) {
            const unsigned index = palette.at(Read16(source + x * 2));
            row[2 + x * bits / 8] |= static_cast<uint8_t>(index << (x * bits % 8));
        }
    }
    return row;
}

std::vector<uint8_t> EncodeRows(const std::vector<uint8_t>& pixels,
    const std::map<uint16_t, uint16_t>& palette, unsigned bits) {
    std::vector<uint8_t> bytes;
    Put16(bytes, static_cast<uint16_t>(palette.size())); Put16(bytes, static_cast<uint16_t>(bits));
    if (bits) {
        std::vector<uint16_t> colors(palette.size());
        for (const auto& [color, index] : palette) colors[index] = color;
        for (uint16_t color : colors) Put16(bytes, color);
    }
    const size_t table = bytes.size();
    bytes.resize(table + Height * 2);
    std::map<std::vector<uint8_t>, uint16_t> rows;
    for (size_t y = 0; y < Height; ++y) {
        const uint8_t* source = pixels.data() + y * Width * 2;
        auto row = EncodeRow(source, palette, bits);
        auto found = rows.find(row);
        uint16_t offset;
        if (found != rows.end()) offset = found->second;
        else {
            offset = static_cast<uint16_t>(bytes.size());
            rows.emplace(row, offset);
            bytes.insert(bytes.end(), row.begin(), row.end());
        }
        bytes[table + y * 2] = static_cast<uint8_t>(offset);
        bytes[table + y * 2 + 1] = static_cast<uint8_t>(offset >> 8);
    }
    return bytes;
}
std::vector<uint8_t> EncodeImage(const std::vector<uint8_t>& pixels) {
    auto bytes = EncodeRows(pixels, {}, 0);
    std::map<uint16_t, uint16_t> palette;
    for (size_t i = 0; i < pixels.size(); i += 2) {
        const uint16_t color = Read16(pixels.data() + i);
        if (!palette.contains(color)) {
            if (palette.size() == 256) return bytes;
            palette.emplace(color, static_cast<uint16_t>(palette.size()));
        }
    }
    const unsigned bits = palette.size() <= 2 ? 1 : palette.size() <= 4 ? 2 : palette.size() <= 16 ? 4 : 8;
    auto indexed = EncodeRows(pixels, palette, bits);
    return indexed.size() < bytes.size() ? std::move(indexed) : std::move(bytes);
}
bool DecodeImage(const std::vector<uint8_t>& bytes, size_t begin, std::vector<uint8_t>& pixels) {
    if (begin > bytes.size() || bytes.size() - begin < 4) return false;
    const unsigned count = Read16(bytes.data() + begin), bits = Read16(bytes.data() + begin + 2);
    if (count > 256 || (count == 0 ? bits != 0 :
        ((bits != 1 && bits != 2 && bits != 4 && bits != 8) || count > (1u << bits)))) return false;
    const size_t table = begin + 4 + count * 2;
    if (table > bytes.size() || bytes.size() - table < Height * 2) return false;
    pixels.clear(); pixels.reserve(PixelBytes);
    for (size_t y = 0; y < Height; ++y) {
        const size_t relative = Read16(bytes.data() + table + y * 2);
        if (relative < 4 + count * 2 + Height * 2 || relative > bytes.size() - begin ||
            bytes.size() - begin - relative < 2) return false;
        size_t at = begin + relative;
        const unsigned kind = Read16(bytes.data() + at); at += 2;
        if (kind == 0) {
            if (bytes.size() - at < Width * 2) return false;
            pixels.insert(pixels.end(), bytes.begin() + at, bytes.begin() + at + Width * 2);
        } else if (kind == 0x8000) {
            if (!count || bytes.size() - at < Width * bits / 8) return false;
            for (size_t x = 0; x < Width; ++x) {
                const unsigned index = (bytes[at + x * bits / 8] >> (x * bits % 8)) & ((1u << bits) - 1);
                if (index >= count) return false;
                Put16(pixels, Read16(bytes.data() + begin + 4 + index * 2));
            }
        } else {
            if (kind > Width || bytes.size() - at < kind * 4) return false;
            size_t x = 0;
            for (unsigned run = 0; run < kind; ++run, at += 4) {
                const size_t repeat = Read16(bytes.data() + at);
                if (!repeat || repeat > Width - x) return false;
                const uint16_t color = Read16(bytes.data() + at + 2);
                for (size_t i = 0; i < repeat; ++i) Put16(pixels, color);
                x += repeat;
            }
            if (x != Width) return false;
        }
    }
    return pixels.size() == PixelBytes;
}
struct SharedRows {
    std::vector<uint8_t> bytes;
    std::array<size_t, TargetCount> offsets{};
};

SharedRows EncodeSharedRows(const Patches& settings,
    const std::map<uint16_t, uint16_t>& palette, unsigned bits) {
    SharedRows result;
    auto& bytes = result.bytes;
    std::array<size_t, TargetCount> tables{};
    std::array<bool, TargetCount> unique{};
    // Every header precedes the common row pool: the unchanged native decoder
    // can keep using forward, image-relative 16-bit row offsets.
    for (size_t target = 0; target < TargetCount; ++target) {
        if (!settings[target].enabled) continue;
        bool duplicate = false;
        for (size_t previous = 0; previous < target; ++previous) {
            if (settings[previous].enabled && settings[previous].pixels == settings[target].pixels) {
                result.offsets[target] = result.offsets[previous];
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;
        unique[target] = true;
        result.offsets[target] = bytes.size();
        Put16(bytes, static_cast<uint16_t>(palette.size()));
        Put16(bytes, static_cast<uint16_t>(bits));
        if (bits) {
            std::vector<uint16_t> colors(palette.size());
            for (const auto& [color, index] : palette) colors[index] = color;
            for (const auto color : colors) Put16(bytes, color);
        }
        tables[target] = bytes.size();
        bytes.resize(bytes.size() + Height * 2);
    }
    std::map<std::vector<uint8_t>, size_t> rows;
    for (size_t target = 0; target < TargetCount; ++target) {
        if (!unique[target]) continue;
        for (size_t y = 0; y < Height; ++y) {
            auto row = EncodeRow(settings[target].pixels.data() + y * Width * 2, palette, bits);
            auto found = rows.find(row);
            size_t at;
            if (found != rows.end()) at = found->second;
            else {
                at = bytes.size();
                rows.emplace(row, at);
                bytes.insert(bytes.end(), row.begin(), row.end());
            }
            const size_t relative = at - result.offsets[target];
            if (relative > UINT16_MAX) return {}; // Use independent encodings instead.
            const size_t entry = tables[target] + y * 2;
            bytes[entry] = static_cast<uint8_t>(relative);
            bytes[entry + 1] = static_cast<uint8_t>(relative >> 8);
        }
    }
    return result;
}

struct EncodedImages {
    std::array<std::vector<uint8_t>, TargetCount> individual;
    SharedRows shared;
};

EncodedImages EncodeImages(const Patches& settings) {
    EncodedImages images;
    size_t individualBytes = 0;
    for (size_t target = 0; target < TargetCount; ++target) {
        if (!settings[target].enabled) continue;
        bool shared = false;
        for (size_t previous = 0; previous < target; ++previous)
            if (settings[previous].enabled && settings[previous].pixels == settings[target].pixels) { shared = true; break; }
        if (!shared) {
            images.individual[target] = EncodeImage(settings[target].pixels);
            individualBytes += images.individual[target].size();
        }
    }
    if (!individualBytes) return images;
    auto candidate = EncodeSharedRows(settings, {}, 0);
    if (!candidate.bytes.empty() && candidate.bytes.size() < individualBytes)
        images.shared = std::move(candidate);
    std::map<uint16_t, uint16_t> palette;
    for (const auto& screen : settings) {
        if (!screen.enabled) continue;
        for (size_t i = 0; i < screen.pixels.size(); i += 2) {
            const auto color = Read16(screen.pixels.data() + i);
            if (palette.contains(color)) continue;
            if (palette.size() == 256) return images;
            palette.emplace(color, static_cast<uint16_t>(palette.size()));
        }
    }
    const unsigned bits = palette.size() <= 2 ? 1 : palette.size() <= 4 ? 2 : palette.size() <= 16 ? 4 : 8;
    candidate = EncodeSharedRows(settings, palette, bits);
    const size_t bestBytes = images.shared.bytes.empty() ? individualBytes : images.shared.bytes.size();
    if (!candidate.bytes.empty() && candidate.bytes.size() < bestBytes)
        images.shared = std::move(candidate);
    return images;
}

std::vector<uint8_t> Extension(const Profile& p, const Patches& settings, const EncodedImages& encoded) {
    std::vector<uint8_t> bytes(HeaderBytes, 0);
    std::copy(Magic.begin(), Magic.end(), bytes.begin());
    bytes.insert(bytes.end(), std::begin(Executor), std::end(Executor));
    bytes.resize(FixedBytes, 0);
    uint32_t mask = 0;
    std::array<uint32_t, TargetCount> images{};
    const bool sharedRows = !encoded.shared.bytes.empty();
    const size_t imageData = bytes.size();
    if (sharedRows)
        bytes.insert(bytes.end(), encoded.shared.bytes.begin(), encoded.shared.bytes.end());
    for (size_t target = 0; target < TargetCount; ++target) {
        const auto& s = settings[target];
        if (!s.enabled) continue;
        mask |= 1u << target;
        if (sharedRows) {
            images[target] = Base(p) + static_cast<uint32_t>(p.size + imageData + encoded.shared.offsets[target]);
            continue;
        }
        for (size_t previous = 0; previous < target; ++previous)
            if (settings[previous].enabled && settings[previous].pixels == s.pixels) {
                images[target] = images[previous]; break;
            }
        if (!images[target]) {
            images[target] = Base(p) + static_cast<uint32_t>(p.size + bytes.size());
            bytes.insert(bytes.end(), encoded.individual[target].begin(), encoded.individual[target].end());
        }
    }
    for (size_t hook = 0; hook < HookCount; ++hook) {
        if (!HookEnabled(settings, hook) ||
            (hook == FirstHook && settings[0].bootStages == BootStages::Both)) continue;
        const size_t target = hook == FirstHook ? 0 : hook;
        const auto& s = settings[target];
        size_t at = HeaderBytes + sizeof(Executor) + DescriptorBytes * hook;
        for (uint32_t value : {p.rgb | 1, p.fill | 1, hook == 0 ? p.flag : 0u, p.watchdog | 1,
            p.cyclesPerMs, static_cast<uint32_t>(s.animation), s.durationMs, images[target]}) {
            Set32(bytes, at, value); at += 4;
        }
        for (uint32_t value : {static_cast<uint32_t>(s.exitAnimation), s.holdMs, s.exitDurationMs}) {
            Set32(bytes, at, value); at += 4;
        }
    }
    if (settings[static_cast<size_t>(Target::DeviceInfo)].enabled) {
        // Every displaced instruction is a complete guarded BL; its original
        // Main target is tail-called with the patched BL's return LR intact.
        size_t at = HeaderBytes + sizeof(Executor) + HookCount * DescriptorBytes;
        // Native page byte and RTOS history subview byte, never spare RAM.
        // The executable tail is LDR.W PC,[PC,#0]; original Main; padding.
        for (uint32_t value : {p.flag, p.servicePage, p.subview, p.history ? p.history | 1 : 0u,
            0xf000f8dfu, p.main | 1, 0u,
            p.backlight ? p.backlight | 1 : 0u}) {
            Set32(bytes, at, value); at += 4;
        }
    }
    size_t at = 8;
    for (uint32_t value : {1u, static_cast<uint32_t>(p.size), uint32_t(p.code), p.edition,
        static_cast<uint32_t>(bytes.size()), mask, uint32_t(sizeof(Executor)),
        uint32_t(BoundaryOffset),
        uint32_t(FirmwarePackage::Crc16Ccitt(bytes.data() + HeaderBytes, bytes.size() - HeaderBytes)),
        static_cast<uint32_t>(settings[0].enabled ? settings[0].bootStages : BootStages::Second)}) {
        Set32(bytes, at, value); at += 4;
    }
    const auto digest = FirmwarePackage::ComputeSha256(bytes.data() + HeaderBytes, bytes.size() - HeaderBytes);
    std::copy(digest.begin(), digest.end(), bytes.begin() + 48);
    return bytes;
}
bool Decode(const HardwareRecord& record, const Profile& p, Patches& settings, std::string& error) {
    if (record.image.size() < p.size + HeaderBytes || !Identity(record, p) ||
        uint64_t(Base(p)) + record.image.size() >= SettingsBase) {
        error = "Unsupported native screen Flash envelope or driver revision"; return false;
    }
    const uint8_t* header = record.image.data() + p.size;
    if (!std::equal(Magic.begin(), Magic.end(), header)) {
        error = "Unrecognized native screen signature"; return false;
    }
    settings = {};
    const uint32_t version = Read32(header + 8);
    const uint32_t mask = Read32(header + 28);
    if (version != 1 || !mask || (mask & ~((1u << TargetCount) - 1)) ||
        record.image.size() < p.size + FixedBytes || Read32(header + 24) != record.image.size() - p.size ||
        Read32(header + 40) != FirmwarePackage::Crc16Ccitt(header + HeaderBytes, record.image.size() - p.size - HeaderBytes)) {
        error = "Invalid native screen envelope bounds, version, or CRC"; return false;
    }
    const auto digest = FirmwarePackage::ComputeSha256(header + HeaderBytes, record.image.size() - p.size - HeaderBytes);
    if (!std::equal(digest.begin(), digest.end(), header + 48)) {
        error = "Native screen executor/image SHA-256 mismatch"; return false;
    }
    for (size_t target = 0; target < TargetCount; ++target) {
        if (!(mask & (1u << target))) continue;
        auto& s = settings[target];
        if (target == 0) s.bootStages = static_cast<BootStages>(Read32(header + 44));
        const size_t hook = target == 0 && s.bootStages == BootStages::First ? FirstHook : target;
        const uint8_t* descriptor = header + HeaderBytes + sizeof(Executor) + DescriptorBytes * hook;
        s.enabled = true; s.animation = static_cast<Animation>(Read32(descriptor + 20)); s.durationMs = Read32(descriptor + 24);
        s.exitAnimation = static_cast<Animation>(Read32(descriptor + 32));
        s.holdMs = Read32(descriptor + 36); s.exitDurationMs = Read32(descriptor + 40);
        const uint32_t address = Read32(descriptor + 28);
        if (address < Base(p) + p.size + FixedBytes || !DecodeImage(record.image, address - Base(p), s.pixels)) {
            error = "Invalid native screen row table, palette, or RGB565 stream"; return false;
        }
        if (!Valid(s, target, error)) return false;
    }
    const auto expected = Extension(p, settings, EncodeImages(settings));
    if (expected.size() != record.image.size() - p.size ||
        !std::equal(expected.begin(), expected.end(), record.image.begin() + p.size)) {
        error = "Unrecognized native screen executor, descriptors, or image encoding"; return false;
    }
    const auto* baseline = Baseline(p);
    for (size_t target = 0; target < HookCount; ++target) {
        const auto jump = HookCode(p, settings, target);
        const size_t offset = Entry(p, target) - Base(p);
        if (HookEnabled(settings, target) ? !std::equal(jump.begin(), jump.end(), record.image.begin() + offset) :
            !EqualRange(record.image, baseline->image, offset, 4)) {
            error = "Damaged native screen drawing trampoline"; return false;
        }
    }
    if (!CloseHooksMatch(record, p, settings[2].enabled)) {
        error = "Damaged native CHIP ID close trampoline"; return false;
    }
    return true;
}
}

bool Supported(const FirmwarePackage& package) {
    FirmwareModel model;
    FirmwareEdition edition;
    if (!DetectFirmwareProfile(package, model, edition) || model != FirmwareModel::DragS ||
        (edition != FirmwareEdition::Score && edition != FirmwareEdition::Factory)) return false;
    for (const auto& record : package.records) {
        const auto* p = Find(record);
        const auto* baseline = p ? Baseline(*p) : nullptr;
        if (!p || !baseline || !Identity(record, *p) || record.image.size() != p->size) return false;
        for (size_t target = 0; target < HookCount; ++target)
            if (!EqualRange(record.image, baseline->image, Entry(*p, target) - Base(*p), 4)) return false;
        if (!CloseHooksMatch(record, *p, false)) return false;
    }
    return true;
}

bool Canonicalize(FirmwarePackage& package, Patches& settings, std::string& error) {
    Patches decoded;
    bool havePatch = false, havePlain = false;
    for (const auto& record : package.records) {
        const auto* p = Find(record);
        if (p && p->edition == 0 && p->code == 0x3103 && record.image.size() == p->size &&
            FirmwarePackage::ComputeSha256(record.image.data() + (p->boot - Base(*p)), 96) ==
                "7f09286c1decdf849fdf6e23fb062a58ad46314deb64e61878dc2a441ac9afb3") {
            error = "The obsolete experimental RGB boot payload is unsupported; open the original firmware instead";
            return false;
        }
        if (!p || record.image.size() == p->size) { havePlain = true; continue; }
        Patches current;
        if (!Decode(record, *p, current, error)) return false;
        if (havePatch && current != decoded) { error = "Custom screen settings differ between hardware records"; return false; }
        decoded = std::move(current);
        havePatch = true;
    }
    if (!havePatch) { settings = {}; return true; }
    if (havePlain || package.format != FirmwareFormat::Zwdf) {
        error = "Incomplete or mixed native screen hardware inventory"; return false;
    }
    // Keep exact imported source_bytes/hash for projects and reset/dirty state.
    FirmwarePackage canonical;
    canonical.format = package.format;
    canonical.records = package.records;
    for (auto& record : canonical.records) {
        const auto& p = *Find(record);
        const auto* baseline = Baseline(p);
        for (size_t target = 0; target < HookCount; ++target) {
            if (!HookEnabled(decoded, target)) continue;
            const size_t offset = Entry(p, target) - Base(p);
            std::copy_n(baseline->image.begin() + offset, 4, record.image.begin() + offset);
        }
        if (decoded[2].enabled)
            for (const uint32_t site : CloseSites(p)) {
                if (!site) continue;
                const size_t offset = site - Base(p);
                std::copy_n(baseline->image.begin() + offset, 4, record.image.begin() + offset);
            }
        record.image.resize(p.size);
    }
    if (!Supported(canonical)) { error = "Unsupported native screen hardware inventory"; return false; }
    package.records = std::move(canonical.records);
    settings = std::move(decoded);
    return true;
}

bool MeasureFlash(const FirmwarePackage& package, const Patches& settings,
    std::vector<FlashUsage>& usage, std::string& error) {
    usage.clear();
    error.clear();
    if (!Supported(package)) {
        error = "Flash accounting requires a recognized DRAG S Score or Factory package";
        return false;
    }
    bool active = false;
    for (size_t target = 0; target < TargetCount; ++target) {
        if (!settings[target].enabled) continue;
        if (!Valid(settings[target], target, error)) return false;
        active = true;
    }
    const auto encoded = EncodeImages(settings);
    usage.reserve(package.records.size());
    for (const auto& record : package.records) {
        const auto& p = *Find(record);
        const size_t tailBytes = active ? Extension(p, settings, encoded).size() : 0;
        usage.push_back({p.code, p.size, tailBytes, SettingsBase - Base(p) - 1});
    }
    return true;
}

bool MeasureFlashChanges(const FirmwarePackage& package, const Patches& baseline, const Patches& settings,
    std::vector<FlashUsage>& usage, std::string& error) {
    if (!MeasureFlash(package, baseline, usage, error)) return false;
    auto progressive = baseline;
    for (size_t target = 0; target < TargetCount; ++target) {
        progressive[target] = settings[target];
        std::vector<FlashUsage> next;
        if (!MeasureFlash(package, progressive, next, error)) return false;
        for (size_t r = 0; r < usage.size(); ++r) {
            usage[r].screenDelta[target] = int64_t(next[r].Used()) - int64_t(usage[r].Used());
            usage[r].patchBytes = next[r].patchBytes;
        }
    }
    return true;
}

bool Build(const FirmwarePackage& package, const Patches& settings,
    const std::map<std::string, std::pair<std::vector<size_t>, std::vector<uint8_t>>>& replacements,
    const std::vector<CodePatch>& patches, std::vector<uint8_t>& output,
    BuildReport& report, std::string& error,
    const std::function<void(const char*)>& progress) {
    size_t active = 0;
    for (size_t target = 0; target < TargetCount; ++target) {
        if (!settings[target].enabled) continue;
        if (!Valid(settings[target], target, error)) return false;
        ++active;
    }
    if (!active) return package.PackWith(replacements, patches, output, report, error, progress);
    if (!Supported(package)) { error = "Custom native screens require a recognized DRAG S Score or Factory package"; return false; }
    std::vector<uint8_t> ordinary;
    BuildReport ordinaryReport;
    if (!package.PackWith(replacements, patches, ordinary, ordinaryReport, error, progress)) return false;
    size_t closeHooks = 0;
    FirmwarePackage extended;
    if (!FirmwarePackage::Parse(ordinary, extended, error, progress)) return false;
    if (!Supported(extended)) { error = "Resource/color/layout edits changed a required native screen driver"; return false; }
    if (progress) progress("Encode custom screen images and native screen extension");
    const auto encoded = EncodeImages(settings);
    for (auto& record : extended.records) {
        const auto& p = *Find(record);
        const auto tail = Extension(p, settings, encoded);
        const size_t available = SettingsBase - Base(p) - p.size - 1;
        if (tail.size() > available) {
            error = "Custom screens exceed safe Flash capacity for hardware " + std::to_string(p.code) +
                ": need " + std::to_string(tail.size()) + " bytes, available " + std::to_string(available) +
                " bytes below protected settings at 0x0801F000. Simplify the images or disable a custom screen.";
            return false;
        }
        for (size_t target = 0; target < HookCount; ++target) {
            if (!HookEnabled(settings, target)) continue;
            const auto jump = HookCode(p, settings, target);
            std::copy(jump.begin(), jump.end(), record.image.begin() + Entry(p, target) - Base(p));
        }
        if (settings[2].enabled) {
            const auto sites = CloseSites(p);
            for (size_t hook = 0; hook < sites.size(); ++hook) {
                if (!sites[hook]) continue;
                const auto jump = CloseHook(p, hook);
                std::copy(jump.begin(), jump.end(), record.image.begin() + sites[hook] - Base(p));
                ++closeHooks;
            }
        }
        record.image.insert(record.image.end(), tail.begin(), tail.end());
    }
    if (!extended.PackWith({}, {}, output, report, error, progress)) return false;
    report.source_sha256 = package.source_sha256;
    report.changed_assets = ordinaryReport.changed_assets + active;
    report.changed_colors = ordinaryReport.changed_colors;
    report.changed_layout_elements = ordinaryReport.changed_layout_elements;
    const size_t hooks = active + (settings[0].enabled && settings[0].bootStages == BootStages::Both ? 1 : 0);
    report.patched_ranges = ordinaryReport.patched_ranges + (hooks + 1) * package.records.size() + closeHooks;
    const size_t hookBytes = 4 * (hooks * package.records.size() + closeHooks);
    report.unchanged_payload_bytes_verified = ordinaryReport.unchanged_payload_bytes_verified > hookBytes ?
        ordinaryReport.unchanged_payload_bytes_verified - hookBytes : 0;
    return true;
}

bool NormalizeForGuard(std::vector<uint8_t>& stock, const HardwareRecord& candidate, std::string& error) {
    const auto* p = Find(candidate);
    if (!p || candidate.image.size() == p->size) return true;
    Patches settings;
    if (!Decode(candidate, *p, settings, error)) return false;
    const auto* baseline = Baseline(*p);
    if (stock.size() != p->size) { error = "Native screen guard requires the stock Flash image size"; return false; }
    for (size_t target = 0; target < HookCount; ++target) {
        if (!HookEnabled(settings, target)) continue;
        const size_t offset = Entry(*p, target) - Base(*p);
        if (!EqualRange(stock, baseline->image, offset, 4)) {
            error = "Native screen guard requires the stock drawing entrypoint"; return false;
        }
        const auto jump = HookCode(*p, settings, target);
        std::copy(jump.begin(), jump.end(), stock.begin() + offset);
    }
    if (settings[2].enabled) {
        const auto sites = CloseSites(*p);
        for (size_t hook = 0; hook < sites.size(); ++hook) {
            if (!sites[hook]) continue;
            const size_t offset = sites[hook] - Base(*p);
            if (!EqualRange(stock, baseline->image, offset, 4)) {
                error = "Native screen guard requires the stock CHIP ID close entrypoint"; return false;
            }
            const auto jump = CloseHook(*p, hook);
            std::copy(jump.begin(), jump.end(), stock.begin() + offset);
        }
    }
    stock.insert(stock.end(), candidate.image.begin() + p->size, candidate.image.end());
    return true;
}

uint32_t FrameBoundary(const HardwareRecord& record, Target target) {
    const auto* p = Find(record);
    const size_t index = static_cast<size_t>(target);
    if (!p || index >= TargetCount || record.image.size() == p->size) return 0;
    Patches settings;
    std::string error;
    if (!Decode(record, *p, settings, error) || !settings[index].enabled) return 0;
    return Base(*p) + static_cast<uint32_t>(p->size + HeaderBytes + BoundaryOffset);
}

uint32_t FrameEntry(const HardwareRecord& record, Target target, uint32_t phase) {
    const auto* p = Find(record);
    const size_t index = static_cast<size_t>(target);
    if (!p || index >= TargetCount || record.image.size() == p->size) return 0;
    Patches settings;
    std::string error;
    if (!Decode(record, *p, settings, error) || !settings[index].enabled) return 0;
    const auto& s = settings[index];
    const uint32_t entrance = s.animation == Animation::None ? 1u : PhaseCount;
    if (target == Target::DeviceInfo && phase >= entrance &&
        s.exitAnimation != Animation::None)
        return Base(*p) + static_cast<uint32_t>(p->size + HeaderBytes + InfoExitOffset);
    return Entry(*p, index == 0 && s.bootStages == BootStages::First ? FirstHook : index);
}

uint32_t FrameCount(const Settings& settings) {
    return (settings.animation == Animation::None ? 1u : PhaseCount) +
        (settings.exitAnimation == Animation::None ? 0u : PhaseCount);
}
}
