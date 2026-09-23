#include "flash_protocol.h"
#include <algorithm>
#include <limits>

namespace voopoo::protocol {
static void Be32(Bytes& bytes, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(uint8_t(value >> shift));
}
uint16_t Crc(const uint8_t* data, size_t size) {
    return FirmwarePackage::Crc16Ccitt(data, size);
}
Bytes Encode(uint16_t address, uint8_t command, const Bytes& payload) {
    if (payload.size() > 4096 - 9) return {};
    Bytes bytes{0x68, uint8_t(address >> 8), uint8_t(address), command,
                uint8_t(payload.size() >> 8), uint8_t(payload.size())};
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    auto crc = Crc(bytes.data() + 1, bytes.size() - 1);
    bytes.insert(bytes.end(), {uint8_t(crc), uint8_t(crc >> 8), 0x16});
    return bytes;
}
bool Decode(const Bytes& bytes, Frame& frame) {
    if (bytes.size() < 9 || bytes.size() > 4096 || bytes[0] != 0x68 || bytes.back() != 0x16) return false;
    const size_t length = (size_t(bytes[4]) << 8) | bytes[5];
    if (length + 9 != bytes.size()) return false;
    const auto crc = uint16_t(bytes[bytes.size() - 3] | (bytes[bytes.size() - 2] << 8));
    if (crc != Crc(bytes.data() + 1, length + 5)) return false;
    frame = {uint16_t((bytes[1] << 8) | bytes[2]), bytes[3], Bytes(bytes.begin() + 6, bytes.end() - 3)};
    return true;
}
bool Reply(const Bytes& request, const Bytes& response, std::string& error) {
    Frame sent{}, received{};
    if (!Decode(request, sent) || !Decode(response, received)) {
        error = "Invalid reply length, framing or CRC"; return false;
    }
    if (sent.address != 0xAAAA && received.address != sent.address) {
        error = "Reply from a different hardware ID"; return false;
    }
    if (received.command == 0xFA) {
        error = received.payload == Bytes{10} ? "Device refused update: LOWPOWER" :
                received.payload == Bytes{1} ? "Device refused update: STOP" : "Device rejected command";
        return false;
    }
    if (received.command != uint8_t(sent.command | 0x80)) {
        error = "Unexpected reply command (stale or unrelated packet)"; return false;
    }
    if (received.address == 0 || received.address == 0xFFFF || received.address == 0xAAAA) {
        error = "Device did not report a concrete hardware ID"; return false;
    }
    return true;
}
bool MakePlan(const FirmwarePackage& package, uint16_t code, Plan& plan, std::string& error) {
    plan = {};
    const HardwareRecord* found = nullptr;
    for (const auto& record : package.records) {
        if (uint16_t((record.device_code[0] << 8) | record.device_code[1]) != code) continue;
        if (found) { error = "Ambiguous package: duplicate hardware ID"; return false; }
        found = &record;
    }
    if (!found || found->image.size() < 8 || found->image.size() > 512u * 65535u) {
        error = "No valid image for the requested hardware ID"; return false;
    }
    plan = {code, found->image, FirmwarePackage::AdditiveChecksum(found->image.data(), found->image.size())};
    return true;
}
Bytes Block(const Plan& plan, uint32_t offset) {
    if (offset >= plan.image.size() || offset % 512) return {};
    Bytes payload;
    Be32(payload, offset);
    const size_t end = (std::min)(size_t(offset) + 512, plan.image.size());
    for (size_t i = offset; i < end; ++i) payload.push_back(uint8_t(plan.image[i] + 0x33));
    return Encode(plan.code, 0x04, payload);
}
Bytes Finish(const Plan& plan) {
    Bytes payload;
    Be32(payload, uint32_t(plan.image.size()));
    Be32(payload, plan.checksum);
    return Encode(plan.code, 0x20, payload);
}
bool Program(const Plan& plan, uint16_t verifiedCode, bool verifiedBootloader,
             const Exchange& exchange, const Cancel& cancel, const Progress& progress, std::string& error) {
    if (!verifiedBootloader || verifiedCode != plan.code || plan.image.empty()) {
        error = "Bootloader identity must match the selected package record"; return false;
    }
    const auto send = [&](const Bytes& request, int timeout) {
        if (cancel()) { error = "Update cancelled; device may remain in bootloader"; return false; }
        Bytes response;
        return exchange(request, response, timeout, error) && Reply(request, response, error);
    };
    const auto erase = Encode(plan.code, plan.image.size() < 0x12000 ? 0x09 : 0x08);
    const int eraseTimeout = plan.image.size() < 0x12000 ? 3000 : 6000;
    // Official Qt updater 0x0041AC30: prepare, block 0, prepare again,
    // then every block starting at zero. Final length/sum excludes record ID.
    if (!send(erase, eraseTimeout) || !send(Block(plan, 0), 500) ||
        !send(erase, eraseTimeout == 3000 ? 2000 : 6000)) return false;
    const size_t count = (plan.image.size() + 511) / 512;
    for (size_t block = 0; block < count; ++block) {
        if (!send(Block(plan, uint32_t(block * 512)), 500)) return false;
        progress(block + 1, count);
    }
    return send(Finish(plan), 1000);
}
}
