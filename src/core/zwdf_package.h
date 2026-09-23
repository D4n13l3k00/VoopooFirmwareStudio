#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include "firmware_color.h"

struct HardwareRecord {
    std::vector<uint8_t> image;
    uint8_t device_code[2] = { 0, 0 };
    std::vector<uint8_t> marker;
    uint32_t linked_base = 0;
    uint32_t sp = 0;
    uint32_t reset = 0;
};

struct BuildReport {
    std::string source_sha256;
    std::string output_sha256;
    size_t output_size = 0;
    size_t records = 0;
    size_t changed_assets = 0;
    size_t changed_colors = 0;
    size_t patched_ranges = 0;
    size_t unchanged_payload_bytes_verified = 0;
    std::vector<std::string> hardware_codes;

    std::string ToJson() const;
};

struct CodePatch {
    std::string id;
    std::vector<uint8_t> original;
    std::vector<uint8_t> replacement;
    std::vector<std::vector<size_t>> locations;
};

class FirmwarePackage {
public:
    std::vector<HardwareRecord> records;
    std::vector<uint8_t> source_bytes;
    std::string source_sha256;

    static bool Open(const std::string& path, FirmwarePackage& out_pkg, std::string& out_err);
    static bool Parse(const std::vector<uint8_t>& bytes, FirmwarePackage& out_pkg, std::string& out_err);

    const uint8_t* BytesAt(size_t record_idx, uint32_t address, size_t len) const;
    bool FindUnique(size_t record_idx, const uint8_t* needle, size_t needle_len, size_t& out_offset) const;
    std::vector<size_t> FindAll(size_t record_idx, const uint8_t* needle, size_t needle_len) const;

    bool PackWith(
        const std::map<std::string, std::pair<std::vector<size_t>, std::vector<uint8_t>>>& replacements,
        const std::vector<CodePatch>& code_patches,
        std::vector<uint8_t>& out_bytes,
        BuildReport& out_report,
        std::string& out_err
    ) const;

    static uint16_t Crc16Ccitt(const uint8_t* data, size_t len);
    static uint32_t AdditiveChecksum(const uint8_t* data, size_t len);
    static std::string ComputeSha256(const uint8_t* data, size_t len);
};

bool BuildColorPatches(
    const FirmwarePackage& fw,
    const std::vector<PaletteState>& palette,
    Rgb8 background,
    std::vector<CodePatch>& out_patches,
    std::string& out_err
);

bool DetectFirmwareColors(
    const FirmwarePackage& fw,
    std::vector<PaletteState>& palette,
    Rgb8& background
);
