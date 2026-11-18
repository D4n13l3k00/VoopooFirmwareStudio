#pragma once
#include "zwdf_package.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace FirmwareMenu {
enum class Item : uint8_t { Information, Voltage, Exit, PuffEditor, ScoreEditor, RankSelector };
inline constexpr size_t ItemCount = 6;
inline constexpr uint32_t InformationMask = 1u, VoltageMask = 2u, PuffEditorMask = 4u,
    ScoreEditorMask = 8u, RankSelectorMask = 16u;
inline constexpr uint32_t DefaultMask = InformationMask | VoltageMask;
inline constexpr uint32_t FunctionalMask = 31u;
inline constexpr unsigned RankCount = 8;
inline constexpr unsigned RankRowCount = RankCount+1, RankRowsY = 30, RankRowPitch = 14;
inline constexpr std::array<uint32_t,RankCount> RankPoints{0,478,2250,6000,11800,21400,31900,46550};
inline constexpr std::array<const char*,RankCount> RankNames{
    "IRON","BRONZE","SILVER","GOLD","PLATINUM","DIAMOND","MASTER","KING"};
inline constexpr uint32_t InfoBatteryMask = 1u, InfoResistanceMask = 2u, InfoDurationMask = 4u;
inline constexpr uint32_t AllInfoFields = InfoBatteryMask | InfoResistanceMask | InfoDurationMask;
inline constexpr unsigned PuffDigitCount = 5, PuffDigitsX = 11, PuffDigitPitch = 12, PuffDigitWidth = 10;
inline constexpr uint32_t PuffEditorMaximum = 99999;
struct Settings {
    bool enabled = false;
    bool batteryVoltage = false; // Initial selection; the device menu can change it.
    bool reverseNavigation = true; // Default: physical Plus moves up, Minus moves down.
    std::array<Item, ItemCount> order{Item::Information, Item::Voltage, Item::PuffEditor,
        Item::ScoreEditor, Item::RankSelector, Item::Exit};
    uint32_t enabledItems = DefaultMask; // Exit is always available; editing is opt-in.
    uint32_t infoFields = AllInfoFields;
    bool operator==(const Settings&) const = default;
};
bool ValidSettings(const Settings&);
bool Supported(const FirmwarePackage&);
// Score-edition records only; Factory omits the two achievement modules per record.
bool AchievementSupported(const HardwareRecord&);
// The outer menu envelope restores exactly its own hooks before logo/layout discovery.
bool Extract(FirmwarePackage&, Settings&, std::string& error);
// Adds the real native menu to already-built firmware; disabled is an unchanged pass.
bool Apply(std::vector<uint8_t>& packedFirmware, Settings, BuildReport&, std::string& error);
bool NormalizeForGuard(const HardwareRecord& stock, HardwareRecord& candidate, std::string& error);
// Exact added bytes including alignment; zero innerImageBytes uses the current image size.
size_t FlashBytes(const HardwareRecord&, Settings, size_t innerImageBytes = 0);
struct FlashBreakdown {
    size_t common = 0, information = 0, voltage = 0, puffEditor = 0, scoreEditor = 0, rankSelector = 0, total = 0;
};
// Additive attribution in module order: INFO, voltage, puffs, points, rank.
FlashBreakdown FlashCosts(const HardwareRecord&, Settings, size_t innerImageBytes = 0);
// Effective editor limit: the native counter bound capped to five decimal digits.
uint32_t PuffLimit(const HardwareRecord&);
struct PreviewValues {
    uint32_t batteryMillivolts = 4000, resistanceMilliOhms = 250, durationCentiseconds = 125;
    uint32_t puffs = 12345, maximumPuffs = PuffEditorMaximum;
    uint32_t scorePoints = 12345;
};
struct PreviewState {
    unsigned selection = 0;
    bool information = false, batteryVoltage = false;
    bool editingPuffs = false;
    unsigned puffDigit = 0;
    uint32_t puffValue = 0, originalPuffs = 0;
    unsigned puffStatus = 0; // 0: editing, 1: save failed, 2: current value exceeds five digits.
    bool editingScore = false, selectingRank = false;
    unsigned rankSelection = 0;
    // The active numeric editor shares puffDigit/puffValue/originalPuffs/puffStatus.
    // Score value is whole points; originalPuffs is the untouched native raw total.
};
// Exact native font, colours and geometry. Values are explicitly preview samples.
bool RenderPreview(Settings, PreviewState, PreviewValues, std::array<uint32_t, 80 * 160>& pixels);
// Zero unless the complete native menu envelope validates.
uint32_t StateAddress(const HardwareRecord&);
} // namespace FirmwareMenu
