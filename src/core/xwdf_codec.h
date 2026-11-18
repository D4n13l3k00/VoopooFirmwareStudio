#pragma once

#include <cstdint>
#include <string>
#include <vector>

class FirmwarePackage;

namespace XwdfCodec {
bool Parse(const std::vector<uint8_t>& bytes, FirmwarePackage& package, std::string& error);
bool Repack(const std::vector<uint8_t>& source, const std::vector<uint8_t>& image,
            std::vector<uint8_t>& output, std::string& error);
}
