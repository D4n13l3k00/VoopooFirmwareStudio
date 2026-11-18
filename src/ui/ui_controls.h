#pragma once
#include "imgui.h"
#include <algorithm>
#include <string_view>

namespace Ui {
inline bool BeginControlTable(const char* id, float width = 0) {
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings,
        ImVec2(width, 0))) return false;
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}
inline void ControlRow(const char* label) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    std::string_view text(label);
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1);
}
inline bool ZoomSlider(const char* label, int& value, int maximum = 16) {
    // Keep grabs uniform across 1–16x and 1–32x without losing integer keyboard steps.
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize,
        (std::max)(ImGui::GetFrameHeight(), ImGui::CalcItemWidth() / 16.0f));
    const bool changed = ImGui::SliderInt(label, &value, 1, maximum, "%dx", ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopStyleVar();
    return changed;
}
} // namespace Ui
