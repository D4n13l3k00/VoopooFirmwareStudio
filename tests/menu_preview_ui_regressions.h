#pragma once
#include "app_state.h"
#include "embedded_firmware.h"
#include "ui_firmware_patches.h"
#include "ui_firmware_menu.h"
#include "ui_firmware_risk.h"
#include "window_chrome.h"
#include "ui_capture.h"
#include "imgui_internal.h"
#include "ui_resource_studio.h"
#include "ui_font_generator.h"
#include "ui_firmware_emulator.h"
#include "ui_firmware_layout.h"
#include "ui_firmware_comparison.h"
#include "ui_firmware_info.h"
#include <ostream>

inline bool RunMenuPreviewUiRegressions(std::string& error, std::ostream* report = nullptr) {
    error.clear();
    const auto check = [&](bool ok, const char* message) { if (!ok) error = message; return ok; };
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> gpu;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &device, nullptr, &gpu))) {
        error = "Menu UI WARP device creation failed"; return false;
    }
    auto* previous = ImGui::GetCurrentContext();
    auto* context = ImGui::CreateContext();
    const bool consent = UiFirmwareRisk::State().accepted;
    const auto preferences = StudioPreferences::Get();
    struct Cleanup {
        ImGuiContext *context, *previous;
        bool consent, renderer = false;
        StudioPreferences::Preferences preferences;
        HWND window = nullptr;
        ~Cleanup() {
            WindowChrome::ReleaseGraphics();
            if (window) DestroyWindow(window);
            if (renderer) ImGui_ImplDX11_Shutdown();
            ImGui::DestroyContext(context); ImGui::SetCurrentContext(previous);
            UiFirmwareRisk::State().accepted = consent;
            StudioPreferences::Get() = preferences;
        }
    } cleanup{context, previous, consent, false, preferences};
    UiFirmwareRisk::State().accepted = true;
    StudioPreferences::Get().menuZoom = 4;
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.DisplaySize = ImVec2(1200, 900); io.DeltaTime = 0.05f;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigInputTrickleEventQueue = false;
    const ImWchar ranges[]{0x0020, 0x024F, 0x0400, 0x052F, 0};
    if (!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17, nullptr, ranges)) {
        error = "Menu UI font unavailable"; return false;
    }
    ImGui::StyleColorsDark();
    if (!ImGui_ImplDX11_Init(device.Get(), gpu.Get())) { error = "Menu UI renderer init failed"; return false; }
    cleanup.renderer = true;
    cleanup.window = CreateWindowExW(0, L"STATIC", L"Menu UI verification", WS_OVERLAPPEDWINDOW,
        0, 0, 1216, 939, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!cleanup.window || !WindowChrome::InitializeGraphics(device.Get(), gpu.Get())) {
        error = "Title icon could not be loaded from the application resource"; return false;
    }
    WindowChrome::Initialize(cleanup.window);
    FirmwarePackage package;
    StudioState studio;
    if (!LoadEmbeddedFirmware(package, error, FirmwareModel::DragS, FirmwareEdition::Score) ||
        !studio.LoadPackage(std::move(package), "Menu UI regression", error)) return false;
    studio.menuPatch.enabled = true;
    studio.menuPatch.enabledItems = FirmwareMenu::InformationMask | FirmwareMenu::VoltageMask |
        FirmwareMenu::PuffEditorMask;
    UiFirmwarePatches::State patches;
    auto& preview = patches.menuPreview;
    bool popup = false, closePopup = false, focus = true, renderMenu = true;
    bool otherTextOpaque = false;
    const auto frame = [&](bool fire = false, bool plus = false, bool minus = false) {
        io.AddKeyEvent(ImGuiKey_Space, fire);
        io.AddKeyEvent(ImGuiKey_UpArrow, plus);
        io.AddKeyEvent(ImGuiKey_DownArrow, minus);
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        WindowChrome::Render(cleanup.window);
        ImGui::SetNextWindowPos(ImVec2(0, 32)); ImGui::SetNextWindowSize(ImVec2(1200, 868));
        ImGui::Begin("Menu UI regression", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        auto* draw = ImGui::GetWindowDrawList();
        const int firstGlyph = draw->VtxBuffer.Size;
        ImGui::TextUnformatted("Other editor controls remain visible");
        otherTextOpaque = draw->VtxBuffer.Size > firstGlyph;
        for (int i = firstGlyph; i < draw->VtxBuffer.Size; ++i)
            otherTextOpaque &= ((draw->VtxBuffer[i].col >> IM_COL32_A_SHIFT) & 255) == 255;
        if (focus) { ImGui::FocusWindow(ImGui::GetCurrentWindow()); focus = false; }
        if (popup) ImGui::OpenPopup("Menu input popup");
        if (ImGui::BeginPopup("Menu input popup")) {
            ImGui::TextUnformatted("Keyboard belongs to this popup");
            if (closePopup) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (ImGui::BeginTabBar("Menu tabs")) {
            if (renderMenu) UiFirmwareMenu::RenderTab(patches, studio, device.Get(), gpu.Get());
            ImGui::EndTabBar();
        }
        ImGui::End(); ImGui::Render();
    };
    frame(); frame(); frame();
    studio.menuPatch.enabled = false;
    for (int i = 0; i < 8; ++i) {
        frame();
        if (!check(otherTextOpaque, "Opening a disabled custom menu faded unrelated editor text")) return false;
    }
    studio.menuPatch.enabled = true; frame(); frame();
    if (!check(otherTextOpaque, "Reenabling the custom menu left the editor invisible")) return false;
    const auto revision = studio.ResourceRevision();
    for (const ImGuiKey modifier : {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiMod_Super, ImGuiMod_Shift}) {
        io.AddKeyEvent(modifier, true); frame(true, true, true); frame();
        io.AddKeyEvent(modifier, false); frame();
        if (!check(preview.state.selection == 0 && !preview.state.information,
            "Modified shortcut navigated the menu or activated Fire")) return false;
    }
    frame(false, false, true); frame();
    if (!check(preview.state.selection == 1, "Down did not advance the menu exactly once")) return false;
    frame(false, true); frame();
    if (!check(preview.state.selection == 0, "Up did not return to the previous menu item")) return false;
    studio.menuPatch.reverseNavigation = false;
    frame(false, true); frame();
    if (!check(preview.state.selection == 1, "Preview keyboard ignored reversed physical navigation")) return false;
    studio.menuPatch.reverseNavigation = true; preview.state.selection = 0;
    frame(true); frame();
    if (!check(preview.state.information, "Space did not open the selected information screen")) return false;
    frame(true); frame();
    if (!check(!preview.state.information, "Space did not return from information")) return false;
    preview.state.editingPuffs = true; preview.state.puffValue = 1230; preview.state.puffDigit = 4;
    frame(false, true); frame(false, true); frame(false, true);
    if (!check(preview.state.puffValue == 1231, "Held Up repeated before its initial delay")) return false;
    for (int i = 0; i < 10; ++i) frame(false, true);
    const auto repeated = preview.state.puffValue;
    frame();
    if (!check(repeated != 1231 && preview.state.puffValue == repeated,
        "Held Up did not repeat, or release applied an extra digit change")) return false;
    const auto committed = preview.values.puffs;
    for (int i = 0; i < 17; ++i) frame(true);
    frame();
    if (!check(!preview.state.editingPuffs && preview.values.puffs == committed,
        "Long Space did not discard the edit without committing")) return false;
    preview.state.editingPuffs = true; preview.state.puffValue = 1230; preview.state.puffDigit = 4;
    frame(true); popup = true; frame(true); popup = false; frame();
    if (!check(preview.heldKey == 0 && preview.state.editingPuffs && preview.values.puffs == committed,
        "Opening a popup retained Fire or committed its release")) return false;
    closePopup = true; frame(); closePopup = false; focus = true; frame(); frame();
    frame(true); renderMenu = false; frame(true); renderMenu = true; frame(); frame();
    if (!check(preview.heldKey == 0 && preview.state.editingPuffs && preview.values.puffs == committed,
        "Returning to the menu committed Fire released while another view was open")) return false;
    preview.state.editingPuffs = false;
    for (const int zoom : {1, 4, 16}) {
        preview.zoom = zoom; frame(); frame();
        bool matched = false;
        const auto* data = ImGui::GetDrawData();
        for (int n = 0; n < data->CmdListsCount; ++n) {
            const auto* list = data->CmdLists[n];
            for (const auto& cmd : list->CmdBuffer) {
                if (cmd.GetTexID() != (ImTextureID)preview.texture.GetSRV() || !cmd.ElemCount) continue;
                ImVec2 low(FLT_MAX, FLT_MAX), high(-FLT_MAX, -FLT_MAX);
                for (unsigned i = 0; i < cmd.ElemCount; ++i) {
                    const auto& p = list->VtxBuffer[cmd.VtxOffset + list->IdxBuffer[cmd.IdxOffset + i]].pos;
                    low.x = (std::min)(low.x, p.x); low.y = (std::min)(low.y, p.y);
                    high.x = (std::max)(high.x, p.x); high.y = (std::max)(high.y, p.y);
                }
                matched |= high.x - low.x == 80 * zoom && high.y - low.y == 160 * zoom;
            }
        }
        if (!check(matched, "Menu preview did not render its selected integer zoom")) return false;
    }
    preview.zoom = 4; frame(); frame();
    char path[32768]{};
    const auto length = GetEnvironmentVariableA("VFS_MENU_UI_CAPTURE", path, sizeof(path));
    if (length && length < sizeof(path) && !CaptureUiDrawData(device.Get(), gpu.Get(), path, error)) return false;
    char capturePrefix[32768]{};
    const auto prefixLength = GetEnvironmentVariableA("VFS_EDITOR_UI_CAPTURE", capturePrefix, sizeof(capturePrefix));
    if (prefixLength && prefixLength < sizeof(capturePrefix)) {
        FontGenState fonts;
        fonts.Init();
        UiFirmwareEmulator::State emulator;
        UiFirmwareLayout::State layout;
        UiFirmwareComparison::State comparison;
        comparison.rightSource = UiFirmwareComparison::Source::Reference;
        StudioState rgbStudio;
        FirmwarePackage rgbPackage;
        if (!LoadEmbeddedFirmware(rgbPackage, error, FirmwareModel::Drag3, FirmwareEdition::Tc) ||
            !rgbStudio.LoadPackage(std::move(rgbPackage), "RGB controls verification", error)) return false;
        const auto rgb = std::find_if(rgbStudio.visibleAssetIndices.begin(), rgbStudio.visibleAssetIndices.end(),
            [&](size_t index) { return rgbStudio.catalog[index].format == AssetFormat::Rgb565; });
        if (!check(rgb != rgbStudio.visibleAssetIndices.end(), "RGB fixture has no RGB565 resource")) return false;
        rgbStudio.selectedVisibleIdx = int(rgb - rgbStudio.visibleAssetIndices.begin());
        const char* names[]{"resources", "fonts", "screens", "device", "patches", "layout", "menu", "comparison",
            "info", "rgb"};
        for (int panel = 0; panel < 10; ++panel) {
            for (int warm = 0; warm < 12; ++warm) {
                ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
                WindowChrome::Render(cleanup.window);
                ImGui::SetNextWindowPos(ImVec2(0, 32)); ImGui::SetNextWindowSize(ImVec2(1200, 868));
                ImGui::Begin("Polished editors", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
                auto* draw = ImGui::GetWindowDrawList();
                const int first = draw->VtxBuffer.Size;
                ImGui::TextUnformatted("Editor controls");
                bool opaque = draw->VtxBuffer.Size > first;
                for (int i = first; i < draw->VtxBuffer.Size; ++i)
                    opaque &= ((draw->VtxBuffer[i].col >> IM_COL32_A_SHIFT) & 255) == 255;
                ImGui::PushID(panel);
                if (ImGui::BeginTabBar("Editors")) {
                    switch (panel) {
                    case 0: UiResourceStudio::RenderTab(studio, cleanup.window, device.Get(), gpu.Get()); break;
                    case 1: UiFontGenerator::RenderTab(studio, fonts, cleanup.window, false, 0); break;
                    case 2: case 3:
                        emulator.wholeDevice = panel == 3;
                        UiFirmwareEmulator::RenderTab(emulator, studio, device.Get(), gpu.Get()); break;
                    case 4: UiFirmwarePatches::RenderTab(patches, studio, device.Get(), gpu.Get()); break;
                    case 5: UiFirmwareLayout::RenderTab(layout, studio, device.Get(), gpu.Get(), patches); break;
                    case 6: UiFirmwareMenu::RenderTab(patches, studio, device.Get(), gpu.Get()); break;
                    case 7:
                        comparison.selected = 0;
                        UiFirmwareComparison::RenderTab(comparison, studio, cleanup.window, device.Get()); break;
                    case 8: UiFirmwareInfo::RenderTab(studio); break;
                    case 9: UiResourceStudio::RenderTab(rgbStudio, cleanup.window, device.Get(), gpu.Get()); break;
                    }
                    ImGui::EndTabBar();
                }
                ImGui::PopID(); ImGui::End(); ImGui::Render();
                if (warm >= 2 && !check(opaque, "Switching editors faded unrelated controls")) return false;
            }
            const std::string filename = std::string(capturePrefix) + "." + names[panel] + ".png";
            if (!CaptureUiDrawData(device.Get(), gpu.Get(), filename.c_str(), error)) return false;
            UiFirmwareDevice::PauseAndRelease(emulator.live);
            UiFirmwareLayout::Deactivate(layout, studio);
        }
    }
    if (!check(studio.ResourceRevision() == revision, "Preview keyboard modified the firmware document")) return false;
    if (report) *report << "MENU PREVIEW UI OK (disabled-menu visibility, Up/Down/Space, reverse navigation, repeat, long Fire cancel, popup release, 1/4/16x, resource icon)\n";
    return true;
}
