#pragma once
#include "firmware_logo_patch.h"
#include "firmware_preview.h"
#include "firmware_bitmap.h"
#include "texture_cache.h"
#include "patch_playback.h"
#include "ui_modal_transition.h"
#include "ui_firmware_menu.h"
#include <future>
#include <optional>

namespace UiFirmwarePatches {
struct DraftSnapshot {
    FirmwareLogo::Settings settings;
    FirmwareLogo::EditableSource source;
    FirmwareLogo::EditableSource commitSource;
    bool sourceModified = false, sourceReplaced = false;
};
struct ImageDraft {
    FirmwareLogo::Settings draft, observed;
    // A fresh stock document's preferred boot stage is UI-only until edited.
    std::optional<FirmwareLogo::BootStages> initialBootStages;
    FirmwareLogo::EditableSource observedSource;
    // Undo can restore a project with no source while retaining its reconstructed
    // editable image. Keep that project metadata distinct across publication.
    std::optional<FirmwareLogo::EditableSource> restoredSource;
    bool sourceModified = false, sourceReplaced = false;
    std::vector<uint8_t> source;
    std::shared_ptr<const std::vector<uint8_t>> sourceImage;
    DraftSnapshot checkpoint;
    std::vector<DraftSnapshot> undo;
    uint32_t undoGesture = 0;
    int sourceWidth = 0, sourceHeight = 0;
    int fitMode = 0; // 0: aspect fit, 1: aspect cover, 2: stretch.
    float scalePercent = 100.0f;
    int x = 0, y = 0;
    int alignX = 1, alignY = 1; // -1: manual, 0: start, 1: center, 2: end.
    FirmwareBitmap::Resampling resampling = FirmwareBitmap::Resampling::Lanczos;
    FirmwareBitmap::ImageAdjustments adjustments, scaledAdjustments;
    std::array<int, 7> scaledKey{};
    std::vector<uint32_t> scaled;
    std::vector<uint32_t> canvas;
    std::array<std::array<uint32_t, 256>, 4> histogram{};
    std::array<uint32_t, 4> histogramMaximum{};
    uint64_t histogramGeneration = 0;
    int curveChannel = 0; // RGB master, R, G, B.
    std::string error;
};
enum class ClipboardPart : uint32_t { Image = 1, Placement = 2, Adjustments = 4, Animation = 8, All = 15 };
enum class AnimationPart : uint32_t { None = 0, EntranceEffect = 1, ExitEffect = 2, Durations = 4, All = 7 };
struct State : ImageDraft {
    FirmwareLogo::Target target = FirmwareLogo::Target::Boot;
    // The active image lives in the ImageDraft base; other targets live here.
    std::array<ImageDraft, FirmwareLogo::TargetCount> drafts;
    uint64_t documentIdentity = 0;
    uint64_t resourceRevision = 0;
    bool initialized = false;
    bool activeTab = false;
    ClipboardPart clipboardPart = ClipboardPart::All;
    AnimationPart clipboardAnimation = AnimationPart::All;
    bool requestPaste = false;
    bool pasteOpen = false;
    Ui::ModalTransition pasteTransition;
    FirmwareLogo::Patches previewPatches;
    uint64_t draftRevision = 1, requestedDraftRevision = 0;
    uint64_t previewSourceRevision = UINT64_MAX;
    std::string buildError;
    Dx11Texture nativeTexture;
    bool nativeTextureError = false;
    ID3D11Device* textureDevice = nullptr;
    ID3D11DeviceContext* textureContext = nullptr;
    FirmwarePreview::Cache preview{FirmwarePreview::Cache::Mode::CurrentOnly};
    FirmwareEmulator::DeviceState device;
    uint64_t nativeGeneration = 0;
    uint64_t compositionTextureRevision = UINT64_MAX;
    bool forceNativeRefresh = false;
    // Direct effect rendering is independent of background native validation.
    std::vector<uint32_t> animationFrame;
    uint64_t phaseKey = UINT64_MAX;
    int uploadedPhase = -1;
    Playback playback;
    int zoom = StudioPreferences::Get().patchZoom;
    UiFirmwareMenu::Preview menuPreview;
    struct MemoryUsage : FirmwareLogo::FlashUsage {
        int64_t menuDelta = 0;
        FirmwareMenu::FlashBreakdown menuCosts;
    };
    struct MemoryResult {
        std::vector<MemoryUsage> usage;
        std::string error;
        FirmwareMenu::Settings menu;
    };
    std::future<MemoryResult> memoryJob;
    MemoryResult memory;
    uint64_t memoryDocument = 0, memorySource = UINT64_MAX, memoryDraft = 0;
};
// Initializes only fresh documents; refreshes externally changed applied settings.
void SynchronizeDocument(State& state, const StudioState& studio);
// Switches active composition without losing PNG source, placement, or draft settings.
void SelectTarget(State& state, FirmwareLogo::Target target);
// Helpers operate on the currently selected target (Boot by default).
// Load the original RGBA image and compose it using the selected fit mode.
bool LoadImage(State& state, const std::string& path, std::string& error);
// Recompose draft.pixels and canvas after placement/scale changes. Pixels are RGB565.
void ComposeLogo(State& state, uint32_t editGesture = 0);
// Repositions only the axes with an active edge/center alignment.
void AlignImage(State& state);
// Record one draft action; a nonzero widget ID coalesces a continuous drag.
void RecordDraftChange(State& state, uint32_t editGesture = 0);
// Draft history is target-local and never changes the applied studio history.
bool UndoDraft(State& state);
bool HasDraftChanges(const State& state, const StudioState& studio);
// The native preview and flash accounting must use the same complete draft set.
FirmwareLogo::Patches ResolveDraftPatches(const State& state, const StudioState& studio);
// Emulator publication retains local undo; saving/exporting closes draft history.
void CommitDrafts(State& state, StudioState& studio, bool retainHistory = false);
// Clipboard data includes the original image, so pasting retains editable placement.
bool CopyToClipboard(State& state, HWND hwnd);
bool PasteFromClipboard(State& state, HWND hwnd);
void RenderTab(State& state, StudioState& studio, ID3D11Device* device, ID3D11DeviceContext* context);
void RenderMemory(State& state, const StudioState& studio);
}
