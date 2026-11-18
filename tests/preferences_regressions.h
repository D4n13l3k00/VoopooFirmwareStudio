#pragma once
#include "app_state.h"
#include "studio_preferences.h"
#include "ui_settings.h"
#include "ui_capture.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ostream>
#include <cstring>

// No HWND, native dialog or real studio_config.ini is used by this regression.
inline bool RunPreferencesRegressions(std::string& error, std::ostream* report = nullptr) {
    error.clear();
    const auto check = [&](bool condition, const char* message) {
        if (!condition) error = message;
        return condition;
    };
    const auto originalPreferences = StudioPreferences::Get();
    const auto originalLanguage = i18n::GetCurrentLanguage();
    struct RestoreSettings {
        StudioPreferences::Preferences preferences;
        i18n::Language language;
        ~RestoreSettings() { StudioPreferences::Get() = preferences; i18n::SetLanguage(language); }
    } restoreSettings{originalPreferences, originalLanguage};
    std::error_code ec;
    auto root = std::filesystem::temp_directory_path(ec);
    if (ec) { error = "Preferences temporary directory unavailable"; return false; }
    root /= "Voopoo_preferences_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(GetTickCount64());
    if (!std::filesystem::create_directory(root, ec)) {
        error = "Preferences temporary directory creation failed"; return false;
    }
    struct RemoveFiles {
        std::filesystem::path path;
        ~RemoveFiles() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
    } removeFiles{root};
    const std::string path = (root / "preferences.ini").string();
    const auto readFile = [&] {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), {});
    };
    const StudioPreferences::Preferences defaults;
    StudioPreferences::Get().patchZoom = 6;
    StudioPreferences::Load(path);
    if (!check(StudioPreferences::Get() == defaults, "Missing preferences did not restore all defaults")) return false;
    {
        std::ofstream file(path, std::ios::binary);
        file << "; unrelated configuration must survive\r\n[Settings]\r\nLanguage=ru\r\n"
            "[Safety]\r\nEmulatorOnlyConsentV1=1\r\n[FutureSection]\r\nKeep=untouched\r\n"
            "[StudioPreferences]\r\nFutureKey=preserved\r\nSmoothAnimations=maybe\r\n"
            "LogReviewSeconds=999999999999999999999\r\nDefaultPatchTarget=3\r\nDefaultBootStages=0\r\n"
            "PatchZoom=17\r\nEmulatorZoom=-1\r\nDeviceZoom=16garbage\r\nLayoutZoom=17\r\n"
            "MenuZoom=17\r\nComparisonZoom=17\r\nResourceZoom=33\r\nFontZoom=0\r\nPlacementBody=2\r\n"
            "LayoutOverlapsBody=-1\r\nDeviceRegistersBody=false\r\n";
        if (!file) { error = "Cannot seed preferences test INI"; return false; }
    }
    StudioPreferences::Load(path);
    if (!check(StudioPreferences::Get() == defaults, "Malformed/out-of-range INI values did not use field defaults")) return false;
    auto saved = defaults;
    saved.smoothAnimations = false;
    saved.logReviewSeconds = 30;
    saved.defaultPatchTarget = 2;
    saved.defaultBootStages = 3;
    saved.patchZoom = saved.emulatorZoom = saved.deviceZoom = 16;
    saved.layoutZoom = saved.comparisonZoom = saved.fontZoom = 16;
    saved.menuZoom = 16;
    saved.resourceZoom = 32;
    for (size_t index = 0; index < saved.sectionOpen.size(); ++index) saved.sectionOpen[index] = !defaults.sectionOpen[index];
    StudioPreferences::Get() = saved;
    if (!check(StudioPreferences::Save(path), "Preferences save failed for isolated writable INI")) return false;
    const auto persisted = readFile();
    if (!check(persisted.find("Language=ru") != std::string::npos &&
        persisted.find("EmulatorOnlyConsentV1=1") != std::string::npos &&
        persisted.find("Keep=untouched") != std::string::npos &&
        persisted.find("FutureKey=preserved") != std::string::npos,
        "Preferences save damaged language, safety or unknown settings")) return false;
    StudioPreferences::Reset();
    StudioPreferences::Load(path);
    if (!check(StudioPreferences::Get() == saved, "Preferences round trip lost a field or upper boundary")) return false;
    auto lower = defaults;
    lower.logReviewSeconds = 1; lower.defaultPatchTarget = 0; lower.defaultBootStages = 1;
    lower.patchZoom = lower.emulatorZoom = lower.deviceZoom = lower.layoutZoom = 1;
    lower.comparisonZoom = lower.resourceZoom = lower.fontZoom = 1;
    lower.menuZoom = 1;
    StudioPreferences::Get() = lower;
    if (!check(StudioPreferences::Save(path), "Preferences lower-bound save failed")) return false;
    StudioPreferences::Reset(); StudioPreferences::Load(path);
    if (!check(StudioPreferences::Get() == lower, "Preferences lower boundary did not round trip")) return false;
    StudioPreferences::Get().fontZoom = -4;
    StudioPreferences::Get().resourceZoom = 400;
    if (!check(StudioPreferences::Save(path), "Invalid in-memory preferences could not be normalized on save")) return false;
    StudioPreferences::Load(path);
    if (!check(StudioPreferences::Get().fontZoom == defaults.fontZoom &&
        StudioPreferences::Get().resourceZoom == defaults.resourceZoom,
        "Save persisted invalid in-memory zooms")) return false;
    const auto beforeFailure = readFile();
    {
        const HANDLE lock = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (!check(lock != INVALID_HANDLE_VALUE, "Cannot lock isolated INI for failure regression")) return false;
        const bool result = StudioPreferences::Save(path);
        CloseHandle(lock);
        if (!check(!result && readFile() == beforeFailure, "Failed atomic save reported success or changed destination")) return false;
    }
    if (!check(!StudioPreferences::Save((root / "missing" / "config.ini").string()),
        "Preferences save succeeded with a missing parent directory")) return false;

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> gpu;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, gpu.GetAddressOf()))) {
        error = "Preferences UI could not create an offscreen WARP device"; return false;
    }
    auto* previousContext = ImGui::GetCurrentContext();
    auto* context = ImGui::CreateContext();
    struct RestoreContext {
        ImGuiContext *context, *previous;
        bool renderer = false;
        ~RestoreContext() {
            if (renderer) ImGui_ImplDX11_Shutdown();
            ImGui::DestroyContext(context); ImGui::SetCurrentContext(previous);
        }
    } restoreContext{context, previousContext};
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.DisplaySize = ImVec2(1200, 900); io.DeltaTime = 0.03f;
    io.ConfigInputTrickleEventQueue = false;
    const ImWchar glyphRanges[]{0x0020, 0x024F, 0x0400, 0x052F, 0x2000, 0x206F, 0};
    if (!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17, nullptr, glyphRanges)) {
        error = "Preferences UI Segoe UI font unavailable"; return false;
    }
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 0; style.FrameRounding = 4; style.ItemSpacing = ImVec2(8, 6);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.65f);
    if (!ImGui_ImplDX11_Init(device.Get(), gpu.Get())) {
        error = "Preferences UI renderer initialization failed"; return false;
    }
    restoreContext.renderer = true;
    D3D11_TEXTURE2D_DESC targetDescription{};
    targetDescription.Width = 1200; targetDescription.Height = 900;
    targetDescription.MipLevels = targetDescription.ArraySize = targetDescription.SampleDesc.Count = 1;
    targetDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    targetDescription.BindFlags = D3D11_BIND_RENDER_TARGET;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> target;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> targetView;
    if (FAILED(device->CreateTexture2D(&targetDescription, nullptr, &target)) ||
        FAILED(device->CreateRenderTargetView(target.Get(), nullptr, &targetView))) {
        error = "Preferences offscreen render target creation failed"; return false;
    }
    StudioPreferences::Get() = defaults;
    i18n::SetLanguage(i18n::Language::English);
    StudioState studio;
    studio.hasFirmware = true; studio.projectDocument = true; studio.previewZoom = 17;
    studio.screenPatches[0].bootStages = FirmwareLogo::BootStages::First;
    studio.MarkSaved();
    const auto originalPatches = studio.screenPatches;
    const auto originalRevision = studio.ResourceRevision();
    UiSettings::State settings;
    settings.configPath = path;
    const auto frame = [&](ImVec2 mouse = ImVec2(-100, -100), bool down = false) {
        io.AddMousePosEvent(mouse.x, mouse.y); io.AddMouseButtonEvent(0, down);
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Settings regression host", nullptr,
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        UiSettings::RenderMenuBar(settings);
        UiSettings::RenderDialog(settings, studio);
        ImGui::End(); ImGui::Render();
        auto* view = targetView.Get(); gpu->OMSetRenderTargets(1, &view, nullptr);
        const float clear[]{0.06f, 0.06f, 0.07f, 1}; gpu->ClearRenderTargetView(view, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    };
    const auto settle = [&] { for (int index = 0; index < 9; ++index) frame(); };
    const auto click = [&](ImVec2 at) { frame(at); frame(at, true); frame(at, false); frame(); };
    const auto visible = [&] {
        const auto* modal = ImGui::FindWindowByName("###StudioSettings");
        return modal && modal->LastFrameActive == context->FrameCount && !modal->Hidden;
    };
    // The last submitted row is Save/Cancel. Use real layout geometry, not fixed screen coordinates.
    const auto footerButton = [&](int button) {
        const auto* modal = ImGui::FindWindowByName("###StudioSettings");
        const float width = (std::max)(96.0f, (std::max)(ImGui::CalcTextSize(Tr(i18n::Str::BtnSaveChanges)).x,
            ImGui::CalcTextSize(Tr(i18n::Str::BtnCancelAction)).x) + style.FramePadding.x * 2);
        const float y = modal->DC.CursorPosPrevLine.y + ImGui::GetFrameHeight() * 0.5f;
        if (button == 0) return ImVec2(modal->Pos.x + modal->WindowPadding.x + 40, y);
        return ImVec2(modal->DC.CursorPosPrevLine.x - width * 0.5f -
            (button == 1 ? width + style.ItemSpacing.x : 0), y);
    };
    settings.requested = true; frame(); frame();
    if (!check(visible() && settings.transition.Opacity() > 0 && settings.transition.Opacity() < 1,
        "Settings modal did not use shared fade-in")) return false;
    settle();
    const auto unchanged = StudioPreferences::Get();
    const auto cancelDisk = readFile();
    settings.draft.resourceZoom = 31; settings.draft.smoothAnimations = false;
    settings.language = i18n::Language::Russian;
    frame();
    if (!check(StudioPreferences::Get() == unchanged && i18n::GetCurrentLanguage() == i18n::Language::English,
        "Editing settings draft applied preferences or language early")) return false;
    click(footerButton(2));
    if (!check(!settings.open && settings.transition.IsClosing(), "Cancel did not start settings fade-out")) return false;
    settle();
    if (!check(!visible() && context->OpenPopupStack.empty() && readFile() == cancelDisk &&
        StudioPreferences::Get() == unchanged && i18n::GetCurrentLanguage() == i18n::Language::English,
        "Cancel left a popup or changed settings/language/INI")) return false;
    settings.requested = true; settle();
    settings.draft.defaultPatchTarget = 2; settings.draft.defaultBootStages = 3;
    settings.draft.resourceZoom = 25; settings.draft.logReviewSeconds = 12;
    settings.language = i18n::Language::Russian;
    const auto committed = settings.draft;
    click(footerButton(1)); settle();
    if (!check(!settings.open && !visible() && StudioPreferences::Get() == committed &&
        i18n::GetCurrentLanguage() == i18n::Language::Russian,
        "Save did not commit the settings draft and selected language")) return false;
    StudioPreferences::Reset(); StudioPreferences::Load(path);
    if (!check(StudioPreferences::Get() == committed &&
        readFile().find("Language=ru") != std::string::npos &&
        studio.previewZoom == 17 && studio.screenPatches == originalPatches &&
        studio.ResourceRevision() == originalRevision && !studio.HasUnsavedChanges(),
        "Save failed persistence or changed/marked the current document")) return false;
    settings.requested = true; settle();
    settings.draft.resourceZoom = 2;
    const auto beforeUiFailure = readFile();
    {
        const HANDLE lock = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (!check(lock != INVALID_HANDLE_VALUE, "Cannot lock UI preferences destination")) return false;
        click(footerButton(1));
        CloseHandle(lock);
    }
    if (!check(settings.open && !settings.error.empty() && StudioPreferences::Get() == committed &&
        readFile() == beforeUiFailure, "Failed UI preference save applied the draft or changed the destination")) return false;
    settings.draft.resourceZoom = 2; settings.language = i18n::Language::German;
    settings.configPath = (root / "missing" / "config.ini").string();
    click(footerButton(1));
    if (!check(settings.open && visible() && !settings.error.empty() && StudioPreferences::Get() == committed &&
        i18n::GetCurrentLanguage() == i18n::Language::Russian,
        "Failed save hid the dialog, omitted the error or mutated global settings")) return false;
    settings.configPath = path;
    click(footerButton(0));
    if (!check(settings.draft == defaults && settings.language == i18n::DetectSystemLanguage() &&
        StudioPreferences::Get() == committed && readFile().find("EmulatorOnlyConsentV1=1") != std::string::npos,
        "Reset modified live settings or safety consent instead of just the draft")) return false;
    settings.error.clear();
    for (int category = 0; category < 3; ++category) {
        settings.category = category; settle();
        if (!check(visible() && ImGui::GetDrawData()->TotalVtxCount > 0,
            "Settings category did not produce visible offscreen draw data")) return false;
    }
    // Capture all categories in Russian using the production font/theme and software rendering.
    char capturePath[32768]{};
    const DWORD captureLength = GetEnvironmentVariableA("VFS_SETTINGS_UI_CAPTURE", capturePath, sizeof(capturePath));
    if (captureLength && captureLength < sizeof(capturePath)) {
        settings.error.clear(); settings.draft = committed;
        settings.category = 0; settle();
        if (!CaptureUiDrawData(device.Get(), gpu.Get(), capturePath, error)) return false;
        settings.category = 1; settle();
        const std::string screensPath = std::string(capturePath) + ".screens.png";
        if (!CaptureUiDrawData(device.Get(), gpu.Get(), screensPath.c_str(), error)) return false;
        settings.category = 2; settle();
        const std::string interfacePath = std::string(capturePath) + ".interface.png";
        if (!CaptureUiDrawData(device.Get(), gpu.Get(), interfacePath.c_str(), error)) return false;
    }
    io.AddKeyEvent(ImGuiKey_Escape, true); frame();
    io.AddKeyEvent(ImGuiKey_Escape, false); settle();
    if (!check(!visible() && StudioPreferences::Get() == committed, "Escape did not cancel settings draft")) return false;
    StudioPreferences::Get().smoothAnimations = false;
    settings.requested = true; frame();
    if (!check(settings.transition.Opacity() == 1, "Disabled motion still animated settings opening")) return false;
    // ImGui may hide the first appearing frame to measure/center the popup.
    frame();
    if (!check(visible() && settings.transition.Opacity() == 1, "Reduced-motion settings did not open fully opaque")) return false;
    const auto* modal = ImGui::FindWindowByName("###StudioSettings");
    const ImVec2 close(modal->Pos.x + modal->Size.x - modal->TitleBarHeight * 0.5f,
        modal->Pos.y + modal->TitleBarHeight * 0.5f);
    click(close); frame();
    if (!check(!visible() && context->OpenPopupStack.empty() && context->DimBgRatio == 0,
        "Disabled motion titlebar close left the settings modal or dimming")) return false;
    if (report) *report << "PREFERENCES REGRESSIONS OK (isolated INI, all fields/bounds, malformed defaults, preserved sections, failed atomic save, WARP categories, draft/cancel/save/reset/error, language commit, untouched document, reduced motion)\n";
    return true;
}
