#include "ui_firmware_device.h"
#include "app_state.h"
#include "ui_collapsible_section.h"
#include "imgui.h"
#include "ui_keyboard.h"
#include "ui_controls.h"
#include "imgui_internal.h"
#include <cstdio>

namespace UiFirmwareDevice {
namespace {
constexpr ImGuiKey buttonKeys[] = {ImGuiKey_Space, ImGuiKey_UpArrow, ImGuiKey_DownArrow};

void ReleaseButtonKeyboard(State& state) {
    if (!state.buttonKeyboardOwner || !ImGui::GetCurrentContext()) return;
    for (const auto key : buttonKeys)
        if (ImGui::GetKeyOwner(key) == state.buttonKeyboardOwner)
            ImGui::SetKeyOwner(key, ImGuiKeyOwner_NoOwner);
    state.buttonKeyboardOwner = 0;
}


void Load(State& state, const StudioState& studio) {
    state.inputs.fire = state.inputs.plus = state.inputs.minus = false;
    state.worker.Load(studio.CreateEmulationSnapshot(), state.hardwareCode);
    state.worker.SetInputs(state.inputs);
    state.sentInputs = state.inputs;
    state.wantsRunning = true;
    state.loadedDocument = studio.DocumentIdentity();
    state.loadedRevision = studio.ResourceRevision();
    state.loadedMenuEnabled = studio.menuPatch.enabled;
    state.texture.Release();
    state.textureGeneration = UINT64_MAX;
}
}

void PauseAndRelease(State& state) {
    ReleaseButtonKeyboard(state);
    if (state.wantsRunning) {
        state.wantsRunning = false;
        state.worker.SetRunning(false);
    }
    state.inputs.fire = state.inputs.plus = state.inputs.minus = false;
    if (state.inputs != state.sentInputs) {
        state.worker.SetInputs(state.inputs);
        state.sentInputs = state.inputs;
    }
}

void RenderButtonControls(State& state) {
    // These are physical, mouse-held switches, not keyboard navigation targets.
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    ImGui::ButtonEx("Fire", ImVec2(86, 32), ImGuiButtonFlags_NoNavFocus);
    bool fire = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    ImGui::SameLine();
    ImGui::ButtonEx("+", ImVec2(64, 32), ImGuiButtonFlags_NoNavFocus);
    bool plus = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    ImGui::SameLine();
    ImGui::ButtonEx("-", ImVec2(64, 32), ImGuiButtonFlags_NoNavFocus);
    bool minus = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    ImGui::PopItemFlag();
    state.buttonMouseOwner = (fire || plus || minus) ? ImGui::GetActiveID() : 0;
    if (Ui::EditorKeyboardAvailable(state.buttonMouseOwner)) {
        state.buttonKeyboardOwner = ImGui::GetID("DeviceButtonKeyboard");
        // Claim even released keys: NewFrame() resolves navigation before widgets
        // render, so the ownership must already exist when the next press arrives.
        for (const auto key : buttonKeys)
            ImGui::SetKeyOwner(key, state.buttonKeyboardOwner, ImGuiInputFlags_LockThisFrame);
        fire |= ImGui::IsKeyDown(ImGuiKey_Space, state.buttonKeyboardOwner);
        plus |= ImGui::IsKeyDown(ImGuiKey_UpArrow, state.buttonKeyboardOwner);
        minus |= ImGui::IsKeyDown(ImGuiKey_DownArrow, state.buttonKeyboardOwner);
    } else {
        ReleaseButtonKeyboard(state);
    }
    state.inputs.fire = fire;
    state.inputs.plus = plus;
    state.inputs.minus = minus;
}

namespace {
void RenderNativeShortcuts(uint16_t code, bool menuEnabled) {
    if (Ui::CollapsibleSection section{"DeviceShortcutsBody", Tr(i18n::Str::DeviceShortcuts)}; section) {
        const bool historyClock = code == 0x3013 || code == 0x3014;
        auto row = [](const char* keys, i18n::Str action, bool hold = false) {
            ImGui::TableNextRow(); ImGui::TableNextColumn();
            ImGui::TextUnformatted(keys);
            if (hold) ImGui::TextDisabled("%s", Tr(i18n::Str::DeviceShortcutHold));
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", Tr(action));
        };
        ImGui::TextDisabled("HW %04X", unsigned(code));
        ImGui::SeparatorText(Tr(i18n::Str::DeviceShortcutMain));
        if (ImGui::BeginTable("NativeMainShortcuts", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerH)) {
            ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, 115);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
            row("Fire x5", i18n::Str::DeviceShortcutPower);
            row("Fire x3", i18n::Str::DeviceShortcutMode);
            row("[+] / [-]", i18n::Str::LayoutElementPower);
            row("Fire + [+]", i18n::Str::DeviceShortcutLock, true);
            row("Fire + [-]", i18n::Str::DeviceShortcutClear, true);
            row("[+] + [-]", historyClock ? i18n::Str::DeviceShortcutHistory : i18n::Str::DeviceShortcutWakeStop, true);
            row("Fire + [+] + [-]", historyClock ? i18n::Str::DeviceShortcutClock : i18n::Str::DeviceShortcutChipId, true);
            ImGui::EndTable();
        }
        if (historyClock) {
            ImGui::SeparatorText(Tr(i18n::Str::DeviceShortcutHistoryContext));
            if (ImGui::BeginTable("NativeHistoryShortcuts", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, 115);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
                row("[+] / [-]", i18n::Str::EmulatorHistoryDay);
                row("Fire", i18n::Str::DeviceShortcutToggleHistory);
                row("Fire", i18n::Str::DeviceShortcutExit, true);
                ImGui::EndTable();
            }
            ImGui::SeparatorText(Tr(i18n::Str::DeviceShortcutClockContext));
            if (ImGui::BeginTable("NativeClockShortcuts", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, 115);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
                row("Fire", i18n::Str::DeviceShortcutClockField);
                row("[+] / [-]", i18n::Str::DeviceShortcutClockAdjust);
                row("Fire", i18n::Str::DeviceShortcutExit, true);
                ImGui::EndTable();
            }
        } else {
            if (ImGui::BeginTable("NativeServiceShortcuts", 2, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, 115);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
                row("Fire", i18n::Str::DeviceShortcutExit, true);
                ImGui::EndTable();
            }
        }
        if (menuEnabled) {
            ImGui::SeparatorText(Tr(i18n::Str::DevicePatchedShortcuts));
            if (ImGui::BeginTable("PatchedMenuShortcuts", 2, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, 115);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
                row("+ - + - +", i18n::Str::DeviceShortcutMenu);
                ImGui::EndTable();
            }
        }
    }
}
}

void RenderDiagnostics(const FirmwareDevice::Snapshot& snapshot) {
    ImGui::SeparatorText(Tr(i18n::Str::DeviceDiagnostics));
    ImGui::Text("%s: %.3f s", Tr(i18n::Str::DeviceTime), snapshot.virtualMicroseconds / 1000000.0);
    ImGui::Text("%s: %llu", Tr(i18n::Str::DeviceInstructions),
        static_cast<unsigned long long>(snapshot.instructions));
    if (snapshot.modeledClockHz)
        ImGui::Text("%s: %.3f MHz", Tr(i18n::Str::DeviceModeledClock), snapshot.modeledClockHz / 1000000.0);
    else ImGui::Text("%s: %s", Tr(i18n::Str::DeviceModeledClock), Tr(i18n::Str::DeviceUnavailable));
    ImGui::TextWrapped("%s", Tr(i18n::Str::DeviceClockNote));
    if (snapshot.mappedSramBytes) {
        ImGui::Text("%s: %u KiB", Tr(i18n::Str::DeviceMappedSram), snapshot.mappedSramBytes / 1024);
        ImGui::Text("%08X - %08X", snapshot.mappedSramBase,
            snapshot.mappedSramBase + snapshot.mappedSramBytes - 1);
    }
    ImGui::TextUnformatted(Tr(snapshot.sleeping ? i18n::Str::DeviceSleeping : i18n::Str::DeviceCpuRunning));
    if (Ui::CollapsibleSection section{"DeviceRegistersBody", Tr(i18n::Str::DeviceCpuRegisters),
        ImGuiTreeNodeFlags_DefaultOpen}; section &&
        ImGui::BeginTable("DeviceRegisters", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("RegisterA", ImGuiTableColumnFlags_WidthFixed, 78);
        ImGui::TableSetupColumn("ValueA", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("RegisterB", ImGuiTableColumnFlags_WidthFixed, 78);
        ImGui::TableSetupColumn("ValueB", ImGuiTableColumnFlags_WidthStretch);
        const auto& registers = snapshot.registers;
        auto cell = [](const char* label, uint32_t value, bool valid) {
            ImGui::TableNextColumn(); ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            if (valid) ImGui::Text("%08X", value);
            else ImGui::TextDisabled("%s", Tr(i18n::Str::DeviceUnavailable));
        };
        constexpr const char* names[]{"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
            "r8", "r9", "r10", "r11", "r12", "r13 SP", "r14 LR", "r15 PC"};
        for (unsigned row = 0; row < 8; ++row) {
            ImGui::TableNextRow();
            for (unsigned i : {row, row + 8})
                cell(names[i], registers.general[i], (registers.validGeneral & (1u << i)) != 0);
        }
        constexpr const char* specialNames[]{"xPSR", "MSP", "PSP", "CONTROL", "PRIMASK", "BASEPRI", "FAULTMASK"};
        const uint32_t specialValues[]{registers.xpsr, registers.msp, registers.psp, registers.control,
            registers.primask, registers.basepri, registers.faultmask};
        for (unsigned i = 0; i < 7; ++i) {
            if (!(i & 1)) ImGui::TableNextRow();
            cell(specialNames[i], specialValues[i], (registers.validSpecial & (1u << i)) != 0);
        }
        ImGui::EndTable();
    }
    if (Ui::CollapsibleSection section{"DeviceStackBody", Tr(i18n::Str::DeviceStack),
        ImGuiTreeNodeFlags_DefaultOpen}; section) {
        const auto& stack = snapshot.stack;
        ImGui::TextWrapped("%s", Tr(i18n::Str::DeviceStackNote));
        if (stack.status == FirmwareDevice::StackStatus::Available) {
            if (ImGui::BeginTable("DeviceStack", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 7))) {
                ImGui::TableSetupColumn(Tr(i18n::Str::DeviceAddress));
                ImGui::TableSetupColumn(Tr(i18n::Str::DeviceValue));
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();
                for (uint32_t i = 0; i < stack.count; ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%08X", stack.address + i * 4);
                    ImGui::TableNextColumn(); ImGui::Text("%08X", stack.words[i]);
                }
                ImGui::EndTable();
            }
        } else {
            const auto message = stack.status == FirmwareDevice::StackStatus::Empty ? i18n::Str::DeviceStackEmpty :
                stack.status == FirmwareDevice::StackStatus::InvalidPointer ? i18n::Str::DeviceStackInvalid :
                stack.status == FirmwareDevice::StackStatus::ReadFailed ? i18n::Str::DeviceStackReadFailed :
                i18n::Str::DeviceUnavailable;
            ImGui::TextWrapped("%s", Tr(message));
        }
    }
    const float logHeight = (std::max)(180.0f, ImGui::GetContentRegionAvail().y -
        ImGui::GetFrameHeightWithSpacing() - 16.0f);
    if (Ui::CollapsibleSection section{"DeviceLogBody", Tr(i18n::Str::DeviceLog),
        ImGuiTreeNodeFlags_DefaultOpen}; section &&
        ImGui::BeginTable("DeviceAccessLog", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX |
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit,
            ImVec2(0, logHeight), 590)) {
        ImGui::TableSetupColumn("us", ImGuiTableColumnFlags_WidthFixed, 96);
        ImGui::TableSetupColumn("PC", ImGuiTableColumnFlags_WidthFixed, 82);
        ImGui::TableSetupColumn("R/W", ImGuiTableColumnFlags_WidthFixed, 36);
        ImGui::TableSetupColumn(Tr(i18n::Str::DeviceAddress), ImGuiTableColumnFlags_WidthFixed, 82);
        ImGui::TableSetupColumn(Tr(i18n::Str::DeviceBytes), ImGuiTableColumnFlags_WidthFixed, 48);
        ImGui::TableSetupColumn(Tr(i18n::Str::DeviceValue), ImGuiTableColumnFlags_WidthFixed, 82);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        for (auto entry = snapshot.recentAccess.rbegin(); entry != snapshot.recentAccess.rend(); ++entry) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(entry->virtualMicroseconds));
            ImGui::TableNextColumn(); ImGui::Text("%08X", entry->pc);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(entry->write ? "W" : "R");
            ImGui::TableNextColumn(); ImGui::Text("%08X", entry->address);
            ImGui::TableNextColumn(); ImGui::Text("%u", unsigned(entry->size));
            ImGui::TableNextColumn(); ImGui::Text("%08X", entry->value);
        }
        ImGui::EndTable();
    }
}

void Render(State& state, const StudioState& studio,
    ID3D11Device* device, ID3D11DeviceContext* context) {
    const bool supported = studio.hasFirmware && studio.model == FirmwareModel::DragS &&
        (studio.edition == FirmwareEdition::Score || studio.edition == FirmwareEdition::Factory) &&
        !studio.firmware.records.empty();
    if (!supported) {
        PauseAndRelease(state);
        ImGui::TextWrapped("%s", Tr(i18n::Str::DeviceUnsupported));
        return;
    }
    if (state.loadedDocument != studio.DocumentIdentity()) {
        const bool hasSelection = std::any_of(studio.firmware.records.begin(), studio.firmware.records.end(),
            [&](const HardwareRecord& record) {
                return (uint16_t(record.device_code[0]) << 8 | record.device_code[1]) == state.hardwareCode;
            });
        if (!hasSelection) {
            const auto& record = studio.firmware.records.front();
            state.hardwareCode = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
        }
        Load(state, studio);
    }
    state.worker.Poll();
    ImGui::TextWrapped("%s", Tr(i18n::Str::DeviceNote));
    const ImVec2 available = ImGui::GetContentRegionAvail();
    // Preserve useful screen/register widths in narrow windows via horizontal
    // scrolling; the dividers resize the three real panels, not the LCD pixels.
    if (!ImGui::BeginTable("LiveDevicePanels", 3,
        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_SizingFixedFit, available, (std::max)(1100.0f, available.x))) return;
    ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthFixed, 300);
    ImGui::TableSetupColumn("Display", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Diagnostics", ImGuiTableColumnFlags_WidthFixed, 440);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    const float panelHeight = (std::max)(100.0f, available.y - ImGui::GetStyle().ScrollbarSize - 4);
    ImGui::BeginChild("LiveDeviceControls", ImVec2(0, panelHeight), true);
    ImGui::Text("DRAG S / %s", FirmwareEditionName(studio.edition));
    char selected[16];
    std::snprintf(selected, sizeof(selected), "HW %04X", unsigned(state.hardwareCode));
    if (Ui::BeginControlTable("LiveHardwareSelection")) {
    Ui::ControlRow(Tr(i18n::Str::LayoutRevision));
    if (ImGui::BeginCombo("##LiveHardware", selected)) {
        for (const auto& record : studio.firmware.records) {
            const uint16_t code = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
            char label[48];
            std::snprintf(label, sizeof(label), "HW %04X / %s", unsigned(code),
                record.linked_base == 0x08003000 ? "GD32F3x0" : "N32G43x");
            const bool active = code == state.hardwareCode;
            if (ImGui::Selectable(label, active) && !active) {
                ReleaseButtonKeyboard(state);
                state.hardwareCode = code;
                Load(state, studio);
            }
            if (active) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::EndTable();
    }
    if (state.loadedRevision != studio.ResourceRevision()) {
        ImGui::TextWrapped("%s", Tr(i18n::Str::DeviceChanged));
        if (ImGui::Button(Tr(i18n::Str::DeviceApply))) Load(state, studio);
    }
    if (ImGui::Button(Tr(i18n::Str::DeviceReset))) Load(state, studio);
    ImGui::SameLine();
    if (ImGui::Button(Tr(state.wantsRunning ? i18n::Str::DevicePause : i18n::Str::DeviceResume))) {
        state.wantsRunning = !state.wantsRunning;
        state.worker.SetRunning(state.wantsRunning);
    }
    ImGui::SameLine();
    if (ImGui::Button(Tr(i18n::Str::DeviceStep))) {
        state.wantsRunning = false;
        state.worker.Step();
    }
    ImGui::Separator();
    RenderButtonControls(state);
    ImGui::TextWrapped("%s", Tr(i18n::Str::DeviceButtons));
    ImGui::Checkbox("USB", &state.inputs.usb);
    ImGui::SameLine();
    ImGui::Checkbox(Tr(i18n::Str::DeviceAtomizerConnected), &state.inputs.atomizerConnected);
    ImGui::Checkbox(Tr(i18n::Str::DeviceAirflow), &state.inputs.airflow);
    if (Ui::BeginControlTable("LiveDeviceParameters")) {
        Ui::ControlRow(Tr(i18n::Str::LayoutElementVoltage));
        ImGui::SliderInt("##BatteryVoltage", &state.inputs.batteryMillivolts, 2500, 4500, "%d mV");
        Ui::ControlRow(Tr(i18n::Str::EmulatorResistance));
        ImGui::SliderInt("##Resistance", &state.inputs.resistanceMilliOhms, 0, 9990);
        Ui::ControlRow(Tr(i18n::Str::LayoutTemperature));
        ImGui::SliderInt("##Temperature", &state.inputs.temperatureCelsius, -20, 120, "%d C");
        Ui::ControlRow(Tr(i18n::Str::LabelZoom));
        Ui::ZoomSlider("##Zoom", state.zoom);
        ImGui::EndTable();
    }
    const auto& view = state.worker.Current();
    if (!view.loaded && view.error.empty()) ImGui::TextUnformatted(Tr(i18n::Str::DeviceLoading));
    if (!view.error.empty()) ImGui::TextWrapped("%s", view.error.c_str());
    else if (!view.state.error.empty()) ImGui::TextWrapped("%s", view.state.error.c_str());
    ImGui::EndChild();
    ImGui::TableSetColumnIndex(1);
    ImGui::BeginChild("LiveDeviceDisplay", ImVec2(0, panelHeight), true,
        ImGuiWindowFlags_HorizontalScrollbar);
    const auto& frame = view.state.display;
    if (state.textureDevice != device) {
        state.texture.Release();
        state.textureDevice = device;
        state.textureGeneration = UINT64_MAX;
    }
    if (view.loaded && frame.width > 0 && frame.height > 0 &&
        frame.rgba.size() == size_t(frame.width) * frame.height) {
        if (state.textureGeneration != view.state.frameGeneration) {
            if (state.texture.Update(device, context, frame.rgba.data(), frame.width, frame.height))
                state.textureGeneration = view.state.frameGeneration;
            else ImGui::TextWrapped("%s", Tr(i18n::Str::CompareTextureError));
        }
        if (state.texture.IsValid()) {
            const ImVec2 size(float(frame.width * state.zoom), float(frame.height * state.zoom));
            const float inset = (ImGui::GetContentRegionAvail().x - size.x) * 0.5f;
            if (inset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + inset);
            ImGui::Image((ImTextureID)state.texture.GetSRV(), size);
        }
    }
    if (frame.emulatedUid) ImGui::TextWrapped("%s", Tr(i18n::Str::EmulatorUidNote));
    ImGui::EndChild();
    ImGui::TableSetColumnIndex(2);
    ImGui::BeginChild("LiveDeviceDiagnostics", ImVec2(0, panelHeight), true,
        ImGuiWindowFlags_HorizontalScrollbar);
    RenderNativeShortcuts(state.hardwareCode, state.loadedMenuEnabled && view.loaded &&
        view.documentIdentity == state.loadedDocument && view.revision == state.loadedRevision &&
        view.hardwareCode == state.hardwareCode);
    RenderDiagnostics(view.state);
    ImGui::EndChild();
    ImGui::EndTable();
    // Settings and popups may have become active after the physical buttons
    // rendered. Return the keys before the next NewFrame() processes navigation.
    if (!Ui::EditorKeyboardAvailable(state.buttonMouseOwner)) {
        ReleaseButtonKeyboard(state);
        state.inputs.fire = state.inputs.plus = state.inputs.minus = false;
    }
    if (state.inputs != state.sentInputs) {
        state.worker.SetInputs(state.inputs);
        state.sentInputs = state.inputs;
    }
}
}
