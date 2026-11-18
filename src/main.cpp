#include "ui_tabs.h"
#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx11.h"

#include <d3d11.h>
#include <tchar.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <new>
#include <psapi.h>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <ctime>
#include <cmath>
#include <functional>

#include "app_state.h"
#include "path_utils.h"
#include "texture_cache.h"
#include "firmware_color.h"
#include "firmware_catalog.h"
#include "firmware_bitmap.h"
#include "zwdf_package.h"
#include "embedded_firmware.h"
#include "i18n.h"

#include "ui_resource_studio.h"
#include "ui_firmware_comparison.h"
#include "ui_firmware_emulator.h"
#include "ui_firmware_patches.h"
#include "ui_firmware_layout.h"
#include "ui_firmware_menu.h"
#include "ui_app_update.h"
#include "studio_preferences.h"
#include "ui_settings.h"
#include "window_chrome.h"
#include "ui_keyboard.h"
#include "../tests/editor_regressions.h"
#include "../tests/comparison_regressions.h"
#include "../tests/emulator_regressions.h"
#include "../tests/layout_regressions.h"
#include "../tests/native_coverage_regressions.h"
#include "../tests/sx_page_layout_regressions.h"
#include "../tests/legacy_coverage_regressions.h"
#include "../tests/three_max_coverage_regressions.h"
#include "../tests/pro_plus_coverage_regressions.h"
#include "../tests/device_regressions.h"
#include "../tests/device_custom_info_regressions.h"
#include "../tests/device_menu_regressions.h"
#include "../tests/device_puff_editor_regressions.h"
#include "../tests/device_achievement_regressions.h"
#include "../tests/device_menu_hold_regressions.h"
#include "../tests/device_diagnostics_ui_regressions.h"
#include "../tests/async_preview_regressions.h"
#include "../tests/device_worker_regressions.h"
#include "../tests/device_input_regressions.h"
#include "../tests/unicorn_jit_regressions.h"
#include "../tests/project_regressions.h"
#include "../tests/flash_usage_regressions.h"
#include "../tests/patch_playback_regressions.h"
#include "../tests/native_effect_regressions.h"
#include "../tests/async_file_regressions.h"
#include "../tests/modal_motion_regressions.h"
#include "../tests/preferences_regressions.h"
#include "../tests/menu_preview_ui_regressions.h"
#include "../tests/preview_regressions.h"
#include "../tests/layout_geometry_regressions.h"
#include "../tests/layout_selection_regressions.h"
#include "../tests/app_update_regressions.h"
#include "../tests/firmware_color_audit.h"
#include "../tests/history_regressions.h"
#include "../tests/save_regressions.h"
#include "../tests/model_regressions.h"
#include "../tests/xwdf_regressions.h"
#include "../tests/factory_regressions.h"
#include "ui_startup.h"
#include "../tests/native_profile_regressions.h"
#include "ui_firmware_info.h"

#include "ui_font_generator.h"
#include "ui_firmware_risk.h"


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;

// Global DirectX 11 Data
static ID3D11Device*           g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;
static std::function<void()>   g_RenderCallback = nullptr;
static bool                     g_InSizeMove = false;
static bool                     g_CloseRequested = false;


// Drag & Drop files queue
static std::vector<std::string> g_DroppedFiles;

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Global Crash Handler
static LONG WINAPI GlobalCrashHandler(EXCEPTION_POINTERS* pEx) {
    std::ofstream crashLog("studio_debug.log", std::ios::app);
    if (crashLog.is_open()) {
        crashLog << "FATAL SEH CRASH! ExceptionCode: 0x" << std::hex << pEx->ExceptionRecord->ExceptionCode
                 << " at Address: 0x" << pEx->ExceptionRecord->ExceptionAddress << "\n" << std::flush;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR commandLine, int) {
    const std::wstring_view args(commandLine);
    const bool smokeTest = args.find(L"--smoke-test") != args.npos;
    const bool layoutTest = args.find(L"--layout-test") != args.npos;
    const bool projectTest = args.find(L"--project-test") != args.npos;
    const auto coverageArgument = args.find(L"--coverage-test=");
    const bool coverageTest = coverageArgument != args.npos;
    auto coverageFamily = coverageTest ? args.substr(coverageArgument + 16) : std::wstring_view{};
    coverageFamily = coverageFamily.substr(0, coverageFamily.find_first_of(L" \t"));
    const bool enableDebugLog = smokeTest || layoutTest || projectTest || coverageTest || args.find(L"--debug") != args.npos;
    SetUnhandledExceptionFilter(GlobalCrashHandler);
    i18n::InitI18n();
    std::ofstream logF;
    if (enableDebugLog) {
        logF.open("studio_debug.log");
    }
    logF << "Starting Voopoo Firmware Studio...\n" << std::flush;

    try {
        if (coverageTest) {
            constexpr std::wstring_view layoutPrefix = L"layout-profile-";
            if (coverageFamily.starts_with(layoutPrefix)) {
                const auto suffix = coverageFamily.substr(layoutPrefix.size());
                size_t index = 0;
                if (suffix.empty()) return 37;
                for (const wchar_t digit : suffix) {
                    if (digit < L'0' || digit > L'9') return 37;
                    index = index * 10 + size_t(digit - L'0');
                    if (index >= FirmwareProfiles().size()) return 37;
                }
                std::string error;
                if (!RunLayoutRegressions(error, &logF, &FirmwareProfiles()[index])) {
                    logF << "LAYOUT REGRESSIONS FAILED: " << error << std::endl;
                    return 31;
                }
                return 0;
            }
            // Split the same native framebuffer matrix across processes without
            // reducing coverage or making the normal "all" family run it twice.
            struct EffectShard {
                std::wstring_view name;
                FirmwareEdition edition;
                FirmwareLogo::Animation effect;
            };
            constexpr EffectShard shards[] = {
                {L"screen-effects-score-10", FirmwareEdition::Score, FirmwareLogo::Animation::SoftWipe},
                {L"screen-effects-score-11", FirmwareEdition::Score, FirmwareLogo::Animation::SlideRight},
                {L"screen-effects-score-12", FirmwareEdition::Score, FirmwareLogo::Animation::Checker},
                {L"screen-effects-score-13", FirmwareEdition::Score, FirmwareLogo::Animation::Diagonal},
                {L"screen-effects-factory-10", FirmwareEdition::Factory, FirmwareLogo::Animation::SoftWipe},
                {L"screen-effects-factory-11", FirmwareEdition::Factory, FirmwareLogo::Animation::SlideRight},
                {L"screen-effects-factory-12", FirmwareEdition::Factory, FirmwareLogo::Animation::Checker},
                {L"screen-effects-factory-13", FirmwareEdition::Factory, FirmwareLogo::Animation::Diagonal},
                {L"screen-effects-score-14", FirmwareEdition::Score, FirmwareLogo::Animation::SlideLeft},
                {L"screen-effects-score-15", FirmwareEdition::Score, FirmwareLogo::Animation::SlideUp},
                {L"screen-effects-score-16", FirmwareEdition::Score, FirmwareLogo::Animation::SlideDown},
                {L"screen-effects-factory-14", FirmwareEdition::Factory, FirmwareLogo::Animation::SlideLeft},
                {L"screen-effects-factory-15", FirmwareEdition::Factory, FirmwareLogo::Animation::SlideUp},
                {L"screen-effects-factory-16", FirmwareEdition::Factory, FirmwareLogo::Animation::SlideDown},
            };
            for (const auto& shard : shards) {
                if (coverageFamily != shard.name) continue;
                std::string error;
                if (!RunNativeEffectShard(error, &logF, shard.effect, int(shard.edition))) {
                    logF << "NATIVE EFFECT SHARD FAILED: " << error << std::endl;
                    return 36;
                }
                return 0;
            }
            struct CoverageCase {
                std::wstring_view name;
                bool (*run)(std::string&, std::ostream*);
            };
            constexpr CoverageCase deviceShards[] = {
                {L"device-achievements-score-3029", [](std::string& error, std::ostream* log) {
                    return RunDeviceAchievementRevisionRegressions(error,0x3029,log);
                }},
                {L"device-achievements-score-3013", [](std::string& error, std::ostream* log) {
                    return RunDeviceAchievementRevisionRegressions(error,0x3013,log);
                }},
                {L"device-achievements-score-3103", [](std::string& error, std::ostream* log) {
                    return RunDeviceAchievementRevisionRegressions(error,0x3103,log);
                }},
                {L"preview-parity-factory-13", [](std::string& error, std::ostream* log) {
                    return RunNativePreviewParity(error, log, 13, 14, int(FirmwareEdition::Factory));
                }},
                {L"screen-preview-factory-from-13", [](std::string& error, std::ostream* log) {
                    return RunNativePreviewParity(error, log, 13, uint32_t(FirmwareLogo::Animation::Count),
                        int(FirmwareEdition::Factory));
                }},
                {L"screen-effects-export", [](std::string& error, std::ostream* log) {
                    return RunNativeEffectShard(error, log, FirmwareLogo::Animation::None, -1);
                }},
                {L"screen-directions-score-from-36", [](std::string& error, std::ostream* log) {
                    return RunNativePreviewParity(error, log, 36, uint32_t(FirmwareLogo::Animation::Count),
                        int(FirmwareEdition::Score));
                }},
                {L"screen-directions-score", [](std::string& error, std::ostream* log) {
                    return RunNativeDirectionRegressions(error, log, int(FirmwareEdition::Score));
                }},
                {L"screen-directions-factory", [](std::string& error, std::ostream* log) {
                    return RunNativeDirectionRegressions(error, log, int(FirmwareEdition::Factory));
                }},
                {L"screen-preview", RunNativeBasePreviewParity},
                {L"device-matrix-score-3103", [](std::string& error, std::ostream* log) {
                    return RunDeviceFunctionalShard(error, log, FirmwareEdition::Score, 0x3103);
                }},
                {L"device-matrix-score-3014", [](std::string& error, std::ostream* log) {
                    return RunDeviceFunctionalShard(error, log, FirmwareEdition::Score, 0x3014);
                }},
            };
            for (const auto& shard : deviceShards) {
                if (coverageFamily != shard.name) continue;
                std::string error;
                if (!shard.run(error, &logF)) {
                    logF << "NATIVE COVERAGE FAILED: " << error << std::endl;
                    return 36;
                }
                logF << "NATIVE COVERAGE REGRESSIONS OK" << std::endl;
                return 0;
            }
            constexpr CoverageCase cases[] = {
                {L"screens", RunNativeCoverageRegressions},
                {L"sx", RunSxPageCoverageRegressions},
                {L"legacy", RunLegacyCoverageRegressions},
                {L"three-max", RunThreeMaxCoverageRegressions},
                {L"layout-landscape-widths", RunLandscapePowerWidthRegressions},
                {L"pro-plus", RunProPlusCoverageRegressions},
                {L"device", RunDeviceRegressions},
                {L"device-revisions", RunDeviceRevisionRegressions},
                {L"device-matrix", RunDeviceFunctionalMatrix},
                {L"device-custom-info", RunDeviceCustomInfoRegressions},
                {L"device-menu", RunDeviceMenuRegressions},
                {L"device-menu-factory-3014", RunDeviceMenuFactory3014Regressions},
                {L"device-menu-main-surface", RunDeviceMenuMainSurfaceRegressions},
                {L"device-puff-editor", RunDevicePuffEditorRegressions},
                {L"device-puff-editor-factory-3148", RunDevicePuffEditorFactory3148Regressions},
                {L"device-achievements", RunDeviceAchievementRegressions},
                {L"device-menu-holds", RunDeviceMenuHoldRegressions},
                {L"menu-preview-ui", RunMenuPreviewUiRegressions},
                {L"export-review", [](std::string& error, std::ostream* log) {
                    if (!RunExportReviewRegressions(error)) return false;
                    if (log) *log << "EXPORT REVIEW REGRESSIONS OK" << std::endl;
                    return true;
                }},
                {L"export-review-ui", RunExportReviewUiRegressions},
                {L"async-preview", [](std::string& error, std::ostream* log) {
                    if (!RunAsyncPreviewRegressions(error)) return false;
                    if (log) *log << "ASYNC PREVIEW REGRESSIONS OK" << std::endl;
                    return true;
                }},
                {L"device-worker", [](std::string& error, std::ostream* log) {
                    if (!RunDeviceWorkerRegressions(error)) return false;
                    if (log) *log << "DEVICE WORKER REGRESSIONS OK" << std::endl;
                    return true;
                }},
                {L"device-input", [](std::string& error, std::ostream* log) {
                    if (!RunDeviceInputRegressions(error)) return false;
                    if (log) *log << "DEVICE INPUT REGRESSIONS OK" << std::endl;
                    return true;
                }},
                {L"unicorn-jit", RunUnicornJitRegressions},
                {L"flash", RunFlashUsageRegressions},
                {L"editable-sources", RunEditableSourceRegressions},
                {L"editable-source-ui", RunEditableSourceUiRegressions},
                {L"patch-playback", RunPatchPlaybackRegressions},
                {L"screen-effects", RunNativeEffectRegressions},
                {L"device-diagnostics-ui", RunDeviceDiagnosticsUiRegressions},
                {L"async-files", [](std::string& error, std::ostream* log) {
                    return RunAsyncFileRegressions(error, log);
                }},
                {L"modal-motion", RunModalMotionRegressions},
                {L"preferences", RunPreferencesRegressions},
            };
            bool matched = false;
            for (const auto& test : cases) {
                if (coverageFamily != L"all" && coverageFamily != test.name) continue;
                matched = true;
                std::string error;
                if (!test.run(error, &logF)) {
                    logF << "NATIVE COVERAGE FAILED: " << error << std::endl;
                    return 36;
                }
            }
            if (!matched) { logF << "Unknown native coverage family" << std::endl; return 37; }
            logF << "NATIVE COVERAGE REGRESSIONS OK" << std::endl;
            return 0;
        }
        if (projectTest) {
            std::string error;
            if (!RunProjectRegressions(error)) {
                logF << "PROJECT REGRESSIONS FAILED: " << error << std::endl;
                return 32;
            }
            logF << "PROJECT REGRESSIONS OK" << std::endl;
            if (!RunPreviewRegressions(error)) {
                logF << "PREVIEW REGRESSIONS FAILED: " << error << std::endl;
                return 33;
            }
            logF << "PREVIEW REGRESSIONS OK" << std::endl;
            if (!RunLayoutGeometryRegressions(error)) {
                logF << "LAYOUT GEOMETRY REGRESSIONS FAILED: " << error << std::endl;
                return 34;
            }
            logF << "LAYOUT GEOMETRY REGRESSIONS OK" << std::endl;
            if (!RunLayoutSelectionRegressions(error)) {
                logF << "LAYOUT SELECTION REGRESSIONS FAILED: " << error << std::endl;
                return 35;
            }
            logF << "LAYOUT SELECTION REGRESSIONS OK" << std::endl;
            return 0;
        }
        if (layoutTest) {
            std::string error;
            if (!RunLayoutRegressions(error, &logF)) {
                logF << "LAYOUT REGRESSIONS FAILED: " << error << std::endl;
                return 31;
            }
            return 0;
        }
        // Enable DPI awareness via ImGui helper
        ImGui_ImplWin32_EnableDpiAwareness();

        // Register window class
        WNDCLASSEXW wc = {
            sizeof(wc),
            CS_CLASSDC,
            WndProc,
            0L,
            0L,
            GetModuleHandle(nullptr),
            static_cast<HICON>(LoadImageW(GetModuleHandle(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON,
                GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED)),
            LoadCursor(nullptr, IDC_ARROW),
            nullptr,
            nullptr,
            L"VoopooAllInOneStudioClass",
            static_cast<HICON>(LoadImageW(GetModuleHandle(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON,
                GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED))
        };
        if (!RegisterClassExW(&wc)) {
            logF << "RegisterClassExW failed! Error=" << GetLastError() << "\n" << std::flush;
        } else {
            logF << "RegisterClassExW succeeded.\n" << std::flush;
        }

        // Compute initial DPI scaling
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int winW = 1280;
        int winH = 860;
        int posX = (screenW > winW) ? (screenW - winW) / 2 : 30;
        int posY = (screenH > winH) ? (screenH - winH) / 2 : 30;
        logF << "Screen: " << screenW << "x" << screenH << ", Pos: " << posX << "," << posY << "\n" << std::flush;

        // Create application window
        HWND hwnd = CreateWindowW(
            wc.lpszClassName,
            L"Voopoo Firmware Studio v" STUDIO_VERSION,
            WS_OVERLAPPEDWINDOW & ~WS_CAPTION,
            posX, posY, winW, winH,
            nullptr, nullptr, wc.hInstance, nullptr
        );

        if (!hwnd) {
            logF << "CreateWindowW failed! Error=" << GetLastError() << "\n" << std::flush;
            MessageBoxW(nullptr, L"CreateWindowW failed!", L"Error", MB_ICONERROR);
            return 1;
        }
        logF << "CreateWindowW succeeded. HWND=" << (void*)hwnd << "\n" << std::flush;
        WindowChrome::Initialize(hwnd);

        // Enable drag and drop files from Windows Explorer
        DragAcceptFiles(hwnd, TRUE);

        // Initialize Direct3D
        if (!CreateDeviceD3D(hwnd)) {
            logF << "CreateDeviceD3D failed!\n" << std::flush;
            CleanupDeviceD3D();
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return 1;
        }
        logF << "CreateDeviceD3D succeeded.\n" << std::flush;
        if (!WindowChrome::InitializeGraphics(g_pd3dDevice, g_pd3dDeviceContext))
            logF << "Could not load the title-bar icon.\n" << std::flush;

        // Show the window explicitly and bring to front
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
        logF << "ShowWindow called.\n" << std::flush;

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.IniFilename = nullptr; // Disable automatic creation of imgui.ini
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Load system UI font with extended glyph ranges for i18n
        ImFontConfig font_cfg;
        font_cfg.OversampleH = 2;
        font_cfg.OversampleV = 2;
        static const ImWchar i18nRanges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x0100, 0x017F, // Latin Extended-A (Polish, Romanian, Azeri)
            0x0180, 0x024F, // Latin Extended-B (Azeri)
            0x0250, 0x02AF, // IPA Extensions
            0x0400, 0x052F, // Cyrillic + Cyrillic Supplement (RU, UK, BE, KK, KY, TG)
            0x0530, 0x058F, // Armenian
            0x10A0, 0x10FF, // Georgian
            0x2DE0, 0x2DFF, // Cyrillic Extended-A
            0xA640, 0xA69F, // Cyrillic Extended-B
            0x2000, 0x206F, // General Punctuation
            0x2100, 0x214F, // Letterlike Symbols
            0x2500, 0x257F, // Box Drawing
            0x25A0, 0x25FF, // Geometric Shapes
            0x2700, 0x27BF, // Dingbats
            0x2600, 0x26FF, // Misc Symbols
            0,
        };
        if (fs::exists("C:\\Windows\\Fonts\\segoeui.ttf")) {
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17.0f, &font_cfg, i18nRanges);
        } else if (fs::exists("C:\\Windows\\Fonts\\arial.ttf")) {
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 17.0f, &font_cfg, i18nRanges);
        } else if (fs::exists("C:\\Windows\\Fonts\\tahoma.ttf")) {
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\tahoma.ttf", 17.0f, &font_cfg, i18nRanges);
        } else {
            io.Fonts->AddFontDefault();
        }

        UiMarkdown::InitializeFonts(i18nRanges);

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.FrameRounding = 4.0f;
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.65f);

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
        logF << "ImGui initialized.\n" << std::flush;

        // -------------------------------------------------------------
        // App State Initialization
        // -------------------------------------------------------------
        logF << "Initializing StudioState...\n" << std::flush;
        StudioPreferences::Load();
        StudioState studio;
        studio.previewZoom = StudioPreferences::Get().resourceZoom;
        UiFirmwareComparison::State comparison;
        UiFirmwareEmulator::State emulator;
        UiFirmwareLayout::State layout;
        UiFirmwarePatches::State patches;
        UiResourceStudio::BindPatchEditor(&patches);
        struct UnbindPatchEditor {
            ~UnbindPatchEditor() { UiResourceStudio::BindPatchEditor(nullptr); }
        } patchEditorBinding;
        UiAppUpdate::State updates;
        UiSettings::State settings;
        studio.showStartupChooser = !smokeTest;
        logF << "StudioState created.\n" << std::flush;
        studio.statusMsg = Tr(i18n::Str::MsgReadyOpenFw);


        FirmwarePackage base;
        std::string baseError;
        if (smokeTest) {
        if (!LoadEmbeddedFirmware(base, baseError) ||
            !studio.LoadPackage(std::move(base), "A050 score version.zwdf", baseError)) {
            char errBuf[256];
            snprintf(errBuf, sizeof(errBuf), Tr(i18n::Str::MsgCannotLoadBuiltinFw), baseError.c_str());
            studio.statusMsg = errBuf;
            studio.statusIsErr = true;
            logF << studio.statusMsg << std::endl;
            if (smokeTest) return 2;
        } else {
            studio.isBuiltin = true;
            char _loadedMsg[256];
            snprintf(_loadedMsg, sizeof(_loadedMsg), Tr(i18n::Str::MsgLoadedBuiltinFw), studio.firmwarePath.c_str());
            studio.statusMsg = _loadedMsg;
            logF << "BASELINE " << studio.firmware.source_sha256 << " resources="
                 << studio.catalog.size() << " records=" << studio.firmware.records.size() << std::endl;
            if (smokeTest) {
                std::vector<uint8_t> packed;
                BuildReport report;
                if (!studio.firmware.PackWith({}, {}, packed, report, baseError) ||
                    packed != studio.firmware.source_bytes) return 3;
                logF << "ROUNDTRIP OK" << std::endl;
                if (!RunEditorRegressions(g_pd3dDevice, g_pd3dDeviceContext, baseError)) {
                    logF << "EDITOR REGRESSIONS FAILED: " << baseError << std::endl;
                    return 20;
                }
                logF << "EDITOR REGRESSIONS OK" << std::endl;
                if (!RunComparisonRegressions(baseError)) {
                    logF << "COMPARISON REGRESSIONS FAILED: " << baseError << std::endl;
                    return 28;
                }
                logF << "COMPARISON REGRESSIONS OK" << std::endl;
                if (!RunAppUpdateRegressions(baseError)) {
                    logF << "UPDATE METADATA REGRESSIONS FAILED: " << baseError << std::endl;
                    return 30;
                }
                logF << "UPDATE METADATA REGRESSIONS OK" << std::endl;
                if (!RunEmulatorRegressions(baseError)) {
                    logF << "EMULATOR REGRESSIONS FAILED: " << baseError << std::endl;
                    return 29;
                }
                logF << "EMULATOR REGRESSIONS OK" << std::endl;
                if (!RunHistoryRegressions(baseError)) {
                    logF << "HISTORY REGRESSIONS FAILED: " << baseError << std::endl;
                    return 22;
                }
                logF << "HISTORY REGRESSIONS OK" << std::endl;
                if (!RunSaveRegressions(baseError)) {
                    logF << "SAVE REGRESSIONS FAILED: " << baseError << std::endl;
                    return 23;
                }
                logF << "SAVE REGRESSIONS OK" << std::endl;
                if (!RunModelRegressions(baseError)) {
                    logF << "MODEL REGRESSIONS FAILED: " << baseError << std::endl;
                    return 24;
                }
                logF << "DRAG S / DRAG X MODEL REGRESSIONS OK" << std::endl;
                if (!RunXwdfRegressions(baseError)) {
                    logF << "XWDF REGRESSIONS FAILED: " << baseError << std::endl;
                    return 25;
                }
                logF << "DRAG 2 XWDF REGRESSIONS OK" << std::endl;
                if (!RunFactoryRegressions(baseError)) {
                    logF << "FACTORY REGRESSIONS FAILED: " << baseError << std::endl;
                    return 26;
                }
                logF << "DRAG S / DRAG X FACTORY REGRESSIONS OK" << std::endl;
                if (!RunNativeProfileRegressions(baseError)) {
                    logF << "NATIVE PROFILE REGRESSIONS FAILED: " << baseError << std::endl;
                    return 27;
                }
                logF << "ALL NATIVE PROFILE REGRESSIONS OK" << std::endl;
                if (!RunFirmwareColorAudit(studio, baseError)) {
                    logF << "FIRMWARE COLOR AUDIT FAILED: " << baseError << std::endl;
                    return 21;
                }
                logF << "PACKED FIRMWARE COLORS OK (all themes + custom palette)" << std::endl;
                size_t whiteGlyphs = 0;
                for (const auto& spec : studio.catalog) {
                    if (spec.id.rfind("font_", 0) != 0) continue;
                    studio.assetPaintColors[spec.id] = Rgb8(255, 0, 0);
                    const auto color = studio.GetForegroundForAsset(spec);
                    studio.assetPaintColors.erase(spec.id);
                    const char* role = PaletteIdForAsset(spec, studio.previewRank, studio.previewCyanMode, studio.edition, studio.model);
                    const auto expected = role ? studio.GetPaletteColor(role) : Rgb8(255, 255, 255);
                    if (color != expected) return 4;
                    ++whiteGlyphs;
                }
                if (!whiteGlyphs) return 4;
                logF << "FIRMWARE GLYPH COLORS OK count=" << whiteGlyphs << std::endl;

                // --- 5.3: Expanded Smoke Tests ---
                // Test 1: Parser resilience test (verify graceful failure on invalid/corrupted payloads)
                {
                    FirmwarePackage corruptedPkg;
                    std::string corruptErr;
                    std::vector<uint8_t> garbage = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03 };
                    if (FirmwarePackage::Parse(garbage, corruptedPkg, corruptErr)) {
                        logF << "PARSER RESILIENCE TEST FAILED: accepted garbage" << std::endl;
                        return 10;
                    }
                    std::vector<uint8_t> truncated(studio.firmware.source_bytes.begin(), studio.firmware.source_bytes.begin() + 8);
                    if (FirmwarePackage::Parse(truncated, corruptedPkg, corruptErr)) {
                        logF << "PARSER RESILIENCE TEST FAILED: accepted truncated header" << std::endl;
                        return 11;
                    }
                    logF << "PARSER RESILIENCE TESTS OK" << std::endl;
                }

                // Test 2: Full 119-asset roundtrip test
                {
                    std::map<std::string, std::pair<std::vector<size_t>, std::vector<uint8_t>>> allReplacements;
                    for (size_t i = 0; i < studio.catalog.size(); ++i) {
                        const auto& spec = studio.catalog[i];
                        allReplacements[spec.id] = { studio.locations[i], studio.originals[i] };
                    }
                    std::vector<uint8_t> allPacked;
                    BuildReport allReport;
                    std::string repErr;
                    if (!studio.firmware.PackWith(allReplacements, {}, allPacked, allReport, repErr)) {
                        logF << "119 ASSETS ROUNDTRIP FAILED: " << repErr << std::endl;
                        return 12;
                    }
                    if (allPacked.size() != studio.firmware.source_bytes.size() || allReport.changed_assets != 119) {
                        logF << "119 ASSETS ROUNDTRIP FAILED: size or count mismatch" << std::endl;
                        return 13;
                    }
                    logF << "119 ASSETS ROUNDTRIP OK count=" << allReport.changed_assets << std::endl;
                }

                // Test 3: Unicode path I/O test
                {
                    std::string uniFilename = "test_unicode_\xd1\x8e\xd0\xbd\xd0\xb8\xd0\xba\xd0\xbe\xd0\xb4_" + PathUtils::FormatTimestampNow() + ".tmp";
                    std::string uniPath = PathUtils::WideToUtf8((fs::temp_directory_path() / PathUtils::ToFsPath(uniFilename)).wstring());
                    auto outUni = PathUtils::OpenWriteStream(uniPath);
                    if (!outUni.is_open()) {
                        logF << "UNICODE PATH WRITE FAILED: " << uniPath << std::endl;
                        return 14;
                    }
                    const char testSig[] = "ZWDF_UNICODE_TEST_OK";
                    outUni.write(testSig, sizeof(testSig));
                    outUni.close();

                    auto inUni = PathUtils::OpenReadStream(uniPath);
                    if (!inUni.is_open()) {
                        logF << "UNICODE PATH READ FAILED: " << uniPath << std::endl;
                        return 15;
                    }
                    char readBuf[sizeof(testSig)] = { 0 };
                    inUni.read(readBuf, sizeof(testSig));
                    inUni.close();
                    fs::remove(PathUtils::ToFsPath(uniPath));

                    if (memcmp(testSig, readBuf, sizeof(testSig)) != 0) {
                        logF << "UNICODE PATH VERIFY FAILED" << std::endl;
                        return 16;
                    }
                    logF << "UNICODE PATH IO TESTS OK" << std::endl;
                }

            }
        }
        }

        // Font Generator State
        logF << "Initializing FontGenState...\n" << std::flush;
        FontGenState fontState;
        fontState.Init();
        logF << "Found " << fontState.installedFonts.size() << " fonts.\n" << std::flush;

        logF << "Entering main loop...\n" << std::flush;

        // Main loop
        bool done = false;
        std::vector<std::string> pendingDroppedFiles;
        size_t pendingDroppedIndex = 0;
        int debugFrame = 0;
        while (!done) {
            if (debugFrame < 5) {
                logF << "Loop iteration " << debugFrame << " begin...\n" << std::flush;
            }
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT) {
                    done = true;
                }
            }
            if (done) break;
            UiResourceStudio::PollFileActions(studio, hwnd);
            if (g_CloseRequested) {
                g_CloseRequested = false;
                if (UiResourceStudio::RequestClose(studio, hwnd)) {
                    done = true;
                    break;
                }
            }

            // Handle Drag & Drop files
            if (UiResourceStudio::IsFileActionActive()) g_DroppedFiles.clear();
            for (auto& path : g_DroppedFiles) pendingDroppedFiles.push_back(std::move(path));
            g_DroppedFiles.clear();
            while (pendingDroppedIndex < pendingDroppedFiles.size() && !UiResourceStudio::IsFileActionActive()) {
                    const std::string dropPath = std::move(pendingDroppedFiles[pendingDroppedIndex++]);
                    if (fs::is_directory(PathUtils::ToFsPath(dropPath))) {
                        UiResourceStudio::ImportFolder(studio, hwnd, dropPath);
                        continue;
                    }

                    std::string ext = fs::path(PathUtils::ToFsPath(dropPath)).extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (ext == ".zwdf" || ext == ".swdf" || ext == ".xwdf" || ext == ".vfwp") {
                        UiResourceStudio::OpenFirmware(studio, hwnd, dropPath);
                    }
                    else if (ext == ".ttf" || ext == ".otf") {
                        if (fontState.fontRenderer.LoadFontFile(dropPath)) {
                            fontState.fontDisplayName = fs::path(PathUtils::ToFsPath(dropPath)).stem().string();
                            fontState.fontDirty = true;
                            char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgLoadedFontDragDrop), fontState.fontDisplayName.c_str());
                            studio.statusMsg = _m;
                            studio.statusIsErr = false;
                        } else {
                            char _m[512]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgFailedLoadFont), dropPath.c_str());
                            studio.statusMsg = _m;
                            studio.statusIsErr = true;
                        }
                    }
                    else if (ext == ".png" && studio.hasFirmware) {
                        std::string stem = fs::path(PathUtils::ToFsPath(dropPath)).stem().string();
                        if (stem == "boot_voopoo_animated_horizontal_136x22") stem = "boot_voopoo_animated";
                        int matchedIdx = studio.FindAssetIndex(stem);
                        if (matchedIdx < 0 && studio.selectedVisibleIdx >= 0 && studio.selectedVisibleIdx < (int)studio.visibleAssetIndices.size()) {
                            matchedIdx = (int)studio.visibleAssetIndices[studio.selectedVisibleIdx];
                        }
                        if (matchedIdx >= 0)
                            UiResourceStudio::ImportImage(studio, hwnd, dropPath, matchedIdx);
                    }
            }
            if (pendingDroppedIndex == pendingDroppedFiles.size()) {
                pendingDroppedFiles.clear();
                pendingDroppedIndex = 0;
            }

            // Define frame rendering lambda
            auto RenderFrame = [&]() {
                static bool s_inRender = false;
                if (s_inRender) return;
                s_inRender = true;

                // Handle window resize
                if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
                    CleanupRenderTarget();
                    g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
                    g_ResizeWidth = g_ResizeHeight = 0;
                    CreateRenderTarget();
                }

                // Start ImGui frame
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                if (ImGui::GetIO().DeltaTime <= 0.0f)
                    ImGui::GetIO().DeltaTime = 0.00001f;
                ImGui::NewFrame();

                // Fill full viewport
                ImGuiIO& curIo = ImGui::GetIO();
                WindowChrome::Render(hwnd);
                const float captionHeight = WindowChrome::Height();
                ImGui::SetNextWindowPos(ImVec2(0.0f, captionHeight));
                ImGui::SetNextWindowSize(ImVec2(curIo.DisplaySize.x, curIo.DisplaySize.y - captionHeight));
            ImGuiWindowFlags mainFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                         ImGuiWindowFlags_MenuBar;

            ImGui::Begin("MainWindow", nullptr, mainFlags);
            {
                ImGui::BeginDisabled(UiResourceStudio::IsFileActionActive());
                UiResourceStudio::RenderMenuBar(studio, hwnd);
                UiAppUpdate::RenderMenuBar(updates);
                UiSettings::RenderMenuBar(settings);
                if (studio.hasFirmware) {
                    ImGui::Text("%s %s", FirmwareModelName(studio.model), FirmwareEditionName(studio.edition));
                    ImGui::SameLine();
                }
                if (studio.hasFirmware) {
                    std::string shortHash = studio.firmware.source_sha256.substr(0, 10);
                    std::transform(shortHash.begin(), shortHash.end(), shortHash.begin(), ::toupper);
                    std::string fwName = PathUtils::WideToUtf8(PathUtils::ToFsPath(
                        studio.projectPath.empty() ? studio.firmwarePath : studio.projectPath).filename().wstring());
                    if (studio.isBuiltin && studio.projectPath.empty()) {
                        fwName += " (";
                        fwName += Tr(i18n::Str::LabelBuiltin);
                        fwName += ")";
                    }
                    char fBuf[256];
                    snprintf(fBuf, sizeof(fBuf), Tr(studio.projectPath.empty() ?
                        i18n::Str::BadgeFirmwareSummary : i18n::Str::BadgeProjectSummary),
                        fwName.c_str(),
                        shortHash.c_str(),
                        studio.firmware.records.size(),
                        studio.replacements.size() + studio.layoutOffsets.size() +
                        size_t(studio.screenPatches != studio.originalScreenPatches) +
                        size_t(studio.menuPatch != studio.originalMenuPatch));
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "%s", fBuf);
                    if (UiResourceStudio::HasUnsavedChanges(studio)) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", Tr(i18n::Str::LabelModified));
                    }
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%s", Tr(i18n::Str::BadgeNoFirmware));
                }


                ImGui::Separator();

                // Reserve footer height for pinned bottom status bar
                float footerHeight = ImGui::GetTextLineHeightWithSpacing() + 6.0f;
                float workspaceHeight = ImGui::GetContentRegionAvail().y - footerHeight;
                if (workspaceHeight < 150.0f) workspaceHeight = 150.0f;

                ImGui::BeginChild("MainWorkspace", ImVec2(0.0f, workspaceHeight), false, ImGuiWindowFlags_NoScrollbar);
                {

                    // Main Tab Bar
                    if (Ui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None)) {
                        // TAB 1: Resource Studio
                        UiResourceStudio::RenderTab(studio, hwnd, g_pd3dDevice, g_pd3dDeviceContext);

                        // TAB 2: Font Generator
                        UiFontGenerator::RenderTab(studio, fontState, hwnd, smokeTest, debugFrame);

                        UiFirmwareEmulator::RenderTab(emulator, studio, g_pd3dDevice, g_pd3dDeviceContext);
                        layout.activeTab = false;
                        patches.activeTab = false;
                        if (Ui::BeginTabItem(Tr(i18n::Str::TabFirmwarePatches))) {
                            if (Ui::BeginTabBar("PatchSections")) {
                                UiFirmwarePatches::RenderTab(patches, studio, g_pd3dDevice, g_pd3dDeviceContext);
                                UiFirmwareLayout::RenderTab(layout, studio, g_pd3dDevice, g_pd3dDeviceContext, patches);
                                UiFirmwareMenu::RenderTab(patches, studio, g_pd3dDevice, g_pd3dDeviceContext);
                                ImGui::EndTabBar();
                            }
                            ImGui::EndTabItem();
                        } else {
                            UiFirmwareLayout::Deactivate(layout, studio);
                        }
                        if (!patches.activeTab) patches.playback.Pause();
                        UiFirmwareComparison::RenderTab(comparison, studio, hwnd, g_pd3dDevice);
                        UiFirmwareInfo::RenderTab(studio);


                        ImGui::EndTabBar();
                    }
                }
                ImGui::EndChild();
                ImGui::EndDisabled();
                UiStartup::Render(studio, hwnd, g_pd3dDevice, g_pd3dDeviceContext);
                if (!UiResourceStudio::IsFileActionActive())
                    UiAppUpdate::RenderDialog(updates, studio, hwnd);
                if (!UiResourceStudio::IsFileActionActive())
                    UiSettings::RenderDialog(settings, studio);

                // Bottom Status Bar (ALWAYS PINNED TO BOTTOM!)
                ImGui::Separator();
                const float versionWidth = ImGui::CalcTextSize("v" STUDIO_VERSION).x;
                const float versionX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - versionWidth;
                const ImVec2 statusPos = ImGui::GetCursorScreenPos();
                ImGui::PushClipRect(statusPos,
                    ImVec2(statusPos.x + ImGui::GetContentRegionAvail().x - versionWidth -
                        ImGui::GetStyle().ItemSpacing.x, statusPos.y + ImGui::GetTextLineHeight()), true);
                ImVec4 statusCol = studio.statusIsErr ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f) : ImVec4(0.4f, 0.9f, 1.0f, 1.0f);
                ImGui::TextColored(statusCol, "%s", studio.statusMsg.c_str());
                ImGui::PopClipRect();
                ImGui::SameLine(versionX);
                ImGui::TextDisabled("v" STUDIO_VERSION);


                // Build Success Modal Dialog
                if (studio.showBuildModal) {
                    ImGui::OpenPopup("Build Report");
                    studio.showBuildModal = false;
                }

                if (ImGui::BeginPopupModal("Build Report", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "%s", Tr(i18n::Str::ModalBuildSuccessBanner));
                    ImGui::Separator();
                    ImGui::Text(Tr(i18n::Str::ModalSavedFile), studio.lastBuildPath.c_str());
                    ImGui::Text(Tr(i18n::Str::ModalOutputSize), studio.lastReport.output_size);
                    ImGui::Text(Tr(i18n::Str::ModalSha256), studio.lastReport.output_sha256.c_str());
                    ImGui::Text(Tr(i18n::Str::ModalModBitmaps), studio.lastReport.changed_assets);
                    ImGui::Text(Tr(i18n::Str::ModalModColorGroups), studio.lastReport.changed_colors);
                    ImGui::Text("%s: %zu", Tr(i18n::Str::LayoutChangedElements), studio.lastReport.changed_layout_elements);
                    ImGui::Text(Tr(i18n::Str::ModalPatchedRanges), studio.lastReport.patched_ranges, studio.lastReport.records);
                    ImGui::Text(Tr(i18n::Str::ModalUntouchedBytes), studio.lastReport.unchanged_payload_bytes_verified);
                    ImGui::Separator();
                    ImGui::TextDisabled("%s", Tr(i18n::Str::ModalReportNote));

                    if (ImGui::Button(Tr(i18n::Str::BtnOk), ImVec2(120, 0))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(Tr(i18n::Str::BtnOpenInExplorer), ImVec2(140, 0))) {
                        const std::wstring wDir = PathUtils::ToFsPath(studio.lastBuildPath).parent_path().wstring();
                        ShellExecuteW(nullptr, L"open", wDir.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
                UiResourceStudio::RenderFileProgress(studio);
            }
            ImGui::End();

            // Global Keyboard Shortcuts
            if (!UiResourceStudio::IsFileActionActive() && Ui::DocumentKeyboardAvailable()) {
                ImGuiIO& curIo = ImGui::GetIO();
                bool ctrl = curIo.KeyCtrl;
                bool shift = curIo.KeyShift;

                // Ctrl + Z: Undo
                if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z)) {
                    if (patches.activeTab && UiFirmwareRisk::Accepted() && UiFirmwarePatches::UndoDraft(patches)) {
                        // Draft edits take precedence over applied document history.
                    } else if (studio.CanUndo()) {
                        std::string desc = studio.undoStack.back().desc;
                        studio.Undo();
                        char buf[256];
                        snprintf(buf, sizeof(buf), Tr(i18n::Str::MsgUndoPerformed), desc.c_str());
                        studio.statusMsg = buf;
                        studio.statusIsErr = false;
                    }
                }
                // Ctrl + Y or Ctrl + Shift + Z: Redo
                else if ((ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) || (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_Z))) {
                    if (studio.CanRedo()) {
                        std::string desc = studio.redoStack.back().desc;
                        studio.Redo();
                        char buf[256];
                        snprintf(buf, sizeof(buf), Tr(i18n::Str::MsgRedoPerformed), desc.c_str());
                        studio.statusMsg = buf;
                        studio.statusIsErr = false;
                    }
                }
                // Copy the complete patch; paste opens the transfer chooser.
                else if (patches.activeTab && UiFirmwareRisk::Accepted() && ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_C)) {
                    UiFirmwarePatches::CopyToClipboard(patches, hwnd);
                }
                else if (patches.activeTab && UiFirmwareRisk::Accepted() && ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_V)) {
                    patches.requestPaste = true;
                }
                // Ctrl + C: Copy selected asset to clipboard
                else if (UiResourceStudio::IsActive() && ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_C)) {
                    CopyCurrentSelectedAsset(studio, hwnd);
                }
                // Ctrl + V: Paste from clipboard into selected asset
                else if (UiResourceStudio::IsActive() && ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_V)) {
                    PasteIntoCurrentSelectedAsset(studio, hwnd);
                }
                // Ctrl + O: Open firmware
                else if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_O)) {
                    UiResourceStudio::OpenFirmware(studio, hwnd);
                }
                // Ctrl+S saves the project; Ctrl+Shift+S chooses a new path.
                else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
                    UiResourceStudio::SaveProject(studio, hwnd, shift);
                }
                // Up / Down arrow navigation across visible assets
                else if (UiResourceStudio::IsActive() && !ctrl && !shift &&
                    ImGui::IsKeyPressed(ImGuiKey_UpArrow, ImGuiInputFlags_Repeat, ImGuiKeyOwner_NoOwner)) {
                    if (!studio.visibleAssetIndices.empty() && studio.selectedVisibleIdx > 0) {
                        studio.selectedVisibleIdx--;
                        size_t catIdx = studio.visibleAssetIndices[studio.selectedVisibleIdx];
                        const auto& spec = studio.catalog[catIdx];
                        Rgb8 c = studio.GetForegroundForAsset(spec);
                        strncpy_s(studio.assetHexEditBuf, c.ToHex().c_str(), sizeof(studio.assetHexEditBuf));
                    }
                }
                else if (UiResourceStudio::IsActive() && !ctrl && !shift &&
                    ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_Repeat, ImGuiKeyOwner_NoOwner)) {
                    if (!studio.visibleAssetIndices.empty() && studio.selectedVisibleIdx + 1 < (int)studio.visibleAssetIndices.size()) {
                        studio.selectedVisibleIdx++;
                        size_t catIdx = studio.visibleAssetIndices[studio.selectedVisibleIdx];
                        const auto& spec = studio.catalog[catIdx];
                        Rgb8 c = studio.GetForegroundForAsset(spec);
                        strncpy_s(studio.assetHexEditBuf, c.ToHex().c_str(), sizeof(studio.assetHexEditBuf));
                    }
                }
            }

            // Direct3D 11 Rendering
            ImGui::Render();
            if (g_mainRenderTargetView) {
                const ImVec4 background = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
                const float clear_color[4] = { background.x, background.y, background.z, 1.0f };
                g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
                g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            }

            g_pSwapChain->Present(g_InSizeMove ? 0 : 1, 0);
            if (debugFrame < 5) {
                logF << "Frame " << debugFrame << " presented successfully.\n" << std::flush;
            }
            debugFrame++;
            if (smokeTest && debugFrame >= 5) done = true;

            s_inRender = false;
        };

        g_RenderCallback = RenderFrame;
        RenderFrame();
    }

    g_RenderCallback = nullptr;

        // Cleanup
        UiStartup::Release();
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);

        logF << "Application exited normally.\n" << std::flush;
        return 0;
    }
    catch (const std::exception& e) {
        logF << "FATAL EXCEPTION: " << e.what() << "\n" << std::flush;
        if (dynamic_cast<const std::bad_alloc*>(&e)) {
            PROCESS_MEMORY_COUNTERS_EX process{};
            MEMORYSTATUSEX memory{sizeof(MEMORYSTATUSEX)};
            const bool measured = GetProcessMemoryInfo(GetCurrentProcess(),
                reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process), sizeof(process)) && GlobalMemoryStatusEx(&memory);
            logF << "ALLOCATION MEMORY measured=" << measured << " private=" << process.PrivateUsage
                << " peak=" << process.PeakPagefileUsage << " availableCommit=" << memory.ullAvailPageFile
                << " availableVirtual=" << memory.ullAvailVirtual << '\n' << std::flush;
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            const bool jobMeasured = QueryInformationJobObject(nullptr, JobObjectExtendedLimitInformation,
                &limits, sizeof(limits), nullptr);
            logF << "ALLOCATION JOB measured=" << jobMeasured << " flags=" << limits.BasicLimitInformation.LimitFlags
                << " processMemoryLimit=" << limits.ProcessMemoryLimit << " jobMemoryLimit=" << limits.JobMemoryLimit
                << '\n' << std::flush;
        }
        if (!coverageTest)
            MessageBoxA(nullptr, e.what(), "Voopoo Firmware Studio - Fatal Error", MB_ICONERROR);
        return 1;
    }
    catch (...) {
        logF << "FATAL UNKNOWN EXCEPTION\n" << std::flush;
        if (!coverageTest)
            MessageBoxA(nullptr, "An unexpected crash occurred.", "Voopoo Firmware Studio - Error", MB_ICONERROR);
        return 1;
    }
}

// -------------------------------------------------------------
// DirectX 11 Helper Functions
// -------------------------------------------------------------
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    WindowChrome::ReleaseGraphics();
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (SUCCEEDED(hr) && pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    }
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LRESULT chromeResult = 0;
    if (WindowChrome::HandleMessage(hWnd, msg, wParam, lParam, chromeResult))
        return chromeResult;
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_CLOSE:
        // Defer prompting until outside WndProc and any active ImGui frame.
        g_CloseRequested = true;
        return 0;
    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i) {
            const UINT length = DragQueryFileW(hDrop, i, nullptr, 0);
            std::wstring path(static_cast<size_t>(length) + 1, L'\0');
            if (DragQueryFileW(hDrop, i, path.data(), length + 1)) {
                path.resize(length);
                g_DroppedFiles.push_back(PathUtils::WideToUtf8(path));
            }
        }
        DragFinish(hDrop);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_ENTERSIZEMOVE:
        g_InSizeMove = true;
        SetTimer(hWnd, 1, 16, nullptr);
        return 0;
    case WM_TIMER:
        if (wParam == 1 && g_RenderCallback) {
            g_RenderCallback();
            return 0;
        }
        break;
    case WM_EXITSIZEMOVE:
        g_InSizeMove = false;
        KillTimer(hWnd, 1);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
        const UINT dpi = (std::max)(96u, GetDpiForWindow(hWnd));
        RECT minimum{0, 0, MulDiv(1000, dpi, 96), MulDiv(700, dpi, 96) + static_cast<LONG>(WindowChrome::Height())};
        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hWnd, GWL_STYLE));
        AdjustWindowRectExForDpi(&minimum, style, FALSE, 0, dpi);
        limits->ptMinTrackSize.x = minimum.right - minimum.left;
        limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
        return 0;
    }
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        if (g_pSwapChain && g_RenderCallback) {
            g_RenderCallback();
        }
        return 0;
    case WM_PAINT:
        if (g_RenderCallback) {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            g_RenderCallback();
            EndPaint(hWnd, &ps);
            return 0;
        }
        break;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU && lParam != L' ')
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
