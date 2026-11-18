#include "ui_tabs.h"
#include "ui_preview_split.h"
#include "ui_firmware_patches.h"
#include "app_state.h"
#include "firmware_bitmap.h"
#include "firmware_logo_preview.h"
#include "path_utils.h"
#include "ui_firmware_risk.h"
#include "ui_collapsible_section.h"
#include "imgui.h"
#include "ui_keyboard.h"
#include "ui_controls.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace UiFirmwarePatches {
namespace {
constexpr int Width = FirmwareLogo::Width;
constexpr int Height = FirmwareLogo::Height;

bool ValidSourceSize(int width, int height, size_t bytes) {
    return width > 0 && height > 0 &&
        width <= FirmwareLogo::MaximumEditableSourceDimension &&
        height <= FirmwareLogo::MaximumEditableSourceDimension &&
        uint64_t(width) * height * 4 <= FirmwareLogo::MaximumEditableSourceBytes &&
        bytes == uint64_t(width) * height * 4;
}

i18n::Str TargetTitle(FirmwareLogo::Target target) {
    switch (target) {
    case FirmwareLogo::Target::Shutdown: return i18n::Str::PatchShutdownLogo;
    case FirmwareLogo::Target::DeviceInfo: return i18n::Str::PatchDeviceInfo;
    default: return i18n::Str::PatchBootLogo;
    }
}

const FirmwareLogo::Settings& Applied(const State& state, const StudioState& studio) {
    return studio.screenPatches[static_cast<size_t>(state.target)];
}

FirmwareLogo::EditableSource DraftSourceMetadata(const ImageDraft& state) {
    FirmwareLogo::EditableSource source;
    source.rgba = state.sourceImage;
    source.width = state.sourceWidth; source.height = state.sourceHeight;
    source.fitMode = state.fitMode; source.scalePercent = state.scalePercent;
    source.x = state.x; source.y = state.y;
    source.alignX = state.alignX; source.alignY = state.alignY;
    source.resampling = state.resampling;
    source.adjustments = state.adjustments;
    return source;
}

bool SourceChanged(const ImageDraft& state) {
    if (state.restoredSource) return *state.restoredSource != state.observedSource;
    if (!state.sourceModified) return false;
    if (state.sourceReplaced || (!state.source.empty() && !state.observedSource.rgba)) return true;
    return DraftSourceMetadata(state) != state.observedSource;
}
bool DraftSettingsChanged(const ImageDraft& state) {
    if (!state.initialBootStages) return state.draft != state.observed;
    // Only pristine stock boot settings receive a preference baseline. Keeping
    // that baseline separate from observed preserves applied-document identity.
    FirmwareLogo::Settings initial;
    initial.bootStages = *state.initialBootStages;
    return state.draft != initial;
}


FirmwareLogo::EditableSource SourceForCommit(const ImageDraft& state) {
    if (state.restoredSource) return *state.restoredSource;
    if (!state.sourceModified) return state.observedSource;
    return DraftSourceMetadata(state);
}

DraftSnapshot Snapshot(const ImageDraft& state) {
    return {state.draft, DraftSourceMetadata(state), SourceForCommit(state), state.sourceModified, state.sourceReplaced};
}

void ResetDraftHistory(ImageDraft& state) {
    state.undo.clear();
    state.undoGesture = 0;
    state.checkpoint = Snapshot(state);
}

void RestartPreview(State& state) {
    state.playback.Stop();
    // Invalidate every cached phase; the next DrawNative re-renders them in order.
    state.phaseKey = UINT64_MAX;
    state.buildError.clear();
}

void UpdateHistogram(ImageDraft& state) {
    for (auto& channel : state.histogram) channel.fill(0);
    state.histogramMaximum.fill(0);
    for (const uint32_t pixel : state.canvas) {
        const uint32_t red = pixel & 255, green = (pixel >> 8) & 255, blue = (pixel >> 16) & 255;
        const uint32_t values[]{(77 * red + 150 * green + 29 * blue + 128) >> 8, red, green, blue};
        for (size_t channel = 0; channel < std::size(values); ++channel) {
            const uint32_t count = ++state.histogram[channel][values[channel]];
            state.histogramMaximum[channel] = (std::max)(state.histogramMaximum[channel], count);
        }
    }
    ++state.histogramGeneration;
}

void ReadSettings(ImageDraft& state, const FirmwareLogo::Settings& settings,
    const FirmwareLogo::EditableSource& source) {
    state.draft = settings;
    state.observed = settings;
    state.initialBootStages.reset();
    state.observedSource = source;
    state.sourceModified = state.sourceReplaced = false;
    state.restoredSource.reset();
    state.source.clear();
    state.sourceWidth = state.sourceHeight = 0;
    state.fitMode = 0;
    state.scalePercent = 100.0f;
    state.x = state.y = 0;
    state.alignX = state.alignY = 1;
    state.resampling = FirmwareBitmap::Resampling::Lanczos;
    state.adjustments = source.adjustments;
    state.scaled.clear();
    state.scaledKey = {};
    state.canvas.assign(Width * Height, 0xff000000);
    FirmwareBitmap::DecodeRgb565(settings.pixels.data(), settings.pixels.size(), Width, Height, state.canvas);
    UpdateHistogram(state);
    if (source.rgba) {
        state.source = *source.rgba;
        state.sourceWidth = source.width; state.sourceHeight = source.height;
        state.fitMode = source.fitMode; state.scalePercent = source.scalePercent;
        state.x = source.x; state.y = source.y;
        state.alignX = source.alignX; state.alignY = source.alignY;
        state.resampling = source.resampling;
    } else if (settings.pixels.size() == size_t(Width) * Height * 2) {
        // Legacy projects and firmware retain only the composed RGB565 frame.
        state.source.resize(Width * Height * 4);
        for (size_t i = 0; i < state.canvas.size(); ++i) {
            const uint32_t pixel = state.canvas[i];
            for (unsigned c = 0; c < 4; ++c)
                state.source[i * 4 + c] = static_cast<uint8_t>(pixel >> (c * 8));
        }
        state.sourceWidth = Width;
        state.sourceHeight = Height;
    }
    state.sourceImage = source.rgba;
    if (!state.sourceImage && !state.source.empty())
        state.sourceImage = std::make_shared<const std::vector<uint8_t>>(state.source);
    ResetDraftHistory(state);
    state.error.clear();
}

std::pair<int, int> ImageSize(const State& state) {
    if (!state.sourceWidth || !state.sourceHeight) return {Width, Height};
    const float sx = float(Width) / state.sourceWidth;
    const float sy = float(Height) / state.sourceHeight;
    const float uniform = state.fitMode == 1 ? (std::max)(sx, sy) : (std::min)(sx, sy);
    const float scale = state.scalePercent / 100.0f;
    return {
        (std::max)(1, int(std::lround(state.sourceWidth * (state.fitMode == 2 ? sx : uniform) * scale))),
        (std::max)(1, int(std::lround(state.sourceHeight * (state.fitMode == 2 ? sy : uniform) * scale)))
    };
}

void CenterImage(State& state) {
    state.alignX = state.alignY = 1;
    AlignImage(state);
}
}
void SynchronizeDocument(State& state, const StudioState& studio) {
    if (!state.initialized || state.documentIdentity != studio.DocumentIdentity()) {
        state.initialized = true;
        state.documentIdentity = studio.DocumentIdentity();
        state.device = FirmwareEmulator::DeviceState{};
        state.target = static_cast<FirmwareLogo::Target>(
            (std::clamp)(StudioPreferences::Get().defaultPatchTarget, 0, int(FirmwareLogo::TargetCount) - 1));
        const bool stockDocument = !studio.firmwareSourceEmbedded && studio.projectPath.empty() &&
            std::all_of(studio.originalScreenPatches.begin(), studio.originalScreenPatches.end(),
                [](const auto& settings) { return settings == FirmwareLogo::Settings{}; });
        for (size_t i = 0; i < FirmwareLogo::TargetCount; ++i) {
            auto& image = i == size_t(state.target) ? static_cast<ImageDraft&>(state) : state.drafts[i];
            ReadSettings(image, studio.screenPatches[i], studio.screenPatchSources[i]);
            if (i == size_t(FirmwareLogo::Target::Boot) && stockDocument &&
                image.draft == FirmwareLogo::Settings{} && !image.observedSource.rgba) {
                image.initialBootStages = static_cast<FirmwareLogo::BootStages>(
                    (std::clamp)(StudioPreferences::Get().defaultBootStages, 1, 3));
                image.draft.bootStages = *image.initialBootStages;
                ResetDraftHistory(image);
            }
        }
        ++state.draftRevision;
        RestartPreview(state);
    } else if (state.resourceRevision != studio.ResourceRevision()) {
        for (size_t i = 0; i < FirmwareLogo::TargetCount; ++i) {
            auto& image = i == size_t(state.target) ? static_cast<ImageDraft&>(state) : state.drafts[i];
            if (image.observed == studio.screenPatches[i] &&
                image.observedSource == studio.screenPatchSources[i]) continue;
            ReadSettings(image, studio.screenPatches[i], studio.screenPatchSources[i]);
            ++state.draftRevision;
        }
    }
    state.resourceRevision = studio.ResourceRevision();
    state.playback.target = state.target;
}


void SelectTarget(State& state, FirmwareLogo::Target target) {
    state.undoGesture = 0;
    if (state.target == target) return;
    std::swap(static_cast<ImageDraft&>(state), state.drafts[static_cast<size_t>(state.target)]);
    std::swap(static_cast<ImageDraft&>(state), state.drafts[static_cast<size_t>(target)]);
    state.target = target;
    state.playback.target = state.target;
    state.undoGesture = 0;
    ++state.draftRevision;
    RestartPreview(state);
}

void RecordDraftChange(State& state, uint32_t editGesture) {
    auto current = Snapshot(state);
    if (current.settings == state.checkpoint.settings && current.source == state.checkpoint.source &&
        current.commitSource == state.checkpoint.commitSource) return;
    if (!editGesture || state.undoGesture != editGesture) {
        constexpr size_t MaximumDraftUndo = 64;
        if (state.undo.size() == MaximumDraftUndo) state.undo.erase(state.undo.begin());
        state.undo.push_back(std::move(state.checkpoint));
    }
    state.checkpoint = std::move(current);
    state.undoGesture = editGesture;
}

bool UndoDraft(State& state) {
    if (state.undo.empty()) return false;
    auto previous = std::move(state.undo.back());
    state.undo.pop_back();
    state.draft = std::move(previous.settings);
    const auto& source = previous.source;
    if (state.sourceImage != source.rgba)
        state.source = source.rgba ? *source.rgba : std::vector<uint8_t>{};
    state.sourceImage = source.rgba;
    state.sourceWidth = source.width; state.sourceHeight = source.height;
    state.fitMode = source.fitMode; state.scalePercent = source.scalePercent;
    state.x = source.x; state.y = source.y;
    state.alignX = source.alignX; state.alignY = source.alignY;
    state.resampling = source.resampling;
    state.adjustments = source.adjustments;
    state.sourceModified = previous.sourceModified;
    state.sourceReplaced = previous.sourceReplaced;
    state.restoredSource = std::move(previous.commitSource);
    state.scaled.clear();
    state.scaledKey = {};
    state.canvas.assign(Width * Height, 0xff000000);
    FirmwareBitmap::DecodeRgb565(state.draft.pixels.data(), state.draft.pixels.size(), Width, Height, state.canvas);
    UpdateHistogram(state);
    state.error.clear();
    state.undoGesture = 0;
    state.checkpoint = Snapshot(state);
    ++state.draftRevision;
    RestartPreview(state);
    return true;
}

void AlignImage(State& state) {
    const auto [width, height] = ImageSize(state);
    if (state.alignX >= 0) state.x = (Width - width) * state.alignX / 2;
    if (state.alignY >= 0) state.y = (Height - height) * state.alignY / 2;
}

void ComposeLogo(State& state, uint32_t editGesture) {
    state.sourceModified = true;
    state.restoredSource.reset();
    state.canvas.assign(Width * Height, 0xff000000);
    if (!state.source.empty()) {
        const auto [width, height] = ImageSize(state);
        const int left = (std::max)(0, state.x), top = (std::max)(0, state.y);
        const int right = (std::min)(Width, state.x + width), bottom = (std::min)(Height, state.y + height);
        if (left < right && top < bottom) {
            const std::array<int, 7> key{width, height, left - state.x, top - state.y,
                right - left, bottom - top, int(state.resampling)};
            if (state.scaled.empty() || state.scaledKey != key || state.scaledAdjustments != state.adjustments) {
                FirmwareBitmap::ResizeRgbaOnBlack(state.source.data(), state.sourceWidth, state.sourceHeight,
                    width, height, state.resampling, state.scaled, key[2], key[3], key[4], key[5]);
                FirmwareBitmap::ApplyAdjustments(state.scaled, key[4], key[5], state.adjustments);
                state.scaledAdjustments = state.adjustments;
                state.scaledKey = key;
            }
            for (int y = top; y < bottom; ++y)
                std::copy_n(state.scaled.data() + size_t(y - top) * (right - left), right - left,
                    state.canvas.data() + y * Width + left);
        }
    }
    FirmwareBitmap::EncodeRgb565(state.canvas.data(), Width, Height, state.draft.pixels);
    // Show the quantized device pixels, not an unrepresentable 24-bit approximation.
    FirmwareBitmap::DecodeRgb565(state.draft.pixels.data(), state.draft.pixels.size(), Width, Height, state.canvas);
    UpdateHistogram(state);
    RecordDraftChange(state, editGesture);
    ++state.draftRevision;
    RestartPreview(state);
}

bool LoadImage(State& state, const std::string& path, std::string& error) {
    std::vector<uint8_t> source;
    int width = 0, height = 0;
    if (!FirmwareBitmap::LoadImageRgba(path, source, width, height, error)) return false;
    if (!ValidSourceSize(width, height, source.size())) {
        error = Tr(i18n::Str::PatchSourceError);
        return false;
    }
    state.source = std::move(source);
    state.sourceWidth = width; state.sourceHeight = height;
    state.sourceReplaced = true;
    state.sourceImage = std::make_shared<const std::vector<uint8_t>>(state.source);
    state.scaled.clear();
    state.scaledKey = {};
    state.scalePercent = 100.0f;
    CenterImage(state);
    state.draft.enabled = true;
    error.clear();
    ComposeLogo(state);
    return true;
}

namespace {
struct ClipboardHeader {
    uint32_t version = 1, parts = 0, target = 0, enabled = 0;
    uint32_t animation = 0, durationMs = 600, bootStages = 2;
    uint32_t exitAnimation = 0, holdMs = 0, exitDurationMs = 600;
    int32_t fitMode = 0, x = 0, y = 0, alignX = 1, alignY = 1;
    float scalePercent = 100;
    uint32_t resampling = 0, width = 0, height = 0;
    FirmwareBitmap::ImageAdjustments adjustments;
};

UINT ClipboardFormat() {
    static const UINT format = RegisterClipboardFormatW(L"VoopooFirmwareStudio.PatchDraft.v1");
    return format;
}

bool ValidClipboardHeader(const ClipboardHeader& h, size_t available) {
    if (h.version != 1 || !h.adjustments.Valid() ||
        h.parts == 0 || (h.parts & ~uint32_t(ClipboardPart::All)) || h.target >= FirmwareLogo::TargetCount ||
        h.enabled > 1 || h.animation >= uint32_t(FirmwareLogo::Animation::Count) ||
        h.exitAnimation >= uint32_t(FirmwareLogo::Animation::Count) ||
        h.durationMs < 200 || h.durationMs > 2000 || h.exitDurationMs < 200 || h.exitDurationMs > 2000 ||
        h.holdMs > 10000 || h.bootStages < 1 || h.bootStages > 3 ||
        h.fitMode < 0 || h.fitMode > 2 || !std::isfinite(h.scalePercent) ||
        h.scalePercent < 1 || h.scalePercent > 400 || h.x < -65536 || h.x > 65536 ||
        h.y < -65536 || h.y > 65536 || h.alignX < -1 || h.alignX > 2 ||
        h.alignY < -1 || h.alignY > 2 || h.resampling > 3) return false;
    return !(h.parts & uint32_t(ClipboardPart::Image)) ||
        (h.width <= FirmwareLogo::MaximumEditableSourceDimension &&
            h.height <= FirmwareLogo::MaximumEditableSourceDimension &&
            ValidSourceSize(int(h.width), int(h.height), size_t(h.width) * h.height * 4) &&
            size_t(h.width) * h.height * 4 <= available);
}
}

bool CopyToClipboard(State& state, HWND hwnd) {
    ClipboardHeader h;
    h.parts = uint32_t(ClipboardPart::All);
    if (state.source.empty()) h.parts &= ~uint32_t(ClipboardPart::Image);
    if (!h.parts) {
        state.error = Tr(i18n::Str::MsgClipboardNoImage);
        return false;
    }
    h.target = uint32_t(state.target);
    h.enabled = state.draft.enabled;
    h.animation = uint32_t(state.draft.animation);
    h.durationMs = state.draft.durationMs;
    h.bootStages = uint32_t(state.draft.bootStages);
    h.exitAnimation = uint32_t(state.draft.exitAnimation);
    h.holdMs = state.draft.holdMs;
    h.exitDurationMs = state.draft.exitDurationMs;
    h.fitMode = state.fitMode; h.scalePercent = state.scalePercent;
    h.x = state.x; h.y = state.y; h.alignX = state.alignX; h.alignY = state.alignY;
    h.resampling = uint32_t(state.resampling);
    h.adjustments = state.adjustments;
    const bool image = (h.parts & uint32_t(ClipboardPart::Image)) != 0;
    h.width = image ? state.sourceWidth : 0;
    h.height = image ? state.sourceHeight : 0;
    const size_t bytes = image ? state.source.size() : 0;
    const UINT format = ClipboardFormat();
    HGLOBAL memory = format ? GlobalAlloc(GMEM_MOVEABLE, sizeof(h) + bytes) : nullptr;
    if (!memory) { state.error = Tr(i18n::Str::PatchClipboardError); return false; }
    void* data = GlobalLock(memory);
    if (!data) { GlobalFree(memory); state.error = Tr(i18n::Str::PatchClipboardError); return false; }
    std::memcpy(data, &h, sizeof(h));
    if (bytes) std::memcpy(static_cast<uint8_t*>(data) + sizeof(h), state.source.data(), bytes);
    GlobalUnlock(memory);
    if ((image && !FirmwareBitmap::CopyImageToClipboard(hwnd, state.canvas.data(), Width, Height)) ||
        !OpenClipboard(hwnd)) {
        GlobalFree(memory);
        state.error = Tr(i18n::Str::PatchClipboardError);
        return false;
    }
    if (!image) EmptyClipboard();
    const bool ok = SetClipboardData(format, memory) != nullptr;
    CloseClipboard();
    if (!ok) GlobalFree(memory);
    state.error = ok ? "" : Tr(i18n::Str::PatchClipboardError);
    return ok;
}

bool PasteFromClipboard(State& state, HWND hwnd) {
    ClipboardHeader h;
    std::vector<uint8_t> source;
    uint32_t parts = uint32_t(state.clipboardPart);
    if (state.clipboardAnimation == AnimationPart::None) parts &= ~uint32_t(ClipboardPart::Animation);
    const UINT format = ClipboardFormat();
    if (format && IsClipboardFormatAvailable(format)) {
        if (!OpenClipboard(hwnd)) { state.error = Tr(i18n::Str::PatchClipboardError); return false; }
        HANDLE memory = GetClipboardData(format);
        const size_t bytes = memory ? GlobalSize(memory) : 0;
        const auto* data = memory ? static_cast<const uint8_t*>(GlobalLock(memory)) : nullptr;
        bool valid = data && bytes >= sizeof(h);
        if (valid) {
            std::memcpy(&h, data, sizeof(h));
            valid = ValidClipboardHeader(h, bytes - sizeof(h));
        }
        if (valid && (parts & h.parts & uint32_t(ClipboardPart::Image)))
            source.assign(data + sizeof(h), data + sizeof(h) + size_t(h.width) * h.height * 4);
        if (data) GlobalUnlock(memory);
        CloseClipboard();
        parts &= h.parts;
        if (!valid || !parts) { state.error = Tr(i18n::Str::PatchClipboardError); return false; }
    } else {
        if (!(parts & uint32_t(ClipboardPart::Image))) {
            state.error = Tr(i18n::Str::PatchClipboardError);
            return false;
        }
        std::vector<uint32_t> rgba;
        int width = 0, height = 0;
        if (!FirmwareBitmap::PasteImageFromClipboard(hwnd, rgba, width, height)) {
            state.error = Tr(i18n::Str::MsgClipboardNoImage);
            return false;
        }
        if (!ValidSourceSize(width, height, rgba.size() * 4)) {
            state.error = Tr(i18n::Str::PatchSourceError);
            return false;
        }
        h.width = width; h.height = height;
        source.resize(rgba.size() * 4);
        std::memcpy(source.data(), rgba.data(), source.size());
        parts = uint32_t(ClipboardPart::Image);
    }
    if (parts & uint32_t(ClipboardPart::Animation)) {
        state.draft.enabled = h.enabled != 0;
        const uint32_t animation = uint32_t(state.clipboardAnimation);
        if (animation & uint32_t(AnimationPart::EntranceEffect))
            state.draft.animation = FirmwareLogo::Animation(h.animation);
        if (state.target == FirmwareLogo::Target::Boot && h.target == uint32_t(FirmwareLogo::Target::Boot))
            state.draft.bootStages = FirmwareLogo::BootStages(h.bootStages);
        if (animation & uint32_t(AnimationPart::ExitEffect))
            state.draft.exitAnimation = FirmwareLogo::Animation(h.exitAnimation);
        if (animation & uint32_t(AnimationPart::Durations)) {
            state.draft.durationMs = h.durationMs;
            state.draft.exitDurationMs = h.exitDurationMs;
            if (state.target != FirmwareLogo::Target::DeviceInfo) state.draft.holdMs = h.holdMs;
        }
    }
    if (parts & uint32_t(ClipboardPart::Placement)) {
        state.fitMode = h.fitMode; state.scalePercent = h.scalePercent;
        state.x = h.x; state.y = h.y; state.alignX = h.alignX; state.alignY = h.alignY;
        state.resampling = FirmwareBitmap::Resampling(h.resampling);
    }
    if (parts & uint32_t(ClipboardPart::Adjustments)) {
        state.adjustments = h.adjustments;
    }
    if (parts & uint32_t(ClipboardPart::Image)) {
        state.source = std::move(source);
        state.sourceWidth = int(h.width); state.sourceHeight = int(h.height);
        state.sourceReplaced = true;
        state.sourceImage = std::make_shared<const std::vector<uint8_t>>(state.source);
    }
    if ((parts & uint32_t(ClipboardPart::Animation)) && state.target != FirmwareLogo::Target::Boot)
        state.draft.bootStages = FirmwareLogo::BootStages::Second;
    if (parts & (uint32_t(ClipboardPart::Image) | uint32_t(ClipboardPart::Placement))) AlignImage(state);
    if (parts & ~uint32_t(ClipboardPart::Animation)) {
        state.scaled.clear();
        state.scaledKey = {};
        ComposeLogo(state);
    } else {
        RecordDraftChange(state);
        ++state.draftRevision;
        RestartPreview(state);
    }
    state.error.clear();
    return true;
}
namespace {

bool Commit(StudioState& studio, FirmwareLogo::Target target, const FirmwareLogo::Settings& settings,
    FirmwareLogo::EditableSource source) {
    if (!source.Valid()) return false;
    const size_t index = static_cast<size_t>(target);
    if (settings == studio.screenPatches[index] && source == studio.screenPatchSources[index]) return true;
    studio.PushUndo(Tr(TargetTitle(target)));
    studio.SetScreenPatch(target, settings);
    studio.SetScreenPatchSource(target, std::move(source));
    studio.EndUndoAction();
    return true;
}

void DrawImage(const Dx11Texture& texture, int width, int height, int zoom) {
    const ImVec2 size(float(width * zoom), float(height * zoom));
    const float inset = (ImGui::GetContentRegionAvail().x - size.x) * 0.5f;
    if (inset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + inset);
    ImGui::Image((ImTextureID)texture.GetSRV(), size);
}


void DrawNative(State& state, const StudioState& studio, ID3D11Device* device, ID3D11DeviceContext* context) {
    ImGui::SeparatorText(Tr(i18n::Str::PatchPreview));
    if (state.requestedDraftRevision != state.draftRevision ||
        state.previewSourceRevision != studio.ResourceRevision()) {
        state.previewPatches = ResolveDraftPatches(state, studio);
        state.requestedDraftRevision = state.draftRevision;
        state.previewSourceRevision = studio.ResourceRevision();
        RestartPreview(state);
    }
    const auto& settings = state.draft;
    state.playback.target = state.target;
    const int frameCount = settings.enabled ? int(FirmwareLogo::FrameCount(settings)) : 1;
    const bool animated = frameCount > 1;
    if (state.target == FirmwareLogo::Target::Boot)
        state.device.screen = settings.bootStages == FirmwareLogo::BootStages::First ? 2 : 1;
    else state.device.screen = state.target == FirmwareLogo::Target::Shutdown ? 3 : 15;

    if (Ui::BeginControlTable("PatchPreviewControls",
        (std::min)(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 30))) {
    Ui::ControlRow(Tr(i18n::Str::LabelZoom));
    Ui::ZoomSlider("##Zoom", state.zoom);
    char selected[16];
    snprintf(selected, sizeof(selected), "%04X", state.device.hardwareCode);
    Ui::ControlRow(Tr(i18n::Str::LayoutRevision));
    if (ImGui::BeginCombo("##PreviewHardware", state.device.hardwareCode ? selected : Tr(i18n::Str::PatchAutoHardware))) {
        if (ImGui::Selectable(Tr(i18n::Str::PatchAutoHardware), !state.device.hardwareCode))
            state.device.hardwareCode = 0;
        for (const auto& record : studio.firmware.records) {
            const auto code = static_cast<uint16_t>((record.device_code[0] << 8) | record.device_code[1]);
            char label[16];
            snprintf(label, sizeof(label), "%04X", code);
            if (ImGui::Selectable(label, state.device.hardwareCode == code)) state.device.hardwareCode = code;
        }
        ImGui::EndCombo();
    }
    ImGui::EndTable();
    }
    if (ImGui::Button(Tr(i18n::Str::EmulatorRedraw))) {
        RestartPreview(state);
        state.forceNativeRefresh = true;
    }
    ImGui::BeginDisabled(!animated);
    const float stepWidth = ImGui::GetFrameHeight() * 1.4f;
    if (ImGui::Button("<##PreviousFrame", ImVec2(stepWidth, 0))) state.playback.Step(settings, -1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Tr(i18n::Str::PatchPreviousFrame));
    ImGui::SameLine();
    if (ImGui::Button(Tr(state.playback.playing ? i18n::Str::DevicePause : i18n::Str::PatchPlay),
        ImVec2(ImGui::GetFontSize() * 9, 0))) {
        if (state.playback.playing) state.playback.Pause();
        else state.playback.Play(settings);
    }
    ImGui::SameLine();
    if (ImGui::Button(Tr(i18n::Str::PatchStop))) state.playback.Stop();
    ImGui::SameLine();
    if (ImGui::Button(">##NextFrame", ImVec2(stepWidth, 0))) state.playback.Step(settings, 1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Tr(i18n::Str::PatchNextFrame));
    ImGui::SameLine();
    ImGui::Checkbox(Tr(i18n::Str::PatchLoop), &state.playback.loop);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);
    const float speeds[] = {0.25f, 0.5f, 1.0f, 1.5f, 2.0f};
    char speedLabel[16];
    snprintf(speedLabel, sizeof(speedLabel), "%.2gx", state.playback.speed);
    if (ImGui::BeginCombo("##PlaybackSpeed", speedLabel)) {
        for (const float speed : speeds) {
            snprintf(speedLabel, sizeof(speedLabel), "%.2gx", speed);
            if (ImGui::Selectable(speedLabel, state.playback.speed == speed)) state.playback.speed = speed;
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Tr(i18n::Str::PatchPlaybackSpeed));
    const double duration = Playback::Duration(settings, state.target);
    float position = float(state.playback.position);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##PlaybackTimeline", &position, 0, float(duration), "%.2f", ImGuiSliderFlags_AlwaysClamp))
        state.playback.Seek(settings, position);
    ImGui::EndDisabled();
    if (!animated) state.playback.Stop();
    ImGui::TextDisabled(Tr(i18n::Str::PatchPlaybackTime), state.playback.position, duration,
        state.playback.Phase(settings) + 1, frameCount);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", Tr(i18n::Str::PatchPlaybackReady));

    const uint64_t key = (state.draftRevision << 20) ^ (uint64_t(state.device.hardwareCode) << 4) ^ state.device.screen;
    if (state.phaseKey != key) {
        state.phaseKey = key;
        state.uploadedPhase = -1;
        state.playback.Stop();
        state.nativeGeneration = UINT64_MAX;
        state.buildError.clear();
    }
    // Validate one representative native frame, never every animation phase.
    // Request deduplicates unchanged draft/hardware/screen inputs before Poll.
    state.device.logoPhase = settings.enabled ?
        (settings.animation == FirmwareLogo::Animation::None ? 0 : int(FirmwareLogo::PhaseCount) - 1) : -1;
    state.preview.Request(studio, state.device, state.forceNativeRefresh, &state.previewPatches, state.draftRevision);
    state.forceNativeRefresh = false;
    state.preview.Poll();
    const auto publication = state.preview.Published();
    const uint64_t generation = state.preview.AfterGeneration();
    if (publication && publication->after && state.nativeGeneration != generation && !state.preview.Busy()) {
        state.nativeGeneration = generation;
        const auto& frame = *publication->after;
        state.buildError = frame.error;
        if (!settings.enabled && !frame.rgba.empty() && frame.error.empty()) {
            state.nativeTextureError = !state.nativeTexture.Update(device, context, frame.rgba.data(), frame.width, frame.height);
            state.uploadedPhase = -3;
        }
    }
    state.playback.Tick(settings, ImGui::GetIO().DeltaTime, true);
    const int shownPhase = state.playback.Phase(settings);
    if (settings.enabled && !state.playback.showAnimation && !state.canvas.empty()) {
        if (state.compositionTextureRevision != state.draftRevision || state.uploadedPhase != -2) {
            state.nativeTextureError = !state.nativeTexture.Update(device, context, state.canvas.data(), Width, Height);
            state.compositionTextureRevision = state.draftRevision;
            state.uploadedPhase = -2;
        }
    } else if (settings.enabled && state.uploadedPhase != shownPhase) {
        FirmwareLogo::PreviewFrame(settings, uint32_t(shownPhase), state.animationFrame);
        state.nativeTextureError = !state.nativeTexture.Update(device, context, state.animationFrame.data(), Width, Height);
        state.uploadedPhase = shownPhase;
    }
    if (state.nativeTextureError) {
        ImGui::TextWrapped("%s", Tr(i18n::Str::CompareTextureError));
        state.playback.Pause();
    } else if (state.nativeTexture.IsValid()) {
        ImGui::Spacing();
        DrawImage(state.nativeTexture, Width, Height, state.zoom);
        if (publication && publication->after && !state.preview.Busy())
            ImGui::TextDisabled("%d x %d / HW %04X", Width, Height, publication->after->hardwareCode);
    }
}
} // namespace

bool HasDraftChanges(const State& state, const StudioState& studio) {
    if (!state.initialized || state.documentIdentity != studio.DocumentIdentity()) return false;
    for (size_t i = 0; i < FirmwareLogo::TargetCount; ++i) {
        const auto& image = i == size_t(state.target) ? static_cast<const ImageDraft&>(state) : state.drafts[i];
        if (image.observed == studio.screenPatches[i] && image.observedSource == studio.screenPatchSources[i] &&
            (DraftSettingsChanged(image) || SourceChanged(image))) return true;
    }
    return false;
}

void CommitDrafts(State& state, StudioState& studio, bool retainHistory) {
    if (!state.initialized || state.documentIdentity != studio.DocumentIdentity()) return;
    const bool changed = HasDraftChanges(state, studio);
    if (!changed && retainHistory) return;
    if (changed) studio.PushUndo("Custom screen drafts");
    for (size_t i = 0; i < FirmwareLogo::TargetCount; ++i) {
        auto& image = i == size_t(state.target) ? static_cast<ImageDraft&>(state) : state.drafts[i];
        if (image.observed != studio.screenPatches[i] || image.observedSource != studio.screenPatchSources[i]) continue;
        if (!DraftSettingsChanged(image) && !SourceChanged(image)) {
            if (!retainHistory) ResetDraftHistory(image);
            continue;
        }
        studio.SetScreenPatch(static_cast<FirmwareLogo::Target>(i), image.draft);
        studio.SetScreenPatchSource(static_cast<FirmwareLogo::Target>(i), SourceForCommit(image));
        if (retainHistory) {
            image.observed = studio.screenPatches[i];
            image.observedSource = studio.screenPatchSources[i];
            image.initialBootStages.reset();
            image.sourceModified = image.sourceReplaced = false;
            image.undoGesture = 0;
            image.checkpoint = Snapshot(image);
        } else ReadSettings(image, studio.screenPatches[i], studio.screenPatchSources[i]);
    }
    if (changed) studio.EndUndoAction();
    state.resourceRevision = studio.ResourceRevision();
}

FirmwareLogo::Patches ResolveDraftPatches(const State& state, const StudioState& studio) {
    auto settings = studio.screenPatches;
    if (!state.initialized || state.documentIdentity != studio.DocumentIdentity()) return settings;
    for (size_t i = 0; i < FirmwareLogo::TargetCount; ++i) {
        const auto& image = i == size_t(state.target) ? static_cast<const ImageDraft&>(state) : state.drafts[i];
        if (image.observed == studio.screenPatches[i] &&
            image.observedSource == studio.screenPatchSources[i]) settings[i] = image.draft;
    }
    return settings;
}

void RenderMemory(State& state, const StudioState& studio) {
    const bool sameDocument = state.initialized && state.documentIdentity == studio.DocumentIdentity();
    const uint64_t draft = sameDocument ? state.draftRevision : 0;
    const bool changed = state.memoryDocument != studio.DocumentIdentity() ||
        state.memorySource != studio.ResourceRevision() || state.memoryDraft != draft;
    if (state.memoryJob.valid() &&
        state.memoryJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto result = state.memoryJob.get();
            std::stable_sort(result.usage.begin(), result.usage.end(), [](const auto& left, const auto& right) {
                const auto available = [](const auto& usage) {
                    return static_cast<int64_t>(usage.capacityBytes) - static_cast<int64_t>(usage.Used());
                };
                return available(left) < available(right);
            });
            state.memory = std::move(result);
        }
        catch (const std::exception& e) { state.memory.error = e.what(); }
    }
    if (changed && !state.memoryJob.valid()) {
        auto settings = ResolveDraftPatches(state, studio);
        const bool documentChanged = state.memoryDocument != studio.DocumentIdentity();
        state.memoryDocument = studio.DocumentIdentity();
        state.memorySource = studio.ResourceRevision();
        state.memoryDraft = draft;
        if (documentChanged) state.memory = {};
        if (studio.hasFirmware && FirmwareLogo::Supported(studio.firmware)) {
            state.memoryJob = std::async(std::launch::async,
                [package = studio.firmware, baseline = studio.originalScreenPatches, settings = std::move(settings),
                    menu = studio.menuPatch, originalMenu = studio.originalMenuPatch]() mutable {
                    State::MemoryResult result;
                    result.menu = menu;
                    std::vector<FirmwareLogo::FlashUsage> original, current;
                    if (!FirmwareLogo::MeasureFlash(package, baseline, original, result.error) ||
                        !FirmwareLogo::MeasureFlashChanges(package, baseline, settings, current, result.error)) return result;
                    result.usage.reserve(current.size());
                    for (size_t i = 0; i < current.size(); ++i) {
                        const auto& record = package.records[i];
                        const size_t originalCost = FirmwareMenu::FlashBytes(record, originalMenu, original[i].Used());
                        State::MemoryUsage usage;
                        static_cast<FirmwareLogo::FlashUsage&>(usage) = current[i];
                        usage.menuCosts = FirmwareMenu::FlashCosts(record, menu, current[i].Used());
                        const size_t cost = usage.menuCosts.total;
                        usage.patchBytes += cost;
                        usage.menuDelta = int64_t(cost) - int64_t(originalCost);
                        result.usage.push_back(usage);
                    }
                    return result;
                });
        }
    }
    ImGui::BeginChild("PatchMemory", ImVec2(0, 0), true);
    ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMemoryTitle));
    if (changed || state.memoryJob.valid())
        ImGui::TextDisabled("%s", Tr(i18n::Str::PatchMemoryPending));
    else
        ImGui::TextDisabled("%s", Tr(i18n::Str::PatchMemoryDeltaNote));
    if (!state.memory.usage.empty()) {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(3, 3));
        const ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoSavedSettings;
        if (ImGui::BeginTable("MemoryRevisions", 9, flags, ImVec2(0, 0))) {
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableSetupColumn("HW");
            ImGui::TableSetupColumn(Tr(i18n::Str::PatchMemoryUsed));
            ImGui::TableSetupColumn(Tr(i18n::Str::PatchMemoryFree));
            ImGui::TableSetupColumn(Tr(i18n::Str::PatchMemoryPatches));
            const i18n::Str deltas[] = {i18n::Str::PatchMemoryBoot, i18n::Str::PatchMemoryShutdown,
                i18n::Str::PatchMemoryDeviceInfo, i18n::Str::PatchMemoryLayout, i18n::Str::PatchMemoryMenu};
            for (const auto label : deltas) ImGui::TableSetupColumn(Tr(label));
            ImGui::TableHeadersRow();
            for (const auto& usage : state.memory.usage) {
                const size_t used = usage.Used();
                const size_t free = used < usage.capacityBytes ? usage.capacityBytes - used : 0;
                const double percent = usage.capacityBytes ? 100.0 * used / usage.capacityBytes : 0;
                const double freePercent = usage.capacityBytes ? 100.0 * free / usage.capacityBytes : 0;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%04X", unsigned(usage.hardwareCode));
                ImGui::TableNextColumn();
                ImGui::Text(Tr(i18n::Str::PatchMemoryBytes), used);
                ImGui::SameLine(0, 3); ImGui::Text("(%.1f%%)", percent);
                if (used > usage.capacityBytes)
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1, 0.3f, 0.2f, 1));
                else
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                ImGui::ProgressBar(float((std::min)(percent / 100.0, 1.0)), ImVec2(-1, 3), "");
                ImGui::PopStyleColor();
                ImGui::TableNextColumn();
                ImGui::Text(Tr(i18n::Str::PatchMemoryBytes), free);
                ImGui::SameLine(0, 3); ImGui::Text("(%.1f%%)", freePercent);
                ImGui::TableNextColumn();
                ImGui::Text(Tr(i18n::Str::PatchMemoryBytes), usage.patchBytes);
                for (const auto delta : usage.screenDelta) {
                    ImGui::TableNextColumn();
                    if (delta == 0) ImGui::Text(Tr(i18n::Str::PatchMemoryBytes), size_t(0));
                    else ImGui::Text(Tr(i18n::Str::PatchMemorySignedBytes), static_cast<long long>(delta));
                }
                ImGui::TableNextColumn();
                ImGui::Text(Tr(i18n::Str::PatchMemoryBytes), size_t(0));
                ImGui::TableNextColumn();
                if (usage.menuDelta == 0) ImGui::Text(Tr(i18n::Str::PatchMemoryBytes), size_t(0));
                else ImGui::Text(Tr(i18n::Str::PatchMemorySignedBytes), static_cast<long long>(usage.menuDelta));
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    } else {
        ImGui::TextWrapped("%s", state.memory.error.empty() ?
            Tr(i18n::Str::PatchMemoryUnavailable) : state.memory.error.c_str());
    }
    ImGui::EndChild();
}

void RenderTab(State& state, StudioState& studio, ID3D11Device* device, ID3D11DeviceContext* context) {
    state.activeTab = false;
    char title[128];
    snprintf(title, sizeof(title), "%s###TabCustomScreens", Tr(i18n::Str::PatchCustomScreens));
    if (!Ui::BeginTabItem(title)) return;
    state.activeTab = true;
    if (!studio.hasFirmware || !FirmwareLogo::Supported(studio.firmware)) {
        ImGui::TextWrapped("%s", Tr(i18n::Str::PatchUnsupported));
        ImGui::EndTabItem();
        return;
    }
    const bool locked = !UiFirmwareRisk::Accepted();
    const ImVec2 overlayOrigin = ImGui::GetCursorScreenPos(), overlaySize = ImGui::GetContentRegionAvail();
    ImGui::BeginDisabled(locked);
    SynchronizeDocument(state, studio);
    if (state.textureDevice != device || state.textureContext != context) {
        state.nativeTexture.Release();
        state.nativeGeneration = UINT64_MAX;
        state.uploadedPhase = -1;
        state.textureDevice = device;
        state.textureContext = context;
    }
    const auto& io = ImGui::GetIO();
    const ImGuiKey numberKeys[] = {ImGuiKey_1, ImGuiKey_2, ImGuiKey_3};
    const bool numberShortcut = !locked && Ui::EditorKeyboardAvailable();
    if (numberShortcut && !state.requestPaste && state.pasteTransition.FullyHidden() &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        for (size_t i = 0; i < FirmwareLogo::TargetCount; ++i)
            if (ImGui::IsKeyPressed(numberKeys[i], false))
                SelectTarget(state, static_cast<FirmwareLogo::Target>(i));
    }
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float controlWidth = (std::clamp)(ImGui::GetContentRegionAvail().x * 0.38f, 340.0f, 440.0f);
    ImGui::BeginChild("PatchControls", ImVec2(controlWidth, 0), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SeparatorText(Tr(i18n::Str::TabFirmwarePatches));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##PatchTarget", Tr(TargetTitle(state.target)))) {
        for (size_t i = 0; i < FirmwareLogo::TargetCount; ++i) {
            const auto target = static_cast<FirmwareLogo::Target>(i);
            if (ImGui::Selectable(Tr(TargetTitle(target)), state.target == target)) SelectTarget(state, target);
        }
        ImGui::EndCombo();
    }
    ImGui::SeparatorText(Tr(i18n::Str::PatchTransfer));
    const float actionWidth = (ImGui::GetContentRegionAvail().x - spacing) / 2;
    if (ImGui::Button(Tr(i18n::Str::BtnCopyClipboard), ImVec2(actionWidth, 0)))
        CopyToClipboard(state, GetActiveWindow());
    ImGui::SameLine();
    if (ImGui::Button(Tr(i18n::Str::BtnPasteClipboard), ImVec2(actionWidth, 0)))
        state.requestPaste = true;
    char pasteTitle[128];
    snprintf(pasteTitle, sizeof(pasteTitle), "%s###PatchPasteOptions", Tr(i18n::Str::BtnPasteClipboard));
    const double pasteNow = ImGui::GetTime();
    if (state.requestPaste) {
        if (!locked && state.pasteTransition.FullyHidden() && !state.pasteOpen) {
            state.clipboardPart = ClipboardPart::All;
            state.clipboardAnimation = AnimationPart::All;
            state.pasteOpen = true;
            ImGui::OpenPopup(pasteTitle);
        }
        state.requestPaste = false;
    }
    state.pasteTransition.Update(state.pasteOpen, pasteNow);
    if (state.pasteTransition.FullyHidden() && ImGui::IsPopupOpen(pasteTitle)) {
        auto* context = ImGui::GetCurrentContext();
        ImGui::ClosePopupToLevel(context->BeginPopupStack.Size, true);
        context->DimBgRatio = 0;
    }
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2((std::min)(ImGui::GetFontSize() * 32, viewport->Size.x - 32), 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
    const float pasteOpacity = state.pasteTransition.Opacity();
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * pasteOpacity);
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0.65f));
    if (ImGui::BeginPopupModal(pasteTitle, nullptr,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::GetCurrentContext()->DimBgRatio = pasteOpacity;
        if (Ui::ModalCloseButton(state.pasteTransition.IsClosing())) {
            state.pasteOpen = false;
            state.pasteTransition.Update(false, pasteNow);
        }
        const bool closing = !state.pasteOpen;
        ImGui::BeginDisabled(closing);
        ImGui::TextUnformatted(Tr(i18n::Str::PatchTransfer));
        ImGui::Separator();
        const ClipboardPart parts[] = {ClipboardPart::Image, ClipboardPart::Placement,
            ClipboardPart::Adjustments, ClipboardPart::Animation};
        const i18n::Str labels[] = {i18n::Str::PatchPasteImage, i18n::Str::PatchPastePlacement,
            i18n::Str::PatchPasteAdjustments, i18n::Str::PatchPasteAnimation};
        const bool dialogShortcut = !closing && Ui::DialogKeyboardAvailable();
        const bool acceptKey = dialogShortcut && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
        // Enter confirms the selection instead of activating a focused checkbox.
        ImGui::BeginDisabled(acceptKey);
        for (int i = 0; i < 4; ++i) {
            char label[192];
            snprintf(label, sizeof(label), "%d  %s", i + 1, Tr(labels[i]));
            const uint32_t bit = uint32_t(parts[i]);
            const uint32_t animation = uint32_t(state.clipboardAnimation);
            bool checked = i == 3 ? animation == uint32_t(AnimationPart::All) :
                (uint32_t(state.clipboardPart) & bit) != 0;
            const bool mixed = i == 3 && animation != 0 && animation != uint32_t(AnimationPart::All);
            ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, mixed);
            const bool changed = ImGui::Checkbox(label, &checked);
            ImGui::PopItemFlag();
            const bool toggled = dialogShortcut && !io.KeyShift &&
                ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i), false);
            if (toggled) checked = i == 3 ? animation != uint32_t(AnimationPart::All) : !checked;
            if (!closing && !locked && (changed || toggled)) {
                state.clipboardPart = ClipboardPart(checked ?
                    uint32_t(state.clipboardPart) | bit : uint32_t(state.clipboardPart) & ~bit);
                if (i == 3) state.clipboardAnimation = checked ? AnimationPart::All : AnimationPart::None;
            }
        }
        ImGui::Indent(ImGui::GetFrameHeight());
        ImGui::BeginGroup();
        const AnimationPart animationParts[] = {AnimationPart::EntranceEffect, AnimationPart::ExitEffect, AnimationPart::Durations};
        const i18n::Str animationLabels[] = {i18n::Str::PatchPasteEntranceEffect,
            i18n::Str::PatchPasteExitEffect, i18n::Str::PatchPasteDurations};
        for (size_t i = 0; i < std::size(animationParts); ++i) {
            const uint32_t bit = uint32_t(animationParts[i]);
            bool checked = (uint32_t(state.clipboardAnimation) & bit) != 0;
            if (ImGui::Checkbox(Tr(animationLabels[i]), &checked) && !closing && !locked) {
                state.clipboardAnimation = AnimationPart(checked ?
                    uint32_t(state.clipboardAnimation) | bit : uint32_t(state.clipboardAnimation) & ~bit);
                state.clipboardPart = ClipboardPart(state.clipboardAnimation != AnimationPart::None ?
                    uint32_t(state.clipboardPart) | uint32_t(ClipboardPart::Animation) :
                    uint32_t(state.clipboardPart) & ~uint32_t(ClipboardPart::Animation));
            }
        }
        ImGui::EndGroup();
        ImGui::Unindent(ImGui::GetFrameHeight());
        ImGui::EndDisabled();
        ImGui::TextWrapped("%s", Tr(i18n::Str::PatchPasteHint));
        ImGui::Spacing();
        ImGui::BeginDisabled(uint32_t(state.clipboardPart) == 0);
        const bool insert = ImGui::Button(Tr(i18n::Str::BtnPasteClipboard), ImVec2(-1, 0));
        ImGui::EndDisabled();
        if (!closing && !locked && uint32_t(state.clipboardPart) && (insert || acceptKey)) {
            PasteFromClipboard(state, GetActiveWindow());
            state.pasteOpen = false;
            state.pasteTransition.Update(false, pasteNow);
        }
        ImGui::Spacing();
        const bool cancel = ImGui::Button(Tr(i18n::Str::BtnCancelAction), ImVec2(-1, 0));
        if (!closing && (cancel || (dialogShortcut && ImGui::IsKeyPressed(ImGuiKey_Escape, false)))) {
            state.pasteOpen = false;
            state.pasteTransition.Update(false, pasteNow);
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImGui::Spacing();
    // Footer = action row plus wrapped error text when present; buttons stay at the bottom edge.
    const auto& footerError = state.error.empty() ? state.buildError : state.error;
    const float errorHeight = footerError.empty() ? 0.0f :
        ImGui::CalcTextSize(footerError.c_str(), nullptr, false, ImGui::GetContentRegionAvail().x).y +
        ImGui::GetStyle().ItemSpacing.y;
    const float footerHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y + errorHeight;
    ImGui::BeginChild("PatchSettings", ImVec2(0, -footerHeight));
    bool changed = false, compose = false;
    uint32_t editGesture = 0;
    if (state.target == FirmwareLogo::Target::Boot) {
        ImGui::TextUnformatted(Tr(i18n::Str::PatchBootStages));
        const i18n::Str stages[] = {i18n::Str::PatchFirstStage, i18n::Str::PatchSecondStage, i18n::Str::PatchBothStages};
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##BootStages", Tr(stages[unsigned(state.draft.bootStages) - 1]))) {
            for (unsigned i = 0; i < 3; ++i)
                if (ImGui::Selectable(Tr(stages[i]), unsigned(state.draft.bootStages) == i + 1)) {
                    state.draft.bootStages = static_cast<FirmwareLogo::BootStages>(i + 1);
                    changed = true;
                }
            ImGui::EndCombo();
        }
    }
    if (ImGui::Checkbox(Tr(i18n::Str::PatchEnabled), &state.draft.enabled)) {
        changed = true;
        compose = state.draft.enabled && state.draft.pixels.empty();
    }
    const float imageActionWidth = (ImGui::GetContentRegionAvail().x - spacing) / 2;
    if (ImGui::Button(Tr(i18n::Str::PatchImportImage), ImVec2(imageActionWidth, 0))) {
        std::string path;
        if (PathUtils::PickFileDialog(GetActiveWindow(),
            L"Images\0*.png;*.jpg;*.jpeg;*.jpe;*.jfif;*.bmp;*.dib;*.gif;*.tif;*.tiff;*.ico;*.wdp;*.jxr;*.tga;*.psd;*.hdr;*.pic;*.ppm;*.pgm;*.pnm;*.webp;*.heic;*.heif;*.avif\0All files\0*.*\0",
            path)) LoadImage(state, path, state.error);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Tr(i18n::Str::PatchImageFormats));
    ImGui::SameLine();
    if (ImGui::Button(Tr(i18n::Str::BtnExportThisPng), ImVec2(imageActionWidth, 0))) {
        std::string path;
        const char* filename = state.target == FirmwareLogo::Target::Boot ? "boot_logo_80x160.png" :
            state.target == FirmwareLogo::Target::Shutdown ? "shutdown_logo_80x160.png" : "service_info_80x160.png";
        if (PathUtils::SaveFileDialog(GetActiveWindow(), L"PNG Image (*.png)\0*.png\0", filename, path)) {
            state.error.clear();
            FirmwareBitmap::SavePngRgb565(path, state.canvas.data(), Width, Height, state.error);
        }
    }
    if (Ui::CollapsibleSection section{"PlacementBody", Tr(i18n::Str::PatchPlacement),
        ImGuiTreeNodeFlags_DefaultOpen}; section) {
        ImGui::BeginDisabled(state.source.empty());
        const i18n::Str fits[] = {i18n::Str::PatchFit, i18n::Str::PatchCover, i18n::Str::PatchStretch};
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##LogoFit", Tr(fits[state.fitMode]))) {
            for (int i = 0; i < 3; ++i)
                if (ImGui::Selectable(Tr(fits[i]), state.fitMode == i)) {
                    state.fitMode = i; state.scalePercent = 100;
                    AlignImage(state); compose = true;
                }
            ImGui::EndCombo();
        }
        const i18n::Str filters[] = {i18n::Str::PatchNearest, i18n::Str::PatchBilinear, i18n::Str::PatchBicubic, i18n::Str::PatchLanczos};
        ImGui::TextUnformatted(Tr(i18n::Str::PatchResampling));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##LogoResampling", Tr(filters[int(state.resampling)]))) {
            for (int i = 0; i < 4; ++i)
                if (ImGui::Selectable(Tr(filters[i]), int(state.resampling) == i)) {
                    state.resampling = static_cast<FirmwareBitmap::Resampling>(i); compose = true;
                }
            ImGui::EndCombo();
        }
        ImGui::TextUnformatted(Tr(i18n::Str::PatchScale));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##LogoScale", &state.scalePercent, 1, 400, "%.0f%%", ImGuiSliderFlags_AlwaysClamp)) {
            AlignImage(state); compose = true;
            editGesture = ImGui::GetItemID();
        }
        const i18n::Str aligns[2][3] = {
            {i18n::Str::LayoutAlignLeft, i18n::Str::LayoutAlignCenter, i18n::Str::LayoutAlignRight},
            {i18n::Str::LayoutAlignTop, i18n::Str::LayoutAlignMiddle, i18n::Str::LayoutAlignBottom}};
        const float cell = ImGui::GetFrameHeight() * 1.5f;
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float gridWidth = cell * 3 + gap * 2;
        const float gridX = ImGui::GetCursorPosX() +
            (std::max)(0.0f, (ImGui::GetContentRegionAvail().x - gridWidth) * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        for (int row = 0; row < 3; ++row) {
            ImGui::SetCursorPosX(gridX);
            for (int col = 0; col < 3; ++col) {
                if (col) ImGui::SameLine(0, gap);
                ImGui::PushID(row * 3 + col);
                const bool selected = state.alignX == col && state.alignY == row;
                if (selected)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                if (ImGui::Button("##Anchor", ImVec2(cell, cell))) {
                    state.alignX = col;
                    state.alignY = row;
                    AlignImage(state);
                    compose = true;
                }
                if (selected) ImGui::PopStyleColor();
                const ImVec2 origin = ImGui::GetItemRectMin();
                const float inset = cell * 0.22f, mark = cell * 0.22f;
                const float travel = cell - inset * 2 - mark;
                const ImVec2 point(origin.x + inset + travel * col * 0.5f,
                    origin.y + inset + travel * row * 0.5f);
                auto* draw = ImGui::GetWindowDrawList();
                draw->AddRect(ImVec2(origin.x + inset, origin.y + inset),
                    ImVec2(origin.x + cell - inset, origin.y + cell - inset),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), 2.0f);
                draw->AddRectFilled(point, ImVec2(point.x + mark, point.y + mark),
                    ImGui::GetColorU32(ImGuiCol_Text), 1.0f);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s / %s", Tr(aligns[0][col]), Tr(aligns[1][row]));
                ImGui::PopID();
            }
        }
        ImGui::PopStyleVar();
        ImGui::SetNextItemWidth(130);
        if (ImGui::DragInt(Tr(i18n::Str::LayoutPositionX), &state.x, 1, -65536, 65536, "%d", ImGuiSliderFlags_AlwaysClamp)) {
            state.alignX = -1; compose = true;
            editGesture = ImGui::GetItemID();
        }
        ImGui::SetNextItemWidth(130);
        if (ImGui::DragInt(Tr(i18n::Str::LayoutPositionY), &state.y, 1, -65536, 65536, "%d", ImGuiSliderFlags_AlwaysClamp)) {
            state.alignY = -1; compose = true;
            editGesture = ImGui::GetItemID();
        }
        ImGui::EndDisabled();
    }
    if (Ui::CollapsibleSection section{"AdjustmentsBody", Tr(i18n::Str::PatchAdjustments),
        ImGuiTreeNodeFlags_DefaultOpen}; section) {
        ImGui::BeginDisabled(state.source.empty());
        auto adjustment = [&](i18n::Str label, float& value, float minimum, float maximum, const char* format) {
            ImGui::TextUnformatted(Tr(label));
            ImGui::PushID(int(label));
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##Adjustment", &value, minimum, maximum, format, ImGuiSliderFlags_AlwaysClamp)) {
                compose = true;
                editGesture = ImGui::GetItemID();
            }
            ImGui::PopID();
        };
        adjustment(i18n::Str::PatchBrightness, state.adjustments.brightness, -100, 100, "%.0f");
        adjustment(i18n::Str::PatchContrast, state.adjustments.contrast, -100, 100, "%.0f");
        adjustment(i18n::Str::PatchGamma, state.adjustments.gamma, 0.2f, 3, "%.2f");
        adjustment(i18n::Str::PatchSaturation, state.adjustments.saturation, -100, 100, "%.0f");
        adjustment(i18n::Str::PatchSharpness, state.adjustments.sharpness, 0, 100, "%.0f");
        ImGui::SeparatorText(Tr(i18n::Str::PatchToneCurve));
        ImGui::TextDisabled("%s", Tr(i18n::Str::PatchHistogramComposition));
        const ImVec2 curveOrigin = ImGui::GetCursorScreenPos();
        const float curveSide = ImGui::GetContentRegionAvail().x;
        ImGui::Dummy(ImVec2(curveSide, curveSide));
        auto* draw = ImGui::GetWindowDrawList();
        auto background = ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
        background.x *= .65f; background.y *= .65f; background.z *= .65f; background.w = 1;
        draw->AddRectFilled(curveOrigin, ImVec2(curveOrigin.x + curveSide, curveOrigin.y + curveSide),
            ImGui::GetColorU32(background));
        const ImVec4 channelColors[]{ImGui::GetStyleColorVec4(ImGuiCol_Text),
            ImVec4(.95f, .3f, .3f, 1), ImVec4(.3f, .85f, .4f, 1), ImVec4(.35f, .55f, 1, 1)};
        auto histogramColor = channelColors[state.curveChannel];
        histogramColor.w *= .25f;
        const auto& histogram = state.histogram[state.curveChannel];
        const auto maximum = state.histogramMaximum[state.curveChannel];
        if (maximum) {
            for (size_t i = 0; i < histogram.size(); ++i) {
                const float left = curveOrigin.x + curveSide * float(i) / 256;
                const float right = curveOrigin.x + curveSide * float(i + 1) / 256;
                const float top = curveOrigin.y + curveSide * (1 - float(histogram[i]) / maximum);
                draw->AddRectFilled(ImVec2(left, top), ImVec2(right, curveOrigin.y + curveSide),
                    ImGui::GetColorU32(histogramColor));
            }
        }
        for (int i = 1; i < 4; ++i) {
            const float offset = curveSide * i * .25f;
            const auto gridColor = ImGui::GetColorU32(ImGuiCol_TextDisabled, .25f);
            draw->AddLine(ImVec2(curveOrigin.x + offset, curveOrigin.y),
                ImVec2(curveOrigin.x + offset, curveOrigin.y + curveSide), gridColor);
            draw->AddLine(ImVec2(curveOrigin.x, curveOrigin.y + offset),
                ImVec2(curveOrigin.x + curveSide, curveOrigin.y + offset), gridColor);
        }
        draw->AddLine(ImVec2(curveOrigin.x, curveOrigin.y + curveSide),
            ImVec2(curveOrigin.x + curveSide, curveOrigin.y), ImGui::GetColorU32(ImGuiCol_TextDisabled, .5f));
        auto& curve = state.curveChannel ? state.adjustments.channelCurves[state.curveChannel - 1] : state.adjustments.curve;
        ImVec2 previous(curveOrigin.x, curveOrigin.y + curveSide);
        for (int i = 0; i < 4; ++i) {
            const float value = i == 3 ? 1.0f : curve[i];
            const ImVec2 point(curveOrigin.x + curveSide * (i + 1) * .25f,
                curveOrigin.y + curveSide * (1 - value));
            draw->AddLine(previous, point, ImGui::GetColorU32(channelColors[state.curveChannel]), 2);
            if (i < 3) draw->AddCircleFilled(point, 3, ImGui::GetColorU32(channelColors[state.curveChannel]));
            previous = point;
        }
        const char* channels[]{"RGB", "R", "G", "B"};
        const float channelWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3) / 4;
        for (int i = 0; i < 4; ++i) {
            if (i) ImGui::SameLine();
            const bool selected = state.curveChannel == i;
            if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(channels[i], ImVec2(channelWidth, 0))) state.curveChannel = i;
            if (selected) ImGui::PopStyleColor();
        }
        auto& selectedCurve = state.curveChannel ? state.adjustments.channelCurves[state.curveChannel - 1] : state.adjustments.curve;
        adjustment(i18n::Str::PatchShadows, selectedCurve[0], 0, 1, "%.2f");
        adjustment(i18n::Str::PatchMidtones, selectedCurve[1], 0, 1, "%.2f");
        adjustment(i18n::Str::PatchHighlights, selectedCurve[2], 0, 1, "%.2f");
        ImGui::TextUnformatted(Tr(i18n::Str::PatchPaletteColors));
        const int colors[] = {0, 256, 64, 16};
        const char* colorLabels[] = {Tr(i18n::Str::PatchPaletteOriginal), "256", "64", "16"};
        int selected = 0;
        for (int i = 1; i < 4; ++i) if (state.adjustments.paletteColors == colors[i]) selected = i;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##PaletteColors", colorLabels[selected])) {
            for (int i = 0; i < 4; ++i)
                if (ImGui::Selectable(colorLabels[i], selected == i)) {
                    state.adjustments.paletteColors = colors[i];
                    compose = true;
                }
            ImGui::EndCombo();
        }
        if (ImGui::Button(Tr(i18n::Str::PatchResetAdjustments), ImVec2(-1, 0))) {
            state.adjustments = {};
            compose = true;
        }
        ImGui::TextWrapped("%s", Tr(i18n::Str::PatchAdjustmentsNote));
        ImGui::EndDisabled();
    }
    if (Ui::CollapsibleSection section{"AnimationBody", Tr(i18n::Str::GroupAnimations),
        ImGuiTreeNodeFlags_DefaultOpen}; section) {
        using Animation = FirmwareLogo::Animation;
        struct Effect {
            i18n::Str label;
            bool directional;
            Animation variants[8];
        };
        // Direction order is shared by all four families; persisted values remain
        // the original animation IDs, with no separate direction setting.
        static constexpr Effect effects[] = {
            {i18n::Str::PatchNone, false, {Animation::None}},
            {i18n::Str::PatchWipe, true, {Animation::WipeDown, Animation::WipeUp,
                Animation::WipeRight, Animation::WipeLeft, Animation::Diagonal,
                Animation::DiagonalDownLeft, Animation::DiagonalUpRight, Animation::DiagonalUpLeft}},
            {i18n::Str::PatchFade, false, {Animation::Fade}},
            {i18n::Str::PatchIris, false, {Animation::Iris}},
            {i18n::Str::PatchDiamond, false, {Animation::Diamond}},
            {i18n::Str::PatchCenterHorizontal, false, {Animation::CenterHorizontal}},
            {i18n::Str::PatchCenterVertical, false, {Animation::CenterVertical}},
            {i18n::Str::PatchBlinds, true, {Animation::Blinds, Animation::BlindsUp,
                Animation::BlindsRight, Animation::BlindsLeft, Animation::BlindsDownRight,
                Animation::BlindsDownLeft, Animation::BlindsUpRight, Animation::BlindsUpLeft}},
            {i18n::Str::PatchDissolve, false, {Animation::Dissolve}},
            {i18n::Str::PatchSoftWipe, true, {Animation::SoftWipe, Animation::SoftWipeUp,
                Animation::SoftWipeRight, Animation::SoftWipeLeft, Animation::SoftWipeDownRight,
                Animation::SoftWipeDownLeft, Animation::SoftWipeUpRight, Animation::SoftWipeUpLeft}},
            {i18n::Str::PatchSlide, true, {Animation::SlideDown, Animation::SlideUp,
                Animation::SlideRight, Animation::SlideLeft, Animation::SlideDownRight,
                Animation::SlideDownLeft, Animation::SlideUpRight, Animation::SlideUpLeft}},
            {i18n::Str::PatchChecker, false, {Animation::Checker}}
        };
        static constexpr i18n::Str directions[] = {i18n::Str::PatchDirectionDown, i18n::Str::PatchDirectionUp,
            i18n::Str::PatchDirectionRight, i18n::Str::PatchDirectionLeft, i18n::Str::PatchDirectionDownRight,
            i18n::Str::PatchDirectionDownLeft, i18n::Str::PatchDirectionUpRight, i18n::Str::PatchDirectionUpLeft};
        auto animation = [&](i18n::Str label, FirmwareLogo::Animation& value, uint32_t& duration) {
            size_t family = 0, direction = 0;
            for (size_t i = 0; i < std::size(effects); ++i)
                for (size_t d = 0; d < (effects[i].directional ? std::size(directions) : 1); ++d)
                    if (effects[i].variants[d] == value) {
                        family = i;
                        direction = d;
                    }
            ImGui::PushID(int(label));
            ImGui::SeparatorText(Tr(label));
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##Effect", Tr(effects[family].label))) {
                for (size_t i = 0; i < std::size(effects); ++i)
                    if (ImGui::Selectable(Tr(effects[i].label), family == i)) {
                        family = i;
                        value = effects[i].variants[effects[i].directional ? direction : 0];
                        changed = true;
                    }
                ImGui::EndCombo();
            }
            if (effects[family].directional) {
                ImGui::TextUnformatted(Tr(i18n::Str::PatchDirection));
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##Direction", Tr(directions[direction]))) {
                    for (size_t d = 0; d < std::size(directions); ++d)
                        if (ImGui::Selectable(Tr(directions[d]), direction == d)) {
                            value = effects[family].variants[d];
                            changed = true;
                        }
                    ImGui::EndCombo();
                }
            }
            ImGui::BeginDisabled(value == FirmwareLogo::Animation::None);
            int milliseconds = int(duration);
            ImGui::TextUnformatted(Tr(i18n::Str::PatchDuration));
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderInt("##Duration", &milliseconds, 200, 2000, "%d", ImGuiSliderFlags_AlwaysClamp)) {
                duration = uint32_t(milliseconds);
                changed = true;
                editGesture = ImGui::GetItemID();
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        };
        animation(i18n::Str::PatchAnimation, state.draft.animation, state.draft.durationMs);
        if (state.target != FirmwareLogo::Target::DeviceInfo) {
            int hold = int(state.draft.holdMs);
            ImGui::TextUnformatted(Tr(i18n::Str::PatchHold));
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderInt("##Hold", &hold, 0, 10000, "%d", ImGuiSliderFlags_AlwaysClamp)) {
                state.draft.holdMs = uint32_t(hold);
                changed = true;
                editGesture = ImGui::GetItemID();
            }
        }
        animation(i18n::Str::PatchExitAnimation, state.draft.exitAnimation, state.draft.exitDurationMs);
    }
    if (compose) ComposeLogo(state, editGesture);
    else if (changed) {
        RecordDraftChange(state, editGesture);
        ++state.draftRevision;
        RestartPreview(state);
    }
    if (!ImGui::IsAnyItemActive()) state.undoGesture = 0;
    ImGui::EndChild();
    ImGui::SetCursorPosY((std::max)(ImGui::GetCursorPosY(),
        ImGui::GetWindowHeight() - ImGui::GetStyle().WindowPadding.y - footerHeight + ImGui::GetStyle().ItemSpacing.y));
    if (!footerError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.35f, 0.25f, 1));
        ImGui::TextWrapped("%s", footerError.c_str());
        ImGui::PopStyleColor();
    }
    if (ImGui::Button(Tr(i18n::Str::BtnRestoreOriginal), ImVec2(-1, 0))) {
        const size_t index = static_cast<size_t>(state.target);
        Commit(studio, state.target, studio.originalScreenPatches[index], studio.originalScreenPatchSources[index]);
        ReadSettings(state, Applied(state, studio), studio.screenPatchSources[index]);
        ++state.draftRevision;
        RestartPreview(state);
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("PatchRight", ImVec2(0, 0), false);
    Ui::BeginBottomAnchoredPreview("PatchPreview", ImGuiWindowFlags_HorizontalScrollbar);
    DrawNative(state, studio, device, context);
    ImGui::EndChild();
    RenderMemory(state, studio);
    ImGui::EndChild();
    ImGui::EndDisabled();
    if (locked) UiFirmwareRisk::Overlay(overlayOrigin, overlaySize);
    ImGui::EndTabItem();
}
}
