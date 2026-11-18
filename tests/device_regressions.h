#pragma once

#include "app_state.h"
#include "firmware_device.h"
#include "firmware_device_regression.h"
#include <algorithm>
#include <ostream>
#include <sstream>
#include "device_matrix_regressions.h"

namespace FirmwareDevice {
bool RunFaultRegressions(Machine&, std::string&);
std::string DescribeRegressionState(const Machine&);
}

inline bool RunDeviceRevisionRegressions(std::string& error, std::ostream* report = nullptr) {
    using namespace FirmwareDevice;
    error.clear();
    StudioState source;
    FirmwarePackage package;
    if (!LoadEmbeddedFirmware(package, error, FirmwareModel::DragS, FirmwareEdition::Score) ||
        !source.LoadPackage(std::move(package), "native revision regression", error)) return false;
    for (const auto& record : source.firmware.records) {
        const uint16_t code = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
        Machine machine;
        auto advance = [&](uint32_t duration) {
            while (duration) {
                const uint32_t slice = (std::min)(duration, 2000u);
                if (!machine.RunFor(slice, error)) {
                    error = "HW " + std::to_string(code) + ": " + error;
                    return false;
                }
                duration -= slice;
            }
            return true;
        };
        if (!machine.Open(source, code, {}, error) || !advance(250000)) return false;
        Inputs keys;
        for (unsigned click = 0; click < 5; ++click) {
            keys.fire = true; machine.SetInputs(keys); if (!advance(70000)) return false;
            keys.fire = false; machine.SetInputs(keys); if (!advance(70000)) return false;
        }
        if (!advance(8000000)) return false;
        const auto& snapshot = machine.Current();
        const uint32_t expectedClock = record.linked_base == 0x08003000 ? 96000000 : 72000000;
        if (snapshot.halted || snapshot.modeledClockHz != expectedClock ||
            snapshot.display.pixelWrites < 80 * 160 || snapshot.display.spiBytes < 80 * 160 * 2 ||
            !std::any_of(snapshot.display.rgba.begin(), snapshot.display.rgba.end(),
                [](uint32_t pixel) { return pixel != 0xff000000u; })) {
            error = "Native revision did not finish physical five-click LCD boot: HW " + std::to_string(code);
            return false;
        }
        if (report) *report << "HW " << std::hex << code << std::dec << ": native reset/five-click LCD, "
            << snapshot.modeledClockHz << " Hz, " << snapshot.display.pixelWrites << " pixel writes\n";
    }
    if (report) *report << "DEVICE REVISION REGRESSIONS OK\n";
    return true;
}

inline bool RunDeviceRegressions(std::string& error, std::ostream* report = nullptr) {
    using namespace FirmwareDevice;
    error.clear();
    StudioState source;
    FirmwarePackage package;
    if (!LoadEmbeddedFirmware(package, error, FirmwareModel::DragS, FirmwareEdition::Score) ||
        !source.LoadPackage(std::move(package), "native device regression", error)) return false;
    auto fail = [&](const std::string& message) { error = message; return false; };
    auto advance = [&](Machine& machine, uint32_t duration, uint32_t quantum = 2000) {
        while (duration) {
            const uint32_t slice = (std::min)(duration, quantum);
            if (!machine.RunFor(slice, error)) return false;
            duration -= slice;
        }
        return true;
    };
    auto same = [](const Snapshot& a, const Snapshot& b) {
        return a.virtualMicroseconds == b.virtualMicroseconds && a.instructions == b.instructions &&
            a.pc == b.pc && a.sp == b.sp && a.sleeping == b.sleeping && a.halted == b.halted &&
            a.frameGeneration == b.frameGeneration && a.display.rgba == b.display.rgba &&
            a.display.spiBytes == b.display.spiBytes && a.display.pixelWrites == b.display.pixelWrites;
    };
    auto mismatch = [](const Snapshot& a, const Snapshot& b) {
        std::ostringstream text;
        auto dump = [&](const char* label, const Snapshot& s) {
            text << label << "{us=" << s.virtualMicroseconds << ", instructions=" << s.instructions
                << ", pc=0x" << std::hex << s.pc << ", sp=0x" << s.sp << std::dec
                << ", sleeping=" << s.sleeping << ", halted=" << s.halted << ", generation=" << s.frameGeneration
                << ", spi=" << s.display.spiBytes << ", pixels=" << s.display.pixelWrites << "}";
            if (!s.recentAccess.empty()) {
                const auto& access = s.recentAccess.back();
                text << " last MMIO{us=" << access.virtualMicroseconds << ", pc=0x" << std::hex << access.pc
                    << ", address=0x" << access.address << ", value=0x" << access.value << std::dec
                    << ", size=" << unsigned(access.size) << ", write=" << access.write << "}";
            }
        };
        dump("2000-us ", a); text << "; "; dump("split ", b);
        if (a.display.rgba != b.display.rgba) {
            const size_t count = (std::min)(a.display.rgba.size(), b.display.rgba.size());
            size_t pixel = 0;
            while (pixel < count && a.display.rgba[pixel] == b.display.rgba[pixel]) ++pixel;
            text << "; first differing pixel=" << pixel;
        }
        return text.str();
    };
    auto visible = [](const Snapshot& state) {
        return std::any_of(state.display.rgba.begin(), state.display.rgba.end(), [](uint32_t pixel) { return pixel != 0xff000000u; });
    };
    Inputs keys;
    Machine device, split;
    auto advanceTogether = [&](uint32_t duration, uint32_t splitQuantum) {
        while (duration) {
            const uint32_t slice = (std::min)(duration, 2000u);
            if (!advance(device, slice) || !advance(split, slice, splitQuantum)) return false;
            if (!same(device.Current(), split.Current()))
                return fail("First native partition divergence at a shared 2000-us checkpoint: " + mismatch(device.Current(), split.Current()));
            duration -= slice;
        }
        return true;
    };
    if (!device.Open(source, {}, error) || !split.Open(source, {}, error)) return false;
    if (device.Current().pc != 0x08005ab0 || device.Current().sp != 0x20003820 ||
        device.Current().instructions || device.Current().virtualMicroseconds || visible(device.Current()))
        return fail("Device did not begin at the real HW3103 reset vector with untouched SRAM/LCD");
    if (!advanceTogether(250000, 137)) return false;
    if (!same(device.Current(), split.Current())) return fail("Native reset execution depends on RunFor partitioning: " + mismatch(device.Current(), split.Current()));
    if (device.Current().instructions == 0 || device.Current().pc == 0x08005ab0 || device.Current().halted)
        return fail("Real reset/startup/scheduler did not execute");
    // No screen selection or guest-RAM writes: real 10-ms TIM2 debounce and
    // five-click power logic consume the exact same physical waveform.
    for (unsigned click = 0; click < 5; ++click) {
        keys.fire = true; device.SetInputs(keys); split.SetInputs(keys);
        if (!advanceTogether(70000, 137)) return false;
        keys.fire = false; device.SetInputs(keys); split.SetInputs(keys);
        if (!advanceTogether(70000, 137)) return false;
    }
    if (!advanceTogether(3000000, 997)) return false;
    // The stock logo really executes ~81.6 million delay-loop instructions,
    // followed by a 2-second RTOS wait. Changing pixels is not proof of Main.
    for (unsigned slice = 0; slice < 800; ++slice) {
        const auto observed = ObserveRegressionState(device);
        if (observed.bootCompletions && observed.page == 1 && (observed.flags & 0x20400) == 0x20400) break;
        if (!advanceTogether(10000, 997)) return false;
    }
    const auto booted = ObserveRegressionState(device);
    if (!booted.bootCompletions || booted.page != 1 || (booted.flags & 0x20400) != 0x20400)
        return fail("Native boot did not finish its animation/delay and enter the real Main loop: " + DescribeRegressionState(device));
    if (!same(device.Current(), split.Current())) return fail("Button/interrupt/LCD execution depends on RunFor partitioning: " + mismatch(device.Current(), split.Current()));
    if (!visible(device.Current()) || device.Current().display.pixelWrites < 80 * 160 ||
        device.Current().display.spiBytes < 80 * 160 * 2 || device.Current().frameGeneration == 0)
        return fail("Native five-click power-on did not initialize and draw the physical LCD");
    if (report) *report << "native reset/five-click boot: " << device.Current().virtualMicroseconds << " us, "
        << device.Current().instructions << " instructions, " << device.Current().display.spiBytes << " SPI bytes\n";
    auto activePower = [](const RegressionObservation& state) {
        return state.mode == 2 || state.mode == 3 ? state.powerB : state.powerA;
    };
    auto powerClick = [&](bool plus) {
        keys.plus = plus; keys.minus = !plus; device.SetInputs(keys);
        if (!advance(device, 120000)) return false;
        keys.plus = keys.minus = false; device.SetInputs(keys);
        return advance(device, 500000);
    };
    // Smart mode may start at its coil-dependent ceiling. Reach an interior
    // setpoint using physical switches, never by changing guest settings/RAM.
    auto power = ObserveRegressionState(device);
    if (power.powerLimit <= 6) return fail("Native power fixture has no adjustable interior setpoint");
    if (activePower(power) >= power.powerLimit && !powerClick(false)) return false;
    power = ObserveRegressionState(device);
    if (activePower(power) <= 5 && !powerClick(true)) return false;
    const auto beforePlus = ObserveRegressionState(device);
    if (activePower(beforePlus) <= 5 || activePower(beforePlus) >= beforePlus.powerLimit)
        return fail("Physical keys did not reach an adjustable native power setpoint: " + DescribeRegressionState(device));
    const auto mainPixels = device.Current().display.rgba;
    if (!powerClick(true)) return false;
    const auto afterPlus = ObserveRegressionState(device);
    if (afterPlus.mode != beforePlus.mode || activePower(afterPlus) <= activePower(beforePlus) ||
        device.Current().display.rgba == mainPixels)
        return fail("Physical Plus did not increase native power and update Main: " + DescribeRegressionState(device));
    const auto plusPixels = device.Current().display.rgba;
    if (!powerClick(false)) return false;
    const auto afterMinus = ObserveRegressionState(device);
    if (afterMinus.mode != afterPlus.mode || activePower(afterMinus) >= activePower(afterPlus) ||
        device.Current().display.rgba == plusPixels || afterMinus.page != 1)
        return fail("Physical Minus did not decrease native power and update Main: " + DescribeRegressionState(device));
    // HW3103's all-three-button hold opens its actual Chip ID service page.
    // It is not one of the newer models' host-selected generic menu screens.
    const auto beforeMenu = device.Current().display.rgba;
    const auto beforeService = ObserveRegressionState(device);
    keys.fire = keys.plus = keys.minus = true; device.SetInputs(keys);
    bool sawServicePage = false;
    for (unsigned sample = 0; sample < 1200; ++sample) {
        if (!advance(device, 1000)) return false;
        const auto service = ObserveRegressionState(device);
        if (service.serviceEntries > beforeService.serviceEntries && service.page == 4 &&
            device.Current().display.rgba != beforeMenu) {
            sawServicePage = true;
            break;
        }
    }
    if (!sawServicePage || !device.Current().display.emulatedUid)
        return fail("Physical all-three-button gesture did not execute and display the native Chip ID page: " + DescribeRegressionState(device));
    keys.fire = keys.plus = keys.minus = false; device.SetInputs(keys);
    if (!advance(device, 300000)) return false;
    // The native Fire handler (0x08006710 -> 0x08007E68 -> 0x0800F188)
    // dismisses Chip ID after its non-Main 150-sample (1.5-second) hold.
    keys.fire = true; device.SetInputs(keys);
    if (!advance(device, 1600000)) return false;
    keys.fire = false; device.SetInputs(keys);
    if (!advance(device, 3000000)) return false;
    if (report) *report << "after native service page: " << DescribeRegressionState(device) << '\n';
    const auto afterService = ObserveRegressionState(device);
    if (afterService.page != 1)
        return fail("Physical Fire did not return from native Chip ID to Main: " + DescribeRegressionState(device));
    // Execute the real power-off path, including settings save and idle/WFI.
    for (unsigned click = 0; click < 5; ++click) {
        keys.fire = true; device.SetInputs(keys); if (!advance(device, 70000)) return false;
        keys.fire = false; device.SetInputs(keys); if (!advance(device, 70000)) return false;
    }
    if (!advance(device, 3000000)) return false;
    if (ObserveRegressionState(device).powerOffEntries <= afterService.powerOffEntries ||
        (ObserveRegressionState(device).railPowered && (ObserveRegressionState(device).flags & 0x400)))
        return fail("Native five-click power-off did not save and reach the actual off/reset path: " + DescribeRegressionState(device));
    const auto saved = device.SaveStorage();
    if (saved.size() != 24 + 4096 || device.StorageRevision() == 0 ||
        std::all_of(saved.begin() + 24, saved.end(), [](uint8_t byte) { return byte == 0xff; }))
        return fail("Native settings erase/program did not produce nonvolatile data: " + DescribeRegressionState(device));
    Machine reopened;
    if (!reopened.Open(source, saved, error) || reopened.SaveStorage() != saved) return fail("Settings Flash did not survive a fresh Machine Open: " + error);
    if (!reopened.Reset(error) || reopened.SaveStorage() != saved || reopened.StorageRevision() != 0)
        return fail("Reset changed nonvolatile settings without a native program/erase");
    auto corrupted = saved; corrupted.back() ^= 1;
    Machine invalid;
    std::string rejected;
    if (invalid.Open(source, corrupted, rejected) || rejected.empty()) return fail("Corrupt settings storage was accepted");
    corrupted = saved; corrupted[12] ^= 1; // Never permit persistence to redirect its Flash destination.
    if (invalid.Open(source, corrupted, rejected)) return fail("Settings storage permitted arbitrary Flash/code override");
    const uint64_t beforeTime = device.Current().virtualMicroseconds;
    if (!advance(device, 35000000)) return false;
    if (device.Current().virtualMicroseconds != beforeTime + 35000000 || device.Current().halted)
        return fail("Native sleep/idle time stopped or faulted");
    if (ObserveRegressionState(device).railPowered && (ObserveRegressionState(device).flags & 0x400))
        return fail("Native idle unexpectedly powered itself on without a physical button edge");
    const uint64_t beforeWake = device.Current().display.spiBytes;
    const uint64_t beforeBoots = ObserveRegressionState(device).bootCompletions;
    for (unsigned click = 0; click < 5; ++click) {
        keys.fire = true; device.SetInputs(keys); if (!advance(device, 70000)) return false;
        keys.fire = false; device.SetInputs(keys); if (!advance(device, 70000)) return false;
    }
    if (!advance(device, 3000000)) return false;
    for (unsigned slice = 0; slice < 800; ++slice) {
        const auto observed = ObserveRegressionState(device);
        if (observed.bootCompletions > beforeBoots && observed.page == 1 && (observed.flags & 0x20400) == 0x20400) break;
        if (!advance(device, 10000)) return false;
    }
    if (ObserveRegressionState(device).bootCompletions <= beforeBoots ||
        (ObserveRegressionState(device).flags & 0x20400) != 0x20400)
        return fail("Native Fire wake did not finish boot and return to its real Main loop: " + DescribeRegressionState(device));
    if (device.Current().display.spiBytes <= beforeWake || !visible(device.Current())) return fail("Native Fire wake/power-on did not restore the panel");
    if (!RunFaultRegressions(reopened, error)) return false;
    if (!RunDiagnosticsRegressions(reopened, error)) return false;
    Machine lowBase;
    const auto lowRecord = std::find_if(source.firmware.records.begin(), source.firmware.records.end(),
        [](const HardwareRecord& record) { return record.linked_base == 0x08003000; });
    if (lowRecord != source.firmware.records.end()) {
        const uint16_t code = uint16_t(lowRecord->device_code[0]) << 8 | lowRecord->device_code[1];
        if (!lowBase.Open(source, code, {}, error) || !RunDiagnosticsRegressions(lowBase, error)) return false;
    }
    if (report) *report << "native diagnostics: real register bank, active MSP/PSP, bounded SRAM stack and invalid-pointer safety\n";
    if (reopened.SaveStorage() != saved) return fail("Fault diagnostics modified persisted settings");
    if (report) *report << "native buttons/menu/sleep/wake and versioned settings reopen: exercised\n";
    // Reproduce the user's electrical inputs through GPIO/ADC and the real
    // scheduler, including USB_Init -> PowerOn's STR at 0x0800AA18. VBUS alone
    // supplies charging power; it does not invent a USB host or enumeration.
    Machine charging;
    if (!charging.Open(source, {}, error) || !advance(charging, 250000)) return false;
    Inputs chargingInputs;
    chargingInputs.usb = true;
    chargingInputs.batteryMillivolts = 2987;
    const auto beforeUsb = ObserveRegressionState(charging);
    charging.SetInputs(chargingInputs);
    if (!advance(charging, 2000000)) return false;
    const auto lowBatteryUsb = ObserveRegressionState(charging);
    if (lowBatteryUsb.usbInitializations <= beforeUsb.usbInitializations ||
        lowBatteryUsb.batteryMillivolts < 2980 || lowBatteryUsb.batteryMillivolts > 2990 ||
        lowBatteryUsb.usbControl != 0x8400 || lowBatteryUsb.usbStatus != 0 ||
        lowBatteryUsb.mainIterations <= beforeUsb.mainIterations || charging.Current().halted)
        return fail("Native USB/2987-mV charging path did not initialize USB and keep scheduling: " + DescribeRegressionState(charging));
    const auto chargingIterations = lowBatteryUsb.mainIterations;
    if (!advance(charging, 1000000) || ObserveRegressionState(charging).mainIterations <= chargingIterations)
        return fail("Native low-battery USB path stopped scheduling: " + DescribeRegressionState(charging));
    if (report) *report << "native USB VBUS/2987-mV battery: real PowerOn/USB_Init, no synthetic host traffic\n";
    auto record = std::find_if(source.firmware.records.begin(), source.firmware.records.end(), [](const HardwareRecord& r) {
        return r.device_code[0] == 0x31 && r.device_code[1] == 3;
    });
    if (record == source.firmware.records.end()) return fail("Missing HW3103 executable guard fixture");
    const uint8_t instruction = record->image[0x2b2];
    record->image[0x2b2] ^= 2;
    const bool acceptedCode = invalid.Open(source, {}, rejected);
    record->image[0x2b2] = instruction;
    if (acceptedCode || rejected.empty()) return fail("Unknown executable modification bypassed native device guard");
    return true;
}
