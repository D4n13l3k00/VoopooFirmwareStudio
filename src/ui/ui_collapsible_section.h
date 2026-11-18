#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "studio_preferences.h"
#include <cstring>

namespace Ui {
// A native, keyboard-navigable header and an auto-height body share one outline.
// The body keeps its own child ID; its transparent window lets the parent paint
// the joined background without a second border or an ItemSpacing-sized seam.
class CollapsibleSection {
public:
    CollapsibleSection(const char* bodyId, const char* title, ImGuiTreeNodeFlags flags = 0) {
        if (ImGui::GetCurrentWindow()->SkipItems) return;
        const float bodyX = ImGui::GetCursorScreenPos().x;
        const float bodyWidth = ImGui::GetContentRegionAvail().x;
        ImGui::PushID(bodyId);
        for (size_t i = 0; i < StudioPreferences::SectionIds.size(); ++i) {
            if (std::strcmp(bodyId, StudioPreferences::SectionIds[i]) != 0) continue;
            // Native storage owns subsequent toggles, even if preferences change.
            ImGui::SetNextItemOpen(StudioPreferences::Get().sectionOpen[i], ImGuiCond_Once);
            break;
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        // Same native implementation as CollapsingHeader, with an ID that does
        // not change when the visible title is translated.
        const bool open = ImGui::TreeNodeBehavior(ImGui::GetID("##SectionHeader"),
            flags | ImGuiTreeNodeFlags_CollapsingHeader, title);
        ImGui::PopStyleVar(2);
        ImGui::PopID();
        bounds_ = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        draw_ = ImGui::GetWindowDrawList();
        border_ = ImGui::GetColorU32(ImGuiCol_Border);
        auto background = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        background.x *= 0.86f; background.y *= 0.86f; background.z *= 0.86f;
        background_ = ImGui::GetColorU32(background);
        if (!open) return;

        // CollapsingHeader already advanced past ItemSpacing; meet its lower edge.
        // Native headers extend half the parent padding; keep that extension
        // out of child layout so it cannot create a horizontal scrollbar.
        ImGui::SetCursorScreenPos(ImVec2(bodyX, bounds_.Max.y));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
        visible_ = ImGui::BeginChild(bodyId, ImVec2(bodyWidth > 0 ? bodyWidth : 1.0f, 0),
            ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding |
            ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
        ImGui::PopStyleVar();
        body_ = ImGui::GetCurrentWindow();
    }

    ~CollapsibleSection() {
        if (!draw_) return;
        if (body_) {
            const float seam = bounds_.Max.y;
            bounds_.Max.y = body_->Pos.y + body_->Size.y;
            ImGui::EndChild();
            draw_->AddRectFilled(ImVec2(bounds_.Min.x, seam), bounds_.Max, background_);
        }
        draw_->AddRect(bounds_.Min, bounds_.Max, border_);
    }

    explicit operator bool() const { return visible_; }
    CollapsibleSection(const CollapsibleSection&) = delete;
    CollapsibleSection& operator=(const CollapsibleSection&) = delete;

private:
    ImDrawList* draw_ = nullptr;
    ImGuiWindow* body_ = nullptr;
    ImRect bounds_;
    ImU32 background_ = 0, border_ = 0;
    bool visible_ = false;
};
} // namespace Ui
