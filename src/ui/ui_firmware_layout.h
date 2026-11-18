#pragma once

#include "firmware_emulator.h"
#include "firmware_preview.h"
#include "firmware_layout.h"
#include "layout_edit_selection.h"
#include "texture_cache.h"
#include "studio_preferences.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace UiFirmwarePatches { struct State; }
namespace UiFirmwareLayout {
struct RevisionGroup {
    std::vector<uint16_t> hardwareCodes;
    // Each row maps one representative element to its matching group members.
    std::vector<std::vector<size_t>> elements;
    std::string title;
    size_t nativeSamples = 0;
};

struct GroupingWorker;
struct Overlap {
    size_t first = 0, second = 0;
    LayoutGeometry::Rect intersection;
};

enum class SelectionScope { All, Group, Revision };


struct State {
    FirmwarePreview::Cache preview{FirmwarePreview::Cache::Mode::CurrentOnly};
    FirmwareEmulator::DeviceState device, requestedDevice;
    std::shared_ptr<const FirmwarePreview::Cache::Publication> publication;
    const FirmwareEmulator::Frame& Frame() const {
        static const FirmwareEmulator::Frame empty;
        return publication && publication->after ? *publication->after : empty;
    }
    int canvasWidth = 0, canvasHeight = 0;
    bool previewReady = false;
    std::string previewError;
    Dx11Texture texture;
    const StudioState* source = nullptr;
    const uint8_t* sourceBytes = nullptr;
    const FirmwareLayout::Element* sourceElements = nullptr;
    size_t sourceSize = 0;
    size_t elementCount = 0;
    std::string sourceHash;
    uint64_t documentIdentity = 0;
    uint64_t revision = UINT64_MAX;
    uint64_t observedRevision = UINT64_MAX;
    ID3D11Device* textureDevice = nullptr;
    std::vector<uint16_t> hardwareCodes;
    std::vector<RevisionGroup> revisionGroups;
    bool revisionGroupsReady = false;
    std::shared_ptr<GroupingWorker> groupingWorker;
    uint64_t groupingGeneration = 0;
    SelectionScope selectionScope = SelectionScope::All;
    std::vector<uint16_t> scopeHardwareCodes;
    std::string scopeTitle;
    uint64_t selectionToken = 0;
    LayoutEdit::Constraints constraints;
    const StudioState* constraintsSource = nullptr;
    uint64_t constraintsRevision = UINT64_MAX;
    uint64_t constraintsToken = UINT64_MAX;
    size_t constraintsElement = SIZE_MAX;
    std::vector<size_t> visibleElements;
    uint32_t visibleScreen = UINT32_MAX;
    bool visibleTemperature = false, visibleLandscape = false;
    size_t selected = SIZE_MAX;
    int zoom = StudioPreferences::Get().layoutZoom;
    bool pixelGrid = false;
    int gridSpacing = 4;
    bool snapGrid = false;
    bool snapElements = true;
    bool movementGuides = true;
    bool elementOverlays = true;
    bool overlapOverlays = true;
    bool keyMoving = false;
    bool guideX = false, guideY = false;
    int guideXTwice = 0, guideYTwice = 0;
    std::vector<Overlap> overlaps;
    uint64_t overlapRevision = UINT64_MAX;
    uint16_t overlapHardwareCode = 0;
    bool dirty = true;
    bool activeTab = false;
    bool historyOpen = false;
    bool dragging = false;
    FirmwareLayout::Point dragOffset;
    float dragMouseX = 0, dragMouseY = 0;
    std::string editError;
};

// Starts/polls isolated native discovery without blocking. Headless verification
// may explicitly request synchronous discovery; the UI never does.
void PrepareRevisionGroups(State& state, StudioState& studio, bool synchronous = false);

// Explicit user scope changes; discovery only updates the available choices.
void SelectScope(State& state, StudioState& studio, SelectionScope scope,
    std::span<const uint16_t> codes = {});
const LayoutEdit::Constraints& SelectionConstraints(State& state, const StudioState& studio);
// Caller owns the single Undo action containing the complete target cohort.
bool ApplySelectionOffset(State& state, StudioState& studio, FirmwareLayout::Point desiredOffset);

void RenderTab(State& state, StudioState& studio, ID3D11Device* device, ID3D11DeviceContext* context,
    UiFirmwarePatches::State& patches);
void Deactivate(State& state, StudioState& studio);
} // namespace UiFirmwareLayout
