#pragma once

#if STUDIO_FONTS

#include "app_state.h"
#include "font_renderer.h"
#include "presets.h"
#include <windows.h>
#include <vector>
#include <string>

struct FontGenState {
    FontRenderer fontRenderer;
    std::vector<SystemFont> popularFonts;
    int selectedFontIdx = 0;
    std::string fontDisplayName = "None";
    std::vector<Preset> presets;
    int selectedPresetIdx = 0;

    RenderOptions fontOpts;
    int fontZoom = 6;
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

#endif // STUDIO_FONTS

