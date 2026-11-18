#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct StudioState;

namespace FirmwareEmulator {
bool Supports(const StudioState& source);
struct DeviceState {
    uint32_t screen = 0;
    uint16_t hardwareCode = 0; // Zero selects the model's canonical record.
    int watts = 35;
    int puffs = 1234;
    int battery = 4;
    int resistanceMilliOhms = 250;
    int timeCentiseconds = 234;
    int score = 0;
    int scoreDisplay = 1234;
    bool rba = false;
    bool locked = false;
    bool activation = true;
    bool charging = false;
    bool temperatureControl = false;
    int temperature = 200;
    bool fahrenheit = false;
    bool landscape = false;
    int menuSelection = 0;
    int temperatureMaterial = 0; // Ni, Ti, SS
    bool menuConfirmed = false;
    std::array<int, 14> puffHistory{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    int historySelection = 13;
    int clockHour = 12;
    int clockMinute = 34;
    std::array<uint8_t, 4> loaderVersion{};
    std::array<uint8_t, 8> loaderId{};
    int logoPhase = -1; // -1 executes the full sequence; 0..FrameCount(settings)-1 captures native frame boundaries.
    bool operator==(const DeviceState&) const = default;
};

struct Frame {
    int width = 0;
    int height = 0;
    bool monochrome = false;
    bool emulatedLoaderMetadata = false;
    bool emulatedUid = false;
    std::vector<uint32_t> rgba;
    std::vector<size_t> resources;
    uint64_t spiBytes = 0;
    uint64_t pixelWrites = 0;
    uint16_t hardwareCode = 0;
    std::string error;
};

// Borrows the guest-written LCD frame at a completed native animation boundary;
// phase is zero-based. Other backends do not invoke the observer.
using FrameObserver = void (*)(uint32_t phase, const Frame& frame, void* context);

class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(Renderer&&) noexcept;
    Renderer& operator=(Renderer&&) noexcept;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Render(const StudioState& source, const DeviceState& device, Frame& output,
        FrameObserver observer = nullptr, void* context = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
