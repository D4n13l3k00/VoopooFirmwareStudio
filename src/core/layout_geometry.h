#pragma once
#include <algorithm>
#include <cmath>
#include <numeric>

// Editor-only geometry: no firmware coordinates or consumers are modified here.
namespace LayoutGeometry {
struct Rect {
    int x = 0, y = 0, width = 0, height = 0;
    bool operator==(const Rect&) const = default;
};
inline Rect Intersection(Rect a, Rect b) {
    const int left = (std::max)(a.x, b.x), top = (std::max)(a.y, b.y);
    const int right = (std::min)(a.x + a.width, b.x + b.width);
    const int bottom = (std::min)(a.y + a.height, b.y + b.height);
    if (a.width <= 0 || a.height <= 0 || b.width <= 0 || b.height <= 0 || right <= left || bottom <= top)
        return {};
    return {left, top, right - left, bottom - top};
}
struct Axis {
    int origin = 0, minimum = 0, maximum = 0, nativeStep = 1;
    int phase = 0; // Legal offsets are congruent to phase modulo nativeStep.
};
struct Lattice {
    int first = 0, last = 0, step = 1;
    bool valid = false;
};
inline int Mod(int value, int divisor) {
    const int remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
}
inline Lattice NativeLattice(Axis axis) {
    const int step = (std::max)(axis.nativeStep, 1);
    const int phase = Mod(axis.phase, step);
    const int first = axis.minimum + Mod(phase - axis.minimum, step);
    const int last = axis.maximum - Mod(axis.maximum - phase, step);
    return {first, last, step, first <= last};
}
inline Lattice GridLattice(Axis axis, int grid) {
    const auto native = NativeLattice(axis);
    if (grid <= 1 || !native.valid) return native;
    const int gcd = std::gcd(native.step, grid);
    if (Mod(axis.origin + Mod(axis.phase, native.step), gcd)) return {0, 0, 1, false};
    const int period = native.step / gcd * grid;
    // At most grid/gcd native samples: the native and screen-grid congruences
    // either intersect once per period or cannot intersect at all.
    int phase = Mod(axis.phase, native.step);
    while (Mod(axis.origin + phase, grid)) phase += native.step;
    const int first = native.first + Mod(phase - native.first, period);
    const int last = native.last - Mod(native.last - phase, period);
    return {first, last, period, first <= last};
}
inline Lattice MovementLattice(Axis axis, int grid) {
    const auto aligned = GridLattice(axis, grid);
    // Some consumers have no representable screen-grid coordinate (e.g. a
    // fixed native axis). Native bounds and encoding always take precedence.
    return aligned.valid ? aligned : NativeLattice(axis);
}
inline int Snap(Axis axis, double desiredOffset, int grid = 1) {
    const auto lattice = MovementLattice(axis, grid);
    if (!lattice.valid) return 0;
    const double steps = std::round((desiredOffset - lattice.first) / lattice.step);
    return static_cast<int>(std::clamp(lattice.first + steps * lattice.step,
        static_cast<double>(lattice.first), static_cast<double>(lattice.last)));
}
inline int Nudge(Axis axis, int current, int direction, int grid = 1, int multiplier = 1) {
    const auto lattice = MovementLattice(axis, grid);
    if (!lattice.valid || !direction) return current;
    const double location = static_cast<double>(current - lattice.first) / lattice.step;
    const double sample = direction > 0 ? std::floor(location) + multiplier : std::ceil(location) - multiplier;
    return static_cast<int>(std::clamp(lattice.first + sample * lattice.step,
        static_cast<double>(lattice.first), static_cast<double>(lattice.last)));
}
struct AxisSnap {
    int offset = 0;
    int guideTwice = 0;
    double distance = 0;
    bool guided = false;
};
inline AxisSnap StartSnap(Axis axis, double desiredOffset, int grid, double threshold) {
    return {Snap(axis, desiredOffset, grid), 0, threshold, false};
}
inline void ConsiderNeighbor(AxisSnap& best, Axis axis, int extent, double desiredOffset,
    int neighborOrigin, int neighborExtent, int grid = 1) {
    const int anchors[] = {0, extent, extent * 2};
    const int targets[] = {neighborOrigin * 2, neighborOrigin * 2 + neighborExtent,
        (neighborOrigin + neighborExtent) * 2};
    for (int target : targets) {
        for (int anchor : anchors) {
            const double wanted = (target - anchor) * 0.5 - axis.origin;
            const double distance = std::abs(wanted - desiredOffset);
            if (distance > best.distance || (best.guided && distance == best.distance)) continue;
            const int offset = Snap(axis, wanted, grid);
            if ((axis.origin + offset) * 2 + anchor != target) continue;
            best = {offset, target, distance, true};
        }
    }
}
} // namespace LayoutGeometry
