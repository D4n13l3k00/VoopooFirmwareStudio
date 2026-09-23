#include "../src/flasher/flash_protocol.h"
#include "official_vectors.h"
#include <stdexcept>

namespace voopoo::protocol {
static Bytes Hex(const char* text) {
    Bytes bytes;
    auto digit = [](char ch) { return ch <= '9' ? ch - '0' : ch - 'a' + 10; };
    for (; *text; text += 2) bytes.push_back(uint8_t(digit(text[0]) * 16 + digit(text[1])));
    return bytes;
}
bool RunTests(const FirmwarePackage& package, std::string& error) {
    try {
        auto require = [](bool condition, const char* message) { if (!condition) throw std::runtime_error(message); };
        Plan plan{0x3103};
        for (size_t i = 0; i < 524; ++i) plan.image.push_back(uint8_t(i));
        plan.checksum = 0xff42;
        require(Block(plan, 0) == Hex(official_vectors::block0), "Full block differs from original Qt updater");
        require(Block(plan, 512) == Hex(official_vectors::block1), "Partial block differs from original Qt updater");
        require(Finish(plan) == Hex(official_vectors::finish), "Final length/checksum differs from original Qt updater");
        require(Encode(plan.code, 9) == Hex(official_vectors::prepare_small), "Small prepare differs from original");
        require(Encode(plan.code, 8) == Hex(official_vectors::prepare_large), "Large prepare differs from original");
        require(Encode(plan.code, 0x37, {'v','o','o','p','o','-','t','e','s','t','-','0','0','0','0',0}) ==
                Hex(official_vectors::boot), "Boot request differs from original");
        require(Block(plan, 524).empty() && Block(plan, 1).empty(), "Invalid block offset accepted");
        const auto noCancel = [] { return false; };
        const auto noProgress = [](size_t, size_t) {};
        std::vector<Bytes> trace;
        const Exchange ack = [&](const Bytes& request, Bytes& response, int, std::string&) {
            trace.push_back(request); Frame frame; require(Decode(request, frame), "Invalid outgoing request");
            response = Encode(frame.address, uint8_t(frame.command | 0x80)); return true;
        };
        require(Program(plan, plan.code, true, ack, noCancel, noProgress, error), "Successful update failed");
        const std::vector<Bytes> expected{Hex(official_vectors::prepare_small), Hex(official_vectors::block0),
            Hex(official_vectors::prepare_small), Hex(official_vectors::block0), Hex(official_vectors::block1), Hex(official_vectors::finish)};
        require(trace == expected, "Incorrect update sequence");
        trace.clear();
        require(!Program(plan, 0x3102, true, ack, noCancel, noProgress, error) && trace.empty(), "Wrong ID allowed writes");
        require(!Program(plan, plan.code, false, ack, noCancel, noProgress, error) && trace.empty(), "Application mode allowed writes");
        require(!Program(plan, plan.code, true, ack, [] { return true; }, noProgress, error) && trace.empty(), "Cancellation allowed writes");
        // At every stage, bad CRC/command/address, explicit refusal and timeout
        // must stop immediately, including both prepares and finalization.
        for (size_t failAt = 0; failAt < expected.size(); ++failAt) {
            for (int fault = 0; fault < 6; ++fault) {
                size_t calls = 0;
                const Exchange broken = [&](const Bytes& request, Bytes& response, int, std::string& why) {
                    Frame frame; require(Decode(request, frame), "Invalid test request");
                    response = Encode(frame.address, uint8_t(frame.command | 0x80));
                    if (calls++ != failAt) return true;
                    switch (fault) {
                    case 0: response[response.size() - 3] ^= 1; break;
                    case 1: response = Encode(frame.address, 0xC4); break;
                    case 2: response = Encode(0x3102, uint8_t(frame.command | 0x80)); break;
                    case 3: response = Encode(frame.address, 0xFA, {1}); break;
                    case 4: response = Encode(frame.address, 0xFA, {10}); break;
                    default: why = "Timeout"; return false;
                    }
                    return true;
                };
                require(!Program(plan, plan.code, true, broken, noCancel, noProgress, error), "Failed response reported success");
                require(calls == failAt + 1, "Update continued or retried after a failed command");
                require(!error.empty(), "Missing failure reason");
            }
        }
        auto exact = plan; exact.image.resize(512); exact.checksum = 0xff00;
        trace.clear();
        require(Program(exact, exact.code, true, ack, noCancel, noProgress, error) && trace.size() == 5,
                "Exact 512-byte image emitted an extra empty block");
        auto large = plan; large.image.resize(0x12000); large.checksum = 0xff42;
        trace.clear();
        require(Program(large, large.code, true, ack, noCancel, noProgress, error) &&
                trace.front() == Hex(official_vectors::prepare_large), "Incorrect large-image prepare command");
        auto cancelled = [&] { return trace.size() >= 4; };
        trace.clear();
        require(!Program(plan, plan.code, true, ack, cancelled, noProgress, error) && trace.size() == 4,
                "Cancellation between blocks was ignored");
        Frame decoded;
        auto corrupt = Hex(official_vectors::block0); corrupt[20] ^= 1;
        require(!Decode(corrupt, decoded), "Payload corruption passed CRC validation");
        require(!Decode(Bytes(5000, 0), decoded), "Oversize frame accepted");
        require(Reply(Encode(0xAAAA, 0x44), Encode(0x3103, 0xC4), error), "Discovery identity refused");
        require(!Reply(Encode(0xAAAA, 0x44), Encode(0xAAAA, 0xC4), error), "Broadcast identity accepted");
        Plan selected;
        require(MakePlan(package, 0x3103, selected, error), "Embedded 3103 section missing");
        require(selected.code == 0x3103 && selected.checksum == FirmwarePackage::AdditiveChecksum(selected.image.data(), selected.image.size()),
                "Selected image checksum incorrect");
        require(!MakePlan(package, 3103, selected, error), "Decimal 3103 incorrectly selected hex 3103");
        FirmwarePackage duplicate = package;
        for (const auto& record : package.records) {
            if (record.device_code[0] == 0x31 && record.device_code[1] == 0x03) duplicate.records.push_back(record);
        }
        require(!MakePlan(duplicate, 0x3103, selected, error), "Ambiguous duplicate hardware record accepted");
        error.clear(); return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
}
