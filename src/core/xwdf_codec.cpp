#include "xwdf_codec.h"
#include "zwdf_package.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace {
constexpr size_t kHeaderSize = 100;
constexpr size_t kBlockSize = 132;
constexpr size_t kDataSize = 128;

uint32_t Read32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

void Write32(uint8_t* p, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) p[i] = uint8_t(value >> (8 * i));
}

// Openlib.IO.CRC.crc32_old complements the accumulator AFTER EVERY BYTE,
// unlike standard CRC-32. The initial value is zero; chaining keeps that state.
uint32_t VerifyCode(const uint8_t* data, size_t length, uint32_t crc = 0) {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> result{};
        for (uint32_t i = 0; i < result.size(); ++i) {
            uint32_t value = i;
            for (int bit = 0; bit < 8; ++bit)
                value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0);
            result[i] = value;
        }
        return result;
    }();
    for (size_t i = 0; i < length; ++i)
        crc = ~(table[(crc ^ data[i]) & 255] ^ (crc >> 8));
    return crc;
}

uint8_t Transform(uint8_t value, size_t position) {
    return position % 3 == 0 ? uint8_t(~value) : uint8_t((value << 4) | (value >> 4));
}

// Recovered from Openlib.IO.SimpleEncrypt.Decrypt128Byte. Its final loop
// reverses each eight-byte group twice, so contributes no transformation.
void Decode(const uint8_t* encoded, uint8_t* decoded) {
    for (size_t i = 0; i < kDataSize; ++i)
        decoded[(i + 64) % kDataSize] = Transform(encoded[i], i);
}

void Encode(const uint8_t* decoded, uint8_t* encoded) {
    for (size_t i = 0; i < kDataSize; ++i)
        encoded[i] = Transform(decoded[(i + 64) % kDataSize], i);
}

struct Segment {
    size_t offset;
    size_t size;
    uint32_t verify_code;
};

bool ReadLayout(const std::vector<uint8_t>& bytes, std::array<Segment, 4>& segments,
                std::string& error) {
    const uint8_t version[] = { 1, 0, 0, 2 };
    if (bytes.size() < kHeaderSize + kBlockSize + 4 ||
        std::memcmp(bytes.data(), "XWDF", 4) != 0) {
        error = "Truncated XWDF header";
        return false;
    }
    if (std::memcmp(bytes.data() + 4, version, sizeof(version)) != 0) {
        error = "Unsupported XWDF container version";
        return false;
    }
    if (Read32(bytes.data() + 28) != bytes.size() || Read32(bytes.data() + 32) != 4) {
        error = "Invalid XWDF file length or segment count";
        return false;
    }
    static const char* names[] = { "HEAD", "ROM ", "RSC ", "BLKL" };
    const size_t payload_end = bytes.size() - 4;
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto* entry = bytes.data() + 36 + i * 16;
        if (std::memcmp(entry, names[i], 4) != 0) {
            error = "Unexpected XWDF segment directory";
            return false;
        }
        auto& segment = segments[i];
        segment = { Read32(entry + 4), Read32(entry + 8), Read32(entry + 12) };
        if (segment.offset < kHeaderSize || segment.offset > payload_end ||
            segment.size > payload_end - segment.offset) {
            error = "XWDF segment is outside the file";
            return false;
        }
        if (!segment.size) continue;
        for (size_t j = 0; j < i; ++j) {
            const auto& previous = segments[j];
            if (previous.size && segment.offset < previous.offset + previous.size &&
                previous.offset < segment.offset + segment.size) {
                error = "Overlapping XWDF segments";
                return false;
            }
        }
    }
    if (segments[0].size != kBlockSize || !segments[1].size ||
        segments[1].size % kBlockSize != 0 || segments[2].size != 0 ||
        segments[3].size < 4 || (segments[3].size - 4) % 12 != 0) {
        error = "Unsupported XWDF segment layout (nonempty RSC is not supported)";
        return false;
    }
    // The official parser does not interpret the empty RSC verification field.
    // Preserve that opaque directory value rather than treating it as a CRC.
    for (size_t i : { size_t(0), size_t(1), size_t(3) }) {
        const auto& segment = segments[i];
        if (VerifyCode(bytes.data() + segment.offset, segment.size) != segment.verify_code) {
            error = std::string("XWDF segment checksum mismatch: ") + names[i];
            return false;
        }
    }
    const auto& blacklist = segments[3];
    if (Read32(bytes.data() + blacklist.offset) != (blacklist.size - 4) / 12) {
        error = "Invalid XWDF blacklist record count";
        return false;
    }
    uint32_t crc = 0;
    for (size_t i : { size_t(0), size_t(1), size_t(3) })
        crc = VerifyCode(bytes.data() + segments[i].offset, segments[i].size, crc);
    crc = VerifyCode(bytes.data(), kHeaderSize, crc);
    if (crc != Read32(bytes.data() + payload_end)) {
        error = "XWDF package checksum mismatch";
        return false;
    }
    return true;
}

bool DecodeBlock(const uint8_t* block, uint8_t* decoded, std::string& error) {
    if (VerifyCode(block, kDataSize) != Read32(block + kDataSize)) {
        error = "XWDF block checksum mismatch";
        return false;
    }
    Decode(block, decoded);
    return true;
}
} // namespace

bool XwdfCodec::Parse(const std::vector<uint8_t>& bytes, FirmwarePackage& package,
                      std::string& error) {
    std::array<Segment, 4> segments{};
    if (!ReadLayout(bytes, segments, error)) return false;
    std::array<uint8_t, kDataSize> head{};
    if (!DecodeBlock(bytes.data() + segments[0].offset, head.data(), error)) return false;
    const size_t image_size = Read32(head.data());
    const size_t blocks = segments[1].size / kBlockSize;
    if (image_size < 8 || image_size > blocks * kDataSize ||
        (image_size + kDataSize - 1) / kDataSize != blocks) {
        error = "XWDF HEAD image length does not match ROM blocks";
        return false;
    }
    HardwareRecord record;
    record.image.resize(image_size);
    record.marker.assign(head.begin() + 76, head.begin() + 88);
    std::array<uint8_t, kDataSize> decoded{};
    for (size_t i = 0; i < blocks; ++i) {
        if (!DecodeBlock(bytes.data() + segments[1].offset + i * kBlockSize,
                         decoded.data(), error)) return false;
        const size_t length = (std::min)(kDataSize, image_size - i * kDataSize);
        std::copy_n(decoded.data(), length, record.image.data() + i * kDataSize);
    }
    record.sp = Read32(record.image.data());
    record.reset = Read32(record.image.data() + 4);
    // This XWDF application is linked after the 12 KiB STM32-compatible loader.
    // The reset stub at image+0x144 and absolute code/data references agree.
    record.linked_base = 0x08003000;
    if (record.sp < 0x20000000 || record.sp >= 0x20010000 || (record.sp & 3) != 0 ||
        (record.reset & 1) == 0 || record.reset < record.linked_base ||
        size_t(record.reset - record.linked_base) >= image_size) {
        error = "Unsupported XWDF Cortex-M vector table";
        return false;
    }
    package.format = FirmwareFormat::Xwdf;
    package.records.push_back(std::move(record));
    return true;
}

bool XwdfCodec::Repack(const std::vector<uint8_t>& source, const std::vector<uint8_t>& image,
                       std::vector<uint8_t>& output, std::string& error) {
    std::array<Segment, 4> segments{};
    if (!ReadLayout(source, segments, error)) return false;
    std::array<uint8_t, kDataSize> head{};
    if (!DecodeBlock(source.data() + segments[0].offset, head.data(), error)) return false;
    if (image.size() != Read32(head.data())) {
        error = "XWDF ROM length cannot change";
        return false;
    }
    output = source;
    std::array<uint8_t, kDataSize> decoded{};
    const size_t blocks = segments[1].size / kBlockSize;
    if (image.size() < 8 || image.size() > blocks * kDataSize ||
        (image.size() + kDataSize - 1) / kDataSize != blocks) {
        error = "XWDF HEAD image length does not match ROM blocks";
        return false;
    }
    for (size_t i = 0; i < blocks; ++i) {
        const size_t offset = segments[1].offset + i * kBlockSize;
        if (!DecodeBlock(source.data() + offset, decoded.data(), error)) return false;
        const size_t length = (std::min)(kDataSize, image.size() - i * kDataSize);
        const auto* replacement = image.data() + i * kDataSize;
        if (std::memcmp(decoded.data(), replacement, length) == 0) continue;
        // Keep final-block padding exactly as supplied, even when that block changes.
        std::copy_n(replacement, length, decoded.data());
        Encode(decoded.data(), output.data() + offset);
        Write32(output.data() + offset + kDataSize, VerifyCode(output.data() + offset, kDataSize));
    }
    Write32(output.data() + 64, VerifyCode(output.data() + segments[1].offset, segments[1].size));
    uint32_t crc = 0;
    for (size_t i : { size_t(0), size_t(1), size_t(3) })
        crc = VerifyCode(output.data() + segments[i].offset, segments[i].size, crc);
    crc = VerifyCode(output.data(), kHeaderSize, crc);
    Write32(output.data() + output.size() - 4, crc);
    return true;
}
