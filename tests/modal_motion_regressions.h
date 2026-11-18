#pragma once
#include "app_state.h"
#include "ui_startup.h"
#include "ui_app_update.h"
#include "ui_capture.h"
#include <cmath>
#include <ostream>
#include <chrono>
#include <thread>

// Production modal rendering and actual titlebar mouse input, with no HWND,
// network requests, native file dialogs, or windows on the user's desktop.
inline bool RunModalMotionRegressions(std::string& error, std::ostream* report = nullptr) {
    error.clear();
    const auto savedPreferences = StudioPreferences::Get();
    struct RestorePreferences {
        StudioPreferences::Preferences saved;
        ~RestorePreferences() { StudioPreferences::Get() = saved; }
    } restorePreferences{savedPreferences};
    StudioPreferences::Get() = {};
    const auto check = [&](bool condition, const char* message) {
        if (!condition) error = message;
        return condition;
    };
    Ui::ModalTransition motion;
    motion.Update(true, 1.0);
    motion.Update(true, 1.075);
    if (!check(std::abs(motion.Opacity() - 0.5f) < 0.001f,
        "Modal fade-in is not a 150ms smoothstep")) return false;
    motion.Update(false, 1.075);
    motion.Update(false, 1.12);
    if (!check(std::abs(motion.Opacity() - 0.15625f) < 0.001f && motion.IsClosing(),
        "Modal close did not preserve phase or use a 180ms fade")) return false;
    motion.Update(true, 1.12);
    motion.Update(true, 1.1575);
    if (!check(std::abs(motion.Opacity() - 0.5f) < 0.001f && !motion.IsClosing(),
        "Modal reversal jumped or retained its closing state")) return false;
    motion.Update(false, 1.1575);
    motion.Update(false, 2.0);
    if (!check(motion.FullyHidden() && motion.Opacity() == 0,
        "Modal transition did not finish hidden")) return false;
    motion.Update(true, 2.0);
    motion.Update(true, 2.075);
    StudioPreferences::Get().smoothAnimations = false;
    motion.Update(true, 2.075);
    if (!check(motion.Opacity() == 1 && !motion.IsClosing(),
        "Disabling motion during fade-in did not snap fully visible")) return false;
    motion.Update(false, 2.075);
    if (!check(motion.FullyHidden() && motion.Opacity() == 0,
        "Disabled motion did not snap closed")) return false;
    motion.Update(true, 2.075);
    StudioPreferences::Get().smoothAnimations = true;
    motion.Update(false, 2.075);
    motion.Update(false, 2.12);
    StudioPreferences::Get().smoothAnimations = false;
    motion.Update(false, 2.12);
    if (!check(motion.FullyHidden(), "Disabling motion during fade-out retained a closing phase")) return false;
    StudioPreferences::Get().smoothAnimations = true;

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> gpu;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, gpu.GetAddressOf()))) {
        error = "Modal motion UI could not create an offscreen WARP device";
        return false;
    }
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGuiContext* context = ImGui::CreateContext();
    struct Cleanup {
        ImGuiContext *context, *previous;
        bool renderer = false;
        ~Cleanup() {
            UiStartup::Release();
            if (renderer) ImGui_ImplDX11_Shutdown();
            ImGui::DestroyContext(context);
            ImGui::SetCurrentContext(previous);
        }
    } cleanup{context, previous};
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1400, 1000);
    io.DeltaTime = 0.03f;
    io.ConfigInputTrickleEventQueue = false;
    const ImWchar glyphRanges[]{0x0020, 0x024F, 0x0400, 0x052F, 0};
    if (!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17, nullptr, glyphRanges)) {
        error = "Modal motion UI font unavailable";
        return false;
    }
    if (!ImGui_ImplDX11_Init(device.Get(), gpu.Get())) {
        error = "Modal motion UI renderer initialization failed";
        return false;
    }
    cleanup.renderer = true;
    StudioState studio;
    UiAppUpdate::State updates;
    studio.showStartupChooser = true;
    bool replaceChooser = false;
    const auto frame = [&](ImVec2 mouse = ImVec2(-100, -100), bool down = false) {
        io.AddMousePosEvent(mouse.x, mouse.y);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, down);
        UiResourceStudio::PollFileActions(studio, nullptr);
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Modal motion host", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        if (replaceChooser) {
            ImGui::ClosePopupToLevel(0, true);
            replaceChooser = false;
        }
        // Deliberately dispatch even when the public visibility flag is false.
        UiStartup::Render(studio, nullptr, device.Get(), gpu.Get());
        UiResourceStudio::RenderFileProgress(studio);
        UiAppUpdate::RenderDialog(updates, studio, nullptr);
        ImGui::End();
        ImGui::Render();
    };
    const auto visible = [&](const char* name) {
        const auto* window = ImGui::FindWindowByName(name);
        return window && window->LastFrameActive == context->FrameCount && !window->Hidden;
    };
    const auto settle = [&] { for (int i = 0; i < 9; ++i) frame(); };
    const auto closeTitlebar = [&](const char* name) {
        auto* window = ImGui::FindWindowByName(name);
        if (!window) return false;
        const ImVec2 close(window->Pos.x + window->Size.x - window->TitleBarHeight * 0.5f,
            window->Pos.y + window->TitleBarHeight * 0.5f);
        frame(close, false);
        frame(close, true);
        frame(close, false);
        frame();
        return true;
    };
    const auto fading = [&](const char* name) {
        auto* window = ImGui::FindWindowByName(name);
        return visible(name) && window && (window->Flags & ImGuiWindowFlags_NoInputs) &&
            context->DimBgRatio > 0 && context->DimBgRatio < 1;
    };
    const auto hidden = [&](const char* name) {
        settle();
        return !visible(name) && context->OpenPopupStack.empty() && context->DimBgRatio == 0;
    };
    const auto startupAlphaMatches = [&] {
        const auto* modal = ImGui::FindWindowByName("###StartupChooser");
        const ImGuiWindow* grid = nullptr;
        for (const auto* window : context->Windows)
            if (window->ParentWindow == modal && std::strstr(window->Name, "DeviceGrid")) grid = window;
        if (!check(grid && visible(grid->Name), "Startup product grid disappeared during its fade")) return false;
        const unsigned expected = unsigned(context->DimBgRatio * 255.0f);
        unsigned maximum = 0, images = 0;
        const auto* draw = grid->DrawList;
        for (const auto& vertex : draw->VtxBuffer)
            maximum = (std::max)(maximum, (vertex.col >> IM_COL32_A_SHIFT) & 255u);
        for (const auto& command : draw->CmdBuffer) {
            for (unsigned at = command.IdxOffset; at + 5 < command.IdxOffset + command.ElemCount; at += 6) {
                const auto& a = draw->VtxBuffer[command.VtxOffset + draw->IdxBuffer[at]];
                const auto& c = draw->VtxBuffer[command.VtxOffset + draw->IdxBuffer[at + 2]];
                if (a.uv.x != 0 || a.uv.y != 0 || c.uv.x != 1 || c.uv.y != 1) continue;
                ++images;
                for (unsigned index = 0; index < 6; ++index) {
                    const auto& vertex = draw->VtxBuffer[command.VtxOffset + draw->IdxBuffer[at + index]];
                    const unsigned alpha = (vertex.col >> IM_COL32_A_SHIFT) & 255u;
                    if (!check(std::abs(int(alpha) - int(expected)) <= 1,
                        "Product photo remained opaque or dimmed separately from the startup fade")) return false;
                }
            }
        }
        return check(images > 0 && std::abs(int(maximum) - int(expected)) <= 1,
            "Startup controls jumped to disabled opacity instead of following the fade");
    };
    char capturePrefix[MAX_PATH]{};
    const DWORD captureLength = GetEnvironmentVariableA("VFS_MODAL_MOTION_CAPTURE", capturePrefix, MAX_PATH);
    if (!check(captureLength < MAX_PATH, "Modal capture path exceeds MAX_PATH")) return false;
    const auto capture = [&](const char* stage) {
        return !captureLength || CaptureUiDrawData(device.Get(), gpu.Get(),
            (std::string(capturePrefix) + "." + stage + ".png").c_str(), error);
    };

    frame();
    frame();
    if (!check(visible("###StartupChooser") && context->DimBgRatio > 0 && context->DimBgRatio < 1,
        "Startup chooser did not fade in")) return false;
    settle();
    if (captureLength) {
        const auto* chooser = ImGui::FindWindowByName("###StartupChooser");
        const ImGuiWindow* grid = nullptr;
        for (const auto* window : context->Windows)
            if (window->ParentWindow == chooser && std::strstr(window->Name, "DeviceGrid")) grid = window;
        if (!check(grid != nullptr, "Startup support capture has no device grid")) return false;
        const ImVec2 card(grid->Pos.x + grid->WindowPadding.x + 40,
            grid->Pos.y + grid->WindowPadding.y + 40);
        for (int i = 0; i < 60; ++i) frame(card);
        bool tooltip = false;
        for (const auto* window : context->Windows)
            tooltip |= (window->Flags & ImGuiWindowFlags_Tooltip) && visible(window->Name);
        if (!check(tooltip, "Device support tooltip did not appear on hover") || !capture("startup-patch-support"))
            return false;
        frame();
    }
    if (!check(closeTitlebar("###StartupChooser") && !studio.showStartupChooser && fading("###StartupChooser"),
        "Startup titlebar X closed immediately or left closing controls enabled")) return false;
    if (!check(hidden("###StartupChooser"), "Startup titlebar close reopened or left modal dimming")) return false;

    studio.showStartupChooser = true;
    settle();
    if (!startupAlphaMatches() || !capture("startup-open")) return false;
    studio.showStartupChooser = false;
    frame();
    if (!startupAlphaMatches() || !capture("startup-close-start")) return false;
    for (int step = 1; step <= 5; ++step) {
        frame();
        if (!check(fading("###StartupChooser"), "Startup close skipped an intermediate fade frame") ||
            !startupAlphaMatches()) return false;
        if ((step == 1 || step == 3 || step == 5) &&
            !capture(("startup-close-" + std::to_string(step)).c_str())) return false;
    }
    if (!check(hidden("###StartupChooser"), "External startup hide reopened the chooser")) return false;
    if (!capture("startup-closed")) return false;

    studio.showStartupChooser = true;
    settle();
    const auto previousDocument = studio.DocumentIdentity();
    const auto* chooser = ImGui::FindWindowByName("###StartupChooser");
    if (!check(chooser != nullptr, "Create scenario has no startup chooser")) return false;
    ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::StartupOpenBuiltin), 0, chooser->ID));
    frame();
    const bool startedBuiltin = UiResourceStudio::IsLoadingBuiltin();
    bool keptChooser = visible("###StartupChooser");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (UiResourceStudio::IsLoadingBuiltin() && std::chrono::steady_clock::now() < deadline) {
        frame();
        keptChooser = keptChooser && visible("###StartupChooser");
        if (UiResourceStudio::IsLoadingBuiltin()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!check(startedBuiltin && !UiResourceStudio::IsFileActionActive() && studio.hasFirmware &&
        studio.DocumentIdentity() != previousDocument && !studio.showStartupChooser && keptChooser &&
        visible("###StartupChooser") && context->DimBgRatio == 1,
        "Create/adoption removed the startup chooser before its closing transition")) return false;
    if (!startupAlphaMatches() || !capture("startup-create-close-start")) return false;
    for (int step = 1; step <= 5; ++step) {
        frame();
        if (!check(fading("###StartupChooser"), "Create skipped an intermediate startup close frame") ||
            !startupAlphaMatches()) return false;
        if (step == 3 && !capture("startup-create-close-middle")) return false;
    }
    if (!check(hidden("###StartupChooser") && !UiResourceStudio::IsFileActionActive(),
        "Create did not finish its fade or retained a file operation")) return false;
    if (!capture("startup-create-closed")) return false;

    studio.showStartupChooser = true;
    settle();
    studio.showStartupChooser = false;
    replaceChooser = true;
    frame();
    if (!check(!visible("###StartupChooser") && context->OpenPopupStack.empty(),
        "External file adoption reopened a replaced startup popup")) return false;
    frame();

    updates.requested = true;
    frame();
    frame();
    if (!check(visible("###AppUpdates") && context->DimBgRatio > 0 && context->DimBgRatio < 1,
        "Update modal did not fade in")) return false;
    settle();
    if (!check(closeTitlebar("###AppUpdates") && !updates.open && fading("###AppUpdates"),
        "Update titlebar X closed immediately or left closing controls enabled")) return false;
    if (!check(hidden("###AppUpdates"), "Update titlebar close reopened or left modal dimming")) return false;

    updates.requested = true;
    settle();
    updates.open = false;
    frame();
    frame();
    if (!check(fading("###AppUpdates"), "External update hide skipped the close animation")) return false;
    const float closingOpacity = updates.transition.Opacity();
    updates.requested = true;
    frame();
    if (!check(updates.open && !updates.transition.IsClosing() &&
        updates.transition.Opacity() < closingOpacity && !updates.transition.FullyHidden(),
        "Update reopen restarted its fade instead of reversing it")) return false;
    settle();
    updates.open = false;
    if (!check(hidden("###AppUpdates") && updates.client.Get().phase == AppUpdate::Phase::Idle,
        "Closing the update view changed client work or left a popup")) return false;
    StudioPreferences::Get().smoothAnimations = false;
    studio.showStartupChooser = true;
    frame();
    frame(); // Allow ImGui's first-frame popup measurement, not an animation.
    if (!check(visible("###StartupChooser") && context->DimBgRatio == 1,
        "Disabled startup motion did not open without fading")) return false;
    studio.showStartupChooser = false;
    frame();
    if (!check(!visible("###StartupChooser") && context->OpenPopupStack.empty() && context->DimBgRatio == 0,
        "Disabled startup motion retained popup or dimming")) return false;
    updates.requested = true;
    frame();
    frame();
    if (!check(visible("###AppUpdates") && updates.transition.Opacity() == 1,
        "Disabled update motion did not snap open")) return false;
    updates.open = false;
    frame();
    if (!check(!visible("###AppUpdates") && context->OpenPopupStack.empty() && context->DimBgRatio == 0,
        "Disabled update motion retained popup or dimming")) return false;
    if (report) *report << "MODAL MOTION REGRESSIONS OK (150/180ms smoothstep, reversal, startup X/external hide, replaced popup, update X/external hide, no closing input, offscreen WARP)\n";
    return true;
}
