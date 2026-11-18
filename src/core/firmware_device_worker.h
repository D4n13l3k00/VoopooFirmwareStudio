#pragma once
#include "firmware_device.h"
#include <filesystem>
#include <memory>

struct StudioState;

namespace FirmwareDevice {
struct View {
    Snapshot state;
    uint64_t documentIdentity = 0;
    uint64_t revision = 0;
    uint64_t sequence = 0;
    uint16_t hardwareCode = 0;
    bool loaded = false;
    bool running = false;
    bool busy = false;
    std::string error;
};

// The worker exclusively owns its MCU. Published views are immutable; GPU work
// belongs to the caller. Button transitions are ordered, not frame requests.
class Worker {
public:
    // Empty directory uses LocalAppData; an explicit directory isolates devices.
    explicit Worker(std::filesystem::path storageDirectory = {});
    ~Worker();
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    void Load(std::shared_ptr<const StudioState> source,
        std::filesystem::path storageFile = {}, bool initiallyRunning = true);
    void Load(std::shared_ptr<const StudioState> source, uint16_t hardwareCode,
        std::filesystem::path storageFile = {}, bool initiallyRunning = true);
    void SetRunning(bool running);
    void SetInputs(const Inputs& inputs);
    void Reset();
    void Step();
    bool Poll();
    const View& Current() const;
    bool Busy() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
