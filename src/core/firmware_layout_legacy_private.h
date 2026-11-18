#pragma once
#include "firmware_layout.h"

namespace FirmwareLayout::LegacyLayout {
// The eight releases share OLED primitives, not record identity or flash maps.
bool DiscoverRelease(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error);
}
