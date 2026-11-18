#pragma once
#include "app_state.h"
#include "firmware_emulator.h"
#include "layout_geometry.h"
#include <climits>
#include <cstdint>
#include <numeric>
#include <span>
#include <string_view>

namespace LayoutEdit {
struct Constraints {
    std::vector<size_t> targets;
    LayoutGeometry::Axis x, y;
    bool available = false;
};

namespace Detail {
inline std::string_view SemanticId(const FirmwareLayout::Element& element) {
    if (!element.semanticId.empty()) return element.semanticId;
    const std::string_view id(element.id);
    return id.size() > 7 && id.starts_with("hw") && id[6] == '.' ? id.substr(7) : std::string_view{};
}
inline FirmwareLayout::Point Offset(const FirmwareLayout::Offsets& offsets,
    const FirmwareLayout::Element& element) {
    const auto found = offsets.find(element.id);
    return found == offsets.end() ? FirmwareLayout::Point{} : found->second;
}
inline bool Valid(const FirmwareLayout::Element& element, int64_t x, int64_t y) {
    return element.stepX > 0 && element.stepY > 0 &&
        x >= element.minimumOffset.x && x <= element.maximumOffset.x &&
        y >= element.minimumOffset.y && y <= element.maximumOffset.y &&
        x % element.stepX == 0 && y % element.stepY == 0;
}
inline bool IncludeStep(int& step, int next) {
    const int64_t common = int64_t(step / std::gcd(step, next)) * next;
    if (common > INT_MAX) return false;
    step = static_cast<int>(common);
    return true;
}
} // namespace Detail

inline bool Resolve(const std::vector<FirmwareLayout::Element>& elements,
    const FirmwareLayout::Offsets& offsets, size_t selected, std::span<const uint16_t> codes,
    Constraints& result, const FirmwareEmulator::DeviceState* view = nullptr) {
    result.available = false;
    result.targets.clear();
    if (selected >= elements.size() || codes.empty()) return false;
    const auto& source = elements[selected];
    const auto semantic = Detail::SemanticId(source);
    if (view && !FirmwareLayout::InView(source, view->screen, view->temperatureControl, view->landscape)) return false;
    if (semantic.empty() || std::find(codes.begin(), codes.end(), source.hardwareCode) == codes.end()) return false;
    result.targets.reserve(codes.size());
    result.targets.push_back(selected);
    for (size_t c = 0; c < codes.size(); ++c) {
        if (std::find(codes.begin(), codes.begin() + c, codes[c]) != codes.begin() + c) return false;
        size_t match = SIZE_MAX;
        for (size_t i = 0; i < elements.size(); ++i) {
            const auto& candidate = elements[i];
            if (candidate.hardwareCode != codes[c] || Detail::SemanticId(candidate) != semantic) continue;
            if (view) {
                if (!FirmwareLayout::InView(candidate, view->screen, view->temperatureControl, view->landscape)) continue;
            } else if (candidate.screen != source.screen || candidate.temperatureMode != source.temperatureMode ||
                candidate.orientation != source.orientation) continue;
            if (match != SIZE_MAX) return false;
            match = i;
        }
        if (match == SIZE_MAX) return false;
        if (match != selected) result.targets.push_back(match);
    }
    const auto current = Detail::Offset(offsets, source);
    int stepX = 1, stepY = 1;
    int64_t minX = int64_t(source.minimumOffset.x) - current.x;
    int64_t maxX = int64_t(source.maximumOffset.x) - current.x;
    int64_t minY = int64_t(source.minimumOffset.y) - current.y;
    int64_t maxY = int64_t(source.maximumOffset.y) - current.y;
    for (size_t index : result.targets) {
        const auto& element = elements[index];
        const auto offset = Detail::Offset(offsets, element);
        if (!Detail::Valid(element, offset.x, offset.y) ||
            !Detail::IncludeStep(stepX, element.stepX) || !Detail::IncludeStep(stepY, element.stepY)) return false;
        minX = (std::max)(minX, int64_t(element.minimumOffset.x) - offset.x);
        maxX = (std::min)(maxX, int64_t(element.maximumOffset.x) - offset.x);
        minY = (std::max)(minY, int64_t(element.minimumOffset.y) - offset.y);
        maxY = (std::min)(maxY, int64_t(element.maximumOffset.y) - offset.y);
    }
    result.x = {source.position.x, static_cast<int>(current.x + minX), static_cast<int>(current.x + maxX),
        stepX, LayoutGeometry::Mod(current.x, stepX)};
    result.y = {source.position.y, static_cast<int>(current.y + minY), static_cast<int>(current.y + maxY),
        stepY, LayoutGeometry::Mod(current.y, stepY)};
    const auto x = LayoutGeometry::NativeLattice(result.x), y = LayoutGeometry::NativeLattice(result.y);
    result.available = x.valid && y.valid;
    if (result.available) {
        result.x.minimum = x.first; result.x.maximum = x.last;
        result.y.minimum = y.first; result.y.maximum = y.last;
    }
    return result.available;
}

// The caller owns the Undo action. All offsets remain relative to the opened
// firmware, and a shared delta preserves record-local edits and native geometry.
inline bool Apply(StudioState& studio, const Constraints& constraints,
    FirmwareLayout::Point desiredOffset, std::string& error) {
    error.clear();
    if (!constraints.available || constraints.targets.empty() ||
        !LayoutGeometry::NativeLattice(constraints.x).valid || !LayoutGeometry::NativeLattice(constraints.y).valid) {
        error = "Native layout selection is unavailable";
        return false;
    }
    for (size_t index : constraints.targets) {
        if (index >= studio.layoutElements.size()) {
            error = "Unknown native layout element";
            return false;
        }
    }
    const auto selected = Detail::Offset(studio.layoutOffsets, studio.layoutElements[constraints.targets.front()]);
    const int64_t dx = int64_t(LayoutGeometry::Snap(constraints.x, desiredOffset.x)) - selected.x;
    const int64_t dy = int64_t(LayoutGeometry::Snap(constraints.y, desiredOffset.y)) - selected.y;
    for (size_t index : constraints.targets) {
        const auto& element = studio.layoutElements[index];
        const auto current = Detail::Offset(studio.layoutOffsets, element);
        if (!Detail::Valid(element, current.x, current.y) ||
            !Detail::Valid(element, current.x + dx, current.y + dy)) {
            error = "Position is outside the native layout coordinate range";
            return false;
        }
    }
    for (size_t index : constraints.targets) {
        const auto current = Detail::Offset(studio.layoutOffsets, studio.layoutElements[index]);
        // The complete set passed the identical SetLayoutOffset validation above.
        if (!studio.SetLayoutOffset(index, {static_cast<int>(current.x + dx), static_cast<int>(current.y + dy)}, error))
            return false;
    }
    return true;
}
} // namespace LayoutEdit
