#include "ui_palette.h"
#include "../imgui/imgui.h"

namespace UiPalette {

void RenderPaletteEditor(StudioState& studio) {
    ImGui::SeparatorText(Tr(i18n::Str::SectionPaletteTheme));
    if (!studio.hasFirmware) {
        ImGui::TextDisabled("%s", Tr(i18n::Str::PromptLoadFwColors));
        return;
    }

    // Quick Color Themes
    const auto& themes = GetPaletteThemes();
    static int selThemeIdx = 0;
    std::vector<const char*> themeNames;
    for (size_t i = 0; i < themes.size(); ++i) themeNames.push_back(GetThemeNameTr(i));

    ImGui::Text("%s:", Tr(i18n::Str::LabelTheme));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##PaletteThemeSelect", &selThemeIdx, themeNames.data(), (int)themeNames.size())) {
        ApplyPaletteTheme(studio, themes[selThemeIdx]);
    }
    ImGui::TextDisabled("%s", GetThemeDescTr((size_t)selThemeIdx));

    float themeBtnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button(Tr(i18n::Str::BtnApply), ImVec2(themeBtnW, 0))) {
        ApplyPaletteTheme(studio, themes[selThemeIdx]);
    }
    ImGui::SameLine();
    if (ImGui::Button(Tr(i18n::Str::BtnResetAll), ImVec2(themeBtnW, 0))) {
        ApplyPaletteTheme(studio, themes[0]);
    }

    ImGui::Spacing();

    std::vector<const char*> palLabels;
    for (const auto& p : studio.palette) {
        palLabels.push_back(GetRoleLabelTr(p.spec.id));
    }
    int palIdx = (int)studio.selectedPaletteIdx;
    ImGui::Text("%s:", Tr(i18n::Str::LabelRole));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##PaletteRoleSelect", &palIdx, palLabels.data(), (int)palLabels.size())) {
        studio.selectedPaletteIdx = (size_t)palIdx;
        strncpy_s(studio.hexEditBuf, studio.palette[palIdx].current.ToHex().c_str(), sizeof(studio.hexEditBuf));
    }

    auto& curEntry = studio.palette[studio.selectedPaletteIdx];
    ImGui::TextDisabled("%s", GetRoleScopeTr(curEntry.spec.id));

    // Color swatch & Hex input & Reset Role button
    Rgb8 sCol = curEntry.current.ScreenColor();
    float roleColFloat[3] = { sCol.r / 255.0f, sCol.g / 255.0f, sCol.b / 255.0f };
    if (ImGui::ColorEdit3("##RoleColEdit", roleColFloat, ImGuiColorEditFlags_NoInputs)) {
        Rgb8 picked((uint8_t)(roleColFloat[0] * 255.0f + 0.5f), (uint8_t)(roleColFloat[1] * 255.0f + 0.5f), (uint8_t)(roleColFloat[2] * 255.0f + 0.5f));
        studio.PushUndo(std::string(Tr(i18n::Str::LabelRole)) + ": " + curEntry.spec.id);
        studio.SetPaletteColor(studio.selectedPaletteIdx, picked);
        strncpy_s(studio.hexEditBuf, picked.ToHex().c_str(), sizeof(studio.hexEditBuf));
        studio.textureNeedsUpdate = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::InputText("##RoleHex", studio.hexEditBuf, sizeof(studio.hexEditBuf), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase)) {
        Rgb8 parsed;
        if (Rgb8::ParseHex(studio.hexEditBuf, parsed)) {
            studio.PushUndo(std::string(Tr(i18n::Str::LabelHex)) + ": " + curEntry.spec.id);
            studio.SetPaletteColor(studio.selectedPaletteIdx, parsed);
            studio.textureNeedsUpdate = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(Tr(i18n::Str::BtnResetRole), ImVec2(-1.0f, 0))) {
        studio.PushUndo(std::string(Tr(i18n::Str::BtnResetRole)) + ": " + curEntry.spec.id);
        studio.SetPaletteColor(studio.selectedPaletteIdx, curEntry.spec.original);
        strncpy_s(studio.hexEditBuf, curEntry.current.ToHex().c_str(), sizeof(studio.hexEditBuf));
        studio.textureNeedsUpdate = true;
    }
}

} // namespace UiPalette

