#pragma once
#include "imgui.h"

namespace Ui {
// Keep tab sizing and its cached label inset identical, without styling content.
inline bool BeginTabBar(const char* id, ImGuiTabBarFlags flags = 0) {
    ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, ImGui::GetFontSize() * .75f);
    const bool visible = ImGui::BeginTabBar(id, flags);
    ImGui::PopStyleVar();
    return visible;
}

inline bool BeginTabItem(const char* label, bool* open = nullptr, ImGuiTabItemFlags flags = 0) {
    ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, ImGui::GetFontSize() * .75f);
    const bool visible = ImGui::BeginTabItem(label, open, flags);
    ImGui::PopStyleVar();
    return visible;
}
} // namespace Ui
