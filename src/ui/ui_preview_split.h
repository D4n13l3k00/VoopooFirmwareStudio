#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cfloat>

namespace Ui {
// Keep the bottom panel's preferred height while the parent changes size.
// The preview's native bottom resize border still adjusts that preference.
inline bool BeginBottomAnchoredPreview(const char* id, ImGuiWindowFlags flags = 0) {
    auto* storage = ImGui::GetStateStorage();
    ImGui::PushID(id);
    const ImGuiID availableKey = ImGui::GetID("SplitAvailable");
    const ImGuiID previewKey = ImGui::GetID("SplitPreview");
    const ImGuiID memoryKey = ImGui::GetID("SplitMemory");
    ImGui::PopID();
    const float available = ImGui::GetContentRegionAvail().y;
    const float previousAvailable = storage->GetFloat(availableKey, -1);
    const float previousPreview = storage->GetFloat(previewKey, 0);
    const float preferredMemory = storage->GetFloat(memoryKey, ImGui::GetFontSize() * 8);
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float minimumPreview = ImGui::GetFontSize() * 6;
    const float minimumMemory = ImGui::GetFontSize() * 5;
    const float maximumPreview = (std::max)(minimumPreview, available - minimumMemory - spacing);
    const bool parentResized = std::abs(available - previousAvailable) > 0.5f;
    if (parentResized)
        ImGui::SetNextWindowSize(ImVec2(0, std::clamp(available - preferredMemory - spacing,
            minimumPreview, maximumPreview)));
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, minimumPreview), ImVec2(FLT_MAX, maximumPreview));
    const bool visible = ImGui::BeginChild(id, ImVec2(0, -preferredMemory - spacing),
        ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY, flags);
    const float preview = ImGui::GetWindowHeight();
    if (!parentResized && previousPreview > 0 && std::abs(preview - previousPreview) > 0.5f)
        storage->SetFloat(memoryKey, (std::max)(minimumMemory, available - preview - spacing));
    storage->SetFloat(availableKey, available);
    storage->SetFloat(previewKey, preview);
    return visible;
}
} // namespace Ui
