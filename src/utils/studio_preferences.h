#pragma once
#include <array>
#include <cstddef>
#include <string>

namespace StudioPreferences {
inline constexpr int MinimumZoom = 1, MaximumZoom = 16, ResourceMaximumZoom = 32;
enum class Section : size_t {
    PatchPlacement, PatchAdjustments, PatchAnimation,
    LayoutOverlaps, LayoutPreview, LayoutPatchSites,
    DeviceRegisters, DeviceStack, DeviceLog, Count
};
inline constexpr std::array<const char*, size_t(Section::Count)> SectionIds{
    "PlacementBody", "AdjustmentsBody", "AnimationBody", "LayoutOverlapsBody", "LayoutPreviewBody",
    "LayoutPatchSitesBody", "DeviceRegistersBody", "DeviceStackBody", "DeviceLogBody"
};
struct Preferences {
    bool smoothAnimations = true;
    int logReviewSeconds = 3;
    int defaultPatchTarget = 0;
    int defaultBootStages = 3;
    int patchZoom = 4, emulatorZoom = 4, deviceZoom = 4, layoutZoom = 4;
    int comparisonZoom = 4, resourceZoom = 4, fontZoom = 6;
    int menuZoom = 4;
    std::array<bool, size_t(Section::Count)> sectionOpen{true, true, true, false, false, false, true, true, true};
    bool operator==(const Preferences&) const = default;
};
Preferences& Get();
// Empty path uses the existing studio_config.ini location. Tests pass an isolated path.
void Load(const std::string& path = {});
bool Save(const std::string& path = {});
void Reset();
} // namespace StudioPreferences
