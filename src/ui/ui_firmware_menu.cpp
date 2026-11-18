#include "ui_firmware_menu.h"
#include "ui_firmware_patches.h"
#include "app_state.h"
#include "ui_firmware_risk.h"
#include "ui_tabs.h"
#include "ui_preview_split.h"
#include "ui_collapsible_section.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui_keyboard.h"
#include "ui_controls.h"
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cfloat>

namespace UiFirmwareMenu {
namespace {
constexpr i18n::Str ItemLabels[]{i18n::Str::PatchMenuInformation,
    i18n::Str::PatchMenuVoltage, i18n::Str::PatchMenuExit, i18n::Str::PatchMenuPuffEditor,
    i18n::Str::PatchMenuScoreEditor, i18n::Str::PatchMenuRankSelector};

uint16_t HardwareCode(const HardwareRecord& record) {
    return uint16_t(record.device_code[0]) << 8 | record.device_code[1];
}

uint32_t ItemMask(FirmwareMenu::Item item) {
    return item == FirmwareMenu::Item::Information ? FirmwareMenu::InformationMask :
        item == FirmwareMenu::Item::Voltage ? FirmwareMenu::VoltageMask :
        item == FirmwareMenu::Item::PuffEditor ? FirmwareMenu::PuffEditorMask :
        item == FirmwareMenu::Item::ScoreEditor ? FirmwareMenu::ScoreEditorMask :
        item == FirmwareMenu::Item::RankSelector ? FirmwareMenu::RankSelectorMask : 0;
}

unsigned VisibleItems(const FirmwareMenu::Settings& settings,
    std::array<FirmwareMenu::Item, FirmwareMenu::ItemCount>& items) {
    unsigned count = 0;
    for (const auto item : settings.order)
        if (!ItemMask(item) || (settings.enabledItems & ItemMask(item))) items[count++] = item;
    return count;
}

void Fire(Preview& preview, const FirmwareMenu::Settings& settings) {
    if (!settings.enabled) return;
    if (preview.state.selectingRank) {
        if (preview.state.rankSelection == FirmwareMenu::RankCount) {
            preview.state.selectingRank = false;
            return;
        }
        const auto points = FirmwareMenu::RankPoints[preview.state.rankSelection];
        preview.puffResult = points != preview.values.scorePoints ? 1 : 2;
        preview.values.scorePoints = points;
        preview.state.selectingRank = false;
        preview.state.selection = 0;
        return;
    }
    if (preview.state.information) { preview.state.information = false; return; }
    if (preview.state.editingPuffs || preview.state.editingScore) {
        if (preview.state.puffStatus == 2) {
            preview.state.editingPuffs = false;
            preview.state.editingScore = false;
            preview.state.puffDigit = 0;
            return;
        }
        if (++preview.state.puffDigit < FirmwareMenu::PuffDigitCount) return;
        const uint32_t original = preview.state.editingScore ?
            preview.state.originalPuffs / 1000 : preview.state.originalPuffs;
        preview.puffResult = preview.state.puffValue != original ? 1 : 2;
        if (preview.puffResult == 1) {
            if (preview.state.editingScore) preview.values.scorePoints = preview.state.puffValue;
            else preview.values.puffs = preview.state.puffValue;
        }
        preview.state.editingPuffs = preview.state.editingScore = false;
        preview.state.puffDigit = 0;
        return;
    }
    std::array<FirmwareMenu::Item, FirmwareMenu::ItemCount> items;
    const auto count = VisibleItems(settings, items);
    switch (items[preview.state.selection % count]) {
    case FirmwareMenu::Item::Information: preview.state.information = true; break;
    case FirmwareMenu::Item::Voltage: preview.state.batteryVoltage = !preview.state.batteryVoltage; break;
    case FirmwareMenu::Item::PuffEditor:
        preview.state.editingPuffs = true;
        preview.state.puffDigit = 0;
        preview.state.puffValue = preview.state.originalPuffs = preview.values.puffs;
        preview.state.puffStatus = preview.values.puffs > FirmwareMenu::PuffEditorMaximum ? 2 : 0;
        preview.puffResult = 0;
        break;
    case FirmwareMenu::Item::ScoreEditor:
        preview.state.editingScore = true;
        preview.state.puffDigit = 0;
        preview.state.puffValue = preview.values.scorePoints;
        preview.state.originalPuffs = preview.values.scorePoints * 1000;
        preview.state.puffStatus = preview.values.scorePoints > FirmwareMenu::PuffEditorMaximum ? 2 : 0;
        preview.puffResult = 0;
        break;
    case FirmwareMenu::Item::RankSelector:
        preview.state.selectingRank = true;
        preview.state.rankSelection = 0;
        while (preview.state.rankSelection + 1 < FirmwareMenu::RankCount &&
            preview.values.scorePoints >= FirmwareMenu::RankPoints[preview.state.rankSelection + 1])
            ++preview.state.rankSelection;
        preview.puffResult = 0;
        break;
    case FirmwareMenu::Item::Exit: preview.state.selection = 0; break;
    }
}

void ChangeDigit(Preview& preview, bool increase) {
    if (preview.state.puffStatus == 2 || preview.state.puffDigit >= FirmwareMenu::PuffDigitCount) return;
    constexpr uint32_t places[]{10000, 1000, 100, 10, 1};
    const uint32_t place = places[preview.state.puffDigit];
    const uint32_t digit = preview.state.puffValue / place % 10;
    const uint32_t base = preview.state.puffValue - digit * place;
    for (unsigned step = 1; step <= 10; ++step) {
        const uint32_t next = increase ? (digit + step) % 10 : (digit + 10 - step) % 10;
        const uint32_t candidate = base + next * place;
        if (candidate <= (preview.state.editingScore ? FirmwareMenu::PuffEditorMaximum : preview.values.maximumPuffs)) {
            preview.state.puffValue = candidate;
            preview.state.puffStatus = 0;
            return;
        }
    }
}

bool Editing(const Preview& preview) {
    return preview.state.editingPuffs || preview.state.editingScore || preview.state.selectingRank;
}

void CancelEdit(Preview& preview) {
    preview.state.editingPuffs = preview.state.editingScore = preview.state.selectingRank = false;
    preview.state.puffValue = preview.state.originalPuffs = preview.state.puffDigit = preview.state.puffStatus = 0;
    preview.state.rankSelection = 0;
    preview.puffResult = 0;
}

void PreviewStep(Preview& preview, const FirmwareMenu::Settings& settings, unsigned key, unsigned count) {
    if (preview.state.editingPuffs || preview.state.editingScore) ChangeDigit(preview, key == 1);
    else {
        auto& selection = preview.state.selectingRank ? preview.state.rankSelection : preview.state.selection;
        const unsigned rows = preview.state.selectingRank ? FirmwareMenu::RankRowCount : count;
        preview.state.information = false;
        const bool up = (key == 1) == settings.reverseNavigation;
        selection = (selection + (up ? rows - 1 : 1)) % rows;
        preview.state.puffStatus = 0;
    }
}

// Mouse release, not Button's click-on-release result, owns an editor action.
// Keyboard/navigation activation remains a complete short press.
void PreviewInput(Preview& preview, const FirmwareMenu::Settings& settings,
    unsigned key, unsigned count, bool activated) {
    const ImGuiKey keyboardKey = key == 1 ? ImGuiKey_UpArrow :
        key == 2 ? ImGuiKey_DownArrow : ImGuiKey_Space;
    const unsigned keyMask = 1u << key;
    if (preview.keyboardOwner && ImGui::IsKeyPressed(keyboardKey, ImGuiInputFlags_None, preview.keyboardOwner))
        preview.keyboardPressed |= keyMask;
    const bool keyboardDown = preview.keyboardOwner && (preview.keyboardPressed & keyMask) &&
        ImGui::IsKeyDown(keyboardKey, preview.keyboardOwner);
    if (preview.keyboardOwner && ImGui::IsKeyReleased(keyboardKey, preview.keyboardOwner)) {
        activated |= (preview.keyboardPressed & keyMask) != 0;
        preview.keyboardPressed &= ~keyMask;
    }
    const bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left) || keyboardDown;
    const bool held = (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) || keyboardDown;
    const double now = ImGui::GetTime();
    if (held && (Editing(preview) || preview.heldKey)) {
        if (!preview.heldKey) {
            preview.heldKey = key;
            preview.keyboardHolding = keyboardDown;
            preview.holdStarted = preview.holdLast = now;
            preview.holdCanceled = false;
            if (key != 3) PreviewStep(preview, settings, key, count);
        }
        if (preview.heldKey != key) return;
        const double elapsed = now - preview.holdStarted;
        if (key == 3) {
            if (!preview.holdCanceled && elapsed >= 0.700) {
                CancelEdit(preview);
                preview.holdCanceled = true;
            }
        } else if (elapsed >= 0.350 && now - preview.holdLast >= (elapsed >= 1.0 ? 0.050 : 0.120)) {
            PreviewStep(preview, settings, key, count);
            preview.holdLast = now;
        }
    } else if (preview.heldKey == key && !down) {
        if (key == 3 && !preview.holdCanceled) {
            if (now - preview.holdStarted >= 0.700) CancelEdit(preview);
            else Fire(preview, settings);
        }
        preview.heldKey = 0;
        preview.keyboardHolding = false;
    } else if (activated && !preview.heldKey) {
        if (key == 3) Fire(preview, settings);
        else PreviewStep(preview, settings, key, count);
    }
}

const UiFirmwarePatches::State::MemoryUsage* SelectedUsage(const UiFirmwarePatches::State& patches) {
    if (patches.memoryDocument != patches.menuPreview.documentIdentity) return nullptr;
    for (const auto& usage : patches.memory.usage)
        if (usage.hardwareCode == patches.menuPreview.hardwareCode) return &usage;
    return nullptr;
}

void UpdatePreview(Preview& preview, const FirmwareMenu::Settings& settings,
    ID3D11Device* device, ID3D11DeviceContext* context) {
    if (preview.device != device) {
        preview.texture.Release();
        preview.device = device;
        preview.rendered = false;
    }
    const bool changed = !preview.rendered || preview.renderedSettings != settings ||
        preview.renderedState.selection != preview.state.selection ||
        preview.renderedState.information != preview.state.information ||
        preview.renderedState.batteryVoltage != preview.state.batteryVoltage ||
        preview.renderedState.editingPuffs != preview.state.editingPuffs ||
        preview.renderedState.editingScore != preview.state.editingScore ||
        preview.renderedState.selectingRank != preview.state.selectingRank ||
        preview.renderedState.rankSelection != preview.state.rankSelection ||
        preview.renderedState.puffDigit != preview.state.puffDigit ||
        preview.renderedState.puffValue != preview.state.puffValue ||
        preview.renderedState.originalPuffs != preview.state.originalPuffs ||
        preview.renderedState.puffStatus != preview.state.puffStatus ||
        preview.renderedValues.batteryMillivolts != preview.values.batteryMillivolts ||
        preview.renderedValues.resistanceMilliOhms != preview.values.resistanceMilliOhms ||
        preview.renderedValues.durationCentiseconds != preview.values.durationCentiseconds ||
        preview.renderedValues.puffs != preview.values.puffs ||
        preview.renderedValues.maximumPuffs != preview.values.maximumPuffs ||
        preview.renderedValues.scorePoints != preview.values.scorePoints;
    if (changed) {
        std::array<uint32_t, 80 * 160> pixels;
        preview.textureError = !FirmwareMenu::RenderPreview(settings, preview.state, preview.values, pixels);
        if (!preview.textureError && (!preview.texture.IsValid() || preview.pixels != pixels)) {
            preview.textureError = !device || !context || FAILED(device->GetDeviceRemovedReason()) ||
                !preview.texture.Update(device, context, pixels.data(), 80, 160);
            if (!preview.textureError) preview.pixels = pixels;
        }
        if (preview.textureError) preview.texture.Release();
        preview.renderedSettings = settings;
        preview.renderedState = preview.state;
        preview.renderedValues = preview.values;
        preview.rendered = true;
    }
}

void DrawPreview(Preview& preview, const FirmwareMenu::Settings& settings,
    ID3D11Device* device, ID3D11DeviceContext* context) {
    std::array<FirmwareMenu::Item, FirmwareMenu::ItemCount> items;
    const auto count = VisibleItems(settings, items);
    if (preview.state.selection >= count) preview.state.selection = 0;
    if (!(settings.enabledItems & FirmwareMenu::InformationMask)) preview.state.information = false;
    if (!settings.enabled || !(settings.enabledItems & FirmwareMenu::PuffEditorMask))
        preview.state.editingPuffs = false;
    if (!settings.enabled || !(settings.enabledItems & FirmwareMenu::ScoreEditorMask))
        preview.state.editingScore = false;
    if (!settings.enabled || !(settings.enabledItems & FirmwareMenu::RankSelectorMask))
        preview.state.selectingRank = false;
    if (!settings.enabled || !(settings.enabledItems & (FirmwareMenu::PuffEditorMask |
        FirmwareMenu::ScoreEditorMask | FirmwareMenu::RankSelectorMask))) preview.puffResult = 0;
    if (preview.observed.batteryVoltage != settings.batteryVoltage)
        preview.state.batteryVoltage = settings.batteryVoltage;
    preview.observed = settings;
    const int frame = ImGui::GetFrameCount();
    if (preview.lastInputFrame != frame - 1) {
        preview.heldKey = preview.keyboardPressed = preview.mouseOwner = 0;
        preview.keyboardHolding = false;
    }
    preview.lastInputFrame = frame;
    constexpr ImGuiKey keys[]{ImGuiKey_Space, ImGuiKey_UpArrow, ImGuiKey_DownArrow};
    const bool keyboardAllowed = settings.enabled && Ui::EditorKeyboardAvailable(preview.mouseOwner);
    if (keyboardAllowed) {
        preview.keyboardOwner = ImGui::GetID("MenuPreviewKeyboard");
        for (const auto key : keys)
            ImGui::SetKeyOwner(key, preview.keyboardOwner, ImGuiInputFlags_LockThisFrame);
    } else {
        for (const auto key : keys)
            if (preview.keyboardOwner && ImGui::GetKeyOwner(key) == preview.keyboardOwner)
                ImGui::SetKeyOwner(key, ImGuiKeyOwner_NoOwner);
        preview.keyboardOwner = 0;
        preview.keyboardPressed = 0;
        if (preview.keyboardHolding) {
            preview.heldKey = 0;
            preview.keyboardHolding = false;
        }
    }
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    ImGui::BeginDisabled(!settings.enabled);
    bool mouseHeld = false;
    const bool plus = ImGui::ButtonEx("+##PreviewPlus", ImVec2(64, 32), ImGuiButtonFlags_NoNavFocus);
    if (ImGui::IsItemActive()) { preview.mouseOwner = ImGui::GetItemID(); mouseHeld = true; }
    PreviewInput(preview, settings, 1, count, plus);
    ImGui::SameLine();
    const bool fire = ImGui::ButtonEx(Tr(preview.state.information ?
        i18n::Str::PatchMenuBack : i18n::Str::PatchMenuFire), ImVec2(86, 32), ImGuiButtonFlags_NoNavFocus);
    if (ImGui::IsItemActive()) { preview.mouseOwner = ImGui::GetItemID(); mouseHeld = true; }
    PreviewInput(preview, settings, 3, count, fire);
    ImGui::SameLine();
    const bool minus = ImGui::ButtonEx("-##PreviewMinus", ImVec2(64, 32), ImGuiButtonFlags_NoNavFocus);
    if (ImGui::IsItemActive()) { preview.mouseOwner = ImGui::GetItemID(); mouseHeld = true; }
    PreviewInput(preview, settings, 2, count, minus);
    if (!mouseHeld) preview.mouseOwner = 0;
    ImGui::EndDisabled();
    ImGui::PopItemFlag();
    ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuPreviewOnly));
    ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuSample));
    UpdatePreview(preview, settings, device, context);
    const int scale = preview.zoom;
    const ImVec2 size(float(80 * scale), float(160 * scale));
    const float inset = (ImGui::GetContentRegionAvail().x - size.x) * 0.5f;
    if (inset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + inset);
    if (preview.textureError) {
        ImGui::TextWrapped("%s", Tr(i18n::Str::CompareTextureError));
    } else {
        const auto origin = ImGui::GetCursorScreenPos();
        const auto& platform = ImGui::GetPlatformIO();
        if (platform.DrawCallback_SetSamplerNearest)
            ImGui::GetWindowDrawList()->AddCallback(platform.DrawCallback_SetSamplerNearest, nullptr);
        ImGui::Image((ImTextureID)preview.texture.GetSRV(), size);
        if (platform.DrawCallback_SetSamplerLinear)
            ImGui::GetWindowDrawList()->AddCallback(platform.DrawCallback_SetSamplerLinear, nullptr);
        if (ImGui::IsItemClicked() && settings.enabled) {
            const float x = (ImGui::GetIO().MousePos.x - origin.x) / scale;
            const float y = (ImGui::GetIO().MousePos.y - origin.y) / scale;
            if (preview.state.editingPuffs || preview.state.editingScore) {
                if (preview.state.puffStatus != 2 && y >= 55 && y < 75 &&
                    x >= FirmwareMenu::PuffDigitsX &&
                    x < FirmwareMenu::PuffDigitsX + FirmwareMenu::PuffDigitCount * FirmwareMenu::PuffDigitPitch - 2)
                    preview.state.puffDigit = (std::min)(FirmwareMenu::PuffDigitCount - 1,
                        unsigned((x - FirmwareMenu::PuffDigitsX) / FirmwareMenu::PuffDigitPitch));
            } else if (preview.state.selectingRank) {
                if (y >= FirmwareMenu::RankRowsY &&
                    y < FirmwareMenu::RankRowsY + FirmwareMenu::RankRowCount * FirmwareMenu::RankRowPitch) {
                    const auto selected = unsigned((y - FirmwareMenu::RankRowsY) / FirmwareMenu::RankRowPitch);
                    if (preview.state.rankSelection == selected) Fire(preview, settings);
                    else preview.state.rankSelection = selected;
                }
            } else if (preview.state.information) Fire(preview, settings);
            else if (y >= 30 && y < float(30 + count * 20)) {
                const auto selected = unsigned((y - 30) / 20);
                if (preview.state.selection == selected) Fire(preview, settings);
                else preview.state.selection = selected;
            }
            UpdatePreview(preview, settings, device, context);
        }
    }
    if (settings.enabledItems & (FirmwareMenu::PuffEditorMask | FirmwareMenu::ScoreEditorMask |
        FirmwareMenu::RankSelectorMask))
        ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuHoldControls));
    if (settings.enabledItems & FirmwareMenu::PuffEditorMask) {
        ImGui::TextWrapped(Tr(i18n::Str::PatchMenuPuffBounds), unsigned(preview.values.maximumPuffs));
        ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuPuffControls));
    }
    if (settings.enabledItems & FirmwareMenu::ScoreEditorMask)
        ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuScoreHelp));
    if (settings.enabledItems & FirmwareMenu::RankSelectorMask)
        ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuRankHelp));
    if (preview.puffResult)
        ImGui::TextWrapped("%s", Tr(preview.puffResult == 1 ?
            i18n::Str::PatchMenuPuffChanged : i18n::Str::PatchMenuPuffUnchanged));
}

void DrawCosts(const UiFirmwarePatches::State& patches, const StudioState& studio,
    const FirmwareMenu::Settings& settings) {
    const auto* usage = SelectedUsage(patches);
    const bool current = usage && patches.memory.menu == settings &&
        patches.memorySource == studio.ResourceRevision() && !patches.memoryJob.valid();
    if (current) {
        const auto& costs = usage->menuCosts;
        ImGui::Text("%s: %zu", Tr(i18n::Str::PatchMenuCommon), costs.common);
        ImGui::Text("%s: %zu", Tr(i18n::Str::PatchMenuInformation), costs.information);
        ImGui::Text("%s: %zu", Tr(i18n::Str::PatchMenuVoltage), costs.voltage);
        ImGui::Text("%s: %zu", Tr(i18n::Str::PatchMenuPuffEditor), costs.puffEditor);
        ImGui::Text("%s: %zu", Tr(i18n::Str::PatchMenuScoreEditor), costs.scoreEditor);
        ImGui::Text("%s: %zu", Tr(i18n::Str::PatchMenuRankSelector), costs.rankSelector);
        ImGui::Text("%s: %zu", Tr(i18n::Str::PatchMenuTotal), costs.total);
    } else if (!patches.memory.error.empty()) {
        ImGui::TextWrapped("%s", patches.memory.error.c_str());
    } else ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMemoryPending));
    if (current) {
        const size_t free = usage->Used() < usage->capacityBytes ? usage->capacityBytes - usage->Used() : 0;
        ImGui::Text("%s: %zu", Tr(i18n::Str::PatchMemoryFree), free);
    }
    ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuCostNote));
}
}

void RenderTab(UiFirmwarePatches::State& patches, StudioState& studio,
    ID3D11Device* device, ID3D11DeviceContext* context) {
    char title[128];
    snprintf(title, sizeof(title), "%s###TabDeviceMenu", Tr(i18n::Str::PatchDeviceMenu));
    if (!Ui::BeginTabItem(title)) return;
    if (!studio.hasFirmware || !FirmwareMenu::Supported(studio.firmware)) {
        ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuUnsupported));
        ImGui::EndTabItem();
        return;
    }
    auto& preview = patches.menuPreview;
    if (!preview.initialized || preview.documentIdentity != studio.DocumentIdentity()) {
        preview.texture.Release();
        preview.documentIdentity = studio.DocumentIdentity();
        preview.hardwareCode = HardwareCode(studio.firmware.records.front());
        preview.state = {0, false, studio.menuPatch.batteryVoltage};
        preview.values = {};
        preview.values.maximumPuffs = FirmwareMenu::PuffLimit(studio.firmware.records.front());
        preview.values.puffs = (std::min)(12345u, preview.values.maximumPuffs);
        preview.achievementsSupported = FirmwareMenu::AchievementSupported(studio.firmware.records.front());
        preview.hasAchievements = std::any_of(studio.firmware.records.begin(), studio.firmware.records.end(),
            [](const auto& record) { return FirmwareMenu::AchievementSupported(record); });
        preview.puffResult = 0;
        preview.observed = studio.menuPatch;
        preview.rendered = false;
        preview.initialized = true;
    }
    const bool locked = !UiFirmwareRisk::Accepted();
    const ImVec2 overlayOrigin = ImGui::GetCursorScreenPos(), overlaySize = ImGui::GetContentRegionAvail();
    ImGui::BeginDisabled(locked);
    auto settings = studio.menuPatch;
    const float controlsWidth = (std::min)((std::clamp)(ImGui::GetContentRegionAvail().x * 0.38f,
        340.0f, 440.0f), (std::max)(180.0f, ImGui::GetContentRegionAvail().x - 180.0f));
    bool changed = false;
    if (ImGui::BeginChild("MenuControls", ImVec2(controlsWidth, 0), ImGuiChildFlags_Borders)) {
        ImGui::SeparatorText(Tr(i18n::Str::PatchDeviceMenu));
        ImGui::BeginDisabled(settings.enabledItems == 0);
        changed = ImGui::Checkbox(Tr(i18n::Str::PatchMenuEnabled), &settings.enabled);
        ImGui::EndDisabled();
        if (auto section = Ui::CollapsibleSection("MenuItems", Tr(i18n::Str::PatchMenuItems), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuDrag));
            int moveFrom = -1, moveTo = -1;
            const auto* usage = SelectedUsage(patches);
            const bool current = usage && patches.memory.menu == settings &&
                patches.memorySource == studio.ResourceRevision() && !patches.memoryJob.valid();
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6, 6));
            if (ImGui::BeginTable("MenuRows", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
                ImGui::TableSetupColumn("Order", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
                ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4);
                ImGui::TableSetupColumn("Move", ImGuiTableColumnFlags_WidthFixed,
                    ImGui::GetFrameHeight() * 2 + ImGui::GetStyle().ItemSpacing.x);
                for (size_t index = 0; index < settings.order.size(); ++index) {
                    const auto item = settings.order[index];
                    const uint32_t bit = ItemMask(item);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::PushID(static_cast<int>(item));
                    char position[32];
                    snprintf(position, sizeof(position), "%u###DragHandle", unsigned(index + 1));
                    ImGui::Selectable(position, false, ImGuiSelectableFlags_AllowOverlap);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Tr(i18n::Str::PatchMenuDrag));
                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("DeviceMenuItem", &item, sizeof(item));
                        ImGui::Text("%s", Tr(ItemLabels[size_t(item)]));
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const auto* payload = ImGui::AcceptDragDropPayload("DeviceMenuItem")) {
                            const auto dragged = *static_cast<const FirmwareMenu::Item*>(payload->Data);
                            for (size_t source = 0; source < settings.order.size(); ++source)
                                if (settings.order[source] == dragged) moveFrom = int(source);
                            moveTo = int(index);
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::TableNextColumn();
                    bool enabled = bit == 0 || (settings.enabledItems & bit) != 0;
                    const bool achievement = item == FirmwareMenu::Item::ScoreEditor ||
                        item == FirmwareMenu::Item::RankSelector;
                    ImGui::BeginDisabled(bit == 0 || (achievement && !preview.hasAchievements));
                    if (ImGui::Checkbox(Tr(ItemLabels[size_t(item)]), &enabled)) {
                        if (enabled) {
                            settings.enabledItems |= bit;
                            if (item == FirmwareMenu::Item::Information && !settings.infoFields)
                                settings.infoFields = FirmwareMenu::AllInfoFields;
                        } else settings.enabledItems &= ~bit;
                        if (!settings.enabledItems) settings.enabled = false;
                        changed = true;
                    }
                    ImGui::EndDisabled();
                    if (bit == 0 && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                        ImGui::SetTooltip("%s", Tr(i18n::Str::PatchMenuMandatory));
                    if (achievement && !preview.hasAchievements &&
                        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                        ImGui::SetTooltip("%s", Tr(i18n::Str::PatchMenuAchievementsUnsupported));
                    if (item == FirmwareMenu::Item::Information) {
                        ImGui::Indent(ImGui::GetStyle().IndentSpacing * 0.5f);
                        constexpr uint32_t fields[]{FirmwareMenu::InfoBatteryMask, FirmwareMenu::InfoResistanceMask,
                            FirmwareMenu::InfoDurationMask};
                        constexpr i18n::Str labels[]{i18n::Str::PatchMenuInfoBattery, i18n::Str::PatchMenuInfoResistance,
                            i18n::Str::PatchMenuInfoDuration};
                        for (size_t field = 0; field < 3; ++field) {
                            bool selected = (settings.infoFields & fields[field]) != 0;
                            if (ImGui::Checkbox(Tr(labels[field]), &selected)) {
                                if (selected) settings.infoFields |= fields[field];
                                else settings.infoFields &= ~fields[field];
                                if (!settings.infoFields) settings.enabledItems &= ~FirmwareMenu::InformationMask;
                                if (!settings.enabledItems) settings.enabled = false;
                                changed = true;
                            }
                        }
                        ImGui::Unindent(ImGui::GetStyle().IndentSpacing * 0.5f);
                    }
                    ImGui::TableNextColumn();
                    if (current) {
                        const size_t bytes = item == FirmwareMenu::Item::Information ? usage->menuCosts.information :
                            item == FirmwareMenu::Item::Voltage ? usage->menuCosts.voltage :
                            item == FirmwareMenu::Item::PuffEditor ? usage->menuCosts.puffEditor :
                            item == FirmwareMenu::Item::ScoreEditor ? usage->menuCosts.scoreEditor :
                            item == FirmwareMenu::Item::RankSelector ? usage->menuCosts.rankSelector : 0;
                        ImGui::Text("%zu", bytes);
                    } else ImGui::TextDisabled("...");
                    ImGui::TableNextColumn();
                    ImGui::BeginDisabled(index == 0);
                    const bool moveUp = ImGui::ArrowButton("##MoveUp", ImGuiDir_Up);
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                        ImGui::SetTooltip("%s", Tr(i18n::Str::PatchMenuMoveUp));
                    ImGui::SameLine();
                    ImGui::BeginDisabled(index + 1 == settings.order.size());
                    const bool moveDown = ImGui::ArrowButton("##MoveDown", ImGuiDir_Down);
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                        ImGui::SetTooltip("%s", Tr(i18n::Str::PatchMenuMoveDown));
                    if (moveUp || moveDown) {
                        moveFrom = int(index);
                        moveTo = moveUp ? moveFrom - 1 : moveFrom + 1;
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
            if (moveFrom >= 0 && moveTo >= 0 && moveFrom != moveTo) {
                const auto moved = settings.order[size_t(moveFrom)];
                const int step = moveFrom < moveTo ? 1 : -1;
                for (int index = moveFrom; index != moveTo; index += step)
                    settings.order[size_t(index)] = settings.order[size_t(index + step)];
                settings.order[size_t(moveTo)] = moved;
                changed = true;
            }
        }
        if (auto section = Ui::CollapsibleSection("MenuDirection", Tr(i18n::Str::PatchMenuDirection), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BeginDisabled(!settings.enabled);
            if (ImGui::RadioButton(Tr(i18n::Str::PatchMenuPlusUp), settings.reverseNavigation)) {
                settings.reverseNavigation = true; changed = true;
            }
            if (ImGui::RadioButton(Tr(i18n::Str::PatchMenuPlusDown), !settings.reverseNavigation)) {
                settings.reverseNavigation = false; changed = true;
            }
            ImGui::EndDisabled();
        }
        if (auto section = Ui::CollapsibleSection("MenuVoltage", Tr(i18n::Str::PatchMenuDefaultVoltage), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BeginDisabled(!settings.enabled || !(settings.enabledItems & FirmwareMenu::VoltageMask));
            if (ImGui::RadioButton(Tr(i18n::Str::PatchMenuBattery), settings.batteryVoltage)) {
                settings.batteryVoltage = true; changed = true;
            }
            if (ImGui::RadioButton(Tr(i18n::Str::PatchMenuOutput), !settings.batteryVoltage)) {
                settings.batteryVoltage = false; changed = true;
            }
            ImGui::EndDisabled();
        }
        if (changed) {
            studio.PushUndo(Tr(i18n::Str::PatchDeviceMenu));
            studio.SetMenuPatch(settings);
            studio.EndUndoAction();
        }
        if (auto section = Ui::CollapsibleSection("MenuCosts", Tr(i18n::Str::PatchMenuTotal), ImGuiTreeNodeFlags_DefaultOpen))
            DrawCosts(patches, studio, settings);
        if (auto section = Ui::CollapsibleSection("MenuHelp", Tr(i18n::Str::MenuHelp))) {
            ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuGesture));
            ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuControls));
            ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuInfo));
            ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuRuntime));
            ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuPuffControls));
            ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuPuffSave));
            ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuScoreHelp));
            ImGui::TextWrapped("%s", Tr(i18n::Str::PatchMenuRankHelp));
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("MenuRight", ImVec2(0, 0), false);
    if (Ui::BeginBottomAnchoredPreview("MenuPreview", ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::SeparatorText(Tr(i18n::Str::PatchMenuPreview));
        if (Ui::BeginControlTable("MenuPreviewControls",
            (std::min)(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 30))) {
        Ui::ControlRow(Tr(i18n::Str::LabelZoom));
        Ui::ZoomSlider("##Zoom", preview.zoom);
        char revision[16];
        snprintf(revision, sizeof(revision), "%04X", unsigned(preview.hardwareCode));
        Ui::ControlRow(Tr(i18n::Str::LayoutRevision));
        if (ImGui::BeginCombo("##MenuHardware", revision)) {
            for (const auto& record : studio.firmware.records) {
                const auto code = HardwareCode(record);
                snprintf(revision, sizeof(revision), "%04X", unsigned(code));
                if (ImGui::Selectable(revision, preview.hardwareCode == code) && preview.hardwareCode != code) {
                    preview.hardwareCode = code;
                    preview.values.maximumPuffs = FirmwareMenu::PuffLimit(record);
                    preview.values.puffs = (std::min)(12345u, preview.values.maximumPuffs);
                    preview.achievementsSupported = FirmwareMenu::AchievementSupported(record);
                    preview.state.editingScore = preview.state.selectingRank = false;
                    preview.state.editingPuffs = false;
                    preview.puffResult = 0;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndTable();
        }
        if (!preview.achievementsSupported) {
            settings.enabledItems &= ~(FirmwareMenu::ScoreEditorMask | FirmwareMenu::RankSelectorMask);
            if (!settings.enabledItems) settings.enabled = false;
        }
        DrawPreview(preview, settings, device, context);
    }
    ImGui::EndChild();
    UiFirmwarePatches::RenderMemory(patches, studio);
    ImGui::EndChild();
    ImGui::EndDisabled();
    if (locked) UiFirmwareRisk::Overlay(overlayOrigin, overlaySize);
    ImGui::EndTabItem();
}
}
