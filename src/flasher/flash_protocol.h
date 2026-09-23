#pragma once
#include "zwdf_package.h"
#include <functional>

namespace voopoo::protocol {
using Bytes = std::vector<uint8_t>;
struct Frame { uint16_t address; uint8_t command; Bytes payload; };
struct Plan { uint16_t code = 0; Bytes image; uint32_t checksum = 0; };
uint16_t Crc(const uint8_t* data, size_t size);
Bytes Encode(uint16_t address, uint8_t command, const Bytes& payload = {});
bool Decode(const Bytes& bytes, Frame& frame);
bool Reply(const Bytes& request, const Bytes& response, std::string& error);
bool MakePlan(const FirmwarePackage& package, uint16_t code, Plan& plan, std::string& error);
Bytes Block(const Plan& plan, uint32_t offset);
Bytes Finish(const Plan& plan);
// Returning false from Exchange aborts the update. Never continue after an
// ambiguous write/erase response: repeated commands may be destructive.
using Exchange = std::function<bool(const Bytes&, Bytes&, int, std::string&)>;
using Cancel = std::function<bool()>;
using Progress = std::function<void(size_t, size_t)>;
bool Program(const Plan& plan, uint16_t verifiedCode, bool verifiedBootloader,
             const Exchange& exchange, const Cancel& cancel,
             const Progress& progress, std::string& error);
bool RunTests(const FirmwarePackage& package, std::string& error);
}
