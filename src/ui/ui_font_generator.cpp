#if STUDIO_FONTS

#include "ui_font_generator.h"
#include "path_utils.h"
#include "i18n.h"
#include "firmware_bitmap.h"
#include "../imgui/imgui.h"

#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

void FontGenState::Init() {
    popularFonts = FontRenderer::ListPopularWindowsFonts();
    selectedFontIdx = 0;
    fontDisplayName = "None";
    if (!popularFonts.empty()) {
        if (fontRenderer.LoadFontFile(popularFonts[0].path)) {
            fontDisplayName = popularFonts[0].name;
        }
    }

    presets = Preset::all();
    selectedPresetIdx = 0;

    fontOpts.threshold = 110;
    fontOpts.oversample = 4;
    fontOpts.stretch = false;
    fontOpts.align_center = false;
    fontOpts.padding_x = 0;
    fontOpts.padding_y = 0;

    fontZoom = 6;
    fontShowGrid = true;
    fontExportAll = false;
    fontDirty = true;
}

namespace UiFontGenerator {

void RenderTab(StudioState& studio, FontGenState& fontState, HWND hwnd, bool smokeTest, int debugFrame) {
    if (fontState.fontDirty) {
        fontState.fontDirty = false;
        if (fontState.fontRenderer.IsLoaded() && fontState.selectedPresetIdx < (int)fontState.presets.size()) {
            const auto& curPreset = fontState.presets[fontState.selectedPresetIdx];
            fontState.fontOpts.target_width = curPreset.width;
            fontState.fontOpts.target_height = curPreset.height;
            auto res = fontState.fontRenderer.RenderPreset(curPreset, fontState.fontOpts);
            fontState.currentFontSize = res.first;
            fontState.fontPreviewGlyphs = std::move(res.second);
        }
    }

    char _tb[128];
    snprintf(_tb, sizeof(_tb), "%s###TabFontGen", Tr(i18n::Str::TabFontGenerator));
    if (ImGui::BeginTabItem(_tb, nullptr, smokeTest && debugFrame == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
        // Left Font Controls (width 390px)
        ImGui::BeginChild("FontControlsPanel", ImVec2(390.0f, 0.0f), true);
        {
            ImGui::SeparatorText(Tr(i18n::Str::FontSec1Title));

            if (!fontState.popularFonts.empty()) {
                if (ImGui::BeginCombo(Tr(i18n::Str::FontSec1WindowsFont), fontState.popularFonts[fontState.selectedFontIdx].name.c_str())) {
                    for (int i = 0; i < (int)fontState.popularFonts.size(); ++i) {
                        bool is_selected = (fontState.selectedFontIdx == i);
                        if (ImGui::Selectable(fontState.popularFonts[i].name.c_str(), is_selected)) {
                            fontState.selectedFontIdx = i;
                            if (fontState.fontRenderer.LoadFontFile(fontState.popularFonts[i].path)) {
                                fontState.fontDisplayName = fontState.popularFonts[i].name;
                                fontState.fontDirty = true;
                            }
                        }
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }

            if (ImGui::Button(Tr(i18n::Str::FontSec1BrowseTtf))) {
                std::string picked;
                if (PathUtils::PickFileDialog(hwnd, L"Font Files (*.ttf;*.otf)\0*.ttf;*.otf\0All Files (*.*)\0*.*\0", picked)) {
                    if (fontState.fontRenderer.LoadFontFile(picked)) {
                        fontState.fontDisplayName = fs::path(PathUtils::ToFsPath(picked)).stem().string();
                        fontState.fontDirty = true;
                    }
                }
            }

            ImGui::TextDisabled(Tr(i18n::Str::FontSec1ActiveFont), fontState.fontDisplayName.c_str());

            ImGui::Spacing();
            ImGui::SeparatorText(Tr(i18n::Str::FontSec2Title));

            const char* presetNames[] = {
                Tr(i18n::Str::FontPreset19x40),
                Tr(i18n::Str::FontPreset11x20),
                Tr(i18n::Str::FontPreset7x13),
                Tr(i18n::Str::FontPreset6x10)
            };

            if (ImGui::Combo(Tr(i18n::Str::LabelFontPreset), &fontState.selectedPresetIdx, presetNames, IM_ARRAYSIZE(presetNames))) {
                fontState.fontDirty = true;
            }

            ImGui::Spacing();
            ImGui::SeparatorText(Tr(i18n::Str::FontSec3Title));

            int th = fontState.fontOpts.threshold;
            if (ImGui::SliderInt(Tr(i18n::Str::FontSec3Threshold), &th, 1, 254)) {
                fontState.fontOpts.threshold = (uint8_t)th;
                fontState.fontDirty = true;
            }
            ImGui::TextDisabled("%s", Tr(i18n::Str::FontSec3ThresholdHint));

            if (ImGui::Checkbox(Tr(i18n::Str::FontSec3Stretch), &fontState.fontOpts.stretch)) {
                fontState.fontDirty = true;
            }

            if (ImGui::RadioButton(Tr(i18n::Str::FontSec3Baseline), !fontState.fontOpts.align_center)) {
                fontState.fontOpts.align_center = false;
                fontState.fontDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(Tr(i18n::Str::FontSec3CenterV), fontState.fontOpts.align_center)) {
                fontState.fontOpts.align_center = true;
                fontState.fontDirty = true;
            }

            if (ImGui::SliderInt(Tr(i18n::Str::FontSec3PaddingX), &fontState.fontOpts.padding_x, 0, 5)) {
                fontState.fontDirty = true;
            }
            if (ImGui::SliderInt(Tr(i18n::Str::FontSec3PaddingY), &fontState.fontOpts.padding_y, 0, 5)) {
                fontState.fontDirty = true;
            }

            const char* osNames[] = {
                Tr(i18n::Str::FontOs1x),
                Tr(i18n::Str::FontOs2x),
                Tr(i18n::Str::FontOs4x),
                Tr(i18n::Str::FontOs8x)
            };
            int osIdx = (fontState.fontOpts.oversample == 1) ? 0 : (fontState.fontOpts.oversample == 2) ? 1 : (fontState.fontOpts.oversample == 4) ? 2 : 3;
            if (ImGui::Combo(Tr(i18n::Str::FontSec3Oversampling), &osIdx, osNames, IM_ARRAYSIZE(osNames))) {
                fontState.fontOpts.oversample = (osIdx == 0) ? 1 : (osIdx == 1) ? 2 : (osIdx == 2) ? 4 : 8;
                fontState.fontDirty = true;
            }

            ImGui::Spacing();
            ImGui::SeparatorText(Tr(i18n::Str::FontSec4Title));

            if (!studio.hasFirmware) ImGui::BeginDisabled();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.70f, 0.32f, 1.0f));

            if (ImGui::Button(Tr(i18n::Str::FontSec4ApplyPreset), ImVec2(-1, 32))) {
                if (!fontState.fontRenderer.IsLoaded()) {
                    studio.statusMsg = Tr(i18n::Str::MsgFontNotLoaded);
                    studio.statusIsErr = true;
                } else if (fontState.selectedPresetIdx < (int)fontState.presets.size()) {
                    const auto& curPreset = fontState.presets[fontState.selectedPresetIdx];
                    studio.PushUndo("Apply font preset " + curPreset.name);
                    size_t stagedCount = 0;

                    for (const auto& g : fontState.fontPreviewGlyphs) {
                        std::string assetId = fs::path(PathUtils::ToFsPath(g.filename)).stem().string();

                        // Find asset in catalog
                        for (size_t i = 0; i < studio.catalog.size(); ++i) {
                            if (studio.catalog[i].id == assetId) {
                                const auto& spec = studio.catalog[i];
                                std::vector<uint8_t> newRaw;
                                if (FirmwareBitmap::EncodeMaskPreserving(g.pixels.data(), spec.width, spec.height, studio.originals[i].data(), spec.byte_len(), newRaw)) {
                                    studio.replacements[assetId] = std::move(newRaw);
                                    stagedCount++;
                                }
                                break;
                            }
                        }
                    }
                    char _m[256];
                    snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgFontInjectedPreset), (int)stagedCount, curPreset.name.c_str());
                    studio.statusMsg = _m;
                    studio.statusIsErr = false;
                }
            }
            ImGui::PopStyleColor(2);

            // Batch Inject All 4 Font Presets Button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.45f, 0.65f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.58f, 0.82f, 1.0f));
            if (ImGui::Button(Tr(i18n::Str::FontSec4ApplyAll), ImVec2(-1, 34))) {
                if (!fontState.fontRenderer.IsLoaded()) {
                    studio.statusMsg = Tr(i18n::Str::MsgFontNotLoaded);
                    studio.statusIsErr = true;
                } else {
                    studio.PushUndo("Apply all font presets");
                    size_t totalStaged = 0;
                    for (const auto& pr : fontState.presets) {
                        RenderOptions curOpts = fontState.fontOpts;
                        curOpts.target_width = pr.width;
                        curOpts.target_height = pr.height;
                        auto res = fontState.fontRenderer.RenderPreset(pr, curOpts);
                        for (const auto& g : res.second) {
                            std::string assetId = fs::path(PathUtils::ToFsPath(g.filename)).stem().string();
                            int catIdx = studio.FindAssetIndex(assetId);
                            if (catIdx >= 0) {
                                const auto& spec = studio.catalog[catIdx];
                                std::vector<uint8_t> newRaw;
                                if (FirmwareBitmap::EncodeMaskPreserving(g.pixels.data(), spec.width, spec.height, studio.originals[catIdx].data(), spec.byte_len(), newRaw)) {
                                    studio.replacements[assetId] = std::move(newRaw);
                                    totalStaged++;
                                }
                            }
                        }
                    }
                    char _m[256];
                    snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgFontInjectedAll), (int)totalStaged);
                    studio.statusMsg = _m;
                    studio.statusIsErr = false;
                }
            }
            ImGui::PopStyleColor(2);
            if (!studio.hasFirmware) ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::SeparatorText(Tr(i18n::Str::FontSec5Title));

            ImGui::InputText(Tr(i18n::Str::FontSec5OutputFolder), fontState.fontOutDirBuf, sizeof(fontState.fontOutDirBuf));

            if (ImGui::Button(Tr(i18n::Str::FontSec5BrowseFolder))) {
                std::string picked;
                if (PathUtils::PickFolderDialog(hwnd, picked)) {
                    strcpy_s(fontState.fontOutDirBuf, picked.c_str());
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(Tr(i18n::Str::FontSec5OpenExplorer))) {
                std::wstring wOut = PathUtils::Utf8ToWide(fontState.fontOutDirBuf);
                ShellExecuteW(nullptr, L"open", wOut.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
            }

            ImGui::Checkbox(Tr(i18n::Str::FontSec5ExportAllPreset), &fontState.fontExportAll);

            if (ImGui::Button(Tr(i18n::Str::FontSec5ExportButton), ImVec2(-1, 30))) {
                if (!fontState.fontRenderer.IsLoaded()) {
                    studio.statusMsg = Tr(i18n::Str::MsgFontNotLoaded);
                    studio.statusIsErr = true;
                } else {
                    std::string outDir(fontState.fontOutDirBuf);
                    std::string err;
                    int total = 0;

                    if (fontState.fontExportAll) {
                        for (const auto& p : fontState.presets) {
                            RenderOptions curOpts = fontState.fontOpts;
                            curOpts.target_width = p.width;
                            curOpts.target_height = p.height;
                            auto r = fontState.fontRenderer.RenderPreset(p, curOpts);
                            if (!FontRenderer::SaveGlyphs(r.second, outDir, err)) {
                                char _m[512];
                                snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgFontExportError), err.c_str());
                                studio.statusMsg = _m;
                                studio.statusIsErr = true;
                                break;
                            }
                            total += (int)r.second.size();
                        }
                        if (!studio.statusIsErr) {
                            char _m[512];
                            snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgFontExportSuccess), total, outDir.c_str());
                            studio.statusMsg = _m;
                            studio.statusIsErr = false;
                        }
                    } else {
                        const auto& curPreset = fontState.presets[fontState.selectedPresetIdx];
                        RenderOptions curOpts = fontState.fontOpts;
                        curOpts.target_width = curPreset.width;
                        curOpts.target_height = curPreset.height;
                        auto r = fontState.fontRenderer.RenderPreset(curPreset, curOpts);
                        if (FontRenderer::SaveGlyphs(r.second, outDir, err)) {
                            char _m[512];
                            snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgFontExportSuccessSingle), (int)r.second.size(), curPreset.name.c_str(), outDir.c_str());
                            studio.statusMsg = _m;
                            studio.statusIsErr = false;
                        } else {
                            char _m[512];
                            snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgFontExportError), err.c_str());
                            studio.statusMsg = _m;
                            studio.statusIsErr = true;
                        }
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Right Font Preview Panel
        ImGui::BeginChild("FontPreviewPanel", ImVec2(0.0f, 0.0f), true);
        {
            ImGui::SeparatorText(Tr(i18n::Str::FontRightLivePreview));

            ImGui::SetNextItemWidth(140.0f);
            ImGui::SliderInt(Tr(i18n::Str::LabelZoom), &fontState.fontZoom, 2, 12);
            ImGui::SameLine();
            ImGui::Checkbox(Tr(i18n::Str::LabelFontPixelGrid), &fontState.fontShowGrid);
            ImGui::SameLine();

            const auto& curPreset = fontState.presets[fontState.selectedPresetIdx];
            ImGui::TextDisabled(Tr(i18n::Str::FontPresetInfo),
                curPreset.name.c_str(), curPreset.width, curPreset.height,
                (int)curPreset.glyphs.size(), fontState.currentFontSize);

            ImGui::Separator();

            // Glyphs grid area
            ImGui::BeginChild("FontGlyphsGridArea", ImVec2(0.0f, 0.0f), false);
            {
                if (fontState.fontPreviewGlyphs.empty()) {
                    ImGui::TextDisabled("%s", Tr(i18n::Str::FontRightNoGlyphs));
                } else {
                    float fZoom = (float)fontState.fontZoom;
                    float gw = curPreset.width * fZoom;
                    float gh = curPreset.height * fZoom;
                    float card_w = (std::max)(gw + 16.0f, 68.0f);
                    float card_h = gh + 44.0f;

                    float avail_w = ImGui::GetContentRegionAvail().x;
                    float current_row_x = 0.0f;

                    ImDrawList* draw_list = ImGui::GetWindowDrawList();

                    for (size_t i = 0; i < fontState.fontPreviewGlyphs.size(); ++i) {
                        const auto& glyph = fontState.fontPreviewGlyphs[i];

                        if (current_row_x + card_w > avail_w && current_row_x > 0.0f) {
                            current_row_x = 0.0f;
                        } else if (i > 0) {
                            ImGui::SameLine();
                        }

                        ImVec2 origin = ImGui::GetCursorScreenPos();

                        // Card background
                        ImVec2 card_p0 = origin;
                        ImVec2 card_p1 = ImVec2(origin.x + card_w, origin.y + card_h);
                        draw_list->AddRectFilled(card_p0, card_p1, IM_COL32(32, 32, 36, 255), 4.0f);
                        draw_list->AddRect(card_p0, card_p1, IM_COL32(65, 65, 75, 255), 4.0f);

                        // Header label
                        char header[16];
                        snprintf(header, sizeof(header), "'%c'", glyph.ch);
                        draw_list->AddText(ImVec2(origin.x + 8.0f, origin.y + 4.0f), IM_COL32(230, 230, 230, 255), header);

                        // Inner black canvas
                        float canvas_x = origin.x + (card_w - gw) * 0.5f;
                        float canvas_y = origin.y + 22.0f;
                        ImVec2 canvas_p0 = ImVec2(canvas_x, canvas_y);
                        ImVec2 canvas_p1 = ImVec2(canvas_x + gw, canvas_y + gh);

                        draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(8, 8, 8, 255));
                        draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(90, 90, 100, 255));

                        // 1-bit pixels
                        for (int py = 0; py < glyph.height; ++py) {
                            for (int px = 0; px < glyph.width; ++px) {
                                uint8_t val = glyph.pixels[py * glyph.width + px];
                                if (val > 0) {
                                    ImVec2 px0 = ImVec2(canvas_p0.x + px * fZoom, canvas_p0.y + py * fZoom);
                                    ImVec2 px1 = ImVec2(px0.x + fZoom, px0.y + fZoom);
                                    draw_list->AddRectFilled(px0, px1, IM_COL32(255, 255, 255, 255));
                                }
                            }
                        }

                        // Grid
                        if (fontState.fontShowGrid && fontState.fontZoom >= 4) {
                            ImU32 gridCol = IM_COL32(50, 50, 60, 140);
                            for (int gx = 1; gx < glyph.width; ++gx) {
                                float x = canvas_p0.x + gx * fZoom;
                                draw_list->AddLine(ImVec2(x, canvas_p0.y), ImVec2(x, canvas_p1.y), gridCol);
                            }
                            for (int gy = 1; gy < glyph.height; ++gy) {
                                float y = canvas_p0.y + gy * fZoom;
                                draw_list->AddLine(ImVec2(canvas_p0.x, y), ImVec2(canvas_p1.x, y), gridCol);
                            }
                        }

                        // Dimensions
                        char footer[32];
                        snprintf(footer, sizeof(footer), "%dx%d", glyph.active_width, glyph.active_height);
                        draw_list->AddText(ImVec2(origin.x + 8.0f, card_p1.y - 18.0f), IM_COL32(160, 160, 160, 255), footer);

                        ImGui::Dummy(ImVec2(card_w, card_h));
                        current_row_x += card_w + 8.0f;
                    }
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        ImGui::EndTabItem();
    }
}

} // namespace UiFontGenerator

#endif // STUDIO_FONTS

