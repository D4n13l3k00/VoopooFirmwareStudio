#pragma once


#include "app_state.h"
#include "studio_preferences.h"
#include "font_renderer.h"
#include "presets.h"
#include <windows.h>
#include <vector>
#include <string>

struct FontGenState {
    FontRenderer fontRenderer;
    std::vector<SystemFont> installedFonts;
    std::string fontCatalogError;
    int selectedFontIdx = 0;
    std::string fontDisplayName = "None";
    std::vector<Preset> presets;
    int selectedPresetIdx = 0;
    FirmwareModel presetModel = FirmwareModel::DragS;
    FirmwareEdition presetEdition = FirmwareEdition::Score;

    RenderOptions fontOpts;
    int fontZoom = StudioPreferences::Get().fontZoom;
    bool fontShowGrid = true;
    char fontOutDirBuf[512] = "exported-fonts";
    bool fontExportAll = false;
    bool fontDirty = true;
    std::vector<RenderedGlyph> fontPreviewGlyphs;
    float currentFontSize = 0.0f;

    void Init();
};

namespace UiFontGenerator {

void RenderTab(StudioState& studio, FontGenState& fontState, HWND hwnd, bool smokeTest, int debugFrame);

} // namespace UiFontGenerator
