#pragma once
#include <unicorn/unicorn.h>
#include <unicorn/arm.h>
#include <array>
#include <span>
#include <string>
#include <vector>
#include <cstdint>

namespace FirmwareEmulator {
// Owns one Cortex-M execution context. Backend hooks observe hardware only;
// native drawing instructions and resource reads must not be replaced.
class ArmRuntime {
public:
    ~ArmRuntime();
    ArmRuntime() = default;
    ArmRuntime(const ArmRuntime&) = delete;
    ArmRuntime& operator=(const ArmRuntime&) = delete;
    bool Open(std::span<const uint8_t> image, uint32_t flashBase, uint32_t ramBase,
        size_t ramSize, int cpuModel, std::span<const uint32_t> peripheralPages, std::string& error);
    bool Reset(std::string& error);
    bool Call(uint32_t address, uint32_t stack, const std::array<uint32_t, 4>& arguments, std::string& error);
    bool Write(uint32_t address, const void* data, size_t size, std::string& error);
    template<class T> bool Write(uint32_t address, T value, std::string& error) {
        return Write(address, &value, sizeof(value), error);
    }
    uc_engine* Raw() const { return engine_; }
    void Close();
private:
    uc_engine* engine_ = nullptr;
    uc_context* resetCpu_ = nullptr;
    uint32_t ramBase_ = 0;
    std::vector<uint8_t> zeroRam_;
    std::vector<uint32_t> pages_;
};
}
