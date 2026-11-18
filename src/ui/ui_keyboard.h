#pragma once
#include "imgui.h"
#include "imgui_internal.h"

namespace Ui {
// Physical/editor keys yield to widgets, popups and modified shortcuts.
inline bool EditorKeyboardAvailable(ImGuiID mouseOwner = 0, bool allowShift = false) {
    const auto& io = ImGui::GetIO();
    return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !io.WantTextInput && !io.KeyCtrl && !io.KeyAlt && !io.KeySuper && (allowShift || !io.KeyShift) &&
        !(ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled) &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup) &&
        (!ImGui::IsAnyItemActive() || (mouseOwner && ImGui::GetActiveID() == mouseOwner));
}
inline bool DialogKeyboardAvailable() {
    const auto& io = ImGui::GetIO();
    const auto& context = *ImGui::GetCurrentContext();
    return !io.WantTextInput && !ImGui::IsAnyItemActive() &&
        !io.KeyCtrl && !io.KeyAlt && !io.KeySuper &&
        context.OpenPopupStack.Size == context.BeginPopupStack.Size;
}
inline bool DocumentKeyboardAvailable() {
    const auto& io = ImGui::GetIO();
    return !io.WantTextInput && !ImGui::IsAnyItemActive() && !io.KeyAlt && !io.KeySuper &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
}
} // namespace Ui
