#include "zwdf_package.h"
#include "path_utils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

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

bool FirmwarePackage::Open(const std::string& path, FirmwarePackage& out_pkg, std::string& out_err) {
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
    return Parse(bytes, out_pkg, out_err);
}

bool FirmwarePackage::Parse(const std::vector<uint8_t>& bytes, FirmwarePackage& out_pkg, std::string& out_err) {
    if (bytes.empty()) {
        out_err = "Empty firmware file";
        return false;
    }

    FirmwarePackage pkg;
    pkg.source_bytes = bytes;
    pkg.source_sha256 = ComputeSha256(bytes.data(), bytes.size());

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

        if (cursor + 4 > bytes.size()) {
            out_err = "Truncated marker length at record " + std::to_string(record_index);
            return false;
        }
        uint32_t marker_len = ReadBeU32(&bytes[cursor]);
        cursor += 4;
        if (cursor + marker_len > bytes.size()) {
            out_err = "Truncated marker bytes at record " + std::to_string(record_index);
            return false;
        }
        const uint8_t* marker_ptr = &bytes[cursor];
        cursor += marker_len;

        // Verify marker
        if (marker_len != 10 || std::memcmp(marker_ptr, expected_marker, 10) != 0) {
            out_err = "Unexpected record marker at record " + std::to_string(record_index);
            return false;
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
    if (offset + len > rec.image.size()) return nullptr;
    return &rec.image[offset];
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
    std::string& out_err
) const {
    std::vector<HardwareRecord> mod_records = records;
    std::vector<std::vector<std::pair<size_t, size_t>>> touched(mod_records.size());
    size_t patched_ranges = 0;

    for (size_t r_idx = 0; r_idx < mod_records.size(); ++r_idx) {
        auto& rec = mod_records[r_idx];

        // Apply bitmap replacements
        for (const auto& kv : replacements) {
            const std::string& id = kv.first;
            const auto& locs = kv.second.first;
            const auto& rep_bytes = kv.second.second;

            if (r_idx >= locs.size()) {
                out_err = id + ": missing offset location for record " + std::to_string(r_idx);
                return false;
            }
            size_t offset = locs[r_idx];
            if (offset + rep_bytes.size() > rec.image.size()) {
                out_err = id + ": replacement is out of bounds in record " + std::to_string(r_idx);
                return false;
            }
            std::memcpy(&rec.image[offset], rep_bytes.data(), rep_bytes.size());
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
                if (offset + patch.original.size() > rec.image.size()) {
                    out_err = patch.id + ": patch is outside firmware image in record " + std::to_string(r_idx);
                    return false;
                }
                if (std::memcmp(&rec.image[offset], patch.original.data(), patch.original.size()) != 0) {
                    out_err = patch.id + ": source instruction mismatch in record " + std::to_string(r_idx);
                    return false;
                }
                std::memcpy(&rec.image[offset], patch.replacement.data(), patch.replacement.size());
                touched[r_idx].push_back({ offset, offset + patch.replacement.size() });
                patched_ranges++;
            }
        }
    }

    // Assemble new binary
    std::vector<uint8_t> output;
    output.reserve(source_bytes.size());

    for (const auto& rec : mod_records) {
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

        WriteBeU32(output, (uint32_t)rec.marker.size());
        output.insert(output.end(), rec.marker.begin(), rec.marker.end());
    }

    if (output.size() != source_bytes.size()) {
        out_err = "Repacked firmware size changed unexpectedly";
        return false;
    }

    // Strict self-verification
    FirmwarePackage reparsed;
    if (!FirmwarePackage::Parse(output, reparsed, out_err)) {
        out_err = "Self-verification parse failed: " + out_err;
        return false;
    }

    if (reparsed.records.size() != records.size()) {
        out_err = "Record count changed in self-verification";
        return false;
    }

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
    for (const auto& p : code_patches) {
        if (p.id.rfind("palette_", 0) == 0) changed_colors_count++;
    }
    bool has_bg = false;
    for (const auto& p : code_patches) {
        if (p.id.rfind("background_", 0) == 0) { has_bg = true; break; }
    }
    if (has_bg) changed_colors_count++;

    out_report.source_sha256 = source_sha256;
    out_report.output_sha256 = ComputeSha256(out_bytes.data(), out_bytes.size());
    out_report.output_size = out_bytes.size();
    out_report.records = mod_records.size();
    out_report.changed_assets = replacements.size();
    out_report.changed_colors = changed_colors_count;
    out_report.patched_ranges = patched_ranges;
    out_report.unchanged_payload_bytes_verified = unchanged_verified;

    for (const auto& r : mod_records) {
        char code_buf[10];
        snprintf(code_buf, sizeof(code_buf), "%02X%02X", r.device_code[0], r.device_code[1]);
        out_report.hardware_codes.push_back(code_buf);
    }

    return true;
}

struct KnownColorSites {
    const char* id;
    std::vector<size_t> sites[7];
};

static const KnownColorSites kKnownColorRoles[] = {
    {
        "accent_red",
        {
            { 0x26ca, 0x554e, 0x5588, 0x56f0, 0x5bca, 0x5c22, 0x5e1a, 0x6358 },
            { 0x2794, 0x5274, 0x5362, 0x5422, 0x5866, 0x58be, 0x5ab2, 0x600c },
            { 0x2716, 0x4c28, 0x4d16, 0x4dd2, 0x522e, 0x5286, 0x547a, 0x59d4 },
            { 0x2794, 0x526c, 0x535a, 0x541a, 0x585e, 0x58b6, 0x5aaa, 0x6004 },
            { 0x2716, 0x4c20, 0x4d0e, 0x4dca, 0x5226, 0x527e, 0x5472, 0x59cc },
            { 0x26ca, 0x5576, 0x55b0, 0x5718, 0x5bf2, 0x5c4a, 0x5e42, 0x6380 },
            { 0x26ca, 0x5576, 0x55b0, 0x5718, 0x5bf2, 0x5c4a, 0x5e42, 0x6380 }
        }
    },
    {
        "accent_cyan",
        {
            { 0x26c2, 0x5546, 0x5796, 0x5bc2, 0x5d84, 0x5fb6, 0x6350 },
            { 0x278c, 0x5354, 0x54d2, 0x585e, 0x5a1c, 0x5c06, 0x6004 },
            { 0x270e, 0x4d08, 0x4e76, 0x5226, 0x53e4, 0x55ce, 0x59cc },
            { 0x278c, 0x534c, 0x54ca, 0x5856, 0x5a14, 0x5bfe, 0x5ffc },
            { 0x270e, 0x4d00, 0x4e6e, 0x521e, 0x53dc, 0x55c6, 0x59c4 },
            { 0x26c2, 0x556e, 0x57be, 0x5bea, 0x5dac, 0x5fde, 0x6378 },
            { 0x26c2, 0x556e, 0x57be, 0x5bea, 0x5dac, 0x5fde, 0x6378 }
        }
    },
    {
        "highlight_yellow",
        {
            { 0x621c, 0x8780, 0x87c4, 0x88b6 },
            { 0x5eca, 0x77fc, 0x783e, 0x7928 },
            { 0x5892, 0x7220, 0x7262, 0x734c },
            { 0x5ec2, 0x77f4, 0x7836, 0x7920 },
            { 0x588a, 0x7218, 0x725a, 0x7344 },
            { 0x6244, 0x87a8, 0x87ec, 0x88de },
            { 0x6244, 0x87a8, 0x87ec, 0x88de }
        }
    },
    {
        "rank_bronze",
        {
            { 0x5db8 }, { 0x5a50 }, { 0x5418 }, { 0x5a48 }, { 0x5410 }, { 0x5de0 }, { 0x5de0 }
        }
    },
    {
        "rank_gold",
        {
            { 0x5dc6 },
            { 0x34d2, 0x5a5e },
            { 0x5426 },
            { 0x34d2, 0x5a56 },
            { 0x541e },
            { 0x5dee },
            { 0x5dee }
        }
    },
    {
        "rank_ice",
        {
            { 0x5dd4 }, { 0x5a6c }, { 0x5434 }, { 0x5a64 }, { 0x542c }, { 0x5dfc }, { 0x5dfc }
        }
    },
    {
        "rank_cyan",
        {
            { 0x5de2 }, { 0x5a7a }, { 0x5442 }, { 0x5a72 }, { 0x543a }, { 0x5e0a }, { 0x5e0a }
        }
    },
    {
        "rank_orange",
        {
            { 0x5df0 }, { 0x5a88 }, { 0x5450 }, { 0x5a80 }, { 0x5448 }, { 0x5e18 }, { 0x5e18 }
        }
    },
    {
        "rank_red",
        {
            { 0x5dfe }, { 0x5a96 }, { 0x545e }, { 0x5a8e }, { 0x5456 }, { 0x5e26 }, { 0x5e26 }
        }
    }
};

static const size_t kWattageDigitSites[7] = { 0x567c, 0x53b0, 0x4d64, 0x53a8, 0x4d5c, 0x56a4, 0x56a4 };
static const size_t kStatsDigitSites[7]   = { 0x5ca0, 0x5944, 0x530c, 0x593c, 0x5304, 0x5cc8, 0x5cc8 };

bool DetectFirmwareColors(
    const FirmwarePackage& fw,
    std::vector<PaletteState>& palette,
    Rgb8& background
) {
    if (fw.records.size() != 7) return false;

    for (auto& entry : palette) {
        if (std::string(entry.spec.id) == "digits_wattage") {
            size_t off = kWattageDigitSites[0];
            if (off + 4 <= fw.records[0].image.size()) {
                uint16_t imm16 = 0;
                if (DecodeMovwInstruction(&fw.records[0].image[off], 8, imm16)) {
                    Rgb8 c = Rgb8::FromRgb565(imm16);
                    entry.current = c;
                    entry.spec.original = c;
                }
            }
            continue;
        }

        if (std::string(entry.spec.id) == "digits_stats") {
            size_t off = kStatsDigitSites[0];
            if (off + 4 <= fw.records[0].image.size()) {
                uint16_t imm16 = 0;
                if (DecodeMovwInstruction(&fw.records[0].image[off], 7, imm16)) {
                    Rgb8 c = Rgb8::FromRgb565(imm16);
                    entry.current = c;
                    entry.spec.original = c;
                }
            }
            continue;
        }

        for (const auto& role : kKnownColorRoles) {
            if (entry.spec.id == std::string(role.id)) {
                size_t off = role.sites[0][0];
                if (off + 6 <= fw.records[0].image.size()) {
                    Rgb8 c;
                    if (DecodeColorInstruction(&fw.records[0].image[off], c)) {
                        entry.current = c;
                        entry.spec.original = c;
                    }
                }
                break;
            }
        }
    }

    background = Rgb8(0, 0, 0);
    return true;
}

bool BuildColorPatches(
    const FirmwarePackage& fw,
    const std::vector<PaletteState>& palette,
    Rgb8 background,
    std::vector<CodePatch>& out_patches,
    std::string& out_err
) {
    std::vector<CodePatch> patches;

    if (fw.records.size() == 7) {
        for (const auto& entry : palette) {
            if (std::string(entry.spec.id) == "digits_wattage") {
                size_t off0 = kWattageDigitSites[0];
                if (off0 + 4 > fw.records[0].image.size()) {
                    out_err = "Firmware image 0 too small for wattage digit site";
                    return false;
                }
                uint16_t curr_imm16 = 0;
                if (!DecodeMovwInstruction(&fw.records[0].image[off0], 8, curr_imm16)) {
                    out_err = "Cannot decode wattage digit instruction in record 0";
                    return false;
                }
                if (curr_imm16 == entry.current.ToRgb565()) {
                    continue;
                }
                CodePatch p;
                p.id = "patch_digits_wattage";
                p.original.assign(&fw.records[0].image[off0], &fw.records[0].image[off0] + 4);
                p.replacement = MovwInstruction(8, entry.current.ToRgb565());
                for (size_t r = 0; r < 7; ++r) {
                    size_t off = kWattageDigitSites[r];
                    if (off + 4 > fw.records[r].image.size() ||
                        std::memcmp(&fw.records[r].image[off], p.original.data(), 4) != 0) {
                        out_err = "Wattage digit instruction mismatch in record " + std::to_string(r);
                        return false;
                    }
                    p.locations.push_back({ off });
                }
                patches.push_back(p);
                continue;
            }

            if (std::string(entry.spec.id) == "digits_stats") {
                size_t off0 = kStatsDigitSites[0];
                if (off0 + 4 > fw.records[0].image.size()) {
                    out_err = "Firmware image 0 too small for stats digit site";
                    return false;
                }
                uint16_t curr_imm16 = 0;
                if (!DecodeMovwInstruction(&fw.records[0].image[off0], 7, curr_imm16)) {
                    out_err = "Cannot decode stats digit instruction in record 0";
                    return false;
                }
                if (curr_imm16 == entry.current.ToRgb565()) {
                    continue;
                }
                CodePatch p;
                p.id = "patch_digits_stats";
                p.original.assign(&fw.records[0].image[off0], &fw.records[0].image[off0] + 4);
                p.replacement = MovwInstruction(7, entry.current.ToRgb565());
                for (size_t r = 0; r < 7; ++r) {
                    size_t off = kStatsDigitSites[r];
                    if (off + 4 > fw.records[r].image.size() ||
                        std::memcmp(&fw.records[r].image[off], p.original.data(), 4) != 0) {
                        out_err = "Stats digit instruction mismatch in record " + std::to_string(r);
                        return false;
                    }
                    p.locations.push_back({ off });
                }
                patches.push_back(p);
                continue;
            }

            const KnownColorSites* role_def = nullptr;
            for (const auto& rk : kKnownColorRoles) {
                if (entry.spec.id == std::string(rk.id)) {
                    role_def = &rk;
                    break;
                }
            }
            if (!role_def) continue;

            size_t off0 = role_def->sites[0][0];
            if (off0 + 6 > fw.records[0].image.size()) {
                out_err = "Firmware image 0 too small for " + std::string(entry.spec.label);
                return false;
            }
            Rgb8 curr_c;
            if (!DecodeColorInstruction(&fw.records[0].image[off0], curr_c)) {
                out_err = "Cannot decode existing color for " + std::string(entry.spec.label) + " in record 0";
                return false;
            }
            if (curr_c == entry.current) {
                continue;
            }

            CodePatch p;
            p.id = std::string("palette_") + entry.spec.id;
            p.original.assign(&fw.records[0].image[off0], &fw.records[0].image[off0] + 6);
            p.replacement = ColorInstruction(entry.current);

            for (size_t r = 0; r < 7; ++r) {
                const auto& rec_sites = role_def->sites[r];
                for (size_t s_off : rec_sites) {
                    if (s_off + 6 > fw.records[r].image.size() ||
                        std::memcmp(&fw.records[r].image[s_off], p.original.data(), 6) != 0) {
                        out_err = std::string(entry.spec.label) + " signature mismatch in record " + std::to_string(r);
                        return false;
                    }
                }
                p.locations.push_back(rec_sites);
            }
            patches.push_back(p);
        }

        // Auto-revert any previously injected broken background patches to clean factory stock instructions
        const uint8_t bg_patched_prefix_digit[6] = { 0xe8, 0x07, 0x44, 0xbf, 0x0e, 0x98 };
        const uint8_t bg_stock_digit[10]  = { 0x15, 0xf0, 0x01, 0x0f, 0x14, 0xbf, 0x0e, 0x98, 0x58, 0x46 };
        const uint8_t bg_stock_bitmap[10] = { 0x14, 0xf0, 0x01, 0x0f, 0x14, 0xbf, 0x10, 0x98, 0x58, 0x46 };
        const uint8_t bg_stock_reg[10]    = { 0x15, 0xf0, 0x01, 0x0f, 0x14, 0xbf, 0x58, 0x46, 0x50, 0x46 };

        if (0x4384 + 10 <= fw.records[0].image.size() &&
            std::memcmp(&fw.records[0].image[0x4384], bg_patched_prefix_digit, 6) == 0) {
            // 1. Digit stack (Rec 0, 5, 6)
            {
                CodePatch p;
                p.id = "revert_background_digit_stack";
                p.original.assign(&fw.records[0].image[0x4384], &fw.records[0].image[0x4384] + 10);
                p.replacement.assign(bg_stock_digit, bg_stock_digit + 10);

                p.locations.resize(7);
                p.locations[0] = { 0x4384 };
                p.locations[5] = { 0x436c };
                p.locations[6] = { 0x436c };
                patches.push_back(p);
            }

            // 2. Bitmap stack (Rec 0, 5, 6)
            {
                CodePatch p;
                p.id = "revert_background_bitmap_stack";
                p.original.assign(&fw.records[0].image[0x4776], &fw.records[0].image[0x4776] + 10);
                p.replacement.assign(bg_stock_bitmap, bg_stock_bitmap + 10);

                p.locations.resize(7);
                p.locations[0] = { 0x4776 };
                p.locations[5] = { 0x475e };
                p.locations[6] = { 0x475e };
                patches.push_back(p);
            }

            // 3. Register stack (Rec 1, 2, 3, 4)
            {
                CodePatch p;
                p.id = "revert_background_register";
                p.original.assign(&fw.records[1].image[0x42e4], &fw.records[1].image[0x42e4] + 10);
                p.replacement.assign(bg_stock_reg, bg_stock_reg + 10);

                p.locations.resize(7);
                p.locations[1] = { 0x42e4, 0x46fc };
                p.locations[2] = { 0x3cbc, 0x40d4 };
                p.locations[3] = { 0x42e4, 0x46fc };
                p.locations[4] = { 0x3cbc, 0x40d4 };
                patches.push_back(p);
            }
        }
    } else {
        for (const auto& entry : palette) {
            if (entry.current == entry.spec.original) continue;

            if (std::string(entry.spec.id) == "digits_wattage") {
                CodePatch p;
                p.id = "patch_digits_wattage";
                p.original = MovwInstruction(8, 0xFFFF);
                p.replacement = MovwInstruction(8, entry.current.ToRgb565());
                const size_t watt_offsets[] = { 0x567c, 0x53b0, 0x4d64, 0x53a8, 0x4d5c, 0x56a4, 0x56a4 };
                for (size_t r = 0; r < fw.records.size(); ++r) {
                    size_t off = (r < 7) ? watt_offsets[r] : 0;
                    if (off + 4 <= fw.records[r].image.size() &&
                        std::equal(p.original.begin(), p.original.end(), fw.records[r].image.begin() + off)) {
                        p.locations.push_back({ off });
                    } else {
                        out_err = "Cannot locate wattage digit instruction in record " + std::to_string(r);
                        return false;
                    }
                }
                patches.push_back(p);
                continue;
            }

            if (std::string(entry.spec.id) == "digits_stats") {
                CodePatch p;
                p.id = "patch_digits_stats";
                p.original = MovwInstruction(7, 0xFFFF);
                p.replacement = MovwInstruction(7, entry.current.ToRgb565());
                const size_t stats_offsets[] = { 0x5ca0, 0x5944, 0x530c, 0x593c, 0x5304, 0x5cc8, 0x5cc8 };
                for (size_t r = 0; r < fw.records.size(); ++r) {
                    size_t off = (r < 7) ? stats_offsets[r] : 0;
                    if (off + 4 <= fw.records[r].image.size() &&
                        std::equal(p.original.begin(), p.original.end(), fw.records[r].image.begin() + off)) {
                        p.locations.push_back({ off });
                    } else {
                        out_err = "Cannot locate stats digit instruction in record " + std::to_string(r);
                        return false;
                    }
                }
                patches.push_back(p);
                continue;
            }

            CodePatch p;
            p.id = std::string("palette_") + entry.spec.id;
            p.original = ColorInstruction(entry.spec.original);
            p.replacement = ColorInstruction(entry.current);

            for (size_t r = 0; r < fw.records.size(); ++r) {
                auto locs = fw.FindAll(r, p.original.data(), p.original.size());
                if (locs.empty()) {
                    out_err = std::string(entry.spec.label) + " signature is missing in hardware record " + std::to_string(r);
                    return false;
                }
                p.locations.push_back(locs);
            }
            patches.push_back(p);
        }
    }

    out_patches = std::move(patches);
    return true;
}
