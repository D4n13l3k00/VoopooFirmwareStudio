#include "zwdf_package.h"
#include "path_utils.h"
#include "firmware_catalog.h"
#include "firmware_color_profiles.h"
#include "factory_color_profiles.h"
#include "xwdf_codec.h"
#include "drag2_color.h"
#include "firmware_extensions.h"
#include "firmware_layout.h"
#include <array>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <set>
#include <string_view>

// --- Lightweight portable SHA-256 implementation ---
namespace {
    inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    inline uint32_t sig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
    inline uint32_t sig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
    inline uint32_t theta0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
    inline uint32_t theta1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

    const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };

    std::string Sha256Bytes(const uint8_t* data, size_t len) {
        uint32_t h[8] = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };

        std::vector<uint8_t> msg(data, data + len);
        uint64_t bit_len = (uint64_t)len * 8;
        msg.push_back(0x80);
        while ((msg.size() % 64) != 56) {
            msg.push_back(0x00);
        }
        for (int i = 7; i >= 0; --i) {
            msg.push_back((uint8_t)((bit_len >> (i * 8)) & 0xFF));
        }

        for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
            uint32_t w[64];
            for (int i = 0; i < 16; ++i) {
                w[i] = (msg[chunk + i * 4] << 24) |
                       (msg[chunk + i * 4 + 1] << 16) |
                       (msg[chunk + i * 4 + 2] << 8) |
                       (msg[chunk + i * 4 + 3]);
            }
            for (int i = 16; i < 64; ++i) {
                w[i] = theta1(w[i - 2]) + w[i - 7] + theta0(w[i - 15]) + w[i - 16];
            }

            uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
            uint32_t e = h[4], f = h[5], g = h[6], nh = h[7];

            for (int i = 0; i < 64; ++i) {
                uint32_t t1 = nh + sig1(e) + ch(e, f, g) + K[i] + w[i];
                uint32_t t2 = sig0(a) + maj(a, b, c);
                nh = g; g = f; f = e; e = d + t1;
                d = c; c = b; b = a; a = t1 + t2;
            }

            h[0] += a; h[1] += b; h[2] += c; h[3] += d;
            h[4] += e; h[5] += f; h[6] += g; h[7] += nh;
        }

        std::ostringstream ss;
        ss << std::hex << std::setfill('0');
        for (int i = 0; i < 8; ++i) {
            ss << std::setw(8) << h[i];
        }
        return ss.str();
    }

    uint16_t ReadBeU16(const uint8_t* p) {
        return (uint16_t)((p[0] << 8) | p[1]);
    }

    uint32_t ReadBeU32(const uint8_t* p) {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    }

    uint32_t ReadLeU32(const uint8_t* p) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    void WriteBeU16(std::vector<uint8_t>& buf, uint16_t val) {
        buf.push_back((uint8_t)(val >> 8));
        buf.push_back((uint8_t)(val & 0xFF));
    }

    void WriteBeU32(std::vector<uint8_t>& buf, uint32_t val) {
        buf.push_back((uint8_t)(val >> 24));
        buf.push_back((uint8_t)(val >> 16));
        buf.push_back((uint8_t)(val >> 8));
        buf.push_back((uint8_t)(val & 0xFF));
    }
}

uint16_t FirmwarePackage::Crc16Ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint32_t FirmwarePackage::AdditiveChecksum(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum += data[i];
    }
    return sum;
}

std::string FirmwarePackage::ComputeSha256(const uint8_t* data, size_t len) {
    return Sha256Bytes(data, len);
}

std::string BuildReport::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"source_sha256\": \"" << source_sha256 << "\",\n";
    ss << "  \"output_sha256\": \"" << output_sha256 << "\",\n";
    ss << "  \"output_size\": " << output_size << ",\n";
    ss << "  \"records\": " << records << ",\n";
    ss << "  \"changed_assets\": " << changed_assets << ",\n";
    ss << "  \"changed_colors\": " << changed_colors << ",\n";
    ss << "  \"changed_layout_elements\": " << changed_layout_elements << ",\n";
    ss << "  \"patched_ranges\": " << patched_ranges << ",\n";
    ss << "  \"unchanged_payload_bytes_verified\": " << unchanged_payload_bytes_verified << ",\n";
    ss << "  \"hardware_codes\": [";
    for (size_t i = 0; i < hardware_codes.size(); ++i) {
        ss << "\"" << hardware_codes[i] << "\"" << (i + 1 < hardware_codes.size() ? ", " : "");
    }
    ss << "]\n";
    ss << "}\n";
    return ss.str();
}

bool FirmwarePackage::Open(const std::string& path, FirmwarePackage& out_pkg, std::string& out_err,
                           const std::function<void(const char*)>& progress) {
    if (progress) progress("Read firmware file");
    auto f = PathUtils::OpenReadStream(path, std::ios::binary);
    if (!f.is_open()) {
        out_err = "Cannot open file: " + path;
        return false;
    }
    f.seekg(0, std::ios::end);
    size_t sz = (size_t)f.tellg();
    f.seekg(0, std::ios::beg);

    std::vector<uint8_t> bytes(sz);
    if (sz > 0) {
        f.read((char*)bytes.data(), sz);
    }
    return Parse(bytes, out_pkg, out_err, progress);
}

bool FirmwarePackage::Parse(const std::vector<uint8_t>& bytes, FirmwarePackage& out_pkg, std::string& out_err,
                            const std::function<void(const char*)>& progress) {
    if (bytes.empty()) {
        out_err = "Empty firmware file";
        return false;
    }

    FirmwarePackage pkg;
    pkg.source_bytes = bytes;
    if (progress) progress("Calculate firmware SHA-256");
    pkg.source_sha256 = ComputeSha256(bytes.data(), bytes.size());
    if (progress) progress("Parse firmware container and validate record checksums");
    if (bytes.size() >= 4 && std::memcmp(bytes.data(), "XWDF", 4) == 0) {
        if (!XwdfCodec::Parse(bytes, pkg, out_err)) return false;
        out_pkg = std::move(pkg);
        return true;
    }
    if (bytes.size() >= 10 && ReadLeU32(bytes.data()) >= 0x20000000 &&
        ReadLeU32(bytes.data()) < 0x20010000) {
        HardwareRecord rec;
        rec.sp = ReadLeU32(bytes.data());
        rec.reset = ReadLeU32(bytes.data() + 4);
        if (!(rec.reset & 1) || rec.reset < 0x08002c00 || rec.reset >= 0x08020000) {
            out_err = "Invalid SWDF reset vector";
            return false;
        }
        rec.linked_base = rec.reset >= 0x08005800 ? 0x08005800 :
            rec.reset >= 0x08003000 ? 0x08003000 : 0x08002c00;
        rec.image.assign(bytes.begin(), bytes.end() - 2);
        rec.device_code[0] = bytes[bytes.size() - 2];
        rec.device_code[1] = bytes.back();
        pkg.format = FirmwareFormat::Swdf;
        pkg.records.push_back(std::move(rec));
        out_pkg = std::move(pkg);
        return true;
    }

    size_t cursor = 0;
    size_t record_index = 0;

    const uint8_t expected_marker[10] = { 0x00, 0x2D, 0x00, 0x2D, 0x00, 0x2D, 0x00, 0x2D, 0x00, 0x2D };

    while (cursor < bytes.size()) {
        if (cursor + 4 > bytes.size()) {
            out_err = "Truncated payload length at record " + std::to_string(record_index);
            return false;
        }
        uint32_t payload_len = ReadBeU32(&bytes[cursor]);
        cursor += 4;
        if (payload_len < 10) {
            out_err = "Record " + std::to_string(record_index) + " payload is too short (< 10 bytes)";
            return false;
        }
        if (cursor + payload_len > bytes.size()) {
            out_err = "Truncated payload at record " + std::to_string(record_index);
            return false;
        }
        const uint8_t* payload = &bytes[cursor];
        cursor += payload_len;

        if (cursor + 6 > bytes.size()) {
            out_err = "Truncated CRC/checksum header at record " + std::to_string(record_index);
            return false;
        }
        uint16_t declared_crc = ReadBeU16(&bytes[cursor]);
        cursor += 2;
        uint32_t declared_sum = ReadBeU32(&bytes[cursor]);
        cursor += 4;

        const uint8_t* marker_ptr = bytes.data() + cursor;
        size_t marker_len = 0;
        static constexpr uint8_t startEnd[] = {
            0, 0, 0, 10, 0, 's', 0, 't', 0, 'a', 0, 'r', 0, 't',
            0, 0, 0, 6, 0, 'e', 0, 'n', 0, 'd'
        };
        if (record_index == 0 && bytes.size() - cursor >= sizeof(startEnd) &&
            std::memcmp(bytes.data() + cursor, startEnd, sizeof(startEnd)) == 0) {
            pkg.format = FirmwareFormat::ZwdfStartEnd;
            cursor += sizeof(startEnd);
        } else if (pkg.format == FirmwareFormat::ZwdfStartEnd) {
            if (record_index != 1 || cursor != bytes.size()) {
                out_err = "Invalid start/end ZWDF record boundary";
                return false;
            }
        } else {
            if (bytes.size() - cursor < 4) {
                out_err = "Truncated marker length";
                return false;
            }
            marker_len = ReadBeU32(bytes.data() + cursor);
            cursor += 4;
            if (marker_len != sizeof(expected_marker) || bytes.size() - cursor < marker_len ||
                std::memcmp(bytes.data() + cursor, expected_marker, marker_len) != 0) {
                out_err = "Unexpected record marker at record " + std::to_string(record_index);
                return false;
            }
            marker_ptr = bytes.data() + cursor;
            cursor += marker_len;
        }

        // Verify CRC16 (limited to payload_len & 0xFFFF)
        uint16_t calc_crc = Crc16Ccitt(payload, (size_t)(payload_len & 0xFFFF));
        if (calc_crc != declared_crc) {
            out_err = "CRC16 mismatch in record " + std::to_string(record_index);
            return false;
        }

        // Verify Additive Checksum
        uint32_t calc_sum = AdditiveChecksum(payload, payload_len);
        if (calc_sum != declared_sum) {
            out_err = "Additive checksum mismatch in record " + std::to_string(record_index);
            return false;
        }

        HardwareRecord rec;
        size_t image_len = payload_len - 2;
        rec.image.assign(payload, payload + image_len);
        rec.device_code[0] = payload[image_len];
        rec.device_code[1] = payload[image_len + 1];
        rec.marker.assign(marker_ptr, marker_ptr + marker_len);

        if (rec.image.size() < 8) {
            out_err = "Record image too small for vector table in record " + std::to_string(record_index);
            return false;
        }
        rec.sp = ReadLeU32(&rec.image[0]);
        rec.reset = ReadLeU32(&rec.image[4]);

        if (rec.sp < 0x20000000 || rec.sp >= 0x20010000) {
            out_err = "Invalid Cortex-M stack pointer in record " + std::to_string(record_index);
            return false;
        }
        if ((rec.reset & 1) != 1) {
            out_err = "Reset vector is not Thumb in record " + std::to_string(record_index);
            return false;
        }

        if (rec.reset >= 0x08005800 && rec.reset < 0x08020000) {
            rec.linked_base = 0x08005800;
        } else if (rec.reset >= 0x08003000 && rec.reset < 0x08005800) {
            rec.linked_base = 0x08003000;
        } else {
            out_err = "Unsupported reset vector 0x" + std::to_string(rec.reset) + " in record " + std::to_string(record_index);
            return false;
        }

        pkg.records.push_back(rec);
        record_index++;
    }
    if (pkg.format == FirmwareFormat::ZwdfStartEnd && pkg.records.size() != 2) {
        out_err = "Start/end ZWDF requires two records";
        return false;
    }

    if (pkg.records.empty()) {
        out_err = "No records found in ZWDF package";
        return false;
    }

    out_pkg = std::move(pkg);
    return true;
}

const uint8_t* FirmwarePackage::BytesAt(size_t record_idx, uint32_t address, size_t len) const {
    if (record_idx >= records.size()) return nullptr;
    const auto& rec = records[record_idx];
    if (address < rec.linked_base) return nullptr;
    size_t offset = (size_t)(address - rec.linked_base);
    if (offset > rec.image.size() || len > rec.image.size() - offset) return nullptr;
    return rec.image.data() + offset;
}

bool FirmwarePackage::FindUnique(size_t record_idx, const uint8_t* needle, size_t needle_len, size_t& out_offset) const {
    if (record_idx >= records.size() || needle_len == 0) return false;
    const auto& img = records[record_idx].image;
    if (needle_len > img.size()) return false;

    size_t match_count = 0;
    size_t found_idx = 0;

    for (size_t i = 0; i <= img.size() - needle_len; ++i) {
        if (std::memcmp(&img[i], needle, needle_len) == 0) {
            if (match_count == 0) {
                found_idx = i;
            }
            match_count++;
            if (match_count > 1) return false; // Ambiguous
        }
    }
    if (match_count == 1) {
        out_offset = found_idx;
        return true;
    }
    return false;
}

std::vector<size_t> FirmwarePackage::FindAll(size_t record_idx, const uint8_t* needle, size_t needle_len) const {
    std::vector<size_t> res;
    if (record_idx >= records.size() || needle_len == 0) return res;
    const auto& img = records[record_idx].image;
    if (needle_len > img.size()) return res;

    for (size_t i = 0; i <= img.size() - needle_len; ++i) {
        if (std::memcmp(&img[i], needle, needle_len) == 0) {
            res.push_back(i);
        }
    }
    return res;
}

bool FirmwarePackage::PackWith(
    const std::map<std::string, std::pair<std::vector<size_t>, std::vector<uint8_t>>>& replacements,
    const std::vector<CodePatch>& code_patches,
    std::vector<uint8_t>& out_bytes,
    BuildReport& out_report,
    std::string& out_err,
    const std::function<void(const char*)>& progress
) const {
    if (progress) progress("Apply resource, palette and code patches");
    std::vector<HardwareRecord> mod_records = records;
    std::vector<std::vector<std::pair<size_t, size_t>>> touched(mod_records.size());
    size_t patched_ranges = 0;
    for (const auto& patch : code_patches) {
        if (patch.original.size() != patch.replacement.size()) {
            out_err = patch.id + ": code patches must preserve instruction length";
            return false;
        }
    }
    for (const auto& replacement : replacements) {
        const auto& locations = replacement.second.first;
        if (locations.size() != records.size()) {
            out_err = replacement.first + ": resource location map must cover every hardware record";
            return false;
        }
        if (std::all_of(locations.begin(), locations.end(), [](size_t offset) { return offset == SIZE_MAX; })) {
            out_err = replacement.first + ": resource is absent from every hardware record";
            return false;
        }
    }

    for (size_t r_idx = 0; r_idx < mod_records.size(); ++r_idx) {
        auto& rec = mod_records[r_idx];

        // Apply bitmap replacements
        for (const auto& kv : replacements) {
            const std::string& id = kv.first;
            const auto& locs = kv.second.first;
            const auto& rep_bytes = kv.second.second;

            size_t offset = locs[r_idx];
            if (offset == SIZE_MAX) continue; // Resource is absent from this hardware layout.
            if (offset > rec.image.size() || rep_bytes.size() > rec.image.size() - offset) {
                out_err = id + ": replacement is out of bounds in record " + std::to_string(r_idx);
                return false;
            }
            if (!rep_bytes.empty())
                std::memcpy(rec.image.data() + offset, rep_bytes.data(), rep_bytes.size());
            touched[r_idx].push_back({ offset, offset + rep_bytes.size() });
            patched_ranges++;
        }

        // Apply code patches
        for (const auto& patch : code_patches) {
            if (r_idx >= patch.locations.size()) {
                out_err = patch.id + ": missing location map for record " + std::to_string(r_idx);
                return false;
            }
            for (size_t offset : patch.locations[r_idx]) {
                if (offset > rec.image.size() || patch.original.size() > rec.image.size() - offset) {
                    out_err = patch.id + ": patch is outside firmware image in record " + std::to_string(r_idx);
                    return false;
                }
                if (!patch.original.empty() &&
                    std::memcmp(rec.image.data() + offset, patch.original.data(), patch.original.size()) != 0) {
                    out_err = patch.id + ": source instruction mismatch in record " + std::to_string(r_idx);
                    return false;
                }
                if (!patch.replacement.empty())
                    std::memcpy(rec.image.data() + offset, patch.replacement.data(), patch.replacement.size());
                touched[r_idx].push_back({ offset, offset + patch.replacement.size() });
                patched_ranges++;
            }
        }
    }

    // Assemble new binary
    if (progress) progress("Pack firmware container and calculate checksums");
    std::vector<uint8_t> output;
    output.reserve(source_bytes.size());

    if (format == FirmwareFormat::Xwdf) {
        if (mod_records.size() != 1) {
            out_err = "XWDF requires exactly one ROM image";
            return false;
        }
        if (!XwdfCodec::Repack(source_bytes, mod_records[0].image, output, out_err)) return false;
    } else if (format == FirmwareFormat::Swdf) {
        if (mod_records.size() != 1) {
            out_err = "SWDF requires exactly one image";
            return false;
        }
        const auto& rec = mod_records.front();
        output.insert(output.end(), rec.image.begin(), rec.image.end());
        output.push_back(rec.device_code[0]);
        output.push_back(rec.device_code[1]);
    } else {
        for (size_t recordIndex = 0; recordIndex < mod_records.size(); ++recordIndex) {
            const auto& rec = mod_records[recordIndex];
            std::vector<uint8_t> payload = rec.image;
            payload.push_back(rec.device_code[0]);
            payload.push_back(rec.device_code[1]);

            uint32_t payload_len = (uint32_t)payload.size();
            WriteBeU32(output, payload_len);
            output.insert(output.end(), payload.begin(), payload.end());

            uint16_t crc = Crc16Ccitt(payload.data(), (size_t)(payload_len & 0xFFFF));
            WriteBeU16(output, crc);

            uint32_t sum = AdditiveChecksum(payload.data(), payload.size());
            WriteBeU32(output, sum);

            if (format == FirmwareFormat::ZwdfStartEnd) {
                if (recordIndex == 0) {
                    static constexpr uint8_t boundary[] = {
                        0, 0, 0, 10, 0, 's', 0, 't', 0, 'a', 0, 'r', 0, 't',
                        0, 0, 0, 6, 0, 'e', 0, 'n', 0, 'd'
                    };
                    output.insert(output.end(), std::begin(boundary), std::end(boundary));
                }
            } else {
                WriteBeU32(output, (uint32_t)rec.marker.size());
                output.insert(output.end(), rec.marker.begin(), rec.marker.end());
            }
        }
    }

    // Editor records can be canonicalized from a reversible native extension,
    // and the logo builder can append its validated ROM envelope. Source bytes
    // remain the original import, so ZWDF size follows the current record set.
    size_t expectedSize = source_bytes.size();
    if (format == FirmwareFormat::Zwdf) {
        expectedSize = 0;
        for (const auto& record : records) expectedSize += record.image.size() + record.marker.size() + 16;
    }
    if (output.size() != expectedSize) {
        out_err = "Repacked firmware size does not match its hardware records";
        return false;
    }

    // Strict self-verification
    if (progress) progress("Validate repacked firmware");
    FirmwarePackage reparsed;
    if (!FirmwarePackage::Parse(output, reparsed, out_err, progress)) {
        out_err = "Self-verification parse failed: " + out_err;
        return false;
    }

    if (reparsed.format != format) {
        out_err = "Container format changed in self-verification";
        return false;
    }
    if (reparsed.records.size() != records.size()) {
        out_err = "Record count changed in self-verification";
        return false;
    }

    if (progress) progress("Verify patched images and unchanged firmware bytes");
    size_t unchanged_verified = 0;
    for (size_t r = 0; r < records.size(); ++r) {
        const auto& old_rec = records[r];
        const auto& new_rec = reparsed.records[r];

        if (old_rec.device_code[0] != new_rec.device_code[0] ||
            old_rec.device_code[1] != new_rec.device_code[1] ||
            old_rec.marker != new_rec.marker ||
            old_rec.image.size() != new_rec.image.size()) {
            out_err = "Record metadata mismatch in record " + std::to_string(r);
            return false;
        }
        if (new_rec.image != mod_records[r].image) {
            out_err = "Patched image mismatch in record " + std::to_string(r);
            return false;
        }

        std::vector<uint8_t> touched_mask(old_rec.image.size(), 0);
        for (const auto& range : touched[r]) {
            size_t start = (std::min)(range.first, old_rec.image.size());
            size_t end = (std::min)(range.second, old_rec.image.size());
            if (end > start) {
                std::fill(touched_mask.begin() + start, touched_mask.begin() + end, 1);
            }
        }

        for (size_t pos = 0; pos < old_rec.image.size(); ++pos) {
            if (!touched_mask[pos]) {
                if (old_rec.image[pos] != new_rec.image[pos]) {
                    out_err = "Unexpected byte modification in record " + std::to_string(r) + " at offset 0x" + std::to_string(pos);
                    return false;
                }
                unchanged_verified++;
            }
        }
    }

    out_bytes = std::move(output);

    size_t changed_colors_count = 0;
    std::set<std::string_view> changed_layouts;
    for (const auto& p : code_patches) {
        if (p.id.rfind("palette_", 0) == 0) changed_colors_count++;
        if (p.id.starts_with("layout:")) changed_layouts.insert(p.id);
    }
    bool has_bg = false;
    for (const auto& p : code_patches) {
        if (p.id.rfind("background_", 0) == 0) { has_bg = true; break; }
    }
    if (has_bg) changed_colors_count++;

    if (progress) progress("Calculate output firmware SHA-256 and build report");
    out_report.source_sha256 = source_sha256;
    out_report.output_sha256 = ComputeSha256(out_bytes.data(), out_bytes.size());
    out_report.output_size = out_bytes.size();
    out_report.records = mod_records.size();
    out_report.changed_assets = replacements.size();
    out_report.changed_colors = changed_colors_count;
    out_report.changed_layout_elements = changed_layouts.size();
    out_report.patched_ranges = patched_ranges;
    out_report.unchanged_payload_bytes_verified = unchanged_verified;

    out_report.hardware_codes.clear();
    for (const auto& r : mod_records) {
        if (format == FirmwareFormat::Xwdf) {
            const auto end = std::find(r.marker.begin(), r.marker.end(), uint8_t(0));
            out_report.hardware_codes.emplace_back(r.marker.begin(), end);
            continue;
        }
        char code_buf[10];
        snprintf(code_buf, sizeof(code_buf), "%02X%02X", r.device_code[0], r.device_code[1]);
        out_report.hardware_codes.push_back(code_buf);
    }

    return true;
}

namespace {
constexpr size_t kMaxColorVariantCount =
    sizeof(kFactoryDragXColorHardware) / sizeof(kFactoryDragXColorHardware[0]);
using ColorVariants = std::array<size_t, kMaxColorVariantCount>;

struct ColorProfile {
    const ColorHardwareIdentity* hardware;
    const ColorRoleSites* roles;
    size_t role_count;
    FirmwareModel model;
    FirmwareEdition edition;
    size_t hardware_count;
    const FactoryColorHardware* factory = nullptr;
};

bool ResolveColorProfile(const FirmwarePackage& fw, ColorProfile& profile,
                         ColorVariants& variants, std::string& error) {
    FirmwareModel model;
    FirmwareEdition edition;
    if (!DetectFirmwareProfile(fw, model, edition) || model == FirmwareModel::Drag2) {
        error = "Unsupported firmware hardware identities for color patches";
        return false;
    }
    if (edition == FirmwareEdition::Factory) {
        profile = { nullptr, nullptr, sizeof(kFactoryColorRoleIds) / sizeof(kFactoryColorRoleIds[0]),
                    model, edition, model == FirmwareModel::DragX ?
                    sizeof(kFactoryDragXColorHardware) / sizeof(kFactoryDragXColorHardware[0]) :
                    sizeof(kFactoryDragSColorHardware) / sizeof(kFactoryDragSColorHardware[0]),
                    model == FirmwareModel::DragX ? kFactoryDragXColorHardware : kFactoryDragSColorHardware };
    } else if (model == FirmwareModel::DragX) {
        profile = { kDragXColorHardware, kDragXColorRoles,
                    sizeof(kDragXColorRoles) / sizeof(kDragXColorRoles[0]), model, edition,
                    sizeof(kDragXColorHardware) / sizeof(kDragXColorHardware[0]) };
    } else {
        profile = { kDragSColorHardware, kDragSColorRoles,
                    sizeof(kDragSColorRoles) / sizeof(kDragSColorRoles[0]), model, edition,
                    sizeof(kDragSColorHardware) / sizeof(kDragSColorHardware[0]) };
    }
    if (fw.records.size() != profile.hardware_count || fw.records.size() > variants.size()) {
        error = "Unsupported firmware record count for color patches";
        return false;
    }
    std::array<bool, kMaxColorVariantCount> seen{};
    for (size_t r = 0; r < fw.records.size(); ++r) {
        const auto& rec = fw.records[r];
        const uint16_t code = (uint16_t)((rec.device_code[0] << 8) | rec.device_code[1]);
        size_t variant = 0;
        while (variant < profile.hardware_count) {
            const auto& identity = profile.factory ? profile.factory[variant].identity : profile.hardware[variant];
            if (identity.code == code) break;
            ++variant;
        }
        if (variant == profile.hardware_count || seen[variant]) {
            error = "Unsupported or duplicate color hardware identity in record " + std::to_string(r);
            return false;
        }
        const auto& identity = profile.factory ? profile.factory[variant].identity : profile.hardware[variant];
        if (rec.image.size() != identity.image_size || rec.linked_base != identity.linked_base) {
            error = "Unsupported color layout in hardware record " + std::to_string(r);
            return false;
        }
        seen[variant] = true;
        variants[r] = variant;
    }
    return true;
}

size_t ColorSiteLength(ColorEncoding encoding) {
    return encoding == ColorEncoding::Rgb ? 6 : encoding == ColorEncoding::SplitRgb ? 8 : 4;
}

bool ReadColorSite(const HardwareRecord& rec, const ColorPatchSite& site,
                   ColorEncoding encoding, Rgb8& color) {
    const size_t length = ColorSiteLength(encoding);
    if (site.offset < 2 || site.offset + length + 2 > rec.image.size()) return false;
    const auto* code = rec.image.data() + site.offset;
    const uint16_t before = (uint16_t)(code[-2] | (code[-1] << 8));
    const uint16_t after = (uint16_t)(code[length] | (code[length + 1] << 8));
    if ((before & site.before_mask) != site.before || (after & site.after_mask) != site.after)
        return false;
    if (encoding == ColorEncoding::Rgb) return DecodeColorInstruction(code, color);
    if (encoding == ColorEncoding::SplitRgb) {
        if (code[1] != 0x22 || code[2] != 0x1c || code[3] != 0xbf ||
            code[5] != 0x21 || code[7] != 0x20) return false;
        color = { code[6], code[4], code[0] };
        return true;
    }
    uint16_t rgb565 = 0;
    if (!DecodeMovwInstruction(code, encoding == ColorEncoding::Movw8 ? 8 : 7, rgb565))
        return false;
    color = Rgb8::FromRgb565(rgb565);
    return true;
}

std::vector<uint8_t> EncodeColorSite(ColorEncoding encoding, Rgb8 color) {
    if (encoding == ColorEncoding::Rgb) return ColorInstruction(color);
    if (encoding == ColorEncoding::SplitRgb)
        return { color.b, 0x22, 0x1c, 0xbf, color.g, 0x21, color.r, 0x20 };
    return MovwInstruction(encoding == ColorEncoding::Movw8 ? 8 : 7, color.ToRgb565());
}

bool ReadPaletteColor(const FirmwarePackage& fw, const ColorProfile& profile,
                      const ColorVariants& variants,
                      const char* id, Rgb8& color, bool& found, std::string& error) {
    found = false;
    for (size_t i = 0; i < profile.role_count; ++i) {
        const auto& role = profile.roles[i];
        if (std::strcmp(id, role.id) != 0) continue;
        for (size_t r = 0; r < fw.records.size(); ++r) {
            for (const auto& site : role.sites[variants[r]]) {
                if (!site.offset) break;
                Rgb8 current;
                if (!ReadColorSite(fw.records[r], site, role.encoding, current)) {
                    error = std::string(id) + ": instruction/context mismatch in record " + std::to_string(r);
                    return false;
                }
                if (found && current != color) {
                    error = std::string(id) + ": inconsistent colors across hardware sites";
                    return false;
                }
                color = current;
                found = true;
            }
        }
    }
    return true;
}

// Only DRAG S could have received the historical, broken background injection.
// Keep its recovery, but select each site by hardware code, never record order.
bool AppendLegacyBackgroundRepairs(const FirmwarePackage& fw,
                                  const ColorVariants& variants,
                                  std::vector<CodePatch>& patches, std::string& error) {
    size_t primary = 0;
    while (primary < fw.records.size() && variants[primary] != 0) ++primary;
    const uint8_t digit_prefix[] = { 0xe8, 0x07, 0x44, 0xbf, 0x0e, 0x98 };
    if (primary == fw.records.size() ||
        std::memcmp(fw.records[primary].image.data() + 0x4384, digit_prefix, sizeof(digit_prefix)) != 0)
        return true;
    struct Repair {
        const char* id;
        uint8_t stock[10];
        size_t sites[kScoreColorVariantCount][2];
    };
    static constexpr Repair repairs[] = {
        { "revert_background_digit_stack",
          { 0x15, 0xf0, 0x01, 0x0f, 0x14, 0xbf, 0x0e, 0x98, 0x58, 0x46 },
          { {0x4384}, {}, {}, {}, {}, {0x436c}, {0x436c} } },
        { "revert_background_bitmap_stack",
          { 0x14, 0xf0, 0x01, 0x0f, 0x14, 0xbf, 0x10, 0x98, 0x58, 0x46 },
          { {0x4776}, {}, {}, {}, {}, {0x475e}, {0x475e} } },
        { "revert_background_register",
          { 0x15, 0xf0, 0x01, 0x0f, 0x14, 0xbf, 0x58, 0x46, 0x50, 0x46 },
          { {}, {0x42e4, 0x46fc}, {0x3cbc, 0x40d4}, {0x42e4, 0x46fc},
            {0x3cbc, 0x40d4}, {}, {} } }
    };
    for (const auto& repair : repairs) {
        CodePatch patch;
        patch.id = repair.id;
        patch.replacement.assign(repair.stock, repair.stock + sizeof(repair.stock));
        patch.locations.resize(fw.records.size());
        for (size_t r = 0; r < fw.records.size(); ++r) {
            for (size_t offset : repair.sites[variants[r]]) {
                if (!offset) break;
                const auto* code = fw.records[r].image.data() + offset;
                if (patch.original.empty()) patch.original.assign(code, code + sizeof(repair.stock));
                else if (std::memcmp(code, patch.original.data(), patch.original.size()) != 0) {
                    error = patch.id + ": inconsistent legacy patch in record " + std::to_string(r);
                    return false;
                }
                patch.locations[r].push_back(offset);
            }
        }
        patches.push_back(std::move(patch));
    }
    return true;
}

constexpr size_t kFactoryColorRoleCount = sizeof(kFactoryColorRoleIds) / sizeof(kFactoryColorRoleIds[0]);

size_t FactoryColorSiteLength(FactoryColorEncoding encoding) {
    return encoding == FactoryColorEncoding::Rgb ? 6 :
           encoding == FactoryColorEncoding::SplitRgb ? 8 :
           encoding == FactoryColorEncoding::Literal565 ? 2 : 4;
}

void FactoryStockBytes(const FactoryColorSite& site, std::array<uint8_t, 8>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = (uint8_t)(site.stock >> (i * 8));
}

Rgb8 FactoryColorBaseline(size_t role) {
    return role == 5 ? Rgb8(0x76, 0xff, 0) : Rgb8::FromRgb565(kFactoryColorBaselines[role]);
}

Rgb8 FactoryComparableColor(size_t role, Rgb8 color) {
    // Only the independent legacy service green is exclusively RGB888.
    return role == 5 ? color : color.ScreenColor();
}

bool MatchesFactoryContext(const HardwareRecord& rec, size_t offset, uint32_t expected, uint32_t mask) {
    const auto* source = rec.image.data() + offset;
    if ((ReadLeU32(source) & mask) == expected) return true;
    std::array<uint8_t, 4> normalized;
    std::copy_n(source, normalized.size(), normalized.data());
    return FirmwareLayout::NormalizeSxLayoutContext(rec, rec.linked_base + static_cast<uint32_t>(offset),
        normalized.data(), normalized.size()) && (ReadLeU32(normalized.data()) & mask) == expected;
}

bool ReadFactoryColorSite(const HardwareRecord& rec, const FactoryColorSite& site,
                          Rgb8& color, bool& stock) {
    const size_t length = FactoryColorSiteLength(site.encoding);
    if (site.offset < 4 || site.offset + length + 4 > rec.image.size()) return false;
    const auto* code = rec.image.data() + site.offset;
    if (!MatchesFactoryContext(rec, site.offset - 4, site.before, site.before_mask) ||
        !MatchesFactoryContext(rec, site.offset + length, site.after, site.after_mask)) return false;
    if (site.reference_offset &&
        (site.reference_offset + 4 > rec.image.size() ||
         !MatchesFactoryContext(rec, site.reference_offset, site.reference, UINT32_MAX))) return false;
    std::array<uint8_t, 8> original{};
    FactoryStockBytes(site, original);
    stock = std::memcmp(code, original.data(), length) == 0;
    if (site.encoding == FactoryColorEncoding::Rgb) return DecodeColorInstruction(code, color);
    if (site.encoding == FactoryColorEncoding::SplitRgb) {
        if (code[1] != 0x22 || code[2] != 0x1c || code[3] != 0xbf ||
            code[5] != 0x21 || code[7] != 0x20) return false;
        color = { code[6], code[4], code[0] };
        return true;
    }
    uint16_t value;
    if (site.encoding == FactoryColorEncoding::Literal565) {
        value = (uint16_t)(code[0] | (code[1] << 8));
    } else if ((code[3] & 0x80) != 0 || !DecodeMovwInstruction(code, site.reg, value)) {
        // The sole modified-immediate encoding in these profiles is MOV.W
        // #0xe800. Accept its exact stock bytes, or a same-register MOVW edit.
        if (!stock || site.role != 0) return false;
        value = 0xe800;
    }
    color = Rgb8::FromRgb565(value);
    return true;
}

bool ReadFactoryPaletteColor(const FirmwarePackage& fw, const ColorProfile& profile,
                             const ColorVariants& variants, size_t role, Rgb8& color,
                             std::string& error) {
    bool found = false, all_stock = true, uniform = true;
    Rgb8 first, firstRgb;
    bool foundRgb = false, uniformRgb = true;
    for (size_t r = 0; r < fw.records.size(); ++r) {
        const auto& hardware = profile.factory[variants[r]];
        for (size_t i = 0; i < hardware.site_count; ++i) {
            const auto& site = hardware.sites[i];
            if (site.role != role) continue;
            Rgb8 current;
            bool stock;
            if (!ReadFactoryColorSite(fw.records[r], site, current, stock)) {
                error = std::string(kFactoryColorRoleIds[role]) +
                        ": Factory instruction/context mismatch in record " + std::to_string(r);
                return false;
            }
            if (site.encoding == FactoryColorEncoding::Rgb || site.encoding == FactoryColorEncoding::SplitRgb) {
                if (foundRgb && current != firstRgb) uniformRgb = false;
                if (!foundRgb) firstRgb = current;
                foundRgb = true;
            }
            current = FactoryComparableColor(role, current);
            if (found && current != first) uniform = false;
            if (!found) first = current;
            found = true;
            all_stock = all_stock && stock;
        }
    }
    // Factory stock intentionally has hardware-specific RGB888 / RGB565
    // values. Preserve that complete stock pattern; otherwise every site must
    // agree on the user-selected display color, including on reopening.
    if (!found || (!all_stock && !uniform)) {
        error = std::string(kFactoryColorRoleIds[role]) + ": inconsistent Factory colors across hardware sites";
        return false;
    }
    // RGB565 establishes display equivalence across mixed encodings, but it
    // cannot reconstruct the original RGB888 operands. Keep their exact value
    // when unambiguous so native executable guards and re-export remain strict.
    color = all_stock ? FactoryColorBaseline(role) : foundRgb && uniformRgb ? firstRgb : first;
    return true;
}

bool ReadFactoryPalette(const FirmwarePackage& fw, const ColorProfile& profile,
                        const ColorVariants& variants,
                        std::array<Rgb8, kFactoryColorRoleCount>& colors, std::string& error) {
    for (size_t role = 0; role < colors.size(); ++role) {
        if (!ReadFactoryPaletteColor(fw, profile, variants, role, colors[role], error)) return false;
    }
    return true;
}

void EncodeFactoryColorSite(const FactoryColorSite& site, Rgb8 color, bool restore,
                            std::array<uint8_t, 8>& bytes) {
    if (restore) {
        FactoryStockBytes(site, bytes);
        return;
    }
    if (site.encoding == FactoryColorEncoding::Rgb) {
        bytes = { color.b, 0x22, color.g, 0x21, color.r, 0x20 };
    } else if (site.encoding == FactoryColorEncoding::SplitRgb) {
        bytes = { color.b, 0x22, 0x1c, 0xbf, color.g, 0x21, color.r, 0x20 };
    } else {
        const uint16_t value = color.ToRgb565();
        if (site.encoding == FactoryColorEncoding::Literal565) {
            bytes[0] = (uint8_t)value;
            bytes[1] = (uint8_t)(value >> 8);
        } else {
            const uint16_t first = (uint16_t)(0xf240 | ((value >> 11 & 1) << 10) | (value >> 12));
            const uint16_t second = (uint16_t)(((value >> 8 & 7) << 12) | (site.reg << 8) | (value & 0xff));
            bytes[0] = (uint8_t)first;
            bytes[1] = (uint8_t)(first >> 8);
            bytes[2] = (uint8_t)second;
            bytes[3] = (uint8_t)(second >> 8);
        }
    }
}

bool BuildFactoryColorPatches(const FirmwarePackage& fw, const ColorProfile& profile,
                              const ColorVariants& variants, const std::vector<PaletteState>& palette,
                              std::vector<CodePatch>& out_patches, std::string& error) {
    std::array<Rgb8, kFactoryColorRoleCount> colors{};
    if (!ReadFactoryPalette(fw, profile, variants, colors, error)) return false;
    std::vector<CodePatch> patches;
    for (const auto& entry : palette) {
        size_t role = 0;
        while (role < colors.size() && std::strcmp(entry.spec.id, kFactoryColorRoleIds[role]) != 0) ++role;
        if (role == colors.size()) {
            if (entry.current != entry.spec.original) {
                error = std::string("Unsupported Factory palette role: ") + entry.spec.id;
                return false;
            }
            continue;
        }
        if (FactoryComparableColor(role, entry.current) == FactoryComparableColor(role, colors[role])) continue;
        const bool restore = FactoryComparableColor(role, entry.current) == FactoryColorBaseline(role);
        const size_t role_begin = patches.size();
        for (size_t r = 0; r < fw.records.size(); ++r) {
            const auto& hardware = profile.factory[variants[r]];
            for (size_t i = 0; i < hardware.site_count; ++i) {
                const auto& site = hardware.sites[i];
                if (site.role != role) continue;
                const size_t length = FactoryColorSiteLength(site.encoding);
                const auto* original = fw.records[r].image.data() + site.offset;
                std::array<uint8_t, 8> replacement{};
                EncodeFactoryColorSite(site, entry.current, restore, replacement);
                if (std::memcmp(original, replacement.data(), length) == 0) continue;
                // Group identical instructions across hardware rather than
                // allocating a patch and record-location vector for every site.
                size_t group = role_begin;
                while (group < patches.size() &&
                       (patches[group].original.size() != length ||
                        std::memcmp(patches[group].original.data(), original, length) != 0 ||
                        std::memcmp(patches[group].replacement.data(), replacement.data(), length) != 0)) ++group;
                if (group == patches.size()) {
                    CodePatch patch;
                    patch.id = std::string(group == role_begin ? "palette_" : "patch_") +
                               kFactoryColorRoleIds[role];
                    patch.original.assign(original, original + length);
                    patch.replacement.assign(replacement.begin(), replacement.begin() + length);
                    patch.locations.resize(fw.records.size());
                    patches.push_back(std::move(patch));
                }
                patches[group].locations[r].push_back(site.offset);
            }
        }
    }
    out_patches = std::move(patches);
    return true;
}
} // namespace

bool MatchesSxPaletteCode(const FirmwarePackage& stock, const HardwareRecord& candidate,
    uint32_t address, size_t length) {
    const auto record = std::find_if(stock.records.begin(), stock.records.end(), [&](const HardwareRecord& item) {
        return item.device_code[0] == candidate.device_code[0] && item.device_code[1] == candidate.device_code[1] &&
            item.linked_base == candidate.linked_base;
    });
    if (record == stock.records.end() || address < record->linked_base) return false;
    const size_t begin = address - record->linked_base;
    if (begin > record->image.size() || length > record->image.size() - begin ||
        begin > candidate.image.size() || length > candidate.image.size() - begin) return false;
    if (std::equal(record->image.begin() + begin, record->image.begin() + begin + length,
        candidate.image.begin() + begin)) return true;
    ColorProfile profile{};
    ColorVariants variants{};
    std::string error;
    if (!ResolveColorProfile(stock, profile, variants, error)) return false;
    const size_t variant = variants[size_t(record - stock.records.begin())];
    const size_t end = begin + length;
    for (size_t at = begin; at < end;) {
        if (record->image[at] == candidate.image[at]) { ++at; continue; }
        size_t next = at;
        Rgb8 color;
        if (profile.factory) {
            const auto& hardware = profile.factory[variant];
            for (size_t i = 0; i < hardware.site_count; ++i) {
                const auto& site = hardware.sites[i];
                const size_t bytes = FactoryColorSiteLength(site.encoding);
                if (site.offset < begin || site.offset > at || at - site.offset >= bytes ||
                    bytes > end - site.offset) continue;
                bool original = false;
                if (ReadFactoryColorSite(candidate, site, color, original)) next = site.offset + bytes;
                break;
            }
        } else {
            for (size_t i = 0; i < profile.role_count && next == at; ++i) {
                const auto& role = profile.roles[i];
                for (const auto& site : role.sites[variant]) {
                    if (!site.offset) break;
                    const size_t bytes = ColorSiteLength(role.encoding);
                    if (site.offset < begin || site.offset > at || at - site.offset >= bytes ||
                        bytes > end - site.offset) continue;
                    if (ReadColorSite(candidate, site, role.encoding, color)) next = site.offset + bytes;
                    break;
                }
            }
        }
        if (next == at) return false;
        at = next;
    }
    return true;
}

bool FirmwareRecordPaletteColor(const FirmwarePackage& fw, uint16_t deviceCode,
                                const char* roleId, Rgb8& color) {
    FirmwareModel model;
    if (DetectFirmwareModel(fw, model))
        if (const auto* extension = FirmwareExtensionFor(model))
            return extension->recordColor(fw, deviceCode, roleId, color);
    ColorProfile profile{};
    ColorVariants variants{};
    std::string error;
    if (!ResolveColorProfile(fw, profile, variants, error)) return false;
    for (size_t r = 0; r < fw.records.size(); ++r) {
        const auto& rec = fw.records[r];
        if ((uint16_t)((rec.device_code[0] << 8) | rec.device_code[1]) != deviceCode) continue;
        if (profile.factory) {
            size_t role = 0;
            while (role < profile.role_count && std::strcmp(roleId, kFactoryColorRoleIds[role]) != 0) ++role;
            if (role == profile.role_count) return false;
            const auto& hardware = profile.factory[variants[r]];
            for (size_t i = 0; i < hardware.site_count; ++i) {
                const auto& site = hardware.sites[i];
                if (site.role != role) continue;
                bool stock;
                return ReadFactoryColorSite(rec, site, color, stock);
            }
        } else {
            for (size_t i = 0; i < profile.role_count; ++i) {
                const auto& role = profile.roles[i];
                if (std::strcmp(roleId, role.id) != 0) continue;
                const auto& site = role.sites[variants[r]][0];
                if (site.offset) return ReadColorSite(rec, site, role.encoding, color);
            }
        }
        return false;
    }
    return false;
}

bool DetectFirmwareColors(
    const FirmwarePackage& fw,
    std::vector<PaletteState>& palette,
    Rgb8& background
) {
    FirmwareModel model;
    if (DetectFirmwareModel(fw, model)) {
        if (const auto* extension = FirmwareExtensionFor(model))
            return extension->detectColors(fw, palette, background);
        if (model == FirmwareModel::Drag2) return DetectDrag2FirmwareColors(fw, palette, background);
    }
    ColorProfile profile{};
    ColorVariants variants{};
    std::string error;
    if (!ResolveColorProfile(fw, profile, variants, error)) return false;
    if (profile.factory) {
        std::array<Rgb8, kFactoryColorRoleCount> colors{};
        if (!ReadFactoryPalette(fw, profile, variants, colors, error)) return false;
        static constexpr const char* labels[] = {
            "Red accent", "Cyan accent", "Main digits", "Small digits", "Yellow highlight", "Legacy green"
        };
        static constexpr const char* scopes[] = {
            "Factory mode labels, panels and graphs",
            "Factory alternate mode labels and panels",
            "Large readout, clock and model header",
            "6x10 and 7x13 readouts and units",
            "Factory service panels",
            "Independent green controls on legacy hardware"
        };
        palette.clear();
        palette.reserve(colors.size());
        for (size_t role = 0; role < colors.size(); ++role)
            palette.push_back({{kFactoryColorRoleIds[role], labels[role], scopes[role], colors[role]}, colors[role]});
        background = { 0, 0, 0 };
        return true;
    }
    // Validate every role before changing caller state, including both encodings
    // of the X red accent. A partial or mismatched palette is not a valid profile.
    std::array<Rgb8, sizeof(kDragXColorRoles) / sizeof(kDragXColorRoles[0])> colors{};
    for (size_t i = 0; i < profile.role_count; ++i) {
        size_t previous = 0;
        while (previous < i && std::strcmp(profile.roles[previous].id, profile.roles[i].id) != 0)
            ++previous;
        if (previous < i) {
            colors[i] = colors[previous];
            continue;
        }
        bool found = false;
        if (!ReadPaletteColor(fw, profile, variants, profile.roles[i].id, colors[i], found, error) || !found)
            return false;
    }
    for (auto& entry : palette) {
        for (size_t i = 0; i < profile.role_count; ++i) {
            if (std::strcmp(entry.spec.id, profile.roles[i].id) != 0) continue;
            entry.current = entry.spec.original = colors[i];
            break;
        }
    }
    background = { 0, 0, 0 };
    return true;
}

bool BuildColorPatches(
    const FirmwarePackage& fw,
    const std::vector<PaletteState>& palette,
    Rgb8 background,
    std::vector<CodePatch>& out_patches,
    std::string& out_err
) {
    FirmwareModel model;
    if (DetectFirmwareModel(fw, model)) {
        if (const auto* extension = FirmwareExtensionFor(model))
            return extension->buildColors(fw, palette, background, out_patches, out_err);
        if (model == FirmwareModel::Drag2)
            return BuildDrag2ColorPatches(fw, palette, background, out_patches, out_err);
    }
    ColorProfile profile{};
    ColorVariants variants{};
    if (!ResolveColorProfile(fw, profile, variants, out_err)) return false;
    if (background != Rgb8(0, 0, 0)) {
        out_err = "Only the firmware's black background is supported";
        return false;
    }
    if (profile.factory)
        return BuildFactoryColorPatches(fw, profile, variants, palette, out_patches, out_err);
    std::vector<CodePatch> patches;
    for (const auto& entry : palette) {
        Rgb8 current;
        bool found = false;
        if (!ReadPaletteColor(fw, profile, variants, entry.spec.id, current, found, out_err)) return false;
        if (!found) {
            if (entry.current != entry.spec.original) {
                out_err = std::string("Unsupported firmware palette role: ") + entry.spec.id;
                return false;
            }
            continue;
        }
        const bool digit = std::strcmp(entry.spec.id, "digits_wattage") == 0 ||
                           std::strcmp(entry.spec.id, "digits_stats") == 0;
        if (digit ? current.ToRgb565() == entry.current.ToRgb565() : current == entry.current) continue;
        for (size_t i = 0; i < profile.role_count; ++i) {
            const auto& role = profile.roles[i];
            if (std::strcmp(entry.spec.id, role.id) != 0) continue;
            CodePatch patch;
            // The extra split-red patch is not a second changed palette role.
            patch.id = std::string(role.encoding == ColorEncoding::SplitRgb ? "patch_split_" :
                                   digit ? "patch_" : "palette_") + role.id;
            patch.replacement = EncodeColorSite(role.encoding, entry.current);
            patch.locations.resize(fw.records.size());
            for (size_t r = 0; r < fw.records.size(); ++r) {
                for (const auto& site : role.sites[variants[r]]) {
                    if (!site.offset) break;
                    if (patch.original.empty()) {
                        const auto* code = fw.records[r].image.data() + site.offset;
                        patch.original.assign(code, code + ColorSiteLength(role.encoding));
                    }
                    patch.locations[r].push_back(site.offset);
                }
            }
            patches.push_back(std::move(patch));
        }
    }
    if (profile.model == FirmwareModel::DragS &&
        !AppendLegacyBackgroundRepairs(fw, variants, patches, out_err)) return false;
    out_patches = std::move(patches);
    return true;
}
