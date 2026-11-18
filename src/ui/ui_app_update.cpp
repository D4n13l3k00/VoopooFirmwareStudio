#include "ui_app_update.h"
#include "app_update_install.h"
#include "app_state.h"
#include "ui_resource_studio.h"
#include "imgui.h"

namespace UiAppUpdate {
void RenderMenuBar(State& state) {
    ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, ImGui::GetStyle().WindowPadding.x * 2.0f);
    if (!ImGui::BeginMenuBar()) {
        ImGui::PopStyleVar();
        return;
    }
    if (ImGui::BeginMenu(Tr(i18n::Str::MenuHelp))) {
        if (ImGui::MenuItem(Tr(i18n::Str::UpdateTitle))) state.requested = true;
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
    ImGui::PopStyleVar();
}
void RenderDialog(State& state, StudioState& studio, HWND hwnd) {
    const auto previousPhase = state.client.Get().phase;
    state.client.Poll();
    if (previousPhase == AppUpdate::Phase::Checking &&
        state.client.Get().phase != AppUpdate::Phase::Checking)
        state.markdownReady = state.markdown.SetText(state.client.Get().release.notes);
    char title[160];
    snprintf(title, sizeof(title), "%s###AppUpdates", Tr(i18n::Str::UpdateTitle));
    const double now = ImGui::GetTime();
    if (state.requested) {
        state.requested = false;
        state.open = true;
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
    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(460, 300), viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * state.transition.Opacity());
    if (!ImGui::BeginPopupModal(title, nullptr,
            state.transition.IsClosing() ? ImGuiWindowFlags_NoInputs : 0)) {
        ImGui::PopStyleVar();
        return;
    }
    ImGui::GetCurrentContext()->DimBgRatio = state.transition.Opacity();
    if (Ui::ModalCloseButton(state.transition.IsClosing())) {
        state.open = false;
        state.transition.Update(false, now);
    }
    // Closing only hides this view. Poll above keeps checks/downloads alive.
    ImGui::BeginDisabled(state.transition.IsClosing());
    const auto& status = state.client.Get();
    using enum AppUpdate::Phase;
    const bool busy = status.phase == Checking || status.phase == Downloading;
    ImGui::Text(Tr(i18n::Str::UpdateVersion), AppUpdate::Version.data());
    ImGui::BeginDisabled(busy);
    if (ImGui::Button(Tr(i18n::Str::UpdateCheck))) {
        state.installError.clear();
        state.client.Check();
    }
    ImGui::EndDisabled();
    switch (status.phase) {
    case Checking: ImGui::TextUnformatted(Tr(i18n::Str::UpdateChecking)); break;
    case Current: ImGui::TextUnformatted(Tr(i18n::Str::UpdateCurrent)); break;
    case Available:
        ImGui::Text(Tr(i18n::Str::UpdateAvailable), status.release.tag.c_str());
        if (ImGui::Button(Tr(i18n::Str::UpdateDownload))) {
            state.installError.clear();
            state.client.Download();
        }
        break;
    case Downloading: ImGui::TextUnformatted(Tr(i18n::Str::UpdateDownloading)); break;
    case Ready:
        ImGui::TextUnformatted(Tr(i18n::Str::UpdateReady));
        if (ImGui::Button(Tr(i18n::Str::UpdateInstall))) {
            auto install = [&state] {
                if (AppUpdate::StartInstall(state.client.Get(), state.installError)) {
                    state.client.ReleaseStage();
                    PostQuitMessage(0);
                }
            };
            if (UiResourceStudio::ConfirmUnsavedChanges(studio, hwnd, install)) install();
        }
        break;
    case Failed: ImGui::TextUnformatted(Tr(i18n::Str::UpdateFailed)); break;
    default: break;
    }
    const auto& error = state.installError.empty() ? status.error : state.installError;
    if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
    if (!status.release.notes.empty()) {
        ImGui::SeparatorText(Tr(i18n::Str::UpdateChangelog));
        ImGui::BeginChild("ReleaseNotes", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
        if (state.markdownReady) state.markdown.Render();
        else ImGui::TextUnformatted("Cannot format release notes.");
        ImGui::EndChild();
    }
    ImGui::EndDisabled();
    ImGui::EndPopup();
    ImGui::PopStyleVar();
}
}
