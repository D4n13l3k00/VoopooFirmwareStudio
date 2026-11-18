#pragma once

#include "device_matrix_regressions.h"
#include <array>
#include <vector>

namespace DeviceCustomInfoRegressionDetail {
using DeviceFunctionalMatrixDetail::Bench;
using FirmwareDevice::ObserveRegressionState;
using FirmwareLogo::Animation;
constexpr size_t Pixels = size_t(FirmwareLogo::Width) * FirmwareLogo::Height;

inline uint16_t Color(int x, int y) {
    constexpr uint16_t colors[]{0x9d73, 0x4ab7, 0xe325, 0x2d69};
    return colors[((x / 7) + (y / 11)) & 3];
}
inline uint32_t Rgba(uint16_t color) {
    const uint32_t r = color >> 11, g = (color >> 5) & 63, b = color & 31;
    return 0xff000000u | (r << 3 | r >> 2) | (g << 2 | g >> 4) << 8 |
        (b << 3 | b >> 2) << 16;
}
inline FirmwareLogo::Settings Settings(bool animated) {
    FirmwareLogo::Settings settings;
    settings.enabled = true;
    settings.animation = animated ? Animation::Fade : Animation::None;
    settings.exitAnimation = animated ? Animation::SlideLeft : Animation::None;
    settings.durationMs = settings.exitDurationMs = 200;
    // Deliberately nonzero: Info must ignore holdMs and wait for stock events.
    settings.holdMs = animated ? 300 : 0;
    settings.pixels.reserve(Pixels * 2);
    for (int y = 0; y < int(FirmwareLogo::Height); ++y)
        for (int x = 0; x < int(FirmwareLogo::Width); ++x) {
            const auto color = Color(x, y);
            settings.pixels.push_back(uint8_t(color));
            settings.pixels.push_back(uint8_t(color >> 8));
        }
    return settings;
}
inline std::vector<uint32_t> Image() {
    std::vector<uint32_t> pixels;
    pixels.reserve(Pixels);
    for (int y = 0; y < int(FirmwareLogo::Height); ++y)
        for (int x = 0; x < int(FirmwareLogo::Width); ++x) pixels.push_back(Rgba(Color(x, y)));
    return pixels;
}
inline bool Black(const std::vector<uint32_t>& pixels) {
    return pixels.size() == Pixels && std::all_of(pixels.begin(), pixels.end(),
        [](uint32_t color) { return color == 0xff000000u; });
}
inline uint32_t ServicePage(const StudioState& source, const Bench& bench) {
    return ObserveRegressionState(bench.machine).historyMenu ? 5u :
        source.edition == FirmwareEdition::Score ? 4u : 7u;
}

// Observe the real service entry before its animation completes. In particular,
// do not hold all three buttons for a fixed second and then inspect only the tail.
inline bool Enter(Bench& bench, uint64_t& enteredAt) {
    const auto before = ObserveRegressionState(bench.machine);
    if (before.historyMenu) {
        if (!bench.Hold(false, true, true, 1150000, 300000) ||
            !bench.Require(ObserveRegressionState(bench.machine).page == 5,
                "physical Plus+Minus did not open native history")) return false;
        bench.inputs.fire = true;
        bench.machine.SetInputs(bench.inputs);
        if (!bench.Advance(120000)) return false;
        bench.inputs.fire = false; // Native history toggles CHIP ID on release.
    } else {
        bench.inputs.fire = bench.inputs.plus = bench.inputs.minus = true;
    }
    bench.machine.SetInputs(bench.inputs);
    const auto deadline = bench.machine.Current().virtualMicroseconds + 2500000;
    while (bench.machine.Current().virtualMicroseconds < deadline) {
        if (!bench.Advance(1000)) return false;
        const auto state = ObserveRegressionState(bench.machine);
        if (state.serviceEntries > before.serviceEntries) {
            enteredAt = bench.machine.Current().virtualMicroseconds;
            bench.inputs.fire = bench.inputs.plus = bench.inputs.minus = false;
            bench.machine.SetInputs(bench.inputs);
            return bench.Require(state.serviceEntries == before.serviceEntries + 1 &&
                state.bootCompletions == before.bootCompletions,
                "physical service entry repeated or reset the firmware");
        }
    }
    return bench.Require(false, "physical gesture never executed the native CHIP ID entry");
}
inline bool ExitHistory(Bench& bench) {
    const auto before = ObserveRegressionState(bench.machine);
    if (!bench.Hold(true, false, false, 1600000, 3000000)) return false;
    const auto after = ObserveRegressionState(bench.machine);
    return bench.Require(after.page == 1 && after.screenEntries[0] > before.screenEntries[0] &&
        after.bootCompletions == before.bootCompletions && DeviceFunctionalMatrixDetail::Visible(bench.machine),
        "native long-Fire did not return history to usable Main");
}

// Independent pixel oracle for the configured Fade entrance and SlideLeft exit.
// Complete rows remain observable while the next row is being transferred over
// SPI, even when raster time overruns a 12.5-ms animation phase deadline.
inline uint32_t RowPixel(bool exiting, unsigned visibility, int x) {
    constexpr int y = 79;
    if (exiting) {
        const int sourceX = x + (16 - int(visibility)) * 5;
        return sourceX < int(FirmwareLogo::Width) ? Rgba(Color(sourceX, y)) : 0xff000000u;
    }
    const uint16_t color = Color(x, y);
    const uint16_t faded = uint16_t((((color >> 11) * visibility) >> 4) << 11 |
        (((((color >> 5) & 63) * visibility) >> 4) << 5) | ((color & 31) * visibility >> 4));
    return Rgba(faded);
}
inline void ObserveRows(const std::vector<uint32_t>& pixels, bool exiting, std::array<bool, 17>& seen) {
    if (pixels.size() != Pixels) return;
    for (unsigned visibility = 1; visibility < 16; ++visibility) {
        bool matches = true;
        for (int x = 0; x < int(FirmwareLogo::Width); ++x)
            if (pixels[size_t(79) * FirmwareLogo::Width + x] != RowPixel(exiting, visibility, x)) {
                matches = false;
                break;
            }
        if (matches) seen[visibility] = true;
    }
}
inline bool StaticEntry(const StudioState& source, Bench& bench, const std::vector<uint32_t>& image,
    uint64_t& enteredAt) {
    if (!Enter(bench, enteredAt)) return false;
    const auto deadline = enteredAt + 2000000;
    std::array<bool, 17> fade{};
    while (bench.machine.Current().display.rgba != image &&
        bench.machine.Current().virtualMicroseconds < deadline) {
        ObserveRows(bench.machine.Current().display.rgba, false, fade);
        if (!bench.Advance(1000)) return false;
    }
    if (!bench.Require(bench.machine.Current().display.rgba == image &&
        std::count(fade.begin(), fade.end(), true) == 0,
        "static CHIP ID omitted the exact RGB565 image or unexpectedly animated Fade")) return false;
    const auto entries = ObserveRegressionState(bench.machine).serviceEntries;
    const auto stableUntil = bench.machine.Current().virtualMicroseconds + 1200000;
    while (bench.machine.Current().virtualMicroseconds < stableUntil) {
        if (!bench.Advance(10000)) return false;
        if (!bench.Require(bench.machine.Current().display.rgba == image,
            "native caller redrew stock CHIP ID over the static custom image")) return false;
    }
    const auto state = ObserveRegressionState(bench.machine);
    return bench.Require(state.page == ServicePage(source, bench) && state.serviceEntries == entries,
        "static service did not retain its native page after custom drawing returned");
}
inline bool AnimatedEntry(const StudioState& source, Bench& bench, const std::vector<uint32_t>& image,
    uint64_t& enteredAt, uint64_t nativeTimeout = 0) {
    if (!Enter(bench, enteredAt)) return false;
    std::array<bool, 17> entrance{};
    uint64_t firstFull = 0;
    const auto before = ObserveRegressionState(bench.machine);
    const auto deadline = enteredAt + 5000000;
    while (bench.machine.Current().virtualMicroseconds < deadline) {
        const auto& snapshot = bench.machine.Current();
        if (snapshot.display.rgba == image) {
            firstFull = snapshot.virtualMicroseconds;
            break;
        }
        ObserveRows(snapshot.display.rgba, false, entrance);
        if (!bench.Advance(1000)) return false;
    }
    if (!bench.Require(firstFull && std::count(entrance.begin(), entrance.end(), true) >= 3 &&
        firstFull + 1000 >= enteredAt + 180000,
        "physical service omitted configured Fade phases or the full custom image")) return false;
    const auto servicePage = ServicePage(source, bench);
    while (ObserveRegressionState(bench.machine).page != servicePage &&
        bench.machine.Current().virtualMicroseconds < firstFull + 500000) {
        if (!bench.Advance(1000)) return false;
        if (!bench.Require(bench.machine.Current().display.rgba == image,
            "custom image changed before the entrance returned to its native caller")) return false;
    }
    if (!bench.Require(ObserveRegressionState(bench.machine).page == servicePage,
        "Fade entrance never returned control to the native service page")) return false;
    const auto stableUntil = nativeTimeout ? (std::min)(bench.machine.Current().virtualMicroseconds + 1200000,
        enteredAt + nativeTimeout - (std::min)(nativeTimeout, uint64_t(20000))) :
        bench.machine.Current().virtualMicroseconds + 1200000;
    while (bench.machine.Current().virtualMicroseconds < stableUntil) {
        if (!bench.Advance(1000)) return false;
        const auto state = ObserveRegressionState(bench.machine);
        if (!bench.Require(bench.machine.Current().display.rgba == image &&
            state.page == servicePage && state.serviceEntries == before.serviceEntries &&
            state.bootCompletions == before.bootCompletions,
            "Info auto-exited/restarted after holdMs instead of retaining the custom image")) return false;
    }
    return bench.Require(ObserveRegressionState(bench.machine).page == ServicePage(source, bench),
        "Fade entrance did not return control to the native service page");
}

enum class CloseKind { Fire, Timeout, HistoryToggle };

inline bool Close(Bench& bench, const std::vector<uint32_t>& image,
    bool animated, CloseKind kind, uint64_t enteredAt, bool& blackObserved, uint64_t nativeTimeout = 0) {
    const auto before = ObserveRegressionState(bench.machine);
    const uint64_t startedAt = bench.machine.Current().virtualMicroseconds;
    const uint64_t earliestTimeout = enteredAt + nativeTimeout - (std::min)(nativeTimeout, uint64_t(50000));
    const uint64_t deadline = kind == CloseKind::Timeout ? enteredAt + 40000000u : startedAt + 5000000u;
    std::array<bool, 17> exit{}, fade{};
    uint64_t firstExit = 0, terminalAt = 0;
    bool released = kind == CloseKind::Timeout, returned = false, nativeAfterTerminal = false;
    blackObserved = false;
    bench.inputs.fire = !released;
    bench.machine.SetInputs(bench.inputs);
    while (bench.machine.Current().virtualMicroseconds < deadline) {
        if (!bench.Advance(1000)) return false;
        const auto& snapshot = bench.machine.Current();
        const auto state = ObserveRegressionState(bench.machine);
        const auto& pixels = snapshot.display.rgba;
        if (!released && kind == CloseKind::HistoryToggle && snapshot.virtualMicroseconds >= startedAt + 120000) {
            bench.inputs.fire = false;
            bench.machine.SetInputs(bench.inputs);
            released = true;
        }
        if (!firstExit && pixels != image) {
            firstExit = snapshot.virtualMicroseconds;
            if (!bench.Require(kind != CloseKind::Timeout || (nativeTimeout &&
                firstExit >= earliestTimeout && firstExit <= enteredAt + nativeTimeout + 50000),
                "custom Info close diverged from the unmodified firmware's inactivity event")) return false;
            if (!bench.Require(kind != CloseKind::HistoryToggle || released,
                "history CHIP ID closed on Fire press instead of short release")) return false;
            if (!released) {
                bench.inputs.fire = false;
                bench.machine.SetInputs(bench.inputs);
                released = true;
            }
        }
        // The zero-visibility raster may be replaced by Main within one sample.
        // Require the exact visibility-one row before allowing any stock pixels;
        // a sampled black frame is evidence, not a new black-hold requirement.
        if (animated && firstExit) {
            if (!terminalAt) {
                ObserveRows(pixels, true, exit);
                if (exit[1]) terminalAt = snapshot.virtualMicroseconds;
            }
            const bool nativePixels = std::any_of(pixels.begin(), pixels.end(), [](uint32_t pixel) {
                return pixel != 0xff000000u && pixel != Rgba(0x9d73) && pixel != Rgba(0x4ab7) &&
                    pixel != Rgba(0xe325) && pixel != Rgba(0x2d69);
            });
            if (!bench.Require(!nativePixels || terminalAt,
                "native redraw preceded the exact near-terminal SlideLeft row")) return false;
            if (terminalAt) {
                blackObserved = blackObserved || Black(pixels);
                nativeAfterTerminal = nativeAfterTerminal || nativePixels ||
                    state.screenEntries[0] > before.screenEntries[0];
            }
        }
        if (!animated && firstExit) {
            ObserveRows(pixels, true, exit);
            ObserveRows(pixels, false, fade);
        }
        if (kind == CloseKind::HistoryToggle) {
            returned = terminalAt && nativeAfterTerminal && state.page == 5 && !Black(pixels) && pixels != image;
        } else {
            returned = state.page == 1 && state.screenEntries[0] > before.screenEntries[0];
        }
        if (returned) break;
    }
    bench.inputs.fire = false;
    bench.machine.SetInputs(bench.inputs);
    if (!animated && !bench.Require(std::count(exit.begin(), exit.end(), true) == 0 &&
        std::count(fade.begin(), fade.end(), true) == 0,
        "static CHIP ID unexpectedly animated its native close")) return false;
    if (!bench.Require(firstExit && returned &&
        (!animated || (terminalAt && std::count(exit.begin(), exit.end(), true) >= 3 &&
            terminalAt + 1000 >= firstExit + 175000 && (blackObserved || nativeAfterTerminal))),
        "native close omitted exact SlideLeft phases/visibility-one row or its real native return")) return false;
    if (kind == CloseKind::HistoryToggle) {
        if (!bench.Advance(500000) || !ExitHistory(bench)) return false;
    } else if (kind == CloseKind::Timeout && !DeviceFunctionalMatrixDetail::Visible(bench.machine)) {
        // RTOS history sleeps before drawing Main. Wake through the physical
        // switch, never Reset, a host-selected page, or a guest SRAM write.
        if (!bench.Hold(true, false, false, 70000, 500000)) return false;
    }
    const auto stableUntil = bench.machine.Current().virtualMicroseconds + 1200000;
    while (bench.machine.Current().virtualMicroseconds < stableUntil) {
        if (!bench.Advance(10000)) return false;
        const auto state = ObserveRegressionState(bench.machine);
        if (!bench.Require(state.serviceEntries == before.serviceEntries &&
            state.bootCompletions == before.bootCompletions,
            "custom Info restarted automatically or reset instead of remaining closed")) return false;
    }
    const auto after = ObserveRegressionState(bench.machine);
    return bench.Require(after.page == 1 && after.mainIterations > before.mainIterations &&
        DeviceFunctionalMatrixDetail::Visible(bench.machine) && bench.machine.Current().display.rgba != image,
        "native close/wake did not leave an executing, visible Main for physical reentry");
}
// Compare against stock rather than imposing a fresh host timer on reentry.
inline bool NativeTimeouts(const StudioState& source, uint16_t code,
    std::array<uint64_t, 2>& delays, std::string& error) {
    Bench bench;
    if (!bench.machine.Open(source, code, {}, bench.error) || !bench.Advance(250000) || !bench.Boot()) {
        error = bench.error; return false;
    }
    for (size_t entry = 0; entry < delays.size(); ++entry) {
        uint64_t enteredAt = 0;
        if (!Enter(bench, enteredAt) || !bench.Advance(350000)) { error = bench.error; return false; }
        const auto before = ObserveRegressionState(bench.machine);
        if (!bench.Require(before.page == ServicePage(source, bench),
            "unmodified firmware did not retain its native service after entry")) { error = bench.error; return false; }
        bool returned = false;
        while (bench.machine.Current().virtualMicroseconds < enteredAt + 40000000) {
            if (!bench.Advance(1000)) { error = bench.error; return false; }
            const auto state = ObserveRegressionState(bench.machine);
            if (!delays[entry] && (state.screenEntries[0] > before.screenEntries[0] ||
                !DeviceFunctionalMatrixDetail::Visible(bench.machine)))
                delays[entry] = bench.machine.Current().virtualMicroseconds - enteredAt;
            if (state.page == 1 && state.screenEntries[0] > before.screenEntries[0]) { returned = true; break; }
        }
        if (!bench.Require(delays[entry] && returned,
            "unmodified firmware never completed its native service timeout")) { error = bench.error; return false; }
        if (!DeviceFunctionalMatrixDetail::Visible(bench.machine) &&
            !bench.Hold(true, false, false, 70000, 500000)) { error = bench.error; return false; }
        if (!bench.Advance(1200000)) { error = bench.error; return false; }
    }
    return true;
}

} // namespace DeviceCustomInfoRegressionDetail

inline bool RunDeviceCustomInfoRegressions(std::string& error, std::ostream* report = nullptr) {
    using namespace DeviceCustomInfoRegressionDetail;
    error.clear();
    std::ostringstream failures;
    size_t records = 0, passed = 0, failed = 0;
    const auto image = Image();
    for (const auto edition : {FirmwareEdition::Score, FirmwareEdition::Factory}) {
        StudioState source;
        FirmwarePackage package;
        std::string detail;
        if (!LoadEmbeddedFirmware(package, detail, FirmwareModel::DragS, edition) ||
            !source.LoadPackage(std::move(package), "physical custom CHIP ID regressions", detail)) {
            failures << "custom CHIP ID/package: " << detail << '\n';
            ++failed;
            continue;
        }
        for (const auto& record : source.firmware.records) {
            ++records;
            const uint16_t code = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
            source.SetScreenPatch(FirmwareLogo::Target::DeviceInfo, {});
            std::array<uint64_t, 2> nativeTimeouts{};
            if (!NativeTimeouts(source, code, nativeTimeouts, detail)) {
                failures << "unmodified native timeout reference/HW" << code << ": " << detail << '\n';
                ++failed; continue;
            }
            for (const bool animated : {false, true}) {
                source.SetScreenPatch(FirmwareLogo::Target::DeviceInfo, Settings(animated));
                for (const auto kind : {CloseKind::Fire, CloseKind::Timeout, CloseKind::HistoryToggle}) {
                    if (!animated && kind != CloseKind::Fire) continue;
                    if (kind == CloseKind::HistoryToggle && code != 0x3013 && code != 0x3014) continue;
                    Bench bench;
                    std::ostringstream label;
                    label << (edition == FirmwareEdition::Score ? "Score" : "Factory") << "/HW0x" <<
                        std::hex << std::uppercase << std::setw(4) << std::setfill('0') << code <<
                        (animated ? "/custom-chip-id-fade-persistent-slide" : "/custom-chip-id-static") <<
                        (kind == CloseKind::Fire ? "/long-fire" :
                            kind == CloseKind::Timeout ? "/stock-timeout" : "/short-fire-history");
                    bool ok = bench.machine.Open(source, code, {}, bench.error) && bench.Advance(250000) && bench.Boot();
                    std::array<bool, 2> observedBlack{};
                    for (unsigned entry = 0; ok && entry < 2; ++entry) {
                        uint64_t enteredAt = 0;
                        const uint64_t nativeTimeout = kind == CloseKind::Timeout ? nativeTimeouts[entry] : 0;
                        ok = (animated ? AnimatedEntry(source, bench, image, enteredAt, nativeTimeout) :
                            StaticEntry(source, bench, image, enteredAt)) &&
                            Close(bench, image, animated, kind, enteredAt, observedBlack[entry], nativeTimeout);
                    }
                    if (ok) {
                        ++passed;
                        if (report) {
                            *report << label.str() << ": PASS physical entry/close/reentry, exact custom pixels";
                            if (animated) {
                                *report << ", Fade/persistent native wait/SlideLeft>=3 phases including visibility1";
                                for (size_t entry = 0; entry < observedBlack.size(); ++entry)
                                    *report << ", close" << entry + 1 << '=' <<
                                        (observedBlack[entry] ? "black-observed" : "native-after-visibility1");
                            } else *report << ", persistent static image";
                            *report << '\n' << std::flush;
                        }
                    } else {
                        ++failed;
                        const auto diagnostic = bench.error + "; " + FirmwareDevice::DescribeRegressionState(bench.machine);
                        failures << label.str() << ": " << diagnostic << '\n';
                        if (report) *report << label.str() << ": FAIL " << diagnostic << '\n' << std::flush;
                    }
                }
            }
        }
    }
    if (records != 16) { failures << "Expected all 7 Score + 9 Factory custom service records; visited " << records << '\n'; ++failed; }
    if (report) *report << "DEVICE CUSTOM CHIP ID " << (failed ? "FAILED" : "OK") << " records=" << records <<
        " scenarios_passed=" << passed << " scenarios_failed=" << failed <<
        " (physical buttons, exported Flash, native LCD/DWT; no PC/RAM writes or direct-renderer entry)\n" << std::flush;
    error = failures.str();
    return failed == 0;
}
