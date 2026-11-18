#pragma once
#include "layout_geometry.h"
#include <cmath>
#include <string>

inline bool RunLayoutGeometryRegressions(std::string& error) {
    using namespace LayoutGeometry;
    error.clear();
    auto fail = [&](const char* reason) { error = reason; return false; };
    if (Intersection({0, 0, 10, 10}, {10, 2, 5, 5}) != Rect{} ||
        Intersection({0, 0, 10, 10}, {2, 10, 5, 5}) != Rect{})
        return fail("Layout rectangles sharing only an edge were reported as overlapping");
    if (Intersection({0, 0, 10, 10}, {9, 8, 5, 5}) != Rect{9, 8, 1, 2} ||
        Intersection({9, 8, 5, 5}, {0, 0, 10, 10}) != Rect{9, 8, 1, 2} ||
        Intersection({0, 0, 10, 10}, {2, 3, 4, 5}) != Rect{2, 3, 4, 5} ||
        Intersection({-10, -10, 20, 20}, {-4, -3, 1, 2}) != Rect{-4, -3, 1, 2})
        return fail("Layout rectangle intersection is not symmetric or consumer-coordinate exact");
    if (Intersection({0, 0, 0, 10}, {0, 0, 10, 10}) != Rect{} ||
        Intersection({0, 0, -5, 10}, {-10, 0, 20, 10}) != Rect{} ||
        Intersection({0, 0, 10, 10}, {20, 20, 10, 10}) != Rect{})
        return fail("Empty or disjoint layout rectangles were reported as overlapping");

    // Exhaustively compare the optimized congruence/lattice implementation with
    // a finite consumer-bounds oracle, including negative offsets and OLED pages.
    constexpr int nativeSteps[] = {1, 2, 3, 6, 8};
    for (int origin = 0; origin <= 13; ++origin) {
        for (int step : nativeSteps) {
            for (int phase = -step; phase <= step; ++phase) {
            const Axis axis{origin, -31, 37, step, phase};
            for (int grid = 1; grid <= 12; ++grid) {
                bool hasGrid = false;
                for (int value = axis.minimum; value <= axis.maximum; ++value)
                    hasGrid |= Mod(value - phase, step) == 0 && Mod(origin + value, grid) == 0;
                for (int sample = -50; sample <= 50; sample += 2) {
                    const double desired = sample + 0.25;
                    const int actual = Snap(axis, desired, grid);
                    if (actual < axis.minimum || actual > axis.maximum || Mod(actual - phase, step) ||
                        (hasGrid && Mod(origin + actual, grid)))
                        return fail("Layout grid snap violated a native consumer bound or representable grid");
                    double minimumDistance = 1000;
                    for (int value = axis.minimum; value <= axis.maximum; ++value) {
                        if (Mod(value - phase, step) || (hasGrid && Mod(origin + value, grid))) continue;
                        minimumDistance = (std::min)(minimumDistance, std::abs(value - desired));
                    }
                    if (std::abs(actual - desired) != minimumDistance)
                        return fail("Layout grid snap did not choose the nearest legal native coordinate");
                    for (int direction : {-1, 1}) {
                        const int nudged = Nudge(axis, actual, direction, grid);
                        if (nudged < axis.minimum || nudged > axis.maximum || Mod(nudged - phase, step) ||
                            (hasGrid && Mod(origin + nudged, grid)) ||
                            (direction > 0 ? nudged < actual : nudged > actual))
                            return fail("Layout arrow nudge violated native/grid bounds or direction");
                        int expected = actual;
                        for (int value = axis.minimum; value <= axis.maximum; ++value) {
                            if (Mod(value - phase, step) || (hasGrid && Mod(origin + value, grid))) continue;
                            if (direction > 0 && value > actual) { expected = value; break; }
                            if (direction < 0 && value < actual) expected = value;
                        }
                        if (nudged != expected)
                            return fail("Phase-aware nudge skipped the next legal native/grid coordinate");
                    }
                }
            }
            }
        }
    }
    const Axis page{8, -8, 40, 8};
    if (Snap(page, 5, 6) != 16 || Nudge(page, 0, 1, 6) != 16 ||
        Nudge(page, 0, -1, 6) != -8 || Nudge(page, 0, 1, 6, 10) != 40 ||
        Nudge(page, 40, 1, 6) != 40 || Nudge(page, -8, -1, 6) != -8)
        return fail("OLED-page grid intersection, off-grid nudge, or Shift boundary clamping failed");
    if (Snap({5, 0, 0, 8}, 50, 4) != 0 || Nudge({5, 0, 0, 8}, 0, 1, 4) != 0 ||
        Snap({0, -31, 31, 8}, -100) != -24 || Snap({0, -31, 31, 8}, 100) != 24)
        return fail("Fixed-axis or non-step-aligned native consumer bounds were not preserved");
    const Axis phasedPage{3, -13, 28, 8, 3};
    const auto phasedNative = NativeLattice(phasedPage);
    if (phasedNative.first != -13 || phasedNative.last != 27 ||
        Snap(phasedPage, 8, 6) != 3 || Snap(phasedPage, 30, 6) != 27 ||
        Snap({0, -2, 2, 8, 3}, 0) != 0 || NativeLattice({0, -2, 2, 8, 3}).valid)
        return fail("Nonzero native phase or empty phase lattice was not preserved");

    const Axis normal{0, 0, 120, 1};
    auto center = StartSnap(normal, 13.0, 1, 2);
    ConsiderNeighbor(center, normal, 10, 13.0, 16, 4);
    if (!center.guided || center.offset != 13 || center.guideTwice != 36)
        return fail("Nearest-neighbor center-to-center snap failed");
    auto edge = StartSnap(normal, 79.0, 1, 2);
    ConsiderNeighbor(edge, normal, 20, 79.0, 100, 20);
    if (!edge.guided || edge.offset != 80 || edge.guideTwice != 200)
        return fail("Nearest-neighbor edge snap failed");
    auto tooFar = StartSnap(normal, 60.0, 1, 2);
    ConsiderNeighbor(tooFar, normal, 20, 60.0, 100, 20);
    if (tooFar.guided || tooFar.offset != 60)
        return fail("Neighbor snapping escaped its capture threshold");
    auto halfPixel = StartSnap(normal, 15.5, 1, 0.1);
    ConsiderNeighbor(halfPixel, normal, 5, 15.5, 16, 4);
    if (halfPixel.guided)
        return fail("Neighbor guide claimed an unrepresentable half-pixel center alignment");
    auto native = StartSnap(page, 14.0, 6, 3);
    ConsiderNeighbor(native, page, 16, 14.0, 37, 2, 6);
    if (native.guided)
        return fail("Neighbor snap bypassed native page/grid restrictions");
    auto nearest = StartSnap(normal, 79.0, 1, 4);
    ConsiderNeighbor(nearest, normal, 20, 79.0, 102, 20);
    ConsiderNeighbor(nearest, normal, 20, 79.0, 100, 20);
    if (!nearest.guided || nearest.offset != 80)
        return fail("Neighbor search did not retain the closest legal guide");
    return true;
}
