#pragma once
#include "app_state.h"
#include "firmware_device_worker.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <thread>

inline bool RunDeviceWorkerRegressions(std::string& error) {
    using namespace FirmwareDevice;
    using Clock = std::chrono::steady_clock;
    error.clear();
    const auto directory = std::filesystem::temp_directory_path() /
        ("vfs-device-worker-" + std::to_string(GetCurrentProcessId()) + "-" +
            std::to_string(Clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
    } cleanup{directory};
    std::string context;
    auto check = [&](bool condition, const std::string& message) {
        if (!condition) error = context + ": " + message;
        return condition;
    };
    auto readStorage = [](const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
    };
    auto writeStorage = [&](const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!bytes.empty()) file.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        return check(bool(file), "Cannot write isolated storage rejection fixture");
    };
    auto sameState = [](const Snapshot& a, const Snapshot& b) {
        return a.instructions == b.instructions && a.virtualMicroseconds == b.virtualMicroseconds &&
            a.pc == b.pc && a.sp == b.sp && a.sleeping == b.sleeping && a.halted == b.halted &&
            a.modeledClockHz == b.modeledClockHz && a.frameGeneration == b.frameGeneration &&
            a.registers.general == b.registers.general && a.registers.xpsr == b.registers.xpsr &&
            a.display.hardwareCode == b.display.hardwareCode && a.display.rgba == b.display.rgba &&
            a.display.spiBytes == b.display.spiBytes && a.display.pixelWrites == b.display.pixelWrites;
    };
    struct Fixture {
        StudioState first, replacement;
        FirmwareEdition edition;
    };
    std::array<Fixture, 2> fixtures;
    fixtures[0].edition = FirmwareEdition::Score;
    fixtures[1].edition = FirmwareEdition::Factory;
    for (auto& fixture : fixtures) {
        FirmwarePackage package;
        if (!LoadEmbeddedFirmware(package, error, FirmwareModel::DragS, fixture.edition) ||
            !fixture.first.LoadPackage(package, "worker native document", error) ||
            !fixture.replacement.LoadPackage(std::move(package), "worker replacement document", error)) return false;
        if (!check(fixture.first.firmware.records.size() ==
            (fixture.edition == FirmwareEdition::Score ? 7u : 9u),
            "Embedded DRAG S hardware matrix changed without worker coverage review")) return false;
    }
    auto storagePath = [&](FirmwareEdition edition, uint16_t code) {
        return directory / (std::string(FirmwareEditionName(edition)) + "-" + std::to_string(code) + ".nvram");
    };
    Worker worker;
    auto wait = [&](const StudioState& source, uint16_t code, const char* operation, auto ready,
                    bool expectError = false) {
        const auto deadline = Clock::now() + std::chrono::seconds(60);
        do {
            worker.Poll();
            const auto& view = worker.Current();
            if (view.documentIdentity == source.DocumentIdentity() &&
                view.revision == source.ResourceRevision() && view.hardwareCode == code) {
                if (!expectError && !view.error.empty())
                    return check(false, std::string(operation) + ": " + view.error);
                if (ready(view)) return true;
            } else if (view.documentIdentity) {
                return check(false, std::string(operation) + ": published a stale document/revision/hardware view");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (Clock::now() < deadline);
        return check(false, std::string(operation) + ": timed out waiting for a native worker transition");
    };
    auto loadPaused = [&](const StudioState& source, uint16_t code, const std::filesystem::path& path) {
        worker.Load(source.CreateEmulationSnapshot(), code, path, false);
        if (!check(!worker.Current().loaded && !worker.Current().documentIdentity &&
            worker.Current().state.display.rgba.empty(), "Load retained a previous device frame")) return false;
        return wait(source, code, "paused load", [](const View& view) { return view.loaded && !view.running; });
    };
    auto acknowledgeInputs = [&](const StudioState& source, uint16_t code, const Inputs& inputs) {
        const auto sequence = worker.Current().sequence;
        worker.SetInputs(inputs);
        return wait(source, code, "paused input delivery", [&](const View& view) {
            return !view.running && view.sequence > sequence;
        });
    };
    auto advanceReference = [&](Machine& reference, uint64_t target) {
        while (reference.Current().virtualMicroseconds < target) {
            const auto remaining = target - reference.Current().virtualMicroseconds;
            if (!reference.RunFor(static_cast<uint32_t>((std::min)(remaining, uint64_t(2000))), error)) {
                error = context + ": reference native execution: " + error; return false;
            }
        }
        return true;
    };
    size_t exercised = 0;
    for (auto& fixture : fixtures) {
        auto& first = fixture.first;
        for (size_t index = 0; index < first.firmware.records.size(); ++index) {
            const auto& record = first.firmware.records[index];
            const uint16_t code = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
            context = std::string(FirmwareEditionName(fixture.edition)) + " / HW " + std::to_string(code);
            const auto storage = storagePath(fixture.edition, code);
            if (!loadPaused(first, code, storage)) return false;
            const auto initial = worker.Current().state;
            if (!check(initial.instructions == 0 && initial.virtualMicroseconds == 0 &&
                initial.pc == (record.reset & ~1u) && initial.sp == record.sp &&
                initial.display.hardwareCode == code && !initial.halted,
                "Initial paused load did not preserve the actual hardware Reset vector")) return false;
            Machine reference;
            const auto initialStorage = readStorage(storage);
            if (!reference.Open(first, code, initialStorage, error)) return false;
            const auto sequence = worker.Current().sequence;
            worker.Step();
            if (!wait(first, code, "single step", [&](const View& view) { return view.sequence > sequence; }) ||
                !reference.Step(error)) return false;
            if (!check(!worker.Current().running && worker.Current().state.instructions == 1 &&
                sameState(worker.Current().state, reference.Current()),
                "Step did not execute exactly one real Reset instruction")) return false;
            Inputs held;
            held.fire = held.plus = held.minus = true;
            if (!acknowledgeInputs(first, code, held)) return false;
            reference.SetInputs(held);
            auto runAndPause = [&](uint64_t duration) {
                const auto before = worker.Current().state;
                worker.SetRunning(true);
                if (!wait(first, code, "run", [&](const View& view) {
                    return view.running && view.state.virtualMicroseconds >= before.virtualMicroseconds + duration;
                })) return false;
                const auto runningSequence = worker.Current().sequence;
                worker.SetRunning(false);
                if (!wait(first, code, "pause", [&](const View& view) {
                    return !view.running && view.sequence > runningSequence;
                })) return false;
                if (!check(worker.Current().state.instructions > before.instructions &&
                    !worker.Current().state.halted, "Run did not advance native instructions")) return false;
                if (!advanceReference(reference, worker.Current().state.virtualMicroseconds) ||
                    !check(sameState(worker.Current().state, reference.Current()),
                        "Worker input/run state differs from the real synchronous MCU at the same guest time")) return false;
                return true;
            };
            if (!runAndPause(250000)) return false;
            Inputs released;
            if (!acknowledgeInputs(first, code, released)) return false;
            reference.SetInputs(released);
            if (!runAndPause(100000)) return false;
            const auto paused = worker.Current().state;
            // Each acknowledged command is a worker-thread barrier. Stability is
            // measured in guest state, never by an exact host-time sleep.
            for (unsigned barrier = 0; barrier < 3; ++barrier) {
                if (!acknowledgeInputs(first, code, released) ||
                    !check(sameState(worker.Current().state, paused),
                        "Paused CPU/LCD advanced while processing released inputs")) return false;
            }
            const auto saved = readStorage(storage);
            if (!check(saved == reference.SaveStorage() && saved.size() == 24 + 4096 &&
                std::memcmp(saved.data(), "VFDN", 4) == 0,
                "Worker did not persist its actual native settings bytes")) return false;
            const auto resetSequence = worker.Current().sequence;
            worker.Reset();
            if (!wait(first, code, "reset", [&](const View& view) { return view.sequence > resetSequence; }) ||
                !reference.Reset(error) || !check(!worker.Current().running &&
                    sameState(worker.Current().state, reference.Current()) && readStorage(storage) == saved,
                    "Reset changed persisted settings or failed to restore the native Reset state")) return false;
            if (!loadPaused(fixture.replacement, code, storage) ||
                !check(readStorage(storage) == saved && sameState(worker.Current().state, reference.Current()),
                    "Fresh native storage reopen lost settings or retained old document execution")) return false;
            const auto accent = std::find_if(first.palette.begin(), first.palette.end(),
                [](const PaletteState& color) { return std::string_view(color.spec.id) == "accent_red"; });
            if (!check(accent != first.palette.end(), "Missing supported foreground palette edit fixture")) return false;
            first.PushUndo("Worker revision replacement");
            first.SetPaletteColor(static_cast<size_t>(accent - first.palette.begin()), Rgb8(0, 255, 0));
            first.EndUndoAction();
            const auto changedRevision = first.ResourceRevision();
            if (!loadPaused(first, code, storage) ||
                !check(worker.Current().revision == changedRevision && readStorage(storage) == saved,
                    "Edited document snapshot reused the old revision or changed native settings")) return false;
            // Supersede a live request before polling. No frame from either the
            // previous document or hardware selection may become visible.
            worker.Load(fixture.replacement.CreateEmulationSnapshot(), code, storage, false);
            if (!loadPaused(first, code, storage)) return false;
            first.Undo();
            const auto& nextRecord = first.firmware.records[(index + 1) % first.firmware.records.size()];
            const uint16_t nextCode = uint16_t(nextRecord.device_code[0]) << 8 | nextRecord.device_code[1];
            const auto nextStorage = storagePath(fixture.edition, nextCode);
            if (!loadPaused(first, nextCode, nextStorage) ||
                !check(worker.Current().state.display.hardwareCode == nextCode && readStorage(storage) == saved,
                    "Hardware switch published the old LCD or overwrote another revision's storage")) return false;
            auto reject = [&](const std::filesystem::path& bad, const std::vector<uint8_t>& bytes,
                              const StudioState& source, uint16_t targetCode) {
                if (!writeStorage(bad, bytes)) return false;
                worker.Load(source.CreateEmulationSnapshot(), targetCode, bad, false);
                if (!check(!worker.Current().loaded && worker.Current().state.display.rgba.empty(),
                    "Rejected-load request retained an old frame")) return false;
                if (!wait(source, targetCode, "storage rejection", [](const View& view) {
                    return !view.error.empty();
                }, true)) return false;
                if (!check(worker.Current().error.find(bytes.empty() ? "empty" : "Invalid/incompatible") !=
                    std::string::npos, "Storage was rejected for an unrelated native load failure: " +
                    worker.Current().error)) return false;
                return check(!worker.Current().loaded && !worker.Current().running &&
                    worker.Current().state.display.rgba.empty() && readStorage(bad) == bytes &&
                    readStorage(storage) == saved, "Invalid storage was accepted, replaced, or leaked a stale LCD");
            };
            if (!reject(directory / "wrong-hardware.nvram", saved, first, nextCode)) return false;
            const auto& other = fixtures[fixture.edition == FirmwareEdition::Score ? 1 : 0];
            const auto matching = std::find_if(other.first.firmware.records.begin(), other.first.firmware.records.end(),
                [&](const HardwareRecord& candidate) {
                    return (uint16_t(candidate.device_code[0]) << 8 | candidate.device_code[1]) == code;
                });
            const auto& otherRecord = matching == other.first.firmware.records.end() ?
                other.first.firmware.records.front() : *matching;
            const uint16_t otherCode = uint16_t(otherRecord.device_code[0]) << 8 | otherRecord.device_code[1];
            if (!reject(directory / "wrong-edition.nvram", saved, other.first, otherCode)) return false;
            auto corrupt = saved;
            corrupt.back() ^= 1;
            if (!reject(directory / "bad-crc.nvram", corrupt, first, code)) return false;
            corrupt = saved; corrupt[12] ^= 1;
            if (!reject(directory / "bad-address.nvram", corrupt, first, code) ||
                !reject(directory / "truncated.nvram", std::vector<uint8_t>(saved.begin(), saved.begin() + 23),
                    first, code) || !reject(directory / "empty.nvram", {}, first, code)) return false;
            if (!loadPaused(first, code, storage) || !check(readStorage(storage) == saved,
                "Worker could not recover by reopening valid native storage after rejection")) return false;
            ++exercised;
        }
    }
    return check(exercised == 16, "Not every Score and Factory hardware record exercised the worker lifecycle");
}
