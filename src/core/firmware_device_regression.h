#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <span>

namespace FirmwareDevice {
class Machine;

// Native execution/RAM observations only; no guest-state mutation or UI routing.
struct RegressionObservation {
    uint64_t bootCompletions = 0, mainIterations = 0, serviceEntries = 0, powerOffEntries = 0;
    uint32_t flags = 0, page = 0, powerA = 0, powerB = 0;
    uint64_t usbInitializations = 0;
    uint32_t mode = 0, powerLimit = 0, batteryMillivolts = 0, usbControl = 0, usbStatus = 0;
    // Normalized from each revision's native word/halfword ABI. Unmapped
    // profiles are explicit, never interpreted using the HW3103 layout.
    bool mapped = false, historyMenu = false, usbPeripheral = false;
    bool railPowered = false;
    uint32_t locked = 0, resistanceMilliOhms = 0, durationCentiseconds = 0, puffs = 0;
    uint32_t scoreRawTotal = 0, scoreBestSinglePuff = 0;
    uint32_t feedbackCurrentMilliAmps = 0, feedbackVoltageMillivolts = 0, feedbackResistanceMilliOhms = 0;
    uint32_t pwmCompare = 0, pwmPeriod = 0;
    bool pwmActive = false;
    uint64_t adcConversions = 0, pwmStarts = 0;
    std::array<uint16_t, 6> adcSamples{};
    // Slots match the existing native S/X screen catalog, not VM UI pages.
    std::array<uint64_t, 17> screenEntries{};
    // Read-only native reserved-box observations; zero if no validated envelope.
    uint32_t menuStateAddress = 0, menuMagic = 0, menuSavedPower = 0;
    uint8_t menuActive = 0, menuSelection = 0, menuSequence = 0;
    bool menuBatteryVoltage = false;
    uint32_t menuPuffValue = 0, menuPuffOriginal = 0, menuPuffDigit = 0;
    uint8_t menuPuffStatus = 0;
    // Captured at the original native formatter's first post-prologue instruction.
    uint32_t voltageCentivolts = 0;
    uint64_t voltageRenders = 0;
};
// Optional destination receives current native settings bytes, never a Flash fallback.
RegressionObservation ObserveRegressionState(const Machine&, std::span<uint8_t> settingsSnapshot = {});
std::string DescribeRegressionState(const Machine&);
bool RunDiagnosticsRegressions(Machine&, std::string&);
} // namespace FirmwareDevice
