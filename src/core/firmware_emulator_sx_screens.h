#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace FirmwareEmulator {
struct SxScreenProfile {
    size_t editionIndex;
    uint16_t code;
    std::array<uint32_t, 17> draw;
    uint32_t fill, rgb, animatedLogo, offLogo, pool;
    uint32_t history = 0, clock = 0, clockState = 0;
    uint32_t historyOffset = 0x32;
    uint32_t historyUpdate = 0, clockUpdate = 0;
};
}
