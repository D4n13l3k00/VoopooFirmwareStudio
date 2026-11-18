#pragma once

#include "app_state.h"
#include "firmware_device.h"
#include "firmware_device_regression.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>
#include <sstream>

namespace DeviceFunctionalMatrixDetail {
using namespace FirmwareDevice;

inline bool Visible(const Machine& machine) {
    const auto& pixels = machine.Current().display.rgba;
    return std::any_of(pixels.begin(), pixels.end(), [](uint32_t p) { return p != 0xff000000u; });
}
inline uint32_t Power(const RegressionObservation& state) {
    return state.mode == 2 || state.mode == 3 ? state.powerB : state.powerA;
}
struct Bench {
    Machine machine;
    Inputs inputs;
    std::string error;
    bool Require(bool condition, const char* message) {
        if (condition) return true;
        error = message;
        return false;
    }
    bool Advance(uint32_t microseconds) {
        while (microseconds) {
            const auto slice = (std::min)(microseconds, 10000u);
            if (!machine.RunFor(slice, error)) return false;
            microseconds -= slice;
        }
        return true;
    }
    bool Hold(bool fire, bool plus, bool minus, uint32_t duration, uint32_t release = 300000) {
        inputs.fire = fire; inputs.plus = plus; inputs.minus = minus;
        machine.SetInputs(inputs);
        if (!Advance(duration)) return false;
        inputs.fire = inputs.plus = inputs.minus = false;
        machine.SetInputs(inputs);
        return Advance(release);
    }
    bool FiveClicks() {
        for (unsigned click = 0; click < 5; ++click)
            if (!Hold(true, false, false, 70000, 70000)) return false;
        return true;
    }
    bool Main(uint64_t previousBoots) {
        // Real stock logo delays and the 2-second RTOS wait must finish. A
        // raster alone is not evidence that the native Main loop is running.
        const uint64_t deadline = machine.Current().virtualMicroseconds + 12000000;
        bool acceptedClock = false;
        while (machine.Current().virtualMicroseconds < deadline) {
            const auto state = ObserveRegressionState(machine);
            if (state.bootCompletions > previousBoots && state.mainIterations &&
                state.screenEntries[0] && state.page == 1 && (state.flags & 0x400) && Visible(machine)) return true;
            if (!acceptedClock && state.historyMenu && state.bootCompletions > previousBoots &&
                state.mainIterations && state.page == 6 && (state.flags & 0x400) && Visible(machine)) {
                // Fresh RTOS 3013/3014 asks for the clock before Main. Native
                // clock event 2 is long-Fire: Score3014 08005C00 saves BCD via
                // 0800DFBC, then calls the real Main renderer at 08005C1E.
                // Keep the prompt interactive; neither select a host page nor
                // treat its visible boot frame as a completed Main contract.
                acceptedClock = true;
                if (!Hold(true, false, false, 1600000, 300000)) return false;
                continue;
            }
            if (!Advance(10000)) return false;
        }
        return Require(false, "native power-on tail/Main/page/LCD contract not reached within 12 virtual seconds");
    }
    bool Boot() {
        const auto before = ObserveRegressionState(machine).bootCompletions;
        return FiveClicks() && Main(before);
    }
    bool PowerClick(bool plus) { return Hold(false, plus, !plus, 120000, 500000); }
    bool ReplaceCoil(int milliohms) {
        // A different installed coil must cross the actual open-atomizer state.
        // Changing the load in-place leaves the firmware's recognized/cached
        // nominal coil in use (e.g. native 170..280 mOhm -> 200-mOhm bank).
        inputs.atomizerConnected = false;
        machine.SetInputs(inputs);
        if (!Advance(1000000) || !Hold(true, false, false, 300000, 300000)) return false;
        const auto removed = ObserveRegressionState(machine);
        if (!Require(!(removed.flags & 8) && removed.pwmCompare == 0,
            "physical coil removal did not clear the native atomizer-present flag")) return false;
        inputs.resistanceMilliOhms = milliohms;
        inputs.atomizerConnected = true;
        machine.SetInputs(inputs);
        return Advance(1000000);
    }
};

inline bool Row(const StudioState& source, const HardwareRecord& record, const std::string& label,
    std::ostringstream& failures, std::ostream* report, size_t& passed, size_t& failed) {
    const uint16_t code = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
    Bench bench;
    bool healthy = true, ok = true;
    auto stage = [&](const char* name, auto&& run) {
        if (!healthy) {
            ++failed;
            if (report) *report << label << '/' << name << ": FAIL (blocked by earlier native lifecycle failure)\n" << std::flush;
            return;
        }
        bench.error.clear();
        if (run()) {
            ++passed;
            if (report) *report << label << '/' << name << ": PASS\n" << std::flush;
        } else {
            ++failed; healthy = ok = false;
            const auto detail = bench.error + "; " + DescribeRegressionState(bench.machine);
            failures << label << '/' << name << ": " << detail << '\n';
            if (report) *report << label << '/' << name << ": FAIL " << detail << '\n' << std::flush;
        }
    };
    stage("reset", [&] {
        if (!bench.machine.Open(source, code, {}, bench.error) || !bench.machine.Reset(bench.error)) return false;
        const auto& reset = bench.machine.Current();
        if (!bench.Require(reset.pc == (record.reset & ~1u) && reset.sp == record.sp &&
            reset.instructions == 0 && reset.virtualMicroseconds == 0 && !Visible(bench.machine),
            "Reset did not preserve the actual record's untouched CPU/SRAM/LCD entry state")) return false;
        if (!bench.Advance(250000)) return false;
        return bench.Require(bench.machine.Current().instructions && !bench.machine.Current().halted &&
            ObserveRegressionState(bench.machine).mapped, "native startup or read-only ABI mapping missing");
    });
    stage("five-click-main", [&] {
        if (!bench.Boot()) return false;
        const auto& current = bench.machine.Current();
        return bench.Require(current.modeledClockHz == (record.linked_base == 0x08003000 ? 96000000u : 72000000u) &&
            current.display.pixelWrites >= 80 * 160 && current.display.spiBytes >= 80 * 160 * 2,
            "native Main did not use the revision's clock and physical LCD");
    });
    stage("plus-minus-visible", [&] {
        auto state = ObserveRegressionState(bench.machine);
        if (!bench.Require(state.powerLimit > 6, "native coil-dependent power limit has no interior setpoint")) return false;
        if (Power(state) >= state.powerLimit && !bench.PowerClick(false)) return false;
        state = ObserveRegressionState(bench.machine);
        if (Power(state) <= 5 && !bench.PowerClick(true)) return false;
        const auto before = ObserveRegressionState(bench.machine);
        const auto pixels = bench.machine.Current().display.rgba;
        if (!bench.PowerClick(true)) return false;
        const auto plus = ObserveRegressionState(bench.machine);
        if (!bench.Require(plus.mode == before.mode && Power(plus) == Power(before) + 1 &&
            bench.machine.Current().display.rgba != pixels, "physical Plus did not add one visible native watt")) return false;
        const auto plusPixels = bench.machine.Current().display.rgba;
        if (!bench.PowerClick(false)) return false;
        const auto minus = ObserveRegressionState(bench.machine);
        return bench.Require(minus.mode == before.mode && Power(minus) == Power(before) && minus.page == 1 &&
            bench.machine.Current().display.rgba != plusPixels, "physical Minus did not undo one visible native watt");
    });
    stage("three-click-four-mode-cycle", [&] {
        const auto original = ObserveRegressionState(bench.machine);
        for (unsigned mode = 0; mode < 4; ++mode) {
            const auto before = ObserveRegressionState(bench.machine);
            const auto pixels = bench.machine.Current().display.rgba;
            for (unsigned click = 0; click < 3; ++click)
                if (!bench.Hold(true, false, false, 70000, 70000)) return false;
            if (!bench.Advance(800000)) return false;
            const auto after = ObserveRegressionState(bench.machine);
            if (!bench.Require(after.mode == (before.mode + 1) % 4 && after.page == 1 &&
                after.screenEntries[0] > before.screenEntries[0] && bench.machine.Current().display.rgba != pixels,
                "physical three-click sequence did not advance/render the native four-mode bank")) return false;
        }
        return bench.Require(ObserveRegressionState(bench.machine).mode == original.mode,
            "native four-mode cycle did not return to its starting mode");
    });
    stage("fire-output-puff", [&] {
        const auto before = ObserveRegressionState(bench.machine);
        const auto pixels = bench.machine.Current().display.rgba;
        if (!bench.Hold(true, false, false, 600000, 1000000)) return false;
        const auto after = ObserveRegressionState(bench.machine);
        return bench.Require(after.pwmStarts > before.pwmStarts && after.pwmCompare == 0 &&
            after.puffs == before.puffs + 1 && after.durationCentiseconds > 0 &&
            after.adcConversions > before.adcConversions && bench.machine.Current().display.rgba != pixels,
            "physical Fire did not drive/release native PWM, count a puff and render its duration");
    });
    stage("held-fire-native-timeout-and-recovery", [&] {
        const auto before = ObserveRegressionState(bench.machine);
        bench.inputs.fire = true; bench.machine.SetInputs(bench.inputs);
        const uint32_t cutoff = record.linked_base == 0x08003000 ? 10000000u : 8000000u;
        if (!bench.Advance(cutoff + 300000)) return false;
        const auto stopped = ObserveRegressionState(bench.machine);
        if (!bench.Require(stopped.screenEntries[12] > before.screenEntries[12] &&
            stopped.pwmCompare == 0 && stopped.puffs == before.puffs + 1,
            "continuous Fire did not reach native Time Out and stop PWM while held")) return false;
        if (!bench.Advance(1000000)) return false;
        const auto held = ObserveRegressionState(bench.machine);
        if (!bench.Require(held.pwmCompare == 0 && held.puffs == stopped.puffs,
            "native timeout restarted output without releasing Fire")) return false;
        if (!bench.Hold(false, false, false, 300000) ||
            !bench.Hold(true, false, false, 600000, 1000000)) return false;
        const auto recovered = ObserveRegressionState(bench.machine);
        return bench.Require(recovered.puffs == held.puffs + 1 && recovered.pwmStarts > held.pwmStarts &&
            recovered.pwmCompare == 0, "Fire release did not recover from native timeout");
    });
    stage("fire-plus-lock-unlock", [&] {
        const auto before = ObserveRegressionState(bench.machine);
        if (!bench.Hold(true, true, false, 1150000, 500000)) return false;
        const auto locked = ObserveRegressionState(bench.machine);
        if (!bench.Require(locked.locked && locked.screenEntries[14] > before.screenEntries[14],
            "Fire+Plus did not execute native lock notification/settings")) return false;
        if (!bench.PowerClick(true)) return false;
        if (!bench.Require(Power(ObserveRegressionState(bench.machine)) == Power(locked),
            "physical Plus changed power while natively locked")) return false;
        if (!bench.Hold(true, true, false, 1150000, 1000000)) return false;
        const auto unlocked = ObserveRegressionState(bench.machine);
        return bench.Require(!unlocked.locked && unlocked.screenEntries[13] > locked.screenEntries[13],
            "second Fire+Plus did not unlock through native code");
    });
    stage("fire-minus-puff-clear", [&] {
        const auto before = ObserveRegressionState(bench.machine);
        if (!bench.Hold(true, false, true, 1150000, 1000000)) return false;
        const auto after = ObserveRegressionState(bench.machine);
        return bench.Require(before.puffs && after.puffs == 0 && after.screenEntries[10] > before.screenEntries[10],
            "Fire+Minus did not clear an actual fired puff and draw the native confirmation");
    });
    stage("native-menu-service-enter-exit", [&] {
        const auto before = ObserveRegressionState(bench.machine);
        const auto pixels = bench.machine.Current().display.rgba;
        if (before.historyMenu) {
            // RTOS 3013/3014: Plus+Minus opens history (page 5); a short
            // Fire release toggles its Chip ID subview. All-three opens clock,
            // so using the HW3103 gesture here would test the wrong function.
            if (!bench.Hold(false, true, true, 1150000, 300000)) return false;
            if (!bench.Require(ObserveRegressionState(bench.machine).page == 5 &&
                bench.machine.Current().display.rgba != pixels, "Plus+Minus did not open native history menu")) return false;
            if (!bench.Hold(true, false, false, 120000, 300000)) return false;
        } else if (!bench.Hold(true, true, true, 1150000, 300000)) return false;
        const auto service = ObserveRegressionState(bench.machine);
        if (!bench.Require(service.serviceEntries > before.serviceEntries &&
            service.page == (before.historyMenu ? 5u : source.edition == FirmwareEdition::Score ? 4u : 7u) &&
            bench.machine.Current().display.rgba != pixels && bench.machine.Current().display.emulatedUid,
            "physical gesture did not execute and render this revision's native Chip ID view")) return false;
        if (!bench.Hold(true, false, false, 1600000, 3000000)) return false;
        const auto after = ObserveRegressionState(bench.machine);
        return bench.Require(after.page == 1 && after.screenEntries[0] > service.screenEntries[0] && Visible(bench.machine),
            "native long-Fire exit did not return from menu/service to Main");
    });
    if (code == 0x3013 || code == 0x3014) {
        stage("all-three-native-clock-enter-adjust-exit", [&] {
            const auto before = ObserveRegressionState(bench.machine);
            const auto pixels = bench.machine.Current().display.rgba;
            if (!bench.Hold(true, true, true, 1150000, 300000)) return false;
            if (!bench.Require(ObserveRegressionState(bench.machine).page == 6 &&
                bench.machine.Current().display.rgba != pixels, "all-three did not open the RTOS native clock menu")) return false;
            const auto clock = bench.machine.Current().display.rgba;
            if (!bench.PowerClick(true)) return false;
            if (!bench.Require(bench.machine.Current().display.rgba != clock,
                "physical Plus did not adjust the native clock selection")) return false;
            if (!bench.Hold(true, false, false, 1600000, 3000000)) return false;
            const auto after = ObserveRegressionState(bench.machine);
            return bench.Require(after.page == 1 && after.screenEntries[0] > before.screenEntries[0],
                "long Fire did not save/exit the native clock menu to Main");
        });
    }
    stage("adc-resistance-short-boundary", [&] {
        const auto before = ObserveRegressionState(bench.machine);
        // Both MCU families compensate high-current V/I by ~1.075. At a
        // 4-V bench, physical 95 mOhm can therefore recognize as 102 mOhm.
        // Bracket the actual native <=99-mOhm boundary, not the raw input.
        // GD32 rejects the initial press silently; its native warning is
        // dispatched by the 1.5-second long-Fire event.
        if (!bench.ReplaceCoil(90) || !bench.Hold(true, false, false, 1600000, 300000)) return false;
        const auto shorted = ObserveRegressionState(bench.machine);
        if (!bench.Require(shorted.adcConversions > before.adcConversions && shorted.puffs == before.puffs &&
            shorted.screenEntries[5] > before.screenEntries[5] && shorted.pwmCompare == 0,
            "90-milliohm ADC load did not take compensated native <=99-milliohm short protection")) return false;
        if (!bench.ReplaceCoil(110)) return false;
        const auto above = ObserveRegressionState(bench.machine);
        if (!bench.Hold(true, false, false, 600000, 500000)) return false;
        const auto fired = ObserveRegressionState(bench.machine);
        if (!bench.Require(fired.puffs == above.puffs + 1 && fired.pwmStarts > above.pwmStarts &&
            fired.screenEntries[5] == above.screenEntries[5], "110-milliohm ADC load did not cross native short boundary")) return false;
        return bench.ReplaceCoil(250) && bench.Advance(3000000);
    });
    stage("adc-open-atomizer", [&] {
        const auto before = ObserveRegressionState(bench.machine);
        bench.inputs.atomizerConnected = false;
        bench.machine.SetInputs(bench.inputs);
        if (!bench.Advance(1000000) || !bench.Hold(true, false, false, 1600000, 300000)) return false;
        const auto after = ObserveRegressionState(bench.machine);
        if (!bench.Require(after.screenEntries[6] > before.screenEntries[6] && after.pwmCompare == 0 &&
            after.puffs == before.puffs, "disconnected coil did not reject Fire via native Check Atomizer")) return false;
        return bench.ReplaceCoil(250) && bench.Advance(3000000);
    });
    stage("adc-hot-cold-boundaries", [&] {
        const auto before = ObserveRegressionState(bench.machine);
        bench.inputs.temperatureCelsius = 100;
        bench.machine.SetInputs(bench.inputs);
        if (!bench.Advance(3000000)) return false;
        // GD32 qualifies over-temperature under load after two seconds of
        // firing plus its debounce; N32 can reject before firing starts.
        bench.inputs.fire = true; bench.machine.SetInputs(bench.inputs);
        if (!bench.Advance(4500000)) return false;
        const auto hot = ObserveRegressionState(bench.machine);
        const uint32_t expectedPuffs = before.puffs + (record.linked_base == 0x08003000 ? 1u : 0u);
        if (!bench.Require(hot.adcSamples[3] < 400 && (hot.flags & 0x80) &&
            hot.screenEntries[11] > before.screenEntries[11] && hot.pwmCompare == 0 && hot.puffs == expectedPuffs,
            "100-C thermistor ADC did not execute native High Temp cutoff while Fire remained held")) return false;
        if (!bench.Hold(false, false, false, 300000) ||
            !bench.Hold(true, false, false, 1600000, 300000)) return false;
        const auto inhibited = ObserveRegressionState(bench.machine);
        if (!bench.Require((inhibited.flags & 0x80) && inhibited.pwmCompare == 0 && inhibited.puffs == hot.puffs,
            "Fire bypassed the latched native High Temp protection")) return false;
        bench.inputs.temperatureCelsius = -20;
        bench.machine.SetInputs(bench.inputs);
        if (!bench.Advance(10000000)) return false;
        const auto cold = ObserveRegressionState(bench.machine);
        if (!bench.Require(cold.adcSamples[3] > 3500 && !(cold.flags & 0x80),
            "cold thermistor ADC did not release native temperature protection")) return false;
        bench.inputs.temperatureCelsius = 25;
        bench.machine.SetInputs(bench.inputs);
        return bench.Advance(3000000);
    });
    stage("low-voltage-fire-inhibit", [&] {
        const auto before = ObserveRegressionState(bench.machine);
        bench.inputs.batteryMillivolts = 3100;
        bench.machine.SetInputs(bench.inputs);
        if (!bench.Advance(2000000) || !bench.Hold(true, false, false, 1600000, 300000)) return false;
        const auto after = ObserveRegressionState(bench.machine);
        if (!bench.Require(after.batteryMillivolts >= 3085 && after.batteryMillivolts <= 3110 &&
            (after.screenEntries[7] > before.screenEntries[7] || after.screenEntries[8] > before.screenEntries[8]) &&
            after.pwmCompare == 0 && after.puffs == before.puffs,
            "3100-mV native ADC path did not render battery/power warning and inhibit Fire")) return false;
        bench.inputs.batteryMillivolts = 4000;
        bench.machine.SetInputs(bench.inputs);
        return bench.Advance(3000000);
    });
    stage("critical-voltage-native-off-and-recovery", [&] {
        const auto before = ObserveRegressionState(bench.machine);
        bench.inputs.batteryMillivolts = 2987;
        bench.machine.SetInputs(bench.inputs);
        if (!bench.Advance(12000000)) return false;
        const auto off = ObserveRegressionState(bench.machine);
        if (!bench.Require(off.batteryMillivolts >= 2975 && off.batteryMillivolts <= 3000 &&
            off.powerOffEntries > before.powerOffEntries && !off.railPowered &&
            off.pwmCompare == 0 && off.puffs == before.puffs,
            "critical ADC battery voltage did not finish native shutdown and remove power")) return false;
        bench.inputs.batteryMillivolts = 4000;
        bench.machine.SetInputs(bench.inputs);
        return bench.Boot();
    });
    RegressionObservation persisted;
    std::vector<uint8_t> storage;
    stage("five-click-off-native-flash", [&] {
        // Save a deliberately changed setpoint, not merely default/erased data.
        auto before = ObserveRegressionState(bench.machine);
        if (!bench.PowerClick(Power(before) < before.powerLimit)) return false;
        persisted = ObserveRegressionState(bench.machine);
        const auto oldStorage = bench.machine.SaveStorage();
        const auto revision = bench.machine.StorageRevision();
        if (!bench.FiveClicks() || !bench.Advance(3000000)) return false;
        const auto off = ObserveRegressionState(bench.machine);
        storage = bench.machine.SaveStorage();
        return bench.Require(off.powerOffEntries > persisted.powerOffEntries &&
            (!off.railPowered || !(off.flags & 0x400)) &&
            bench.machine.StorageRevision() > revision && storage.size() == 24 + 4096 && storage != oldStorage &&
            !std::all_of(storage.begin() + 24, storage.end(), [](uint8_t b) { return b == 0xff; }),
            "five-click shutdown did not execute native OFF, Flash erase/program and power transition");
    });
    stage("fresh-reopen-preserves-settings", [&] {
        Machine reopened;
        if (!reopened.Open(source, code, storage, bench.error)) return false;
        if (!bench.Require(reopened.SaveStorage() == storage && reopened.StorageRevision() == 0,
            "fresh Open changed versioned native Flash bytes")) return false;
        bench.machine = std::move(reopened);
        bench.inputs = {};
        if (!bench.Advance(250000) || !bench.Boot()) return false;
        const auto restored = ObserveRegressionState(bench.machine);
        return bench.Require(restored.mode == persisted.mode && restored.powerA == persisted.powerA &&
            restored.powerB == persisted.powerB && restored.locked == persisted.locked,
            "native fresh boot did not restore saved mode/power/lock settings from Flash");
    });
    stage("idle-sleep-physical-screen-wake", [&] {
        const auto before = ObserveRegressionState(bench.machine);
        const auto start = bench.machine.Current().virtualMicroseconds;
        bool slept = false;
        for (unsigned slice = 0; slice < 3500; ++slice) {
            if (!bench.Advance(10000)) return false;
            slept |= bench.machine.Current().sleeping;
        }
        const auto idle = ObserveRegressionState(bench.machine);
        if (!bench.Require(slept && !Visible(bench.machine) && (idle.flags & 0x400) &&
            bench.machine.Current().virtualMicroseconds == start + 35000000 && idle.mainIterations > before.mainIterations,
            "35-second native idle did not enter WFI and blank the active device's LCD")) return false;
        if (!bench.Hold(true, false, false, 120000, 500000)) return false;
        const auto awake = ObserveRegressionState(bench.machine);
        return bench.Require(Visible(bench.machine) && awake.page == 1 && (awake.flags & 0x400) &&
            awake.bootCompletions == idle.bootCompletions, "physical Fire did not wake idle Main without a synthetic reboot");
    });
    stage("off-idle-five-click-wake", [&] {
        if (!bench.FiveClicks() || !bench.Advance(3000000)) return false;
        const auto off = ObserveRegressionState(bench.machine);
        if (!bench.Require(!off.railPowered || !(off.flags & 0x400), "native second shutdown did not reach OFF")) return false;
        if (!bench.Advance(35000000)) return false;
        const auto idleOff = ObserveRegressionState(bench.machine);
        if (!bench.Require((!idleOff.railPowered || !(idleOff.flags & 0x400)) && !Visible(bench.machine),
            "OFF powered itself on without a physical edge")) return false;
        return bench.Boot();
    });
    stage("in-flight-zero-ohm-output-cutoff", [&] {
        const auto before = ObserveRegressionState(bench.machine);
        bench.inputs.fire = true; bench.machine.SetInputs(bench.inputs);
        // Fault during the native initial resistance-sampling window (<400 ms),
        // not the background probe or a later, deliberately cached reading.
        if (!bench.Advance(100000)) return false;
        const auto firing = ObserveRegressionState(bench.machine);
        if (!bench.Require((firing.flags & 1) && firing.pwmActive && firing.feedbackCurrentMilliAmps > 500 &&
            firing.feedbackVoltageMillivolts > 2000 && firing.puffs == before.puffs,
            "valid coil did not reach native firing before the in-flight short")) return false;
        // Terminal voltage is prescribed: the bench does not calculate droop.
        // GD32's software protection qualifies a short at 0.5–2.6 V.
        if (record.linked_base == 0x08003000) bench.inputs.batteryMillivolts = 2500;
        bench.inputs.resistanceMilliOhms = 0; bench.machine.SetInputs(bench.inputs);
        if (!bench.Advance(50000)) return false;
        if (!bench.Require(!ObserveRegressionState(bench.machine).pwmActive,
            "in-flight zero-ohm load left native PWM enabled")) return false;
        if (!bench.Advance(1600000)) return false;
        const auto shorted = ObserveRegressionState(bench.machine);
        if (!bench.Require(!shorted.pwmActive && shorted.puffs >= before.puffs && shorted.puffs <= before.puffs + 1 &&
            shorted.screenEntries[5] > before.screenEntries[5],
            "in-flight short did not remain inhibited and report native Short")) return false;
        bench.inputs.batteryMillivolts = 4000; bench.machine.SetInputs(bench.inputs);
        return bench.Hold(false, false, false, 300000) && bench.ReplaceCoil(250) && bench.Advance(3000000);
    });

    // Independent OFF/VBUS fixture still executes if this record's lifecycle
    // failed. A failing revision never prevents opening any later record.
    healthy = true;
    stage("usb-low-voltage-charging-detach", [&] {
        Bench charging;
        if (!charging.machine.Open(source, code, {}, charging.error)) {
            bench.error = charging.error; bench.machine = std::move(charging.machine); return false;
        }
        charging.inputs.usb = true; charging.inputs.batteryMillivolts = 2987;
        const auto before = ObserveRegressionState(charging.machine);
        charging.machine.SetInputs(charging.inputs);
        // The GD32 charging task is created only when VBUS is present at
        // reset. Model cold USB power, not hot-plug during startup.
        if (!charging.machine.Reset(charging.error)) {
            bench.error = charging.error; bench.machine = std::move(charging.machine); return false;
        }
        if (!charging.Advance(3000000)) {
            bench.error = charging.error; bench.machine = std::move(charging.machine); return false;
        }
        const auto plugged = ObserveRegressionState(charging.machine);
        const bool usbInitialized = plugged.usbInitializations > before.usbInitializations &&
            (record.linked_base == 0x08003000 ?
                (plugged.usbControl & 1) && !(plugged.usbStatus & 0x3008) :
                plugged.usbControl == 0x8400 && plugged.usbStatus == 0);
        const bool charged = plugged.batteryMillivolts >= 2975 && plugged.batteryMillivolts <= 3000 &&
            plugged.adcConversions > before.adcConversions && plugged.mainIterations > before.mainIterations &&
            plugged.screenEntries[4] > before.screenEntries[4] && Visible(charging.machine) && usbInitialized;
        if (!charged) {
            bench.error = "VBUS/2987-mV did not execute native charging/ADC/scheduler/USB_Init without host traffic";
            bench.machine = std::move(charging.machine); return false;
        }
        charging.inputs.usb = false; charging.machine.SetInputs(charging.inputs);
        if (!charging.Advance(3000000)) {
            bench.error = charging.error; bench.machine = std::move(charging.machine); return false;
        }
        const auto detached = ObserveRegressionState(charging.machine);
        const bool stopped = !Visible(charging.machine) &&
            (!detached.railPowered || (!(detached.flags & 0x400) && detached.mainIterations > plugged.mainIterations));
        if (!stopped) {
            bench.error = "physical VBUS detach did not leave charging for native OFF or GPIO-held power removal";
            bench.machine = std::move(charging.machine); return false;
        }
        if (!detached.railPowered) {
            const auto instructions = charging.machine.Current().instructions;
            if (!charging.Advance(100000) || charging.machine.Current().instructions != instructions ||
                ObserveRegressionState(charging.machine).adcConversions != detached.adcConversions) {
                bench.error = "unpowered board continued executing CPU/ADC after native VBUS detach";
                bench.machine = std::move(charging.machine); return false;
            }
        }
        return true;
    });
    return ok;
}
} // namespace DeviceFunctionalMatrixDetail

inline bool RunDeviceFunctionalShard(std::string& error, std::ostream* report,
    FirmwareEdition edition, uint16_t code) {
    error.clear();
    if (edition != FirmwareEdition::Score && edition != FirmwareEdition::Factory) {
        error = "Native DRAG S functional shard requires Score or Factory"; return false;
    }
    StudioState source;
    FirmwarePackage package;
    if (!LoadEmbeddedFirmware(package, error, FirmwareModel::DragS, edition) ||
        !source.LoadPackage(std::move(package), "isolated native functional shard", error)) return false;
    const auto record = std::find_if(source.firmware.records.begin(), source.firmware.records.end(),
        [code](const HardwareRecord& record) {
            return (uint16_t(record.device_code[0]) << 8 | record.device_code[1]) == code;
        });
    if (record == source.firmware.records.end()) {
        error = "Native functional shard hardware is not embedded in the selected edition"; return false;
    }
    std::ostringstream label, failures;
    label << (edition == FirmwareEdition::Score ? "Score" : "Factory") << "/HW0x" <<
        std::hex << std::uppercase << std::setw(4) << std::setfill('0') << code;
    size_t passed = 0, failed = 0;
    const bool ok = DeviceFunctionalMatrixDetail::Row(source, *record, label.str(), failures, report, passed, failed);
    error = failures.str();
    if (report) *report << "DEVICE FUNCTIONAL SHARD " << (ok ? "OK" : "FAILED") <<
        " records=1 scenarios_passed=" << passed << " scenarios_failed_or_blocked=" << failed << '\n' << std::flush;
    return ok;
}

inline bool RunDeviceFunctionalMatrix(std::string& error, std::ostream* report = nullptr) {
    error.clear();
    std::ostringstream failures;
    size_t records = 0, passed = 0, failed = 0;
    bool ok = true;
    for (const auto edition : {FirmwareEdition::Score, FirmwareEdition::Factory}) {
        StudioState source;
        FirmwarePackage package;
        std::string detail;
        const char* editionName = edition == FirmwareEdition::Score ? "Score" : "Factory";
        if (!LoadEmbeddedFirmware(package, detail, FirmwareModel::DragS, edition) ||
            !source.LoadPackage(std::move(package), "isolated native functional matrix", detail)) {
            failures << editionName << "/package: " << detail << '\n'; ok = false;
            if (report) *report << editionName << "/package: FAIL " << detail << '\n' << std::flush;
            continue;
        }
        for (const auto& record : source.firmware.records) {
            ++records;
            const uint16_t code = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
            std::ostringstream label;
            label << editionName << "/HW0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << code;
            if (!DeviceFunctionalMatrixDetail::Row(source, record, label.str(), failures, report, passed, failed)) ok = false;
        }
    }
    if (records != 16) { failures << "Expected all 7 Score + 9 Factory records; visited " << records << '\n'; ok = false; }
    if (report) *report << "DEVICE FUNCTIONAL MATRIX " << (ok ? "OK" : "FAILED") << " records=" << records
        << " scenarios_passed=" << passed << " scenarios_failed_or_blocked=" << failed
        << " (physical inputs only; isolated in-memory Flash; no USB host enumeration)\n" << std::flush;
    error = failures.str();
    return ok;
}
