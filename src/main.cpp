#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx11.h"

#include <d3d11.h>
#include <tchar.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
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
#include "../tests/editor_regressions.h"
#include "../tests/firmware_color_audit.h"
#include "../tests/history_regressions.h"
#include "../tests/save_regressions.h"
#include "ui_firmware_info.h"

#include "ui_font_generator.h"


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
    const bool smokeTest = std::wstring(commandLine).find(L"--smoke-test") != std::wstring::npos;
    const bool enableDebugLog = smokeTest || std::wstring(commandLine).find(L"--debug") != std::wstring::npos;
    SetUnhandledExceptionFilter(GlobalCrashHandler);
    i18n::InitI18n();
    std::ofstream logF;
    if (enableDebugLog) {
        logF.open("studio_debug.log");
    }
    logF << "Starting Voopoo DRAG S Studio...\n" << std::flush;

    try {
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
            LoadIcon(nullptr, IDI_APPLICATION),
            LoadCursor(nullptr, IDC_ARROW),
            nullptr,
            nullptr,
            L"VoopooAllInOneStudioClass",
            nullptr
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
            L"Voopoo DRAG S - All-in-One Firmware & Resource Studio (Dear ImGui C++)",
            WS_OVERLAPPEDWINDOW,
            posX, posY, winW, winH,
            nullptr, nullptr, wc.hInstance, nullptr
        );

        if (!hwnd) {
            logF << "CreateWindowW failed! Error=" << GetLastError() << "\n" << std::flush;
            MessageBoxW(nullptr, L"CreateWindowW failed!", L"Error", MB_ICONERROR);
            return 1;
        }
        logF << "CreateWindowW succeeded. HWND=" << (void*)hwnd << "\n" << std::flush;

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

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.FrameRounding = 4.0f;
        style.ItemSpacing = ImVec2(8.0f, 6.0f);

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
        logF << "ImGui initialized.\n" << std::flush;

        // -------------------------------------------------------------
        // App State Initialization
        // -------------------------------------------------------------
        logF << "Initializing StudioState...\n" << std::flush;
        StudioState studio;
        logF << "StudioState created.\n" << std::flush;
        studio.statusMsg = Tr(i18n::Str::MsgReadyOpenFw);


        FirmwarePackage base;
        std::string baseError;
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
                    const char* role = PaletteIdForAsset(spec, studio.previewRank, studio.previewCyanMode);
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

        // Font Generator State
        logF << "Initializing FontGenState...\n" << std::flush;
        FontGenState fontState;
        fontState.Init();
        logF << "Found " << fontState.popularFonts.size() << " fonts.\n" << std::flush;

        logF << "Entering main loop...\n" << std::flush;

        // Main loop
        bool done = false;
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
            if (g_CloseRequested) {
                g_CloseRequested = false;
                if (UiResourceStudio::ConfirmUnsavedChanges(studio, hwnd)) {
                    done = true;
                    break;
                }
            }

            // Handle Drag & Drop files
            if (!g_DroppedFiles.empty()) {
                const auto droppedFiles = std::move(g_DroppedFiles);
                g_DroppedFiles.clear();
                for (const auto& dropPath : droppedFiles) {
                    if (fs::is_directory(PathUtils::ToFsPath(dropPath))) {
                        studio.PushUndo(std::string(Tr(i18n::Str::BtnImportFolder)) + ": " + dropPath);
                        int imported = 0;
                        for (const auto& entry : fs::directory_iterator(PathUtils::ToFsPath(dropPath))) {
                            if (!entry.is_regular_file()) continue;
                            std::string p = PathUtils::WideToUtf8(entry.path().wstring());
                            std::string ext = entry.path().extension().string();
                            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                            if (ext != ".png") continue;
                            std::string stem = entry.path().stem().string();
                            if (stem == "boot_voopoo_animated_horizontal_136x22") stem = "boot_voopoo_animated";
                            int matchedIdx = studio.FindAssetIndex(stem);
                            if (matchedIdx >= 0) {
                                const auto& spec = studio.catalog[matchedIdx];
                                if (spec.format == AssetFormat::Rgb565) {
                                    std::vector<uint32_t> rgba;
                                    std::string err;
                                    if (FirmwareBitmap::LoadAndFitRgb565(p, spec.width, spec.height, rgba, err)) {
                                        std::vector<uint8_t> newRaw;
                                        if (FirmwareBitmap::EncodeRgb565(rgba.data(), spec.width, spec.height, newRaw)) {
                                            if (newRaw != studio.originals[matchedIdx]) imported++;
                                            studio.SetBitmapReplacement(matchedIdx, std::move(newRaw));
                                        }
                                    }
                                } else {
                                    std::vector<uint8_t> gray;
                                    std::string err;
                                    if (FirmwareBitmap::LoadAndFit(p, spec.width, spec.height, gray, err)) {
                                        std::vector<uint8_t> newRaw;
                                        if (FirmwareBitmap::EncodeMaskPreserving(gray.data(), spec.width, spec.height, studio.originals[matchedIdx].data(), spec.byte_len(), newRaw)) {
                                            if (newRaw != studio.originals[matchedIdx]) imported++;
                                            studio.SetBitmapReplacement(matchedIdx, std::move(newRaw));
                                        }
                                    }
                                }
                            }
                        }
                        char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgImportedMatchingPngs), imported);
                        studio.statusMsg = _m;
                        studio.statusIsErr = false;
                        continue;
                    }

                    std::string ext = fs::path(PathUtils::ToFsPath(dropPath)).extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (ext == ".zwdf" || ext == ".swdf") {
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
                        if (matchedIdx >= 0) {
                            const auto& spec = studio.catalog[matchedIdx];
                            studio.PushUndo("Drop PNG " + spec.id);
                            if (spec.format == AssetFormat::Rgb565) {
                                std::vector<uint32_t> rgba;
                                std::string err;
                                if (FirmwareBitmap::LoadAndFitRgb565(dropPath, spec.width, spec.height, rgba, err)) {
                                    std::vector<uint8_t> newRaw;
                                    if (FirmwareBitmap::EncodeRgb565(rgba.data(), spec.width, spec.height, newRaw)) {
                                        if (newRaw == studio.originals[matchedIdx]) {
                                            char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgPngMatchesOriginal), spec.id.c_str());
                                            studio.statusMsg = _m;
                                        } else {
                                            char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgStagedReplacement), spec.id.c_str());
                                            studio.statusMsg = _m;
                                        }
                                        studio.SetBitmapReplacement(matchedIdx, std::move(newRaw));
                                        studio.statusIsErr = false;
                                    }
                                } else {
                                    char _m[512]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgPngLoadError), err.c_str());
                                    studio.statusMsg = _m;
                                    studio.statusIsErr = true;
                                }
                            } else {
                                std::vector<uint8_t> gray;
                                std::string err;
                                if (FirmwareBitmap::LoadAndFit(dropPath, spec.width, spec.height, gray, err)) {
                                    std::vector<uint8_t> newRaw;
                                    if (FirmwareBitmap::EncodeMaskPreserving(gray.data(), spec.width, spec.height, studio.originals[matchedIdx].data(), spec.byte_len(), newRaw)) {
                                        if (newRaw == studio.originals[matchedIdx]) {
                                            char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgPngMatchesOriginal), spec.id.c_str());
                                            studio.statusMsg = _m;
                                        } else {
                                            char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgStagedReplacement), spec.id.c_str());
                                            studio.statusMsg = _m;
                                        }
                                        studio.SetBitmapReplacement(matchedIdx, std::move(newRaw));
                                        studio.statusIsErr = false;
                                    }
                                } else {
                                    char _m[512]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgPngLoadError), err.c_str());
                                    studio.statusMsg = _m;
                                    studio.statusIsErr = true;
                                }
                            }
                        }
                    }
                }
            }

            // Define frame rendering lambda
            auto RenderFrame = [&]() {
                static bool s_inRender = false;
                if (s_inRender || UiResourceStudio::IsFileActionActive()) return;
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
                ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
                ImGui::SetNextWindowSize(curIo.DisplaySize);
            ImGuiWindowFlags mainFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

            ImGui::Begin("MainWindow", nullptr, mainFlags);
            {
                // Top App Bar: Title and Status
                ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "%s", Tr(i18n::Str::AppTitle));
                ImGui::SameLine();

                if (studio.hasFirmware) {
                    std::string shortHash = studio.firmware.source_sha256.substr(0, 10);
                    std::transform(shortHash.begin(), shortHash.end(), shortHash.begin(), ::toupper);
                    std::string fwName = PathUtils::WideToUtf8(PathUtils::ToFsPath(studio.firmwarePath).filename().wstring());
                    if (studio.isBuiltin) {
                        fwName += " (";
                        fwName += Tr(i18n::Str::LabelBuiltin);
                        fwName += ")";
                    }
                    char fBuf[256];
                    snprintf(fBuf, sizeof(fBuf), Tr(i18n::Str::BadgeFirmwareSummary),
                        fwName.c_str(),
                        shortHash.c_str(),
                        studio.firmware.records.size(),
                        studio.replacements.size());
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "%s", fBuf);
                    if (studio.HasUnsavedChanges()) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", Tr(i18n::Str::LabelModified));
                    }
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%s", Tr(i18n::Str::BadgeNoFirmware));
                }


                ImGui::Separator();

                // Reserve footer height for pinned bottom status bar
                float footerHeight = ImGui::GetFrameHeightWithSpacing() + 6.0f;
                float workspaceHeight = ImGui::GetContentRegionAvail().y - footerHeight;
                if (workspaceHeight < 150.0f) workspaceHeight = 150.0f;

                ImGui::BeginChild("MainWorkspace", ImVec2(0.0f, workspaceHeight), false, ImGuiWindowFlags_NoScrollbar);
                {

                    // Main Tab Bar
                    if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None)) {
                        // TAB 1: Resource Studio
                        UiResourceStudio::RenderTab(studio, hwnd, g_pd3dDevice, g_pd3dDeviceContext);

                        // TAB 2: Font Generator
                        UiFontGenerator::RenderTab(studio, fontState, hwnd, smokeTest, debugFrame);

                        // TAB 3: Firmware Info
                        UiFirmwareInfo::RenderTab(studio);


                        ImGui::EndTabBar();
                    }
                }
                ImGui::EndChild();

                // Bottom Status Bar (ALWAYS PINNED TO BOTTOM!)
                ImGui::Separator();
                ImVec4 statusCol = studio.statusIsErr ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f) : ImVec4(0.4f, 0.9f, 1.0f, 1.0f);
                ImGui::TextColored(statusCol, "%s", studio.statusMsg.c_str());

                // Language Selector Dropdown (Bottom Right)
                {
                    const auto& langs = i18n::GetSupportedLanguages();
                    const auto& curLang = i18n::GetCurrentLanguageInfo();
                    ImGui::SameLine(ImGui::GetWindowWidth() - 170.0f);
                    ImGui::SetNextItemWidth(150.0f);
                    if (ImGui::BeginCombo("##LangSelect", curLang.displayName, ImGuiComboFlags_NoArrowButton)) {
                        for (const auto& li : langs) {
                            bool selected = (li.lang == i18n::GetCurrentLanguage());
                            if (ImGui::Selectable(li.displayName, selected)) {
                                i18n::SetLanguage(li.lang);
                                i18n::SaveSettings();
                            }
                            if (selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }

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
            }
            ImGui::End();

            // Global Keyboard Shortcuts
            if (!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
                ImGuiIO& curIo = ImGui::GetIO();
                bool ctrl = curIo.KeyCtrl;
                bool shift = curIo.KeyShift;

                // Ctrl + Z: Undo
                if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z)) {
                    if (studio.CanUndo()) {
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
                // Ctrl + C: Copy selected asset to clipboard
                else if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_C)) {
                    CopyCurrentSelectedAsset(studio, hwnd);
                }
                // Ctrl + V: Paste from clipboard into selected asset
                else if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_V)) {
                    PasteIntoCurrentSelectedAsset(studio, hwnd);
                }
                // Ctrl + O: Open firmware
                else if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_O)) {
                    UiResourceStudio::OpenFirmware(studio, hwnd);
                }
                // Ctrl + S: Build firmware
                else if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_S)) {
                    UiResourceStudio::SaveFirmware(studio, hwnd);
                }
                // Up / Down arrow navigation across visible assets
                else if (!ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                    if (!studio.visibleAssetIndices.empty() && studio.selectedVisibleIdx > 0) {
                        studio.selectedVisibleIdx--;
                        size_t catIdx = studio.visibleAssetIndices[studio.selectedVisibleIdx];
                        const auto& spec = studio.catalog[catIdx];
                        Rgb8 c = studio.GetForegroundForAsset(spec);
                        strncpy_s(studio.assetHexEditBuf, c.ToHex().c_str(), sizeof(studio.assetHexEditBuf));
                    }
                }
                else if (!ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
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
                const float clear_color[4] = { 0.10f, 0.10f, 0.12f, 1.00f };
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
        MessageBoxA(nullptr, e.what(), "Voopoo DRAG S Studio - Fatal Error", MB_ICONERROR);
        return 1;
    }
    catch (...) {
        logF << "FATAL UNKNOWN EXCEPTION\n" << std::flush;
        MessageBoxA(nullptr, "An unexpected crash occurred.", "Voopoo DRAG S Studio - Error", MB_ICONERROR);
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
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
