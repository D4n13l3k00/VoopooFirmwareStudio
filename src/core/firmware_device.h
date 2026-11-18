#pragma once

#include "firmware_emulator.h"
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

struct StudioState;

namespace FirmwareDevice {
struct RegressionObservation;
struct Inputs {
    bool fire = false, plus = false, minus = false, usb = false;
    bool atomizerConnected = true;
    int batteryMillivolts = 4000, resistanceMilliOhms = 250, temperatureCelsius = 25;
    // Physical automatic-draw sensor, active high on PA4 on both board families.
    bool airflow = false;
    bool operator==(const Inputs&) const = default;
};

struct Access {
    uint64_t virtualMicroseconds = 0;
    uint32_t pc = 0, address = 0, value = 0;
    uint8_t size = 0;
    bool write = false;
};

// Captured only by the machine's owner; invalid/unsupported registers are not
// represented as zero readings. Bits follow r0-r15 and the special fields below.
struct CpuRegisters {
    std::array<uint32_t, 16> general{};
    uint32_t xpsr = 0, msp = 0, psp = 0, control = 0, primask = 0, basepri = 0, faultmask = 0;
    uint16_t validGeneral = 0;
    uint8_t validSpecial = 0;
};

enum class StackStatus { Unavailable, Available, Empty, InvalidPointer, ReadFailed };
struct StackSnapshot {
    std::array<uint32_t, 32> words{};
    uint32_t address = 0, count = 0;
    StackStatus status = StackStatus::Unavailable;
};

struct Snapshot {
    FirmwareEmulator::Frame display;
    uint64_t virtualMicroseconds = 0, instructions = 0, frameGeneration = 0;
    uint32_t pc = 0, sp = 0;
    CpuRegisters registers;
    StackSnapshot stack;
    uint32_t modeledClockHz = 0, mappedSramBase = 0, mappedSramBytes = 0;
    bool sleeping = false, halted = false;
    std::string error;
    std::vector<Access> recentAccess;
};

// Synchronous, single-owner API. Call on a worker, never the UI thread.
// RunFor accepts at most 10,000 virtual microseconds per call.
// Reset preserves nonvolatile settings but resets CPU, SRAM and peripherals.
class Machine {
public:
    Machine();
    ~Machine();
    Machine(Machine&&) noexcept;
    Machine& operator=(Machine&&) noexcept;
    Machine(const Machine&) = delete;
    Machine& operator=(const Machine&) = delete;

    bool Open(const StudioState&, std::span<const uint8_t> storage, std::string& error);
    bool Open(const StudioState&, uint16_t hardwareCode,
        std::span<const uint8_t> storage, std::string& error);
    bool Reset(std::string& error);
    void SetInputs(const Inputs&);
    bool RunFor(uint32_t microseconds, std::string& error);
    // One instruction; sleeping CPUs advance to the next peripheral event.
    // With no scheduled wake event, the CPU remains asleep.
    bool Step(std::string& error);
    const Snapshot& Current() const;
    std::vector<uint8_t> SaveStorage() const;
    uint64_t StorageRevision() const;

private:
    friend bool RunFaultRegressions(Machine&, std::string&);
    friend bool RunDiagnosticsRegressions(Machine&, std::string&);
    friend std::string DescribeRegressionState(const Machine&);
    friend RegressionObservation ObserveRegressionState(const Machine&, std::span<uint8_t>);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace FirmwareDevice
