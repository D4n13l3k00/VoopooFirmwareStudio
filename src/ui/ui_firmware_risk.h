#pragma once
#include "i18n.h"
#include "imgui.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace UiFirmwareRisk {
// One acknowledgement covers both code-patching editors, not resource edits.
struct Consent {
    std::string configPath = i18n::GetConfigPath();
    bool accepted = GetPrivateProfileIntA("Safety", "EmulatorOnlyConsentV1", 0, configPath.c_str()) == 1;
    bool saveFailed = false;
    double started = 0;
    int lastFrame = -2;
};

inline Consent& State() {
    static Consent consent;
    return consent;
}

inline bool Accepted() { return State().accepted; }

// Caller renders the tab disabled first; this draws the dim layer and banner over it.
inline void Overlay(ImVec2 origin, ImVec2 available) {
    auto& consent = State();
    if (consent.accepted || available.x <= 0 || available.y <= 0) return;
    const int frame = ImGui::GetFrameCount();
    if (consent.lastFrame != frame - 1) consent.started = ImGui::GetTime();
    consent.lastFrame = frame;
    const int remaining = (std::max)(0, int(std::ceil(10.0 - (ImGui::GetTime() - consent.started))));

    // A later sibling child draws above the already rendered tab content.
    ImGui::SetCursorScreenPos(origin);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("FirmwareRiskOverlay", available, false,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImGui::GetWindowDrawList()->AddRectFilled(origin,
        ImVec2(origin.x + available.x, origin.y + available.y), IM_COL32(8, 8, 10, 150));
    const float side = (std::min)({available.x - 32.0f, available.y - 32.0f, 520.0f});
    const ImVec2 size((std::max)(1.0f, side), (std::max)(1.0f, side));
    ImGui::SetCursorScreenPos(ImVec2(origin.x + (available.x - size.x) * 0.5f, origin.y + (available.y - size.y) * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.07f, 0.04f, 1));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.95f, 0.32f, 0.12f, 1));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.20f, 0.07f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.88f, 0.29f, 0.10f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.62f, 0.15f, 0.05f, 1));
    ImGui::BeginChild("FirmwareRiskConsent", size, true);
    ImGui::TextColored(ImVec4(1, 0.48f, 0.20f, 1), "%s", Tr(i18n::Str::RiskTitle));
    ImGui::Spacing();
    ImGui::TextWrapped("%s", Tr(i18n::Str::RiskBody));
    ImGui::Spacing();
    ImGui::TextWrapped("%s", Tr(i18n::Str::RiskScope));
    if (consent.saveFailed) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1, 0.35f, 0.25f, 1), "%s", Tr(i18n::Str::RiskSaveError));
    }
    const float buttonHeight = ImGui::GetFrameHeight() * 1.5f;
    ImGui::SetCursorPosY((std::max)(ImGui::GetCursorPosY(), ImGui::GetWindowHeight() - 24 - buttonHeight));
    char button[256];
    if (remaining) snprintf(button, sizeof(button), Tr(i18n::Str::RiskWait), remaining);
    else snprintf(button, sizeof(button), "%s", Tr(i18n::Str::RiskAccept));
    ImGui::BeginDisabled(remaining != 0);
    if (ImGui::Button(button, ImVec2(-1, buttonHeight))) {
        consent.accepted = WritePrivateProfileStringA("Safety", "EmulatorOnlyConsentV1", "1", consent.configPath.c_str()) != FALSE;
        consent.saveFailed = !consent.accepted;
    }
    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);
    ImGui::EndChild();
}
} // namespace UiFirmwareRisk
