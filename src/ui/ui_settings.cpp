#include "ui_settings.h"
#include "app_state.h"
#include "ui_keyboard.h"
#include "ui_controls.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <string_view>

namespace UiSettings {
namespace {
void WrappedNote(i18n::Str text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", Tr(text));
    ImGui::PopStyleColor();
}
void Zoom(i18n::Str label, const char* id, int& value, int maximum = StudioPreferences::MaximumZoom) {
    ImGui::PushID(id);
    Ui::ControlRow(Tr(label));
    Ui::ZoomSlider("##Zoom", value, maximum);
    ImGui::PopID();
}
void Sections(StudioPreferences::Preferences& draft, size_t first, size_t last) {
    static constexpr i18n::Str labels[]{
        i18n::Str::PatchPlacement, i18n::Str::PatchAdjustments, i18n::Str::GroupAnimations,
        i18n::Str::LayoutOverlaps, i18n::Str::LayoutPreviewState, i18n::Str::LayoutPatchSites,
        i18n::Str::DeviceCpuRegisters, i18n::Str::DeviceStack, i18n::Str::DeviceLog
    };
    for (size_t index = first; index < last; ++index) {
        ImGui::PushID(StudioPreferences::SectionIds[index]);
        ImGui::Checkbox(Tr(labels[index]), &draft.sectionOpen[index]);
        ImGui::PopID();
    }
}
void General(State& state) {
    ImGui::SeparatorText(Tr(i18n::Str::MenuLanguage));
    const char* current = "";
    for (const auto& language : i18n::GetSupportedLanguages())
        if (language.lang == state.language) current = language.displayName;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##Language", current)) {
        for (const auto& language : i18n::GetSupportedLanguages()) {
            const bool selected = language.lang == state.language;
            if (ImGui::Selectable(language.displayName, selected)) state.language = language.lang;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::Spacing();
    ImGui::SeparatorText(Tr(i18n::Str::SettingsGeneral));
    ImGui::Checkbox(Tr(i18n::Str::SettingsSmoothAnimations), &state.draft.smoothAnimations);
    ImGui::TextWrapped("%s", Tr(i18n::Str::SettingsLogReviewSeconds));
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderInt("##LogReviewSeconds", &state.draft.logReviewSeconds, 1, 30, "%d s", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Spacing();
    WrappedNote(i18n::Str::SettingsImmediateNote);
}
void CustomScreens(State& state) {
    ImGui::SeparatorText(Tr(i18n::Str::PatchCustomScreens));
    const i18n::Str targets[]{i18n::Str::PatchBootLogo, i18n::Str::PatchShutdownLogo, i18n::Str::PatchDeviceInfo};
    ImGui::TextWrapped("%s", Tr(i18n::Str::SettingsDefaultTarget));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##DefaultTarget", Tr(targets[state.draft.defaultPatchTarget]))) {
        for (int index = 0; index < 3; ++index)
            if (ImGui::Selectable(Tr(targets[index]), state.draft.defaultPatchTarget == index)) state.draft.defaultPatchTarget = index;
        ImGui::EndCombo();
    }
    const i18n::Str stages[]{i18n::Str::PatchFirstStage, i18n::Str::PatchSecondStage, i18n::Str::PatchBothStages};
    ImGui::TextWrapped("%s", Tr(i18n::Str::SettingsDefaultBootStages));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##BootStages", Tr(stages[state.draft.defaultBootStages - 1]))) {
        for (int index = 0; index < 3; ++index)
            if (ImGui::Selectable(Tr(stages[index]), state.draft.defaultBootStages == index + 1)) state.draft.defaultBootStages = index + 1;
        ImGui::EndCombo();
    }
    ImGui::SeparatorText(Tr(i18n::Str::SettingsExpandedSections));
    Sections(state.draft, 0, 3);
}
void Interface(State& state) {
    ImGui::SeparatorText(Tr(i18n::Str::SettingsDefaultZooms));
    if (Ui::BeginControlTable("ZoomDefaults")) {
        Zoom(i18n::Str::PatchCustomScreens, "Patch", state.draft.patchZoom);
        Zoom(i18n::Str::PatchDeviceMenu, "Menu", state.draft.menuZoom);
        Zoom(i18n::Str::EmulatorModeScreens, "Emulator", state.draft.emulatorZoom);
        Zoom(i18n::Str::EmulatorModeDevice, "Device", state.draft.deviceZoom);
        Zoom(i18n::Str::TabFirmwareLayout, "Layout", state.draft.layoutZoom);
        Zoom(i18n::Str::TabFirmwareComparison, "Comparison", state.draft.comparisonZoom);
        Zoom(i18n::Str::TabResourceStudio, "Resource", state.draft.resourceZoom, StudioPreferences::ResourceMaximumZoom);
        Zoom(i18n::Str::TabFontGenerator, "Font", state.draft.fontZoom);
        ImGui::EndTable();
    }
    ImGui::SeparatorText(Tr(i18n::Str::SettingsExpandedSections));
    ImGui::TextDisabled("%s", Tr(i18n::Str::TabFirmwareLayout));
    Sections(state.draft, 3, 6);
    ImGui::TextDisabled("%s", Tr(i18n::Str::EmulatorModeDevice));
    Sections(state.draft, 6, 9);
}
bool Commit(State& state, StudioState& studio) {
    const auto previousLanguage = i18n::GetCurrentLanguage();
    const bool languageChanged = previousLanguage != state.language;
    if (languageChanged) {
        i18n::SetLanguage(state.language);
        if (!i18n::SaveSettings(state.configPath)) {
            i18n::SetLanguage(previousLanguage);
            state.error = Tr(i18n::Str::SettingsSaveError);
            return false;
        }
    }
    const auto previous = StudioPreferences::Get();
    StudioPreferences::Get() = state.draft;
    if (!StudioPreferences::Save(state.configPath)) {
        StudioPreferences::Get() = previous;
        i18n::SetLanguage(previousLanguage);
        const bool restored = !languageChanged || i18n::SaveSettings(state.configPath);
        state.error = Tr(restored ? i18n::Str::SettingsSaveError : i18n::Str::SettingsRollbackError);
        return false;
    }
    if (languageChanged) studio.SynchronizeEditorState();
    return true;
}
}
void RenderMenuBar(State& state) {
    ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, ImGui::GetStyle().WindowPadding.x * 2.0f);
    if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem(Tr(i18n::Str::SettingsTitle))) state.requested = true;
        ImGui::EndMenuBar();
    }
    ImGui::PopStyleVar();
}
void RenderDialog(State& state, StudioState& studio) {
    char title[160];
    snprintf(title, sizeof(title), "%s###StudioSettings", Tr(i18n::Str::SettingsTitle));
    const double now = ImGui::GetTime();
    if (state.requested) {
        state.requested = false;
        state.open = true;
        state.draft = StudioPreferences::Get();
        state.language = i18n::GetCurrentLanguage();
        state.error.clear();
        ImGui::OpenPopup(title);
    }
    if (!ImGui::IsPopupOpen(title)) {
        state.open = false;
        state.transition = {};
        return;
    }
    state.transition.Update(state.open, now);
    if (state.transition.FullyHidden()) {
        ImGui::ClosePopupToLevel(ImGui::GetCurrentContext()->BeginPopupStack.Size, true);
        ImGui::GetCurrentContext()->DimBgRatio = 0;
        return;
    }
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(800, 660), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(660, 500), viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * state.transition.Opacity());
    if (!ImGui::BeginPopupModal(title, nullptr, state.transition.IsClosing() ? ImGuiWindowFlags_NoInputs : 0)) {
        ImGui::PopStyleVar();
        return;
    }
    ImGui::GetCurrentContext()->DimBgRatio = state.transition.Opacity();
    const auto close = [&] { state.open = false; state.transition.Update(false, now); };
    if (Ui::ModalCloseButton(state.transition.IsClosing()) ||
        (Ui::DialogKeyboardAvailable() && ImGui::IsKeyPressed(ImGuiKey_Escape, false))) close();
    ImGui::BeginDisabled(!state.open);
    const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing() * 3.0f;
    ImGui::BeginChild("SettingsNavigation", ImVec2(190, -footer), ImGuiChildFlags_Borders);
    const i18n::Str categories[]{i18n::Str::SettingsGeneral, i18n::Str::PatchCustomScreens, i18n::Str::SettingsInterfaceEmulator};
    for (int index = 0; index < 3; ++index) {
        ImGui::PushID(index);
        const ImVec2 labelSize = ImGui::CalcTextSize(Tr(categories[index]), nullptr, false, 164);
        const ImVec2 start = ImGui::GetCursorScreenPos();
        if (ImGui::Selectable("##Category", state.category == index, 0,
            ImVec2(0, labelSize.y + ImGui::GetStyle().FramePadding.y * 2))) state.category = index;
        ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(start.x + 6, start.y + ImGui::GetStyle().FramePadding.y),
            ImGui::GetColorU32(ImGuiCol_Text), Tr(categories[index]), nullptr, 164);
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("SettingsContent", ImVec2(0, -footer), ImGuiChildFlags_Borders);
    if (state.category == 0) General(state);
    else {
        if (state.category == 1) CustomScreens(state);
        else Interface(state);
    }
    ImGui::EndChild();
    ImGui::Separator();
    if (!state.error.empty()) ImGui::TextWrapped("%s", state.error.c_str());
    else WrappedNote(i18n::Str::SettingsDefaultsNote);
    if (ImGui::Button(Tr(i18n::Str::SettingsResetDefaults))) {
        state.draft = {};
        state.language = i18n::DetectSystemLanguage();
        state.error.clear();
    }
    const float buttonWidth = (std::max)(96.0f, (std::max)(ImGui::CalcTextSize(Tr(i18n::Str::BtnSaveChanges)).x,
        ImGui::CalcTextSize(Tr(i18n::Str::BtnCancelAction)).x) + ImGui::GetStyle().FramePadding.x * 2);
    ImGui::SameLine((std::max)(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - buttonWidth * 2 - ImGui::GetStyle().ItemSpacing.x));
    if (ImGui::Button(Tr(i18n::Str::BtnSaveChanges), ImVec2(buttonWidth, 0)) && Commit(state, studio)) close();
    ImGui::SameLine();
    if (ImGui::Button(Tr(i18n::Str::BtnCancelAction), ImVec2(buttonWidth, 0))) close();
    ImGui::EndDisabled();
    ImGui::EndPopup();
    ImGui::PopStyleVar();
}
} // namespace UiSettings
