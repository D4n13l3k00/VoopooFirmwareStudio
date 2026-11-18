#pragma once
#include "firmware_layout.h"
#include <array>
#include <span>

namespace FirmwareLayout {
// One embedded release cache is shared by main and auxiliary S/X discovery.
const FirmwarePackage* SxStockPackage(size_t edition);
bool MatchesSxPageExecutable(const std::vector<Element>& elements, uint16_t code,
    uint32_t base, uint32_t stack, const uint8_t* stock, const uint8_t* current, size_t length);
bool NormalizeSxPageLayoutContext(const HardwareRecord& record, uint32_t address,
    uint8_t* bytes, size_t length);
// Read the staged native caller's actual R0..R3 producers for the completed
// boot/OFF phase. In particular, no host-side X/Y constants mask exported edits.
bool SxPagePhaseArguments(size_t edition, uint16_t code, uint32_t screen,
    uint32_t base, std::span<const uint8_t> image, std::array<uint32_t, 4>& arguments);
}
