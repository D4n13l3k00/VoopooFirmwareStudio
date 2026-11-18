#include "firmware_project.h"
#include "app_state.h"
#include "firmware_logo_patch.h"
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <set>
#include <span>
#include <lzma.h>

namespace FirmwareProject {
namespace {
// Public V1: fixed 112-byte little-endian header and stored or XZ-compressed body.
// Header: magic[8], version:u32, headerBytes:u32, storedTotal:u64,
// decodedSource/decodedEdits:u64, source SHA-256[64 ASCII hex],
// CRC-32:u32, codec:u32 (0 = stored, 1 = single XZ/LZMA2 CRC64 stream).
// CRC-32/ISO-HDLC covers the stored file with its checksum field zeroed.
// Body: original firmware, source filename, counted bitmap/palette/layout/brush
// tables, three screen settings, editable RGBA sources with placement and image
// adjustments, then the modular menu settings.
// Strings, lengths and counts are u32; colors are three u32 channels (0..255);
// coordinates are signed i32; image adjustments are IEEE754 floats.
constexpr std::array<uint8_t, 8> Magic{'V', 'F', 'W', 'P', '\r', '\n', 0x1a, '\n'};
constexpr size_t HeaderBytes = 112;
constexpr size_t ChecksumOffset = 104;
constexpr size_t MenuBytes = (5 + FirmwareMenu::ItemCount) * sizeof(uint32_t);
constexpr size_t MaximumSourceBytes = 64 * 1024 * 1024;
constexpr uint64_t DecoderMemoryLimit = 64 * 1024 * 1024;
constexpr size_t MaximumIdBytes = 256;
constexpr size_t MaximumNameBytes = 1024;

uint32_t Checksum(std::span<const uint8_t> bytes) {
    static constexpr auto table = [] {
        std::array<uint32_t, 256> result{};
        for (uint32_t i = 0; i < result.size(); ++i) {
            uint32_t value = i;
            for (unsigned bit = 0; bit < 8; ++bit)
                value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0u);
            result[i] = value;
        }
        return result;
    }();
    uint32_t value = 0xffffffffu;
    for (size_t i = 0; i < bytes.size(); ++i) {
        const uint8_t byte = i >= ChecksumOffset && i < ChecksumOffset + 4 ? 0 : bytes[i];
        value = (value >> 8) ^ table[(value ^ byte) & 255];
    }
    return ~value;
}

void Put32(std::vector<uint8_t>& bytes, uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(uint8_t(value >> shift));
}
void Put64(std::vector<uint8_t>& bytes, uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8) bytes.push_back(uint8_t(value >> shift));
}
void Patch32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = uint8_t(value >> (i * 8));
}
void Patch64(std::vector<uint8_t>& bytes, size_t offset, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) bytes[offset + i] = uint8_t(value >> (i * 8));
}
void PutString(std::vector<uint8_t>& bytes, std::string_view text) {
    Put32(bytes, static_cast<uint32_t>(text.size()));
    bytes.insert(bytes.end(), text.begin(), text.end());
}
void PutColor(std::vector<uint8_t>& bytes, Rgb8 color) {
    Put32(bytes, color.r); Put32(bytes, color.g); Put32(bytes, color.b);
}

struct Reader {
    std::span<const uint8_t> bytes;
    size_t offset = 0;

    bool U32(uint32_t& value) {
        if (bytes.size() - offset < 4) return false;
        value = 0;
        for (unsigned i = 0; i < 4; ++i) value |= uint32_t(bytes[offset++]) << (i * 8);
        return true;
    }
    bool U64(uint64_t& value) {
        if (bytes.size() - offset < 8) return false;
        value = 0;
        for (unsigned i = 0; i < 8; ++i) value |= uint64_t(bytes[offset++]) << (i * 8);
        return true;
    }
    bool String(std::string& text, size_t maximum) {
        uint32_t length;
        if (!U32(length) || !length || length > maximum || length > bytes.size() - offset) return false;
        text.assign(reinterpret_cast<const char*>(bytes.data() + offset), length);
        offset += length;
        return text.find('\0') == std::string::npos;
    }
    bool Color(Rgb8& color) {
        uint32_t r, g, b;
        if (!U32(r) || !U32(g) || !U32(b) || r > 255 || g > 255 || b > 255) return false;
        color = {uint8_t(r), uint8_t(g), uint8_t(b)};
        return true;
    }
    bool Count(uint32_t& count, size_t maximum) { return U32(count) && count <= maximum; }
};

bool ValidName(const std::string& name) {
    return !name.empty() && name.size() <= MaximumNameBytes && name != "." && name != ".." &&
        name.find_first_of("/\\") == std::string::npos && name.find('\0') == std::string::npos &&
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), nullptr, 0) > 0;
}

bool ValidateEdits(const StudioState& studio, std::string& error) {
    if (!studio.hasFirmware || studio.firmware.source_bytes.empty() ||
        studio.firmware.source_bytes.size() > MaximumSourceBytes ||
        studio.originals.size() != studio.catalog.size()) {
        error = "Project has no supported original firmware source";
        return false;
    }
    if (!FirmwareMenu::ValidSettings(studio.menuPatch)) {
        error = "Invalid project device menu settings";
        return false;
    }
    if (studio.menuPatch.enabled && !FirmwareMenu::Supported(studio.firmware)) {
        error = "Device menu is not supported by the project firmware";
        return false;
    }
    constexpr size_t pixelBytes = size_t(FirmwareLogo::Width) * FirmwareLogo::Height * 2;
    const bool supported = FirmwareLogo::Supported(studio.firmware);
    for (size_t i = 0; i < studio.screenPatches.size(); ++i) {
        const auto& settings = studio.screenPatches[i];
        if (!studio.screenPatchSources[i].Valid()) {
            error = "Invalid project original image or placement";
            return false;
        }
        if (static_cast<uint32_t>(settings.animation) >= static_cast<uint32_t>(FirmwareLogo::Animation::Count) ||
            static_cast<uint32_t>(settings.exitAnimation) >= static_cast<uint32_t>(FirmwareLogo::Animation::Count) ||
            static_cast<uint32_t>(settings.bootStages) < 1 || static_cast<uint32_t>(settings.bootStages) > 3 ||
            (i != 0 && settings.bootStages != FirmwareLogo::BootStages::Second) ||
            settings.holdMs > 10000 || settings.exitDurationMs < 200 || settings.exitDurationMs > 2000 ||
            settings.durationMs < 200 || settings.durationMs > 2000 ||
            (!settings.pixels.empty() && settings.pixels.size() != pixelBytes) ||
            (settings.enabled && settings.pixels.size() != pixelBytes)) {
            error = "Invalid project screen image, animation, or timing (transitions 200-2000 ms; hold 0-10000 ms)";
            return false;
        }
        if (settings.enabled && !supported) {
            error = "Custom screen images are not supported by the project firmware";
            return false;
        }
    }
    for (const auto& [id, raw] : studio.replacements) {
        const int index = studio.FindAssetIndex(id);
        if (id.empty() || id.size() > MaximumIdBytes || index < 0 ||
            raw.size() != studio.catalog[index].byte_len() || raw.size() != studio.originals[index].size()) {
            error = "Unknown project bitmap or invalid bitmap length: " + id;
            return false;
        }
    }
    for (const auto& [id, color] : studio.assetPaintColors) {
        if (id.empty() || id.size() > MaximumIdBytes || studio.FindAssetIndex(id) < 0) {
            error = "Unknown project brush: " + id;
            return false;
        }
    }
    std::set<std::string_view> roles;
    for (const auto& entry : studio.palette) {
        if (!entry.spec.id || !*entry.spec.id || std::strlen(entry.spec.id) > MaximumIdBytes ||
            !roles.emplace(entry.spec.id).second) {
            error = "Invalid or duplicate project palette role";
            return false;
        }
    }
    for (const auto& [id, point] : studio.layoutOffsets) {
        if (id.empty() || id.size() > MaximumIdBytes) { error = "Invalid project layout identifier"; return false; }
    }
    std::vector<CodePatch> patches;
    return FirmwareLayout::BuildPatches(studio.firmware, studio.layoutElements, studio.layoutOffsets, patches, error);
}

bool DecodeBody(std::span<const uint8_t> body, size_t sourceSize, std::string_view expectedHash,
                StudioState& document, std::string& error,
                const std::function<void(const char*)>& progress) {
    auto fail = [&](const char* message) { error = message; return false; };
    const auto source = body.first(sourceSize);
    Reader edits{body.subspan(sourceSize)};
    std::string name;
    if (!edits.String(name, MaximumNameBytes) || !ValidName(name)) return fail("Invalid project source filename");
    FirmwareMenu::Settings projectMenu;
    if (edits.bytes.size() - edits.offset < MenuBytes) return fail("Truncated project device menu settings");
    Reader menu{edits.bytes.last(MenuBytes)};
    uint32_t enabled, batteryVoltage, reverseNavigation;
    if (!menu.U32(enabled) || enabled > 1 || !menu.U32(batteryVoltage) || batteryVoltage > 1 ||
        !menu.U32(reverseNavigation) || reverseNavigation > 1)
        return fail("Invalid project device menu settings");
    projectMenu = {enabled != 0, batteryVoltage != 0, reverseNavigation != 0};
    if (!menu.U32(projectMenu.enabledItems)) return fail("Invalid project device menu items");
    for (auto& item : projectMenu.order) {
        uint32_t id;
        if (!menu.U32(id) || id >= FirmwareMenu::ItemCount)
            return fail("Invalid project device menu item");
        item = static_cast<FirmwareMenu::Item>(id);
    }
    if (!menu.U32(projectMenu.infoFields)) return fail("Truncated project device menu INFO fields");
    if (!FirmwareMenu::ValidSettings(projectMenu)) return fail("Invalid project device menu settings");
    edits.bytes = edits.bytes.first(edits.bytes.size() - MenuBytes);
    FirmwarePackage package;
    const std::vector<uint8_t> sourceBytes(source.begin(), source.end());
    if (!FirmwarePackage::Parse(sourceBytes, package, error, progress)) return false;
    if (progress) progress("Verify original firmware SHA-256");
    if (package.source_sha256 != expectedHash) return fail("Project original firmware SHA-256 mismatch");
    if (progress) progress("Restore project resources and editable images");
    if (!document.LoadPackage(std::move(package), name, error)) return false;
    // Original source settings remain the reset baseline; project edits override them.
    document.menuPatch = projectMenu;
    uint32_t count;
    if (!edits.Count(count, document.catalog.size())) return fail("Invalid project bitmap count");
    for (uint32_t i = 0; i < count; ++i) {
        std::string id;
        uint32_t length;
        if (!edits.String(id, MaximumIdBytes) || !edits.U32(length)) return fail("Invalid project bitmap entry");
        const int index = document.FindAssetIndex(id);
        if (index < 0 || document.replacements.contains(id) || length != document.catalog[index].byte_len() ||
            length > edits.bytes.size() - edits.offset) return fail("Unknown or duplicate project bitmap, or invalid bitmap length");
        document.replacements.emplace(std::move(id), std::vector<uint8_t>(
            edits.bytes.begin() + edits.offset, edits.bytes.begin() + edits.offset + length));
        edits.offset += length;
    }
    if (!edits.Count(count, document.palette.size()) || count != document.palette.size())
        return fail("Invalid project palette count");
    std::set<std::string> roles;
    for (uint32_t i = 0; i < count; ++i) {
        std::string id;
        Rgb8 color;
        if (!edits.String(id, MaximumIdBytes) || !edits.Color(color) || !roles.emplace(id).second)
            return fail("Invalid or duplicate project palette color");
        const auto found = std::find_if(document.palette.begin(), document.palette.end(),
            [&](const PaletteState& role) { return id == role.spec.id; });
        if (found == document.palette.end()) return fail("Unknown project palette role");
        found->current = color;
    }
    if (!edits.Color(document.backgroundColor)) return fail("Invalid project background color");
    if (!edits.Count(count, document.layoutElements.size())) return fail("Invalid project layout count");
    for (uint32_t i = 0; i < count; ++i) {
        std::string id;
        uint32_t x, y;
        if (!edits.String(id, MaximumIdBytes) || !edits.U32(x) || !edits.U32(y) ||
            !document.layoutOffsets.emplace(std::move(id), FirmwareLayout::Point{
                std::bit_cast<int32_t>(x), std::bit_cast<int32_t>(y)}).second)
            return fail("Invalid or duplicate project layout entry");
    }
    if (!edits.Count(count, document.catalog.size())) return fail("Invalid project brush count");
    for (uint32_t i = 0; i < count; ++i) {
        std::string id;
        Rgb8 color;
        if (!edits.String(id, MaximumIdBytes) || !edits.Color(color) ||
            document.FindAssetIndex(id) < 0 || !document.assetPaintColors.emplace(std::move(id), color).second)
            return fail("Unknown or duplicate project brush, or invalid brush color");
    }
    document.screenPatches = {};
    for (size_t i = 0; i < FirmwareLogo::TargetCount; ++i) {
        uint32_t enabled, animation, durationMs, pixelBytes;
        constexpr size_t expectedPixels = size_t(FirmwareLogo::Width) * FirmwareLogo::Height * 2;
        if (!edits.U32(enabled) || enabled > 1 || !edits.U32(animation) ||
            animation >= static_cast<uint32_t>(FirmwareLogo::Animation::Count) ||
            !edits.U32(durationMs) || durationMs < 200 || durationMs > 2000 ||
            !edits.U32(pixelBytes) || (pixelBytes != 0 && pixelBytes != expectedPixels) ||
            (enabled && pixelBytes != expectedPixels) || pixelBytes > edits.bytes.size() - edits.offset)
            return fail("Invalid project screen image, animation, or duration (200-2000 ms)");
        auto& settings = document.screenPatches[i];
        settings.enabled = enabled != 0;
        settings.animation = static_cast<FirmwareLogo::Animation>(animation);
        settings.durationMs = durationMs;
        settings.pixels.assign(edits.bytes.begin() + edits.offset,
            edits.bytes.begin() + edits.offset + pixelBytes);
        edits.offset += pixelBytes;
        uint32_t stages, exit;
        if (!edits.U32(stages) || stages < 1 || stages > 3 || (i != 0 && stages != 2) ||
            !edits.U32(exit) || exit >= static_cast<uint32_t>(FirmwareLogo::Animation::Count) ||
            !edits.U32(settings.holdMs) || settings.holdMs > 10000 ||
            !edits.U32(settings.exitDurationMs) || settings.exitDurationMs < 200 || settings.exitDurationMs > 2000)
            return fail("Invalid project startup stages, exit animation, or timing");
        settings.bootStages = static_cast<FirmwareLogo::BootStages>(stages);
        settings.exitAnimation = static_cast<FirmwareLogo::Animation>(exit);
    }
    for (auto& source : document.screenPatchSources) {
        uint32_t width, height, fit, scale, x, y, alignX, alignY, filter, length;
        if (!edits.U32(width) || !edits.U32(height) || !edits.U32(fit) ||
            !edits.U32(scale) || !edits.U32(x) || !edits.U32(y) ||
            !edits.U32(alignX) || !edits.U32(alignY) || !edits.U32(filter) ||
            !edits.U32(length) || length > edits.bytes.size() - edits.offset ||
            width > FirmwareLogo::MaximumEditableSourceDimension ||
            height > FirmwareLogo::MaximumEditableSourceDimension ||
            length > FirmwareLogo::MaximumEditableSourceBytes ||
            (length == 0 ? (width != 0 || height != 0) :
                (width == 0 || height == 0 || uint64_t(width) * height * 4 != length)))
            return fail("Invalid project original RGBA image");
        source.width = static_cast<int>(width);
        source.height = static_cast<int>(height);
        source.fitMode = std::bit_cast<int32_t>(fit);
        source.scalePercent = std::bit_cast<float>(scale);
        source.x = std::bit_cast<int32_t>(x);
        source.y = std::bit_cast<int32_t>(y);
        source.alignX = std::bit_cast<int32_t>(alignX);
        source.alignY = std::bit_cast<int32_t>(alignY);
        source.resampling = static_cast<FirmwareBitmap::Resampling>(filter);
        const size_t rgbaOffset = edits.offset;
        edits.offset += length;
        auto& a = source.adjustments;
        for (float* value : {&a.brightness, &a.contrast, &a.saturation, &a.sharpness,
            &a.gamma, &a.curve[0], &a.curve[1], &a.curve[2]}) {
            uint32_t encoded;
            if (!edits.U32(encoded)) return fail("Truncated project image adjustments");
            *value = std::bit_cast<float>(encoded);
        }
        if (!edits.U32(a.paletteColors)) return fail("Truncated project image adjustments");
        for (auto& channel : a.channelCurves)
            for (float& value : channel) {
                uint32_t encoded;
                if (!edits.U32(encoded)) return fail("Truncated project channel curves");
                value = std::bit_cast<float>(encoded);
            }
        if (!a.Valid()) return fail("Invalid project image adjustments");
        // Validate placement and adjustments before allocating original pixels.
        auto placement = source;
        placement.width = placement.height = 0;
        if (!placement.Valid()) return fail("Invalid project image placement or adjustments");
        if (length)
            source.rgba = std::make_shared<const std::vector<uint8_t>>(
                edits.bytes.begin() + rgbaOffset, edits.bytes.begin() + rgbaOffset + length);
    }
    if (edits.offset != edits.bytes.size()) return fail("Unexpected trailing project bytes");
    if (progress) progress("Validate restored project edits");
    return ValidateEdits(document, error);
}

bool Decode(std::span<const uint8_t> bytes, StudioState& document, std::string& error,
            const std::function<void(const char*)>& progress) {
    if (progress) progress("Validate project header and section sizes");
    auto fail = [&](const char* message) { error = message; return false; };
    if (bytes.size() < HeaderBytes || bytes.size() > MaximumFileBytes ||
        !std::equal(Magic.begin(), Magic.end(), bytes.begin())) return fail("Invalid Voopoo Firmware Project header");
    Reader header{bytes, Magic.size()};
    uint32_t version, headerSize, checksum, codec;
    uint64_t totalSize, sourceSize, editsSize;
    if (!header.U32(version) || version != FirmwareProject::Version)
        return fail("Unsupported Voopoo Firmware Project version");
    if (!header.U32(headerSize) || !header.U64(totalSize) || !header.U64(sourceSize) || !header.U64(editsSize) ||
        headerSize != HeaderBytes || totalSize != bytes.size() || !sourceSize || sourceSize > MaximumSourceBytes ||
        editsSize > MaximumDecodedBodyBytes - sourceSize)
        return fail("Invalid project section sizes");
    const std::string_view expectedHash(reinterpret_cast<const char*>(bytes.data() + header.offset), 64);
    header.offset += 64;
    // Verify stored-byte integrity before allocating a decoded body or invoking liblzma.
    if (progress) progress("Verify project CRC-32 checksum");
    if (!header.U32(checksum) || !header.U32(codec) || checksum != Checksum(bytes))
        return fail("Project checksum mismatch");
    if (codec > 1) return fail("Unsupported project codec or flags");
    const size_t decodedSize = static_cast<size_t>(sourceSize + editsSize);
    const auto payload = bytes.subspan(HeaderBytes);
    if (codec == 0) {
        if (payload.size() != decodedSize) return fail("Invalid stored project body length");
        return DecodeBody(payload, static_cast<size_t>(sourceSize), expectedHash, document, error, progress);
    }
    if (progress) progress("Decompress project body (XZ / LZMA2, CRC64)");
    lzma_stream stream = LZMA_STREAM_INIT;
    struct EndStream {
        lzma_stream& stream;
        ~EndStream() { lzma_end(&stream); }
    } endStream{stream};
    if (lzma_stream_decoder(&stream, DecoderMemoryLimit, LZMA_TELL_ANY_CHECK) != LZMA_OK)
        return fail("Cannot initialize project XZ decoder");
    std::vector<uint8_t> decoded(decodedSize);
    stream.next_in = payload.data();
    stream.avail_in = payload.size();
    stream.next_out = decoded.data();
    stream.avail_out = decoded.size();
    bool checkedCrc64 = false;
    for (;;) {
        const lzma_ret result = lzma_code(&stream, LZMA_FINISH);
        if (result == LZMA_GET_CHECK) {
            if (lzma_get_check(&stream) != LZMA_CHECK_CRC64) return fail("Project XZ stream must use CRC64");
            checkedCrc64 = true;
            continue;
        }
        if (result == LZMA_STREAM_END) {
            // No concatenation or stream padding: exactly one frame consumes all input.
            if (!checkedCrc64 || stream.avail_in != 0 || stream.total_out != decodedSize)
                return fail("Invalid project XZ stream length or trailing bytes");
            break;
        }
        if (result == LZMA_MEMLIMIT_ERROR) return fail("Project XZ decoder memory limit exceeded");
        if (result != LZMA_OK) return fail("Invalid project XZ data");
    }
    return DecodeBody(decoded, static_cast<size_t>(sourceSize), expectedHash, document, error, progress);
}
} // namespace

bool IsProjectPath(const std::string& utf8Path) {
    auto extension = PathUtils::ToFsPath(utf8Path).extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t c) {
        return c >= L'A' && c <= L'Z' ? wchar_t(c + L'a' - L'A') : c;
    });
    return extension == L".vfwp";
}

bool Build(const StudioState& studio, std::vector<uint8_t>& bytes, std::string& error,
           const std::function<void(const char*)>& progress) {
    bytes.clear();
    error.clear();
    if (progress) progress("Validate project edits");
    if (!ValidateEdits(studio, error)) return false;
    const auto name = PathUtils::WideToUtf8(PathUtils::ToFsPath(studio.firmwarePath).filename().wstring());
    if (!ValidName(name)) { error = "Invalid project source filename"; return false; }
    size_t editsSize = 4 + name.size() + 4 * 4 + 12 + MenuBytes;
    for (const auto& settings : studio.screenPatches) editsSize += 32 + settings.pixels.size();
    for (const auto& source : studio.screenPatchSources) editsSize += 112 + (source.rgba ? source.rgba->size() : 0);
    for (const auto& [id, raw] : studio.replacements) editsSize += 8 + id.size() + raw.size();
    for (const auto& role : studio.palette) editsSize += 4 + std::strlen(role.spec.id) + 12;
    for (const auto& [id, point] : studio.layoutOffsets) editsSize += 4 + id.size() + 8;
    for (const auto& [id, color] : studio.assetPaintColors) editsSize += 4 + id.size() + 12;
    const size_t sourceSize = studio.firmware.source_bytes.size();
    if (editsSize > MaximumDecodedBodyBytes - sourceSize) { error = "Project exceeds its size limit"; return false; }
    const size_t totalSize = HeaderBytes + sourceSize + editsSize;
    if (progress) progress("Serialize original firmware, edits and editable images");
    bytes.reserve(totalSize);
    bytes.insert(bytes.end(), Magic.begin(), Magic.end());
    Put32(bytes, Version); Put32(bytes, static_cast<uint32_t>(HeaderBytes));
    Put64(bytes, totalSize); Put64(bytes, sourceSize); Put64(bytes, editsSize);
    if (progress) progress("Calculate original firmware SHA-256");
    const auto hash = FirmwarePackage::ComputeSha256(studio.firmware.source_bytes.data(), sourceSize);
    bytes.insert(bytes.end(), hash.begin(), hash.end());
    Put32(bytes, 0); Put32(bytes, 0);
    if (progress) progress("Serialize project body");
    bytes.insert(bytes.end(), studio.firmware.source_bytes.begin(), studio.firmware.source_bytes.end());
    PutString(bytes, name);
    Put32(bytes, static_cast<uint32_t>(studio.replacements.size()));
    for (const auto& [id, raw] : studio.replacements) {
        PutString(bytes, id); Put32(bytes, static_cast<uint32_t>(raw.size()));
        bytes.insert(bytes.end(), raw.begin(), raw.end());
    }
    Put32(bytes, static_cast<uint32_t>(studio.palette.size()));
    for (const auto& role : studio.palette) { PutString(bytes, role.spec.id); PutColor(bytes, role.current); }
    PutColor(bytes, studio.backgroundColor);
    Put32(bytes, static_cast<uint32_t>(studio.layoutOffsets.size()));
    for (const auto& [id, point] : studio.layoutOffsets) {
        PutString(bytes, id); Put32(bytes, static_cast<uint32_t>(point.x)); Put32(bytes, static_cast<uint32_t>(point.y));
    }
    Put32(bytes, static_cast<uint32_t>(studio.assetPaintColors.size()));
    for (const auto& [id, color] : studio.assetPaintColors) { PutString(bytes, id); PutColor(bytes, color); }
    for (const auto& settings : studio.screenPatches) {
        Put32(bytes, settings.enabled ? 1 : 0);
        Put32(bytes, static_cast<uint32_t>(settings.animation));
        Put32(bytes, settings.durationMs);
        Put32(bytes, static_cast<uint32_t>(settings.pixels.size()));
        bytes.insert(bytes.end(), settings.pixels.begin(), settings.pixels.end());
        Put32(bytes, static_cast<uint32_t>(settings.bootStages));
        Put32(bytes, static_cast<uint32_t>(settings.exitAnimation));
        Put32(bytes, settings.holdMs);
        Put32(bytes, settings.exitDurationMs);
    }
    for (const auto& source : studio.screenPatchSources) {
        Put32(bytes, static_cast<uint32_t>(source.width));
        Put32(bytes, static_cast<uint32_t>(source.height));
        Put32(bytes, static_cast<uint32_t>(source.fitMode));
        Put32(bytes, std::bit_cast<uint32_t>(source.scalePercent));
        Put32(bytes, static_cast<uint32_t>(source.x));
        Put32(bytes, static_cast<uint32_t>(source.y));
        Put32(bytes, static_cast<uint32_t>(source.alignX));
        Put32(bytes, static_cast<uint32_t>(source.alignY));
        Put32(bytes, static_cast<uint32_t>(source.resampling));
        Put32(bytes, source.rgba ? static_cast<uint32_t>(source.rgba->size()) : 0);
        if (source.rgba) bytes.insert(bytes.end(), source.rgba->begin(), source.rgba->end());
        const auto& a = source.adjustments;
        for (float value : {a.brightness, a.contrast, a.saturation, a.sharpness,
            a.gamma, a.curve[0], a.curve[1], a.curve[2]})
            Put32(bytes, std::bit_cast<uint32_t>(value));
        Put32(bytes, a.paletteColors);
        for (const auto& channel : a.channelCurves)
            for (float value : channel) Put32(bytes, std::bit_cast<uint32_t>(value));
    }
    Put32(bytes, studio.menuPatch.enabled ? 1 : 0);
    Put32(bytes, studio.menuPatch.batteryVoltage ? 1 : 0);
    Put32(bytes, studio.menuPatch.reverseNavigation ? 1 : 0);
    Put32(bytes, studio.menuPatch.enabledItems);
    for (const auto item : studio.menuPatch.order) Put32(bytes, static_cast<uint32_t>(item));
    Put32(bytes, studio.menuPatch.infoFields);
    // The original builder is also the stored fallback. A bounded output buffer
    // allows XZ to succeed only when it actually saves space, without recopying
    // the source and edit tables into another uncompressed body.
    if (progress) progress("Compress project body (XZ / LZMA2, preset 6, CRC64)");
    std::vector<uint8_t> compressed(totalSize - 1);
    std::copy_n(bytes.begin(), HeaderBytes, compressed.begin());
    size_t compressedEnd = HeaderBytes;
    const lzma_ret result = lzma_easy_buffer_encode(6, LZMA_CHECK_CRC64, nullptr,
        bytes.data() + HeaderBytes, totalSize - HeaderBytes,
        compressed.data(), &compressedEnd, compressed.size());
    if (result == LZMA_OK) {
        compressed.resize(compressedEnd);
        Patch64(compressed, 16, compressed.size());
        Patch32(compressed, 108, 1);
        bytes = std::move(compressed);
        if (progress) progress("Use compressed project body");
    } else if (result != LZMA_BUF_ERROR) {
        bytes.clear();
        error = "Cannot compress project body";
        return false;
    }
    if (result == LZMA_BUF_ERROR && progress) progress("Use stored body (compression did not reduce size)");
    if (progress) progress("Calculate project CRC-32 checksum");
    Patch32(bytes, ChecksumOffset, Checksum(bytes));
    return true;
}

SaveResult Save(StudioState& studio, const std::string& utf8Path,
                const std::function<void(const char*)>& progress) {
    SaveResult result;
    if (utf8Path.empty()) { result.error = "Project destination is empty"; return result; }
    std::vector<uint8_t> bytes;
    if (!Build(studio, bytes, result.error, progress)) return result;
    std::filesystem::path protectedSource;
    if (!studio.isBuiltin && !studio.firmwareSourceEmbedded) {
        const auto source = PathUtils::ToFsPath(studio.firmwarePath);
        std::error_code ec;
        if (std::filesystem::exists(source, ec) && !ec) protectedSource = source;
    }
    if (!AtomicFile::Write(PathUtils::ToFsPath(utf8Path), bytes.data(), bytes.size(), true,
                           result.fileError, protectedSource, {}, progress)) return result;
    studio.projectPath = utf8Path;
    studio.EnableProjectDocument();
    studio.MarkSaved();
    result.saved = true;
    return result;
}

bool Open(StudioState& studio, const std::string& utf8Path, std::string& error,
          const std::function<void(const char*)>& progress) {
    error.clear();
    if (progress) progress("Read project file");
    auto file = PathUtils::OpenReadStream(utf8Path, std::ios::binary | std::ios::ate);
    if (!file) { error = "Cannot open project file"; return false; }
    const auto length = file.tellg();
    if (length < static_cast<std::streamoff>(HeaderBytes) || length > static_cast<std::streamoff>(MaximumFileBytes)) {
        error = "Project file size is outside its supported range";
        return false;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())) ||
        file.peek() != std::char_traits<char>::eof()) { error = "Cannot read complete project file"; return false; }
    StudioState document;
    if (!Decode(bytes, document, error, progress)) return false;
    document.projectPath = utf8Path;
    studio.AdoptProjectDocument(document);
    return true;
}

} // namespace FirmwareProject
