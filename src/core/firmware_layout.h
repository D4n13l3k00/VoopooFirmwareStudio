#pragma once
#include "zwdf_package.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace FirmwareLayout {
struct Point {
    int x = 0, y = 0;
    bool operator==(const Point&) const = default;
};
enum class Encoding { ThumbImm8, ThumbMovImm, ThumbAddImm8, ThumbMovw, ThumbMovAliasImm8, UInt8, UInt16, UInt32, ThumbAddImm3 };
struct Site {
    uint32_t address = 0;
    Encoding encoding = Encoding::ThumbImm8;
    bool vertical = false;
    int original = 0;
    int scale = 1; // Display pixels per encoded unit (OLED page coordinates: 8).
};
struct NativePatch {
    uint32_t address = 0;
    std::vector<uint8_t> original, replacement;
};
struct Element {
    std::string id, title;
    uint16_t hardwareCode = 0;
    int screenWidth = 0, screenHeight = 0;
    uint32_t screen = 0;
    std::vector<uint32_t> sharedScreens; // Same native coordinate storage, one editable block.
    // -1 shares coordinates across modes; otherwise 0/1 selects the native view.
    int temperatureMode = -1;
    int orientation = -1; // 0 portrait, 1 landscape.
    Point position;
    int width = 0, height = 0;
    Point minimumOffset, maximumOffset;
    int stepX = 1, stepY = 1;
    // Verified native draw sequence; -1 when conditional or not recovered.
    int drawOrder = -1;
    // First element owns native call order as record-local element indices.
    std::vector<uint8_t> drawSequence, tcDrawSequence;
    bool drawOrderInterleaved = false;
    std::vector<Site> sites;
    // The first block of a native draw lifecycle owns its redraw templates.
    std::vector<NativePatch> repaint;
    bool repaintRequired = false;
    // Optional cross-record identity proved from native call/resource semantics.
    // Record-local IDs remain stable when native addresses differ.
    std::string semanticId;
};

inline bool OnScreen(const Element& element, uint32_t screen) {
    if (element.screen == screen) return true;
    for (uint32_t shared : element.sharedScreens)
        if (shared == screen) return true;
    return false;
}
inline bool InView(const Element& element, uint32_t screen, bool temperature, bool landscape) {
    return OnScreen(element, screen) &&
        (element.temperatureMode < 0 || element.temperatureMode == int(temperature)) &&
        (element.orientation < 0 || element.orientation == int(landscape));
}
inline bool ViewsOverlap(const Element& a, const Element& b) {
    if (a.temperatureMode >= 0 && b.temperatureMode >= 0 && a.temperatureMode != b.temperatureMode) return false;
    if (a.orientation >= 0 && b.orientation >= 0 && a.orientation != b.orientation) return false;
    if (OnScreen(b, a.screen)) return true;
    for (uint32_t shared : a.sharedScreens)
        if (OnScreen(b, shared)) return true;
    return false;
}
using Offsets = std::map<std::string, Point, std::less<>>;

bool Discover(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error);
bool DiscoverSx(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error);
bool DiscoverDrag2(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error);
bool DiscoverLegacy(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error);
bool DiscoverThreeMax(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error);
bool DiscoverProPlus(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error);
bool DiscoverSxPages(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error);
bool BuildPatches(const FirmwarePackage& package, const std::vector<Element>& elements,
    const Offsets& offsets, std::vector<CodePatch>& patches, std::string& error);
// Native ROM guards admit only validated coordinate producers, never unrelated code.
bool MatchesCode(const std::vector<Element>& elements, uint16_t code, uint32_t address,
    const uint8_t* stock, const uint8_t* current, size_t length);
bool MatchesSxExecutable(const std::vector<Element>& elements, uint16_t code, uint32_t base,
    uint32_t stack, const uint8_t* stock, const uint8_t* current, size_t length);
// Normalize validated native layout instructions for adjacent palette guards.
bool NormalizeSxLayoutContext(const HardwareRecord& record, uint32_t address,
    uint8_t* bytes, size_t length);
bool NormalizeThreeMaxLayoutContext(const HardwareRecord& record, uint32_t address,
    uint8_t* bytes, size_t length);
bool NormalizeProPlusLayoutContext(const HardwareRecord& record, uint32_t address,
    uint8_t* bytes, size_t length);
}
