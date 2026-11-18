#pragma once

#include "firmware_comparison.h"
#include "texture_cache.h"
#include "studio_preferences.h"
#include <windows.h>

struct StudioState;

namespace UiFirmwareComparison {

enum class Source { Opened, Reference, File };

struct State {
    FirmwareComparison::Snapshot leftFile;
    FirmwareComparison::Snapshot rightFile;
    FirmwareComparison::Snapshot editor;
    FirmwareComparison::Snapshot reference;
    Source leftSource = Source::Opened;
    Source rightSource = Source::File;
    bool referenceCached = false;
    FirmwareModel referenceModel = FirmwareModel::DragS;
    FirmwareEdition referenceEdition = FirmwareEdition::Score;
    std::string referenceError;
    std::string leftError;
    bool activeTab = false;
    std::string rightError;
    char query[256] = "";
    int statusFilter = 0;
    int zoom = StudioPreferences::Get().comparisonZoom;
    int rank = 0;
    bool cyan = false;
    bool highlight = true;
    std::vector<FirmwareComparison::Entry> entries;
    std::vector<size_t> visible;
    size_t selected = FirmwareComparison::MissingIndex;
    bool comparisonDirty = true;
    bool filterDirty = true;
    bool previewDirty = true;
    Dx11Texture leftTexture;
    Dx11Texture rightTexture;
    Dx11Texture differenceTexture;
    std::vector<uint32_t> differencePixels;
    size_t previewDifferentPixels = 0;
    bool textureError = false;
    ID3D11Device* textureDevice = nullptr; // Non-owning; textures retain their device.
};

// Read-only editor access; all selected files, caches, and textures belong to State.
void RenderTab(State& state, const StudioState& studio, HWND hwnd, ID3D11Device* device);

} // namespace UiFirmwareComparison
