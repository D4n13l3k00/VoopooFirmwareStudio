#include "ui_tabs.h"
#include "ui_preview_split.h"
#include "ui_firmware_layout.h"
#include "app_state.h"
#include "ui_firmware_emulator.h"
#include "ui_firmware_risk.h"
#include "ui_firmware_patches.h"
#include "ui_collapsible_section.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui_keyboard.h"
#include "ui_controls.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <climits>
#include <cmath>
#include <exception>
#include <cstdio>
#include <span>
#include <memory>
#include <thread>
#include <string_view>

namespace UiFirmwareLayout {
struct GroupingResult {
    std::atomic<bool> cancelled{false};
    std::atomic<bool> done{false};
    bool failed = false;
    uint64_t generation = 0;
    uint64_t revision = 0;
    std::vector<RevisionGroup> groups;
    std::string error;
};

struct GroupingWorker {
    std::shared_ptr<GroupingResult> result;
    ~GroupingWorker() { result->cancelled.store(true, std::memory_order_relaxed); }
};

namespace {
using FirmwareLayout::Element;
using FirmwareLayout::Point;

const char* ElementTitle(const Element& element) {
    const std::string_view title(element.title);
    if (title == "Power" || title == "Tc power") return Tr(i18n::Str::LayoutElementPower);
    if (title == "Logo") return Tr(i18n::Str::LayoutElementLogo);
    if (title == "Battery") return Tr(i18n::Str::LayoutElementBattery);
    if (title == "Resistance") return Tr(i18n::Str::LayoutElementResistance);
    if (title == "Duration") return Tr(i18n::Str::LayoutElementDuration);
    if (title == "Puffs") return Tr(i18n::Str::LayoutElementPuffs);
    if (title == "Mode" || title == "Mode badge") return Tr(i18n::Str::LayoutElementMode);
    if (title == "Status" || title == "Firing status" || title == "Lock state") return Tr(i18n::Str::LayoutElementStatus);
    if (title == "Score") return Tr(i18n::Str::LayoutElementScore);
    if (title == "Statistics") return Tr(i18n::Str::LayoutElementStatistics);
    if (title == "Voltage") return Tr(i18n::Str::LayoutElementVoltage);
    if (title == "Graph") return Tr(i18n::Str::LayoutElementGraph);
    if (title == "Rank") return Tr(i18n::Str::LayoutElementRank);
    if (title == "Mode selection") return Tr(i18n::Str::LayoutElementModeSelection);
    if (title == "Temperature") return Tr(i18n::Str::LayoutTemperature);
    return i18n::ResourceText(element.title.c_str());
}

Point Offset(const StudioState& studio, const Element& element) {
    const auto found = studio.layoutOffsets.find(element.id);
    return found == studio.layoutOffsets.end() ? Point{} : found->second;
}

void EndAction(State& state, StudioState& studio) {
    if (state.historyOpen) studio.EndUndoAction();
    state.historyOpen = false;
    state.keyMoving = false;
    state.guideX = state.guideY = false;
}

void BeginMove(State& state, StudioState& studio, const Element& element) {
    EndAction(state, studio);
    studio.PushUndo(std::string(Tr(i18n::Str::LayoutMoveElement)) + ": " + ElementTitle(element));
    state.historyOpen = true;
}

std::span<const uint16_t> ScopeCodes(const State& state) {
    return state.selectionScope == SelectionScope::All ?
        std::span<const uint16_t>(state.hardwareCodes) : std::span<const uint16_t>(state.scopeHardwareCodes);
}

void InvalidateRevisionGroups(State& state);

void MarkEdited(State& state, const StudioState& studio) {
    state.dirty = true;
    state.observedRevision = studio.ResourceRevision();
    // A uniform cohort move preserves proven identical interfaces. A single
    // revision edit can split its group; pending snapshots must also be renewed.
    if (state.selectionScope == SelectionScope::Revision || !state.revisionGroupsReady)
        InvalidateRevisionGroups(state);
}

bool ValidOffset(const Element& element, Point offset) {
    return element.stepX > 0 && element.stepY > 0 &&
        offset.x >= element.minimumOffset.x && offset.x <= element.maximumOffset.x &&
        offset.y >= element.minimumOffset.y && offset.y <= element.maximumOffset.y &&
        offset.x % element.stepX == 0 && offset.y % element.stepY == 0;
}

bool FixedAxis(const LayoutGeometry::Axis& axis) {
    const auto lattice = LayoutGeometry::NativeLattice(axis);
    return !lattice.valid || lattice.first == lattice.last;
}



LayoutGeometry::Rect ElementRect(const StudioState& studio, const Element& element) {
    const auto offset = Offset(studio, element);
    return {element.position.x + offset.x, element.position.y + offset.y, element.width, element.height};
}

void RefreshOverlaps(State& state, const StudioState& studio) {
    if (state.overlapRevision == studio.ResourceRevision() &&
        state.overlapHardwareCode == state.device.hardwareCode) return;
    state.overlapRevision = studio.ResourceRevision();
    state.overlapHardwareCode = state.device.hardwareCode;
    state.overlaps.clear();
    for (size_t a = 0; a < state.visibleElements.size(); ++a) {
        const auto first = state.visibleElements[a];
        const auto left = ElementRect(studio, studio.layoutElements[first]);
        for (size_t b = a + 1; b < state.visibleElements.size(); ++b) {
            const auto second = state.visibleElements[b];
            const auto intersection = LayoutGeometry::Intersection(left, ElementRect(studio, studio.layoutElements[second]));
            if (intersection.width > 0) state.overlaps.push_back({first, second, intersection});
        }
    }
}

Point MovementOffset(State& state, const StudioState& studio, const Element& element, double x, double y) {
    const int grid = state.snapGrid ? state.gridSpacing : 1;
    const auto& constraints = SelectionConstraints(state, studio);
    const auto horizontal = constraints.x, vertical = constraints.y;
    auto sx = LayoutGeometry::StartSnap(horizontal, x, grid, 6.0 / state.zoom);
    auto sy = LayoutGeometry::StartSnap(vertical, y, grid, 6.0 / state.zoom);
    if (state.snapElements) {
        for (size_t index : state.visibleElements) {
            if (index == state.selected) continue;
            const auto rect = ElementRect(studio, studio.layoutElements[index]);
            LayoutGeometry::ConsiderNeighbor(sx, horizontal, element.width, x, rect.x, rect.width, grid);
            LayoutGeometry::ConsiderNeighbor(sy, vertical, element.height, y, rect.y, rect.height, grid);
        }
    }
    state.guideX = sx.guided;
    state.guideY = sy.guided;
    state.guideXTwice = sx.guideTwice;
    state.guideYTwice = sy.guideTwice;
    return {sx.offset, sy.offset};
}

void NudgeSelection(State& state, StudioState& studio, ImGuiID canvasId) {
    constexpr ImGuiKey keys[] = {ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_UpArrow, ImGuiKey_DownArrow};
    const auto& io = ImGui::GetIO();
    const bool allowed = state.selected < studio.layoutElements.size() &&
        ImGui::IsItemFocused() && ImGui::IsWindowFocused() && Ui::EditorKeyboardAvailable(0, true);
    bool held = false;
    for (const auto key : keys) held |= ImGui::IsKeyDown(key, canvasId);
    if (state.keyMoving && (!allowed || !held)) EndAction(state, studio);
    if (!allowed) return;
    const auto& element = studio.layoutElements[state.selected];
    const auto& constraints = SelectionConstraints(state, studio);
    if (!constraints.available) return;
    Point next = Offset(studio, element);
    bool pressed = false;
    for (int i = 0; i < 4; ++i) {
        ImGui::SetKeyOwner(keys[i], canvasId, ImGuiInputFlags_LockThisFrame);
        if (!ImGui::IsKeyPressed(keys[i], ImGuiInputFlags_Repeat, canvasId)) continue;
        const bool vertical = i >= 2;
        int& value = vertical ? next.y : next.x;
        value = LayoutGeometry::Nudge(vertical ? constraints.y : constraints.x, value, (i & 1) ? 1 : -1,
            state.snapGrid ? state.gridSpacing : 1, io.KeyShift ? 10 : 1);
        pressed = true;
    }
    if (!pressed || next == Offset(studio, element)) return;
    if (!state.keyMoving) {
        BeginMove(state, studio, element);
        state.keyMoving = true;
    }
    ApplySelectionOffset(state, studio, next);
}

void AlignmentControls(State& state, StudioState& studio) {
    const auto& element = studio.layoutElements[state.selected];
    const auto& constraints = SelectionConstraints(state, studio);
    ImGui::TextUnformatted(Tr(i18n::Str::LayoutAlign));
    constexpr i18n::Str labels[] = {i18n::Str::LayoutAlignLeft, i18n::Str::LayoutAlignCenter,
        i18n::Str::LayoutAlignRight, i18n::Str::LayoutAlignTop, i18n::Str::LayoutAlignMiddle,
        i18n::Str::LayoutAlignBottom};
    for (int i = 0; i < 6; ++i) {
        if (i % 3) ImGui::SameLine();
        const bool vertical = i >= 3;
        ImGui::BeginDisabled(FixedAxis(vertical ? constraints.y : constraints.x));
        const bool clicked = ImGui::Button(Tr(labels[i]));
        ImGui::EndDisabled();
        if (!clicked) continue;
        EndAction(state, studio);
        const int extent = vertical ? element.height : element.width;
        const int canvas = vertical ? element.screenHeight : element.screenWidth;
        const auto axis = vertical ? constraints.y : constraints.x;
        const double coordinate = (canvas - extent) * (i % 3) * 0.5;
        auto next = Offset(studio, element);
        // Explicit canvas alignment overrides the optional editing grid.
        (vertical ? next.y : next.x) = LayoutGeometry::Snap(axis, coordinate - axis.origin);
        if (next == Offset(studio, element)) continue;
        BeginMove(state, studio, element);
        ApplySelectionOffset(state, studio, next);
        EndAction(state, studio);
    }
}

void OverlapControls(State& state, const StudioState& studio) {
    Ui::CollapsibleSection section{"LayoutOverlapsBody", Tr(i18n::Str::LayoutOverlaps)};
    if (!section) return;
    RefreshOverlaps(state, studio);
    const Element* owner = state.visibleElements.empty() ? nullptr :
        &studio.layoutElements[state.visibleElements.front()];
    const bool interleaved = owner && owner->drawOrderInterleaved;
    if (owner) {
        const auto& sequence = state.device.temperatureControl && !owner->tcDrawSequence.empty() ?
            owner->tcDrawSequence : owner->drawSequence;
        if (!sequence.empty()) {
            ImGui::TextUnformatted(Tr(i18n::Str::LayoutDrawSequence));
            ImGui::TextWrapped("%s", Tr(i18n::Str::LayoutDrawOrderNote));
            for (size_t i = 0; i < sequence.size(); ++i) {
                if (sequence[i] >= state.visibleElements.size()) continue;
                const auto& element = studio.layoutElements[state.visibleElements[sequence[i]]];
                ImGui::TextWrapped("%zu. %s", i + 1, ElementTitle(element));
            }
            if (interleaved) ImGui::TextWrapped("%s", Tr(i18n::Str::LayoutDrawInterleaved));
        }
    }
    if (state.overlaps.empty()) ImGui::TextDisabled("%s", Tr(i18n::Str::LayoutNoOverlaps));
    for (const auto& pair : state.overlaps) {
        const auto& first = studio.layoutElements[pair.first];
        const auto& second = studio.layoutElements[pair.second];
        if (!interleaved && first.drawOrder >= 0 && second.drawOrder >= 0 && first.drawOrder != second.drawOrder) {
            const auto& later = first.drawOrder > second.drawOrder ? first : second;
            const auto& earlier = first.drawOrder > second.drawOrder ? second : first;
            ImGui::TextWrapped(Tr(i18n::Str::LayoutDrawOrderPair),
                ElementTitle(later), later.drawOrder + 1, ElementTitle(earlier), earlier.drawOrder + 1);
        } else {
            ImGui::TextWrapped("%s / %s: %s", ElementTitle(first), ElementTitle(second),
                Tr(interleaved ? i18n::Str::LayoutDrawInterleaved : i18n::Str::LayoutDrawOrderUnknown));
        }
    }
}

void SelectRevision(State& state, StudioState& studio, uint16_t code) {
    EndAction(state, studio);
    state.dragging = false;
    const std::string_view previousTitle = state.selected < studio.layoutElements.size() ?
        std::string_view(studio.layoutElements[state.selected].title) : std::string_view{};
    state.selected = SIZE_MAX;
    state.visibleElements.clear();
    state.device.hardwareCode = code;
    const auto screens = FirmwareEmulator::AvailableScreens(studio, code);
    if (std::none_of(screens.begin(), screens.end(),
        [&](const auto& screen) { return screen.id == state.device.screen; })) state.device.screen = 0;
    const auto current = std::find_if(screens.begin(), screens.end(),
        [&](const auto& screen) { return screen.id == state.device.screen; });
    if (current != screens.end()) FirmwareEmulator::ApplyScreenMode(state.device, *current);
    state.overlapRevision = UINT64_MAX;
    for (size_t i = 0; i < studio.layoutElements.size(); ++i) {
        const auto& element = studio.layoutElements[i];
        if (element.hardwareCode != code || !FirmwareLayout::InView(element, state.device.screen,
            state.device.temperatureControl, state.device.landscape)) continue;
        state.visibleElements.push_back(i);
        if (state.selected == SIZE_MAX || element.title == previousTitle) state.selected = i;
    }
    state.canvasWidth = state.visibleElements.empty() ? 0 :
        studio.layoutElements[state.visibleElements.front()].screenWidth;
    state.canvasHeight = state.visibleElements.empty() ? 0 :
        studio.layoutElements[state.visibleElements.front()].screenHeight;
    state.visibleScreen = state.device.screen;
    state.visibleTemperature = state.device.temperatureControl;
    state.visibleLandscape = state.device.landscape;
    ++state.selectionToken;
    state.editError.clear();
    state.dirty = true;
}

void SynchronizeView(State& state, StudioState& studio) {
    if (state.visibleScreen != state.device.screen ||
        state.visibleTemperature != state.device.temperatureControl ||
        state.visibleLandscape != state.device.landscape)
        SelectRevision(state, studio, state.device.hardwareCode);
}

std::vector<size_t> RevisionElements(const StudioState& studio, uint16_t code) {
    std::vector<size_t> indices;
    for (size_t i = 0; i < studio.layoutElements.size(); ++i)
        if (studio.layoutElements[i].hardwareCode == code) indices.push_back(i);
    return indices;
}

bool SameGeometry(const StudioState& studio, std::span<const size_t> left,
    std::span<const size_t> right) {
    if (left.empty() || left.size() != right.size()) return false;
    for (size_t i = 0; i < left.size(); ++i) {
        const auto& a = studio.layoutElements[left[i]];
        const auto& b = studio.layoutElements[right[i]];
        if (LayoutEdit::Detail::SemanticId(a) != LayoutEdit::Detail::SemanticId(b) ||
            a.title != b.title || a.screenWidth != b.screenWidth || a.screenHeight != b.screenHeight ||
            a.screen != b.screen || a.sharedScreens != b.sharedScreens ||
            a.temperatureMode != b.temperatureMode || a.orientation != b.orientation ||
            a.position != b.position || a.width != b.width || a.height != b.height ||
            a.minimumOffset != b.minimumOffset || a.maximumOffset != b.maximumOffset ||
            a.stepX != b.stepX || a.stepY != b.stepY || Offset(studio, a) != Offset(studio, b)) return false;
    }
    return true;
}

bool SameFrame(const FirmwareEmulator::Frame& a, const FirmwareEmulator::Frame& b) {
    return a.width == b.width && a.height == b.height && a.monochrome == b.monochrome &&
        a.rgba == b.rgba;
}

// Only the isolated discovery snapshot is probed. No live document, texture,
// history, or revision counter is touched; each coordinate sample gets a fresh
// private ROM cache, and its offset is restored before the next sample.
struct ProbeOffset {
    FirmwareLayout::Offsets& offsets;
    const std::string& id;
    Point previous;
    bool existed;

    ProbeOffset(StudioState& studio, const Element& element, Point next)
        : offsets(studio.layoutOffsets), id(element.id), previous(Offset(studio, element)),
          existed(offsets.contains(id)) {
        if (next == Point{}) offsets.erase(id);
        else offsets[id] = next;
    }
    ~ProbeOffset() {
        if (existed) offsets[id] = previous;
        else offsets.erase(id);
    }
};

template<class Consume>
bool VisitNativeSamples(StudioState& studio, uint16_t code, std::span<const size_t> elements,
    const std::atomic<bool>& cancelled, Consume&& consume) {
    using FirmwareEmulator::DeviceState;
    std::array<DeviceState, 5> devices;
    devices[1].watts = 42;
    devices[1].puffs = 98765;
    devices[1].battery = 1;
    devices[1].resistanceMilliOhms = 1250;
    devices[1].timeCentiseconds = 876;
    devices[1].rba = true;
    devices[1].score = 21400;
    devices[1].scoreDisplay = 9876;
    devices[2].locked = true;
    devices[2].charging = true;
    devices[2].battery = 0;
    devices[2].score = 99999;
    size_t deviceCount = 3;
    const bool legacy = studio.model == FirmwareModel::Drag1 || studio.model == FirmwareModel::Drag2 ||
        studio.model == FirmwareModel::DragMini || studio.model == FirmwareModel::Drag2Refresh ||
        studio.model == FirmwareModel::DragMiniRefresh;
    if (legacy || studio.edition == FirmwareEdition::Tc) {
        auto& tc = devices[deviceCount++];
        tc.temperatureControl = true;
        tc.temperature = 315;
        tc.fahrenheit = true;
    }
    if (studio.model == FirmwareModel::Drag3 || studio.model == FirmwareModel::DragMax ||
        studio.model == FirmwareModel::DragXPlus || studio.model == FirmwareModel::DragXPlusPro)
        devices[deviceCount++].landscape = true;
    for (size_t i = 0; i < deviceCount; ++i) devices[i].hardwareCode = code;
    FirmwareEmulator::Frame frame;
    auto sample = [&](FirmwareEmulator::Renderer& renderer, const DeviceState& device) {
        if (cancelled.load(std::memory_order_relaxed)) return false;
        if (!renderer.Render(studio, device, frame) || !frame.error.empty() ||
            frame.hardwareCode != code || frame.width <= 0 || frame.height <= 0 ||
            frame.rgba.size() != size_t(frame.width) * frame.height) return false;
        return consume(std::move(frame));
    };
    FirmwareEmulator::Renderer baseline;
    for (size_t i = 0; i < deviceCount; ++i)
        if (!sample(baseline, devices[i])) return false;
    for (size_t index : elements) {
        const auto& element = studio.layoutElements[index];
        const Point current = Offset(studio, element);
        for (bool vertical : {false, true}) {
            const int low = vertical ? element.minimumOffset.y : element.minimumOffset.x;
            const int high = vertical ? element.maximumOffset.y : element.maximumOffset.x;
            const int step = vertical ? element.stepY : element.stepX;
            if (step <= 0) return false;
            const int minimum = static_cast<int>(std::ceil(double(low) / step)) * step;
            const int maximum = static_cast<int>(std::floor(double(high) / step)) * step;
            for (int target : {minimum, maximum}) {
                if (target == (vertical ? current.y : current.x) ||
                    (target == maximum && maximum == minimum)) continue;
                Point next = current;
                (vertical ? next.y : next.x) = target;
                if (!ValidOffset(element, next)) return false;
                ProbeOffset offset(studio, element, next);
                FirmwareEmulator::Renderer moved;
                auto view = devices[0];
                if (element.temperatureMode >= 0) view.temperatureControl = element.temperatureMode != 0;
                if (element.orientation >= 0) view.landscape = element.orientation != 0;
                for (size_t page = 0; page <= element.sharedScreens.size(); ++page) {
                    view.screen = page ? element.sharedScreens[page - 1] : element.screen;
                    if (!sample(moved, view)) return false;
                    auto alternate = devices[1];
                    alternate.screen = view.screen;
                    alternate.temperatureControl = view.temperatureControl;
                    alternate.landscape = view.landscape;
                    if (!sample(moved, alternate)) return false;
                }
            }
        }
    }
    return true;
}

std::vector<RevisionGroup> BuildRevisionGroups(StudioState& studio,
    std::span<const uint16_t> hardwareCodes, const std::atomic<bool>& cancelled) {
    std::vector<RevisionGroup> groups;
    std::vector<std::vector<size_t>> indices;
    indices.reserve(hardwareCodes.size());
    for (uint16_t code : hardwareCodes) indices.push_back(RevisionElements(studio, code));
    std::vector<bool> grouped(hardwareCodes.size(), false);
    for (size_t representative = 0; representative < hardwareCodes.size(); ++representative) {
        if (cancelled.load(std::memory_order_relaxed)) return {};
        if (grouped[representative]) continue;
        RevisionGroup group;
        group.hardwareCodes.push_back(hardwareCodes[representative]);
        for (size_t index : indices[representative]) group.elements.push_back({index});
        std::vector<FirmwareEmulator::Frame> reference;
        reference.reserve(4 + indices[representative].size() * 8);
        bool sampled = false, referenceValid = false;
        for (size_t candidate = representative + 1; candidate < hardwareCodes.size(); ++candidate) {
            if (cancelled.load(std::memory_order_relaxed)) return {};
            if (grouped[candidate] || !SameGeometry(studio, indices[representative], indices[candidate])) continue;
            if (!sampled) {
                sampled = true;
                referenceValid = VisitNativeSamples(studio, hardwareCodes[representative],
                    indices[representative], cancelled, [&](FirmwareEmulator::Frame&& frame) {
                        reference.push_back(std::move(frame));
                        return true;
                    });
            }
            if (!referenceValid) break; // Unverifiable revisions remain separate.
            size_t sample = 0;
            const bool equal = VisitNativeSamples(studio, hardwareCodes[candidate],
                indices[candidate], cancelled, [&](FirmwareEmulator::Frame&& frame) {
                    return sample < reference.size() && SameFrame(reference[sample++], frame);
                });
            if (!equal || sample != reference.size()) continue;
            group.nativeSamples = reference.size();
            grouped[candidate] = true;
            group.hardwareCodes.push_back(hardwareCodes[candidate]);
            for (size_t i = 0; i < group.elements.size(); ++i)
                group.elements[i].push_back(indices[candidate][i]);
        }
        groups.push_back(std::move(group));
    }
    return groups;
}
void SetGroupTitles(std::vector<RevisionGroup>& groups) {
    // Translation state is accessed only on the UI thread, never by discovery.
    for (auto& group : groups) {
        group.title.clear();
        for (uint16_t code : group.hardwareCodes) {
            char title[64];
            snprintf(title, sizeof(title), Tr(i18n::Str::LayoutRevisionCode), code);
            if (!group.title.empty()) group.title += ", ";
            group.title += title;
        }
    }
}

void InvalidateRevisionGroups(State& state) {
    ++state.groupingGeneration;
    state.groupingWorker.reset(); // Cancellation never joins the worker.
    state.revisionGroups.clear();
    state.revisionGroupsReady = false;
}

std::unique_ptr<StudioState> GroupingSnapshot(const StudioState& source) {
    auto snapshot = std::make_unique<StudioState>();
    // These are the complete inputs to native Render/BuildZwdfBytes. In
    // particular, do not copy UI textures, undo history, or source identity.
    snapshot->hasFirmware = source.hasFirmware;
    snapshot->firmware = source.firmware;
    snapshot->model = source.model;
    snapshot->edition = source.edition;
    snapshot->catalog = source.catalog;
    snapshot->locations = source.locations;
    snapshot->replacements = source.replacements;
    snapshot->palette = source.palette;
    snapshot->backgroundColor = source.backgroundColor;
    snapshot->layoutElements = source.layoutElements;
    snapshot->layoutOffsets = source.layoutOffsets;
    snapshot->originals = source.originals;
    snapshot->screenPatches = source.screenPatches;
    snapshot->originalScreenPatches = source.originalScreenPatches;
    snapshot->menuPatch = source.menuPatch;
    snapshot->originalMenuPatch = source.originalMenuPatch;
    snapshot->referencePalette = source.referencePalette;
    snapshot->referenceBackground = source.referenceBackground;
    snapshot->referenceRecordPalette = source.referenceRecordPalette;
    return snapshot;
}

void AcceptRevisionGroups(State& state, std::vector<RevisionGroup> groups) {
    state.revisionGroups = std::move(groups);
    SetGroupTitles(state.revisionGroups);
    state.revisionGroupsReady = true;
}

void UpdateRevisionGroups(State& state, StudioState& studio, bool synchronous) {
    if (state.revisionGroupsReady) return;
    if (state.revisionGroups.empty()) {
        // Editing is immediately available while proof runs: unproven records
        // remain individual revisions, never provisional multi-record groups.
        for (uint16_t code : state.hardwareCodes) {
            RevisionGroup group;
            group.hardwareCodes.push_back(code);
            for (size_t index : RevisionElements(studio, code)) group.elements.push_back({index});
            state.revisionGroups.push_back(std::move(group));
        }
        SetGroupTitles(state.revisionGroups);
    }
    if (synchronous) {
        state.groupingWorker.reset();
        auto snapshot = GroupingSnapshot(studio);
        const std::atomic<bool> cancelled{false};
        AcceptRevisionGroups(state, BuildRevisionGroups(*snapshot, state.hardwareCodes, cancelled));
        return;
    }
    if (state.groupingWorker) {
        const auto result = state.groupingWorker->result;
        if (result->generation != state.groupingGeneration || result->revision != studio.ResourceRevision()) {
            state.groupingWorker.reset();
        } else {
            if (!result->done.load(std::memory_order_acquire) || state.dragging || state.historyOpen) return;
            if (!result->cancelled.load(std::memory_order_relaxed)) {
                if (!result->failed) AcceptRevisionGroups(state, std::move(result->groups));
                else {
                    state.editError = result->error.empty() ? Tr(i18n::Str::LayoutRenderFailed) : result->error;
                    state.revisionGroupsReady = true;
                }
            }
            state.groupingWorker.reset();
            return;
        }
    }
    // A pending proof cancelled by an edit is restarted once per completed
    // action, not once per drag event or intermediate text-input revision.
    if (state.dragging || state.historyOpen) return;
    auto snapshot = GroupingSnapshot(studio);
    auto result = std::make_shared<GroupingResult>();
    result->generation = state.groupingGeneration;
    result->revision = studio.ResourceRevision();
    auto owner = std::make_shared<GroupingWorker>();
    owner->result = result;
    try {
        std::thread([result, snapshot = std::move(snapshot), codes = state.hardwareCodes]() mutable {
            try {
                result->groups = BuildRevisionGroups(*snapshot, codes, result->cancelled);
            } catch (const std::exception& error) {
                result->failed = true;
                result->error = error.what();
            } catch (...) {
                result->failed = true;
            }
            result->done.store(true, std::memory_order_release);
        }).detach();
        state.groupingWorker = std::move(owner);
    } catch (const std::exception& error) {
        state.editError = error.what();
        state.revisionGroupsReady = true;
    }
}



void SynchronizeSource(State& state, StudioState& studio) {
    const bool changed = state.source != &studio || state.documentIdentity != studio.DocumentIdentity() ||
        state.sourceHash != studio.firmware.source_sha256 ||
        state.sourceBytes != studio.firmware.source_bytes.data() ||
        state.sourceSize != studio.firmware.source_bytes.size() ||
        state.sourceElements != studio.layoutElements.data() || state.elementCount != studio.layoutElements.size();
    if (!changed) {
        if (state.observedRevision != studio.ResourceRevision()) {
            EndAction(state, studio);
            state.dragging = false;
            state.dirty = true;
            InvalidateRevisionGroups(state);
            state.editError.clear();
            state.observedRevision = studio.ResourceRevision();
        }
        return;
    }
    if (state.source == &studio) EndAction(state, studio);
    state.historyOpen = false;
    state.dragging = false;
    state.source = &studio;
    state.documentIdentity = studio.DocumentIdentity();
    state.sourceHash = studio.firmware.source_sha256;
    state.sourceBytes = studio.firmware.source_bytes.data();
    state.sourceSize = studio.firmware.source_bytes.size();
    state.sourceElements = studio.layoutElements.data();
    state.elementCount = studio.layoutElements.size();
    state.revision = UINT64_MAX;
    state.observedRevision = studio.ResourceRevision();
    state.selected = SIZE_MAX;
    state.editError.clear();
    state.overlapRevision = UINT64_MAX;
    state.overlaps.clear();
    state.hardwareCodes.clear();
    InvalidateRevisionGroups(state);
    state.selectionScope = SelectionScope::All;
    state.scopeHardwareCodes.clear();
    state.scopeTitle.clear();
    ++state.selectionToken;
    state.constraintsSource = nullptr;
    state.constraintsRevision = UINT64_MAX;
    state.constraintsToken = UINT64_MAX;
    state.constraintsElement = SIZE_MAX;
    state.constraints.targets.clear();
    state.constraints.available = false;
    for (const auto& record : studio.firmware.records) {
        const auto code = static_cast<uint16_t>((record.device_code[0] << 8) | record.device_code[1]);
        if (std::find(state.hardwareCodes.begin(), state.hardwareCodes.end(), code) == state.hardwareCodes.end())
            state.hardwareCodes.push_back(code);
    }
    uint16_t initialCode = state.hardwareCodes.empty() ? 0 : state.hardwareCodes.front();
    for (uint16_t code : state.hardwareCodes) {
        if (std::any_of(studio.layoutElements.begin(), studio.layoutElements.end(),
            [code](const auto& element) { return element.hardwareCode == code; })) {
            initialCode = code;
            break;
        }
    }
    // Keep the sleeping worker alive across document changes: replacing it
    // would join an in-flight native frame on the UI thread.
    state.device = FirmwareEmulator::DeviceState{};
    state.publication.reset();
    state.previewReady = false;
    state.previewError.clear();
    state.texture.Release();
    SelectRevision(state, studio, initialCode);
    state.preview.Request(studio, state.device);
}

void RefreshPreview(State& state, const StudioState& studio, ID3D11Device* device,
    ID3D11DeviceContext* context) {
    const bool force = state.dirty && state.revision == studio.ResourceRevision() &&
        state.requestedDevice == state.device;
    state.preview.Request(studio, state.device, force);
    state.dirty = false;
    state.revision = studio.ResourceRevision();
    state.requestedDevice = state.device;
    const bool published = state.preview.Poll();
    const bool changedDevice = state.textureDevice != device;
    if (changedDevice) state.texture.Release();
    state.textureDevice = device;
    if (!published && !changedDevice) return;
    state.publication = state.preview.Published();
    state.previewReady = false;
    state.previewError.clear();
    if (!state.publication) return;
    const auto& frame = state.Frame();
    if (!frame.error.empty()) {
        state.previewError = frame.error;
        return;
    }
    state.canvasWidth = frame.width;
    state.canvasHeight = frame.height;
    if (!state.texture.Update(device, context, frame.rgba.data(), frame.width, frame.height)) {
        state.previewError = Tr(i18n::Str::CompareTextureError);
        return;
    }
    state.previewReady = true;
}

bool ResetTargets(State& state, StudioState& studio, std::span<const size_t> targets, i18n::Str label) {
    EndAction(state, studio);
    state.dragging = false;
    bool changed = false;
    for (size_t index : targets) {
        if (index >= studio.layoutElements.size() || !ValidOffset(studio.layoutElements[index], {})) {
            state.editError = Tr(i18n::Str::LayoutSelectionUnavailable);
            return false;
        }
        changed |= Offset(studio, studio.layoutElements[index]) != Point{};
    }
    if (!changed) return true;
    studio.PushUndo(Tr(label));
    for (size_t index : targets) {
        if (!studio.SetLayoutOffset(index, {}, state.editError)) {
            studio.EndUndoAction();
            return false;
        }
    }
    studio.EndUndoAction();
    MarkEdited(state, studio);
    return true;
}

bool InScope(const State& state, uint16_t code) {
    const auto codes = ScopeCodes(state);
    return std::find(codes.begin(), codes.end(), code) != codes.end();
}

void ResetOffsets(State& state, StudioState& studio, bool all) {
    std::vector<size_t> targets;
    targets.reserve(studio.layoutElements.size());
    for (size_t i = 0; i < studio.layoutElements.size(); ++i)
        if (all || InScope(state, studio.layoutElements[i].hardwareCode)) targets.push_back(i);
    ResetTargets(state, studio, targets, all ? i18n::Str::LayoutResetAll : i18n::Str::LayoutResetScope);
}

void PositionInput(State& state, StudioState& studio, bool vertical) {
    const auto& element = studio.layoutElements[state.selected];
    const Point current = Offset(studio, element);
    int value = vertical ? element.position.y + current.y : element.position.x + current.x;
    const auto& constraints = SelectionConstraints(state, studio);
    const auto axis = vertical ? constraints.y : constraints.x;
    const int step = (std::max)(1, axis.nativeStep);
    const bool fixed = FixedAxis(axis);
    ImGui::TextUnformatted(Tr(vertical ? i18n::Str::LayoutPositionY : i18n::Str::LayoutPositionX));
    ImGui::BeginDisabled(fixed);
    ImGui::SetNextItemWidth(-1);
    const bool changed = ImGui::InputInt(vertical ? "##PositionY" : "##PositionX", &value, step, step * 10);
    if (ImGui::IsItemActivated()) BeginMove(state, studio, element);
    if (changed) {
        if (!state.historyOpen) BeginMove(state, studio, element);
        Point next = current;
        (vertical ? next.y : next.x) = LayoutGeometry::Snap(axis,
            static_cast<double>(value) - (vertical ? element.position.y : element.position.x),
            state.snapGrid ? state.gridSpacing : 1);
        ApplySelectionOffset(state, studio, next);
    }
    if (ImGui::IsItemDeactivated()) EndAction(state, studio);
    ImGui::EndDisabled();
}

void PreviewControls(State& state, const StudioState& studio) {
    Ui::CollapsibleSection section{"LayoutPreviewBody", Tr(i18n::Str::LayoutPreviewState)};
    if (!section) return;
    state.dirty |= UiFirmwareEmulator::DrawDeviceControls(state.device, studio);
}

void ElementControls(State& state, StudioState& studio) {
    ImGui::TextUnformatted(Tr(i18n::Str::LayoutRevision));
    ImGui::SetNextItemWidth(-1);
    char revisionTitle[1024];
    if (state.selectionScope == SelectionScope::All)
        snprintf(revisionTitle, sizeof(revisionTitle), "%s", Tr(i18n::Str::LayoutAllRevisions));
    else
        snprintf(revisionTitle, sizeof(revisionTitle),
            Tr(state.selectionScope == SelectionScope::Group ?
                i18n::Str::LayoutRevisionGroup : i18n::Str::LayoutRevisionIndividual),
            state.scopeTitle.c_str());
    if (ImGui::BeginCombo("##LayoutRevision", revisionTitle)) {
        const bool allSelected = state.selectionScope == SelectionScope::All;
        if (ImGui::Selectable(Tr(i18n::Str::LayoutAllRevisions), allSelected))
            SelectScope(state, studio, SelectionScope::All);
        if (allSelected) ImGui::SetItemDefaultFocus();
        ImGui::Separator();
        if (state.revisionGroupsReady) {
            for (const auto& group : state.revisionGroups) {
                if (group.hardwareCodes.size() <= 1) continue;
                const bool selected = state.selectionScope == SelectionScope::Group &&
                    state.scopeHardwareCodes == group.hardwareCodes;
                char title[1024];
                snprintf(title, sizeof(title), Tr(i18n::Str::LayoutRevisionGroup), group.title.c_str());
                if (ImGui::Selectable(title, selected))
                    SelectScope(state, studio, SelectionScope::Group, group.hardwareCodes);
                if (selected) ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::Separator();
        for (uint16_t code : state.hardwareCodes) {
            char codeTitle[64], title[128];
            snprintf(codeTitle, sizeof(codeTitle), Tr(i18n::Str::LayoutRevisionCode), code);
            snprintf(title, sizeof(title), Tr(i18n::Str::LayoutRevisionIndividual), codeTitle);
            const bool selected = state.selectionScope == SelectionScope::Revision &&
                state.scopeHardwareCodes.size() == 1 && state.scopeHardwareCodes.front() == code;
            if (ImGui::Selectable(title, selected))
                SelectScope(state, studio, SelectionScope::Revision, std::span<const uint16_t>(&code, 1));
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (!state.revisionGroupsReady)
        ImGui::TextDisabled("%s", Tr(i18n::Str::LayoutCheckingRevisions));
    else
        ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
    const auto screens = FirmwareEmulator::AvailableScreens(studio, state.device.hardwareCode);
    const auto current = std::find_if(screens.begin(), screens.end(),
        [&](const auto& screen) { return screen.id == state.device.screen; });
    ImGui::TextUnformatted(Tr(i18n::Str::EmulatorScreen));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##LayoutScreen", current == screens.end() ?
        Tr(i18n::Str::EmulatorMainScreen) : UiFirmwareEmulator::ScreenTitle(*current))) {
        for (const auto& screen : screens) {
            ImGui::PushID(static_cast<int>(screen.id));
            if (ImGui::Selectable(UiFirmwareEmulator::ScreenTitle(screen), state.device.screen == screen.id)) {
                state.device.screen = screen.id;
                SynchronizeView(state, studio);
            }
            if (state.device.screen == screen.id) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SeparatorText(Tr(i18n::Str::LayoutElements));
    if (ImGui::BeginChild("LayoutElements", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 7), true)) {
        if (state.visibleElements.empty()) ImGui::TextWrapped("%s", Tr(i18n::Str::LayoutNoElements));
        for (size_t index : state.visibleElements) {
            const auto& element = studio.layoutElements[index];
            ImGui::PushID(element.id.c_str());
            if (ImGui::Selectable(ElementTitle(element), state.selected == index)) {
                EndAction(state, studio);
                state.dragging = false;
                state.selected = index;
                state.editError.clear();
            }
            if (Offset(studio, element) != Point{}) {
                ImGui::SameLine();
                ImGui::TextDisabled("*");
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    if (state.selected < studio.layoutElements.size()) {
        const auto& element = studio.layoutElements[state.selected];
        ImGui::PushID(element.id.c_str());
        if (!element.sharedScreens.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", Tr(i18n::Str::LayoutSharedBlock));
            ImGui::PopStyleColor();
        }
        const auto& constraints = SelectionConstraints(state, studio);
        const bool compatible = constraints.available;
        if (!compatible) ImGui::TextWrapped("%s", Tr(i18n::Str::LayoutSelectionUnavailable));
        ImGui::BeginDisabled(!compatible);
        PositionInput(state, studio, false);
        PositionInput(state, studio, true);
        AlignmentControls(state, studio);
        const Point offset = Offset(studio, element);
        ImGui::TextDisabled(Tr(i18n::Str::LayoutOffset), offset.x, offset.y);
        if (compatible) {
            ImGui::TextDisabled(Tr(i18n::Str::LayoutGridStep), constraints.x.nativeStep, constraints.y.nativeStep);
            ImGui::TextDisabled(Tr(i18n::Str::LayoutBounds),
                constraints.x.origin + constraints.x.minimum, constraints.x.origin + constraints.x.maximum,
                constraints.y.origin + constraints.y.minimum, constraints.y.origin + constraints.y.maximum);
        }
        bool modified = false;
        for (size_t index : constraints.targets)
            modified |= Offset(studio, studio.layoutElements[index]) != Point{};
        ImGui::BeginDisabled(!modified);
        if (ImGui::Button(Tr(i18n::Str::LayoutResetElement), ImVec2(-1, 0)))
            ResetTargets(state, studio, constraints.targets, i18n::Str::LayoutResetElement);
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (Ui::CollapsibleSection section{"LayoutPatchSitesBody", Tr(i18n::Str::LayoutPatchSites)}; section) {
            ImGui::TextWrapped("%s", element.id.c_str());
            ImGui::TextDisabled(Tr(i18n::Str::LayoutHardware), element.hardwareCode,
                element.screenWidth, element.screenHeight);
            for (const auto& site : element.sites)
                ImGui::TextDisabled("%c  0x%08X  %d %+d", site.vertical ? 'Y' : 'X',
                    site.address, site.original, (site.vertical ? offset.y : offset.x) / site.scale);
        }
        ImGui::PopID();
    }
    if (!state.editError.empty()) ImGui::TextWrapped("%s", state.editError.c_str());
    bool revisionModified = false;
    for (const auto& element : studio.layoutElements)
        if (InScope(state, element.hardwareCode)) revisionModified |= Offset(studio, element) != Point{};
    ImGui::BeginDisabled(!revisionModified);
    if (ImGui::Button(Tr(i18n::Str::LayoutResetScope), ImVec2(-1, 0))) ResetOffsets(state, studio, false);
    ImGui::EndDisabled();
    if (state.selectionScope != SelectionScope::All) {
        ImGui::BeginDisabled(studio.layoutOffsets.empty());
        if (ImGui::Button(Tr(i18n::Str::LayoutResetAll), ImVec2(-1, 0))) ResetOffsets(state, studio, true);
        ImGui::EndDisabled();
    }
    ImGui::Separator();
    OverlapControls(state, studio);
    PreviewControls(state, studio);
}

void Canvas(State& state, StudioState& studio, ID3D11Device* device, ID3D11DeviceContext* context) {
    const ImVec2 size(static_cast<float>(state.canvasWidth * state.zoom),
        static_cast<float>(state.canvasHeight * state.zoom));
    const float inset = (ImGui::GetContentRegionAvail().x - size.x) * 0.5f;
    if (inset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + inset);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("LayoutCanvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_EnableNav);
    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    const bool deactivated = ImGui::IsItemDeactivated();
    NudgeSelection(state, studio, ImGui::GetItemID());
    RefreshPreview(state, studio, device, context);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    size_t hit = SIZE_MAX;
    if (hovered && !state.dragging) {
        const float x = (mouse.x - origin.x) / state.zoom;
        const float y = (mouse.y - origin.y) / state.zoom;
        int smallestArea = INT_MAX;
        for (size_t index : state.visibleElements) {
            const auto& element = studio.layoutElements[index];
            const Point offset = Offset(studio, element);
            const int left = element.position.x + offset.x, top = element.position.y + offset.y;
            if (x >= left && x < left + element.width && y >= top && y < top + element.height &&
                element.width * element.height <= smallestArea) {
                hit = index;
                smallestArea = element.width * element.height;
            }
        }
    }
    if (ImGui::IsItemActivated() && hit != SIZE_MAX) {
        EndAction(state, studio);
        state.dragging = false;
        state.editError.clear();
        state.selected = hit;
        if (SelectionConstraints(state, studio).available) {
            state.dragOffset = Offset(studio, studio.layoutElements[hit]);
            state.dragMouseX = mouse.x;
            state.dragMouseY = mouse.y;
            state.dragging = true;
            BeginMove(state, studio, studio.layoutElements[hit]);
        }
    }
    if (active && state.dragging && state.selected < studio.layoutElements.size()) {
        const auto& element = studio.layoutElements[state.selected];
        const Point next = MovementOffset(state, studio, element,
            state.dragOffset.x + (mouse.x - state.dragMouseX) / state.zoom,
            state.dragOffset.y + (mouse.y - state.dragMouseY) / state.zoom);
        ApplySelectionOffset(state, studio, next);
        if (Offset(studio, element) != next) state.guideX = state.guideY = false;
        RefreshPreview(state, studio, device, context);
    }
    if (deactivated || (state.dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
        EndAction(state, studio);
        state.dragging = false;
    }
    if (!state.previewError.empty()) return;
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 end(origin.x + size.x, origin.y + size.y);
    if (state.previewReady) draw->AddImage((ImTextureID)state.texture.GetSRV(), origin, end);
    else draw->AddRectFilled(origin, end, ImGui::GetColorU32(ImGuiCol_FrameBg));
    draw->PushClipRect(origin, end, true);
    if (state.pixelGrid && (state.zoom >= 3 || state.gridSpacing > 1)) {
        const ImU32 color = ImGui::GetColorU32(ImGuiCol_Border, 0.65f);
        for (int x = state.gridSpacing; x < state.canvasWidth; x += state.gridSpacing)
            draw->AddLine(ImVec2(origin.x + x * state.zoom, origin.y),
                ImVec2(origin.x + x * state.zoom, end.y), color);
        for (int y = state.gridSpacing; y < state.canvasHeight; y += state.gridSpacing)
            draw->AddLine(ImVec2(origin.x, origin.y + y * state.zoom),
                ImVec2(end.x, origin.y + y * state.zoom), color);
    }
    if (state.overlapOverlays) {
        RefreshOverlaps(state, studio);
        const ImU32 fill = IM_COL32(255, 100, 60, 65), border = IM_COL32(255, 100, 60, 225);
        for (const auto& pair : state.overlaps) {
            const auto& rect = pair.intersection;
            const ImVec2 top(origin.x + rect.x * state.zoom, origin.y + rect.y * state.zoom);
            const ImVec2 bottom(top.x + rect.width * state.zoom, top.y + rect.height * state.zoom);
            draw->AddRectFilled(top, bottom, fill);
            draw->AddRect(top, bottom, border);
        }
    }
    if (state.elementOverlays) for (size_t index : state.visibleElements) {
        const auto& element = studio.layoutElements[index];
        const Point offset = Offset(studio, element);
        const ImVec2 top(origin.x + (element.position.x + offset.x) * state.zoom,
            origin.y + (element.position.y + offset.y) * state.zoom);
        const ImVec2 bottom(top.x + element.width * state.zoom, top.y + element.height * state.zoom);
        const bool selected = index == state.selected;
        draw->AddRect(top, bottom, ImGui::GetColorU32(selected ? ImGuiCol_NavCursor : ImGuiCol_TextDisabled,
            selected || hit == index ? 1.0f : 0.55f), 0, 0, selected ? 2.0f : 1.0f);
    }
    if (state.movementGuides && (state.dragging || state.keyMoving) &&
        state.selected < studio.layoutElements.size()) {
        const auto rect = ElementRect(studio, studio.layoutElements[state.selected]);
        const float x = origin.x + (state.guideX ? state.guideXTwice * 0.5f : float(rect.x)) * state.zoom;
        const float y = origin.y + (state.guideY ? state.guideYTwice * 0.5f : float(rect.y)) * state.zoom;
        const ImU32 color = ImGui::GetColorU32(ImGuiCol_PlotHistogram, 0.95f);
        draw->AddLine(ImVec2(x, origin.y), ImVec2(x, end.y), color);
        draw->AddLine(ImVec2(origin.x, y), ImVec2(end.x, y), color);
    }
    draw->PopClipRect();
    if (hovered && (hit != SIZE_MAX || state.dragging)) {
        if (state.dragging || hit != state.selected || SelectionConstraints(state, studio).available)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        if (!state.dragging) ImGui::SetTooltip("%s", ElementTitle(studio.layoutElements[hit]));
    }
}
} // namespace

void SelectScope(State& state, StudioState& studio, SelectionScope scope, std::span<const uint16_t> codes) {
    SynchronizeSource(state, studio);
    if (scope != SelectionScope::All) {
        if ((scope == SelectionScope::Revision && codes.size() != 1) ||
            (scope == SelectionScope::Group && codes.size() <= 1) ||
            std::any_of(codes.begin(), codes.end(), [&](uint16_t code) {
                return std::find(state.hardwareCodes.begin(), state.hardwareCodes.end(), code) ==
                    state.hardwareCodes.end();
            })) {
            state.editError = Tr(i18n::Str::LayoutSelectionUnavailable);
            return;
        }
    }
    EndAction(state, studio);
    state.dragging = false;
    state.editError.clear();
    state.selectionScope = scope;
    state.scopeHardwareCodes.assign(codes.begin(), codes.end());
    state.scopeTitle.clear();
    if (scope == SelectionScope::All) state.scopeHardwareCodes.clear();
    else {
        for (uint16_t code : state.scopeHardwareCodes) {
            char title[64];
            snprintf(title, sizeof(title), Tr(i18n::Str::LayoutRevisionCode), code);
            if (!state.scopeTitle.empty()) state.scopeTitle += ", ";
            state.scopeTitle += title;
        }
        SelectRevision(state, studio, state.scopeHardwareCodes.front());
    }
    ++state.selectionToken;
}

const LayoutEdit::Constraints& SelectionConstraints(State& state, const StudioState& studio) {
    if (state.constraintsSource != &studio || state.constraintsRevision != studio.ResourceRevision() ||
        state.constraintsToken != state.selectionToken || state.constraintsElement != state.selected) {
        state.constraintsSource = &studio;
        state.constraintsRevision = studio.ResourceRevision();
        state.constraintsToken = state.selectionToken;
        state.constraintsElement = state.selected;
        LayoutEdit::Resolve(studio.layoutElements, studio.layoutOffsets, state.selected,
            ScopeCodes(state), state.constraints, &state.device);
    }
    return state.constraints;
}

bool ApplySelectionOffset(State& state, StudioState& studio, Point desiredOffset) {
    const auto& constraints = SelectionConstraints(state, studio);
    if (!constraints.available) {
        state.editError = Tr(i18n::Str::LayoutSelectionUnavailable);
        return false;
    }
    const auto revision = studio.ResourceRevision();
    if (!LayoutEdit::Apply(studio, constraints, desiredOffset, state.editError)) return false;
    if (studio.ResourceRevision() != revision) MarkEdited(state, studio);
    return true;
}
void PrepareRevisionGroups(State& state, StudioState& studio, bool synchronous) {
    SynchronizeSource(state, studio);
    if (!state.revisionGroupsReady && studio.hasFirmware && !studio.layoutElements.empty() &&
        FirmwareEmulator::Supports(studio)) UpdateRevisionGroups(state, studio, synchronous);
}


void Deactivate(State& state, StudioState& studio) {
    EndAction(state, studio);
    state.dragging = false;
    state.activeTab = false;
}

void RenderTab(State& state, StudioState& studio, ID3D11Device* device, ID3D11DeviceContext* context,
    UiFirmwarePatches::State& patches) {
    SynchronizeSource(state, studio);
    SynchronizeView(state, studio);
    state.activeTab = false;
    char title[128];
    snprintf(title, sizeof(title), "%s###TabFirmwareLayout", Tr(i18n::Str::TabFirmwareLayout));
    if (!Ui::BeginTabItem(title)) {
        EndAction(state, studio);
        state.dragging = false;
        return;
    }
    state.activeTab = true;
    if (!studio.hasFirmware || studio.layoutElements.empty() || !FirmwareEmulator::Supports(studio)) {
        EndAction(state, studio);
        state.dragging = false;
        ImGui::TextWrapped("%s", Tr(i18n::Str::LayoutUnsupported));
        if (!studio.layoutError.empty()) ImGui::TextWrapped("%s", studio.layoutError.c_str());
        ImGui::EndTabItem();
        return;
    }
    const bool locked = !UiFirmwareRisk::Accepted();
    if (locked) {
        EndAction(state, studio);
        state.dragging = false;
    }
    const ImVec2 overlayOrigin = ImGui::GetCursorScreenPos(), overlaySize = ImGui::GetContentRegionAvail();
    ImGui::BeginDisabled(locked);
    UpdateRevisionGroups(state, studio, false);
    ImGui::TextWrapped("%s", Tr(i18n::Str::LayoutNativeNote));
    const float controlsWidth = (std::min)(ImGui::GetFontSize() * 25, ImGui::GetContentRegionAvail().x * 0.48f);
    if (ImGui::BeginChild("LayoutControls", ImVec2(controlsWidth, 0), true)) ElementControls(state, studio);
    ImGui::EndChild();
    SynchronizeView(state, studio);
    ImGui::SameLine();
    ImGui::BeginChild("LayoutRight", ImVec2(0, 0), false);
    if (Ui::BeginBottomAnchoredPreview("LayoutPreview", ImGuiWindowFlags_HorizontalScrollbar)) {
        if (Ui::BeginControlTable("LayoutPreviewControls",
            (std::min)(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 30))) {
            Ui::ControlRow(Tr(i18n::Str::LabelZoom));
            Ui::ZoomSlider("##Zoom", state.zoom);
            Ui::ControlRow(Tr(i18n::Str::LayoutGridSpacing));
            if (ImGui::InputInt("##GridSpacing", &state.gridSpacing))
                state.gridSpacing = std::clamp(state.gridSpacing, 1, 64);
            ImGui::EndTable();
        }
        ImGui::Checkbox(Tr(i18n::Str::LabelPixelGrid), &state.pixelGrid);
        ImGui::Checkbox(Tr(i18n::Str::LayoutSnapGrid), &state.snapGrid);
        if (state.snapGrid && state.selected < studio.layoutElements.size()) {
            const auto& constraints = SelectionConstraints(state, studio);
            if (constraints.available &&
                (!LayoutGeometry::GridLattice(constraints.x, state.gridSpacing).valid ||
                 !LayoutGeometry::GridLattice(constraints.y, state.gridSpacing).valid))
                ImGui::TextWrapped("%s", Tr(i18n::Str::LayoutGridConstraint));
        }
        ImGui::Checkbox(Tr(i18n::Str::LayoutSnapElements), &state.snapElements);
        ImGui::Checkbox(Tr(i18n::Str::LayoutGuides), &state.movementGuides);
        ImGui::Checkbox(Tr(i18n::Str::LayoutOverlays), &state.elementOverlays);
        ImGui::Checkbox(Tr(i18n::Str::LayoutOverlapOverlay), &state.overlapOverlays);
        ImGui::TextWrapped("%s", Tr(i18n::Str::LayoutNudgeHint));
        if (ImGui::Button(Tr(i18n::Str::EmulatorRedraw))) state.dirty = true;
        RefreshPreview(state, studio, device, context);
        if (state.Frame().emulatedLoaderMetadata)
            ImGui::TextWrapped("%s", Tr(i18n::Str::EmulatorLoaderNote));
        if (state.previewError.empty() && state.canvasWidth > 0 && state.canvasHeight > 0)
            Canvas(state, studio, device, context);
        if (!state.previewError.empty()) {
            if (state.dragging || state.keyMoving) EndAction(state, studio);
            state.dragging = false;
            ImGui::TextWrapped("%s", Tr(i18n::Str::LayoutRenderFailed));
            ImGui::TextWrapped("%s", state.previewError.c_str());
        } else if (state.previewReady) {
            ImGui::TextDisabled(Tr(i18n::Str::LayoutHardware), state.Frame().hardwareCode,
                state.Frame().width, state.Frame().height);
        }
    }
    if (state.keyMoving && !ImGui::IsWindowFocused()) EndAction(state, studio);
    ImGui::EndChild();
    UiFirmwarePatches::RenderMemory(patches, studio);
    ImGui::EndChild();
    ImGui::EndDisabled();
    if (locked) UiFirmwareRisk::Overlay(overlayOrigin, overlaySize);
    ImGui::EndTabItem();
}
} // namespace UiFirmwareLayout
