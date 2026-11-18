#pragma once

#include "imgui_internal.h"
#include "studio_preferences.h"
#include <algorithm>

namespace Ui {

// The caller supplies its clock, so transitions can be driven without wall time.
class ModalTransition {
public:
    void Update(bool visible, double now) {
        if (!StudioPreferences::Get().smoothAnimations) {
            phase_ = visible ? 1.0f : 0.0f;
        } else if (initialized_) {
            const float elapsed = static_cast<float>((std::max)(0.0, now - updatedAt_));
            phase_ = (std::clamp)(phase_ + (visible_ ? elapsed / 0.15f : -elapsed / 0.18f), 0.0f, 1.0f);
        }
        initialized_ = true;
        updatedAt_ = now;
        visible_ = visible;
    }

    float Opacity() const { return phase_ * phase_ * (3.0f - 2.0f * phase_); }
    bool FullyHidden() const { return !visible_ && phase_ == 0.0f; }
    bool IsClosing() const { return !visible_ && phase_ > 0.0f; }

private:
    double updatedAt_ = 0;
    float phase_ = 0;
    bool initialized_ = false;
    bool visible_ = false;
};

// BeginPopupModal's native p_open closes immediately. Draw the same titlebar X
// as a request instead, leaving the popup alive until its fade completes.
inline bool ModalCloseButton(bool disabled) {
    auto* window = ImGui::GetCurrentWindow();
    auto& context = *ImGui::GetCurrentContext();
    const auto& style = ImGui::GetStyle();
    const auto previousLayer = window->DC.NavLayerCurrent;
    const auto previousItem = context.LastItemData;
    ImGui::BeginDisabled(disabled);
    ImGui::PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus | ImGuiItemFlags_NoFocus, true);
    window->DC.NavLayerCurrent = ImGuiNavLayer_Menu;
    ImGui::PushClipRect(window->Pos, ImVec2(window->Pos.x + window->Size.x,
        window->Pos.y + window->Size.y), false);
    const bool pressed = ImGui::CloseButton(window->GetID("#CLOSE"),
        ImVec2(window->Pos.x + window->Size.x - style.FramePadding.x - ImGui::GetFontSize(),
            window->Pos.y + style.FramePadding.y));
    ImGui::PopClipRect();
    window->DC.NavLayerCurrent = previousLayer;
    ImGui::PopItemFlag();
    ImGui::EndDisabled();
    context.LastItemData = previousItem;
    return pressed;
}

} // namespace Ui
