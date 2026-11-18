#pragma once
#include "zwdf_package.h"
#include "firmware_bitmap.h"
#include <cmath>
#include <memory>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace FirmwareLogo {
inline constexpr uint32_t Width = 80, Height = 160;
inline constexpr uint32_t MinimumDurationMs = 200, MaximumDurationMs = 2000;
inline constexpr uint32_t PhaseCount = 16;
enum class Animation : uint32_t {
    None = 0, WipeDown = 1, WipeRight = 2, Fade = 3, Iris = 4,
    Diamond = 5, CenterHorizontal = 6, CenterVertical = 7, Blinds = 8, Dissolve = 9,
    SoftWipe = 10, SlideRight = 11, Checker = 12, Diagonal = 13,
    SlideLeft = 14, SlideUp = 15, SlideDown = 16,
    WipeUp = 17, WipeLeft = 18, DiagonalDownLeft = 19,
    DiagonalUpRight = 20, DiagonalUpLeft = 21,
    SoftWipeUp = 22, SoftWipeRight = 23, SoftWipeLeft = 24,
    SoftWipeDownRight = 25, SoftWipeDownLeft = 26,
    SoftWipeUpRight = 27, SoftWipeUpLeft = 28,
    SlideDownRight = 29, SlideDownLeft = 30, SlideUpRight = 31, SlideUpLeft = 32,
    BlindsUp = 33, BlindsRight = 34, BlindsLeft = 35,
    BlindsDownRight = 36, BlindsDownLeft = 37, BlindsUpRight = 38, BlindsUpLeft = 39,
    Count = 40
};
enum class Target : uint32_t { Boot = 0, Shutdown = 1, DeviceInfo = 2 };
enum class BootStages : uint32_t { First = 1, Second = 2, Both = 3 };
inline constexpr size_t TargetCount = 3;
struct Settings {
    bool enabled = false;
    std::vector<uint8_t> pixels;
    Animation animation = Animation::None;
    uint32_t durationMs = 600;
    BootStages bootStages = BootStages::Second;
    Animation exitAnimation = Animation::None;
    // holdMs applies only to boot/shutdown; CHIP ID keeps its native timeout.
    uint32_t holdMs = 0, exitDurationMs = 600;
    bool operator==(const Settings&) const = default;
};
using Patches = std::array<Settings, TargetCount>;
inline constexpr int MaximumEditableSourceDimension = 16384;
inline constexpr size_t MaximumEditableSourceBytes = 64 * 1024 * 1024;
// Original decoded pixels and editable placement are project-only metadata.
// Shared immutable pixels keep placement edits and undo inexpensive.
struct EditableSource {
    std::shared_ptr<const std::vector<uint8_t>> rgba;
    int width = 0, height = 0, fitMode = 0;
    float scalePercent = 100.0f;
    int x = 0, y = 0, alignX = 1, alignY = 1;
    FirmwareBitmap::Resampling resampling = FirmwareBitmap::Resampling::Lanczos;
    FirmwareBitmap::ImageAdjustments adjustments;

    bool Valid() const {
        if (fitMode < 0 || fitMode > 2 || !std::isfinite(scalePercent) ||
            scalePercent < 1 || scalePercent > 400 || x < -65536 || x > 65536 ||
            y < -65536 || y > 65536 || alignX < -1 || alignX > 2 ||
            alignY < -1 || alignY > 2 || !adjustments.Valid() ||
            static_cast<uint32_t>(resampling) > static_cast<uint32_t>(FirmwareBitmap::Resampling::Lanczos))
            return false;
        if (!rgba) return width == 0 && height == 0;
        return width > 0 && height > 0 &&
            width <= MaximumEditableSourceDimension && height <= MaximumEditableSourceDimension &&
            uint64_t(width) * height * 4 <= MaximumEditableSourceBytes &&
            rgba->size() == uint64_t(width) * height * 4;
    }
    bool operator==(const EditableSource& other) const {
        return width == other.width && height == other.height && fitMode == other.fitMode &&
            scalePercent == other.scalePercent && x == other.x && y == other.y &&
            alignX == other.alignX && alignY == other.alignY && resampling == other.resampling &&
            adjustments == other.adjustments &&
            (rgba == other.rgba || (rgba && other.rgba && *rgba == *other.rgba));
    }
};
using EditableSources = std::array<EditableSource, TargetCount>;
// Entrance None emits one static frame; animated entrances/exits emit 16 each.
// CHIP ID's ordinals span separate entrance and native-close invocations.
uint32_t FrameCount(const Settings& settings);
bool Supported(const FirmwarePackage& package);
bool Canonicalize(FirmwarePackage& package, Patches& settings, std::string& error);
struct FlashUsage {
    uint16_t hardwareCode = 0;
    size_t firmwareBytes = 0, patchBytes = 0, capacityBytes = 0;
    std::array<int64_t, TargetCount> screenDelta{};
    size_t Used() const { return firmwareBytes + patchBytes; }
};
// Exact extension encoding and per-record protected boundary, including overflow.
bool MeasureFlash(const FirmwarePackage& package, const Patches& settings,
    std::vector<FlashUsage>& usage, std::string& error);
// Deltas are additive contributions in Boot / Shutdown / DeviceInfo order.
bool MeasureFlashChanges(const FirmwarePackage& package, const Patches& baseline, const Patches& settings,
    std::vector<FlashUsage>& usage, std::string& error);
bool Build(const FirmwarePackage& package, const Patches& settings,
    const std::map<std::string, std::pair<std::vector<size_t>, std::vector<uint8_t>>>& replacements,
    const std::vector<CodePatch>& patches, std::vector<uint8_t>& output,
    BuildReport& report, std::string& error,
    const std::function<void(const char*)>& progress = {});
bool NormalizeForGuard(std::vector<uint8_t>& stock, const HardwareRecord& candidate, std::string& error);
// Returns the native frame-boundary address in a validated exported record.
// Preview observes this instruction; it never substitutes pixels or guest code.
uint32_t FrameBoundary(const HardwareRecord& record, Target target);
// Validated native function used by stateless frame previews. CHIP ID exit
// phases call its exit-only renderer; physical close events use the stock path.
uint32_t FrameEntry(const HardwareRecord& record, Target target, uint32_t phase);
}
