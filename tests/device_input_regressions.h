#pragma once
#include "ui_firmware_device.h"
#include "imgui_internal.h"
#include "ui_controls.h"
#include <string>

// Real ImGui events and the production physical-button renderer; no firmware,
// graphics device, or USB connection is needed.
inline bool RunDeviceInputRegressions(std::string& error) {
    error.clear();
    auto check = [&](bool ok, const char* message) {
        if (!ok) { error += message; error += "; "; }
    };
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGuiContext* test = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(900, 600);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigInputTrickleEventQueue = false;
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID((ImTextureID)1);

    UiFirmwareDevice::State state;
    int resetCount = 0, unrelatedCount = 0, slider = 50;
    int zoom = 4, choice = 0;
    bool focusZoom = false, comboOpen = false;
    ImVec2 comboPosition;
    char text[64] = "text";
    ImVec2 firePosition, plusPosition, minusPosition;
    ImGuiID resetId = 0;
    bool focusReset = false, focusSlider = false, focusText = false, focusOther = false, popup = false;
    auto frame = [&](bool fire = false, bool plus = false, bool minus = false,
                     bool mouse = false, ImVec2 position = ImVec2(-100, -100)) {
        io.AddKeyEvent(ImGuiKey_Space, fire);
        io.AddKeyEvent(ImGuiKey_UpArrow, plus);
        io.AddKeyEvent(ImGuiKey_DownArrow, minus);
        io.AddMousePosEvent(position.x, position.y);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, mouse);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(450, 580));
        ImGui::Begin("Device input regression", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        if (ImGui::Button("Reset")) ++resetCount;
        resetId = ImGui::GetItemID();
        if (focusReset) {
            ImGui::FocusWindow(ImGui::GetCurrentWindow());
            ImGui::SetFocusID(resetId, ImGui::GetCurrentWindow());
            ImGui::SetNavCursorVisible(true);
            focusReset = false;
        }
        ImGui::SliderInt("Setting", &slider, 0, 100);
        if (focusSlider) {
            ImGui::FocusWindow(ImGui::GetCurrentWindow());
            ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
            ImGui::SetNavCursorVisible(true);
            focusSlider = false;
        }
        if (Ui::BeginControlTable("ZoomControls")) {
            Ui::ControlRow("Zoom");
            Ui::ZoomSlider("##Zoom", zoom, 32);
            if (focusZoom) {
                ImGui::FocusWindow(ImGui::GetCurrentWindow());
                ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
                ImGui::SetNavCursorVisible(true);
                focusZoom = false;
            }
            ImGui::EndTable();
        }
        if (focusText) { ImGui::SetKeyboardFocusHere(); focusText = false; }
        ImGui::InputText("Text", text, sizeof(text));
        comboOpen = ImGui::BeginCombo("Choice", choice ? "Second" : "First");
        comboPosition = ImVec2(ImGui::GetItemRectMin().x + 20, ImGui::GetItemRectMin().y + 10);
        if (comboOpen) {
            if (ImGui::Selectable("First", choice == 0)) choice = 0;
            if (choice == 0) ImGui::SetItemDefaultFocus();
            if (ImGui::Selectable("Second", choice == 1)) choice = 1;
            if (choice == 1) ImGui::SetItemDefaultFocus();
            ImGui::EndCombo();
        }
        if (popup) ImGui::OpenPopup("Device popup");
        if (ImGui::BeginPopup("Device popup")) {
            ImGui::Button("Popup button");
            ImGui::EndPopup();
        }
        const ImVec2 start = ImGui::GetCursorScreenPos();
        firePosition = ImVec2(start.x + 43, start.y + 16);
        plusPosition = ImVec2(start.x + 86 + ImGui::GetStyle().ItemSpacing.x + 32, start.y + 16);
        minusPosition = ImVec2(plusPosition.x + 64 + ImGui::GetStyle().ItemSpacing.x, start.y + 16);
        UiFirmwareDevice::RenderButtonControls(state);
        ImGui::End();
        ImGui::SetNextWindowPos(ImVec2(470, 0));
        ImGui::SetNextWindowSize(ImVec2(400, 300));
        ImGui::Begin("Unrelated input regression", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        if (ImGui::Button("Unrelated button")) ++unrelatedCount;
        if (focusOther) {
            ImGui::FocusWindow(ImGui::GetCurrentWindow());
            ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
            ImGui::SetNavCursorVisible(true);
            focusOther = false;
        }
        ImGui::End();
        ImGui::Render();
    };
    auto buttons = [&](bool fire, bool plus, bool minus, const char* message) {
        check(state.inputs.fire == fire && state.inputs.plus == plus && state.inputs.minus == minus, message);
    };

    frame();
    focusReset = true;
    frame();
    frame();
    frame(true, true, true);
    buttons(true, true, true, "three-key combo was not captured");
    check(resetCount == 0, "Space activated Reset while captured by the device");
    check(ImGui::GetFocusID() == resetId, "device arrows moved ImGui navigation focus");
    frame(true, true, true);
    buttons(true, true, true, "held three-key combo was interrupted");
    frame(true, false, true);
    buttons(true, false, true, "releasing plus lost the remaining combo");
    frame();
    buttons(false, false, false, "keyboard release stuck a physical button");
    for (const ImGuiKey modifier : {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiMod_Super, ImGuiMod_Shift}) {
        io.AddKeyEvent(modifier, true);
        frame(true, true, true);
        buttons(false, false, false, "modified shortcut leaked into physical device buttons");
        frame();
        io.AddKeyEvent(modifier, false);
        frame();
    }

    frame(false, true, true, true, firePosition);
    buttons(true, true, true, "mouse Fire suppressed plus/minus keys");
    frame(false, true, true, true, firePosition);
    buttons(true, true, true, "held mouse Fire interrupted keyboard combo");
    frame();
    frame(true, false, true, true, plusPosition);
    buttons(true, true, true, "mouse plus suppressed Fire/minus keys");
    frame();
    frame(true, true, false, true, minusPosition);
    buttons(true, true, true, "mouse minus suppressed Fire/plus keys");
    frame();

    focusText = true;
    frame();
    frame();
    frame();
    io.AddInputCharacter(' ');
    frame(true, true, true);
    buttons(false, false, false, "text editing leaked keys into the device");
    check(std::string(text).find(' ') != std::string::npos, "device ownership swallowed text input");
    io.AddKeyEvent(ImGuiKey_Escape, true);
    frame();
    io.AddKeyEvent(ImGuiKey_Escape, false);
    frame();

    focusSlider = true;
    frame();
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame();
    io.AddKeyEvent(ImGuiKey_Enter, false);
    io.AddInputCharacter('7');
    frame();
    buttons(false, false, false, "slider text editing leaked into the device");
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame();
    check(slider == 7, "device ownership prevented slider keyboard editing");
    io.AddKeyEvent(ImGuiKey_Enter, false);
    frame();
    frame(false, false, false, false, comboPosition);
    frame(false, false, false, true, comboPosition);
    frame(false, false, false, false, comboPosition);
    frame();
    check(comboOpen, "choice combo did not open");
    frame(false, false, true);
    buttons(false, false, false, "combo navigation leaked into physical device buttons");
    frame();
    io.AddKeyEvent(ImGuiKey_Enter, true); frame();
    io.AddKeyEvent(ImGuiKey_Enter, false); frame();
    check(choice == 1, "device keyboard ownership prevented choosing the next combo item");

    popup = true;
    frame();
    popup = false;
    frame(true, true, true);
    buttons(false, false, false, "popup leaked keys into the device");
    check(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup), "device keys dismissed a popup");
    io.AddKeyEvent(ImGuiKey_Escape, true);
    frame();
    io.AddKeyEvent(ImGuiKey_Escape, false);
    frame();

    focusOther = true;
    frame();
    frame();
    frame(true, true, true);
    buttons(false, false, false, "unrelated window keys reached the device");
    check(unrelatedCount == 1, "device ownership disabled unrelated Space navigation");
    check((io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard) != 0, "device input disabled global navigation");
    focusReset = true;
    frame();
    frame(true, true, true);
    buttons(true, true, true, "device could not reacquire keyboard ownership after focus returned");
    state.sentInputs = state.inputs;
    state.wantsRunning = true;
    UiFirmwareDevice::PauseAndRelease(state);
    buttons(false, false, false, "leaving device mode retained held physical inputs");
    check(!state.wantsRunning && state.sentInputs == state.inputs,
        "leaving device mode did not send released inputs and request pause");
    for (const auto key : {ImGuiKey_Space, ImGuiKey_UpArrow, ImGuiKey_DownArrow})
        check(ImGui::GetKeyOwner(key) == ImGuiKeyOwner_NoOwner,
            "leaving device mode while keys were held retained a keyboard owner");
    frame();
    UiFirmwareDevice::PauseAndRelease(state);
    for (const auto key : {ImGuiKey_Space, ImGuiKey_UpArrow, ImGuiKey_DownArrow})
        check(ImGui::GetKeyOwner(key) == ImGuiKeyOwner_NoOwner, "leaving device mode retained a key owner");
    focusZoom = true; frame(); frame();
    io.AddKeyEvent(ImGuiKey_Enter, true); frame();
    io.AddKeyEvent(ImGuiKey_Enter, false); frame();
    io.AddInputCharacter('5'); frame();
    frame(false, true, true);
    buttons(false, false, false, "zoom keyboard editing leaked into physical device buttons");
    io.AddKeyEvent(ImGuiKey_Enter, true); frame();
    io.AddKeyEvent(ImGuiKey_Enter, false); frame();
    check(zoom == 5, "integer zoom did not accept its keyboard-entered value");

    ImGui::DestroyContext(test);
    ImGui::SetCurrentContext(previous);
    return error.empty();
}
