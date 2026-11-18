#pragma once

#include "ui_resource_studio.h"
#include "ui_firmware_patches.h"
#include "ui_markdown.h"
#include "firmware_project.h"
#include "firmware_save.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui_capture.h"
#include <algorithm>
#include <commctrl.h>
#include <chrono>
#include <cstring>
#include <cwchar>
#include <ostream>
#include <thread>

namespace AsyncFileRegression {
// Exercise the real native unsaved/error dialogs with automated button input.
inline thread_local int responseButton = IDOK;
inline void CALLBACK RespondToDialog(HWND, UINT, UINT_PTR, DWORD) {
    EnumThreadWindows(GetCurrentThreadId(), [](HWND window, LPARAM button) -> BOOL {
        wchar_t name[32]{};
        GetClassNameW(window, name, 32);
        if (std::wcscmp(name, L"#32770") == 0) {
            SendMessageW(window, TDM_CLICK_BUTTON, static_cast<WPARAM>(button), 0);
            if (IsWindow(window)) {
                const HWND control = GetDlgItem(window, static_cast<int>(button));
                if (control) PostMessageW(window, WM_COMMAND, MAKEWPARAM(button, BN_CLICKED), reinterpret_cast<LPARAM>(control));
            }
        }
        return TRUE;
    }, responseButton);
}
struct DialogResponse {
    int previous = responseButton;
    UINT_PTR timer = 0;
    explicit DialogResponse(int button) {
        responseButton = button;
        timer = SetTimer(nullptr, 0, 10, RespondToDialog);
    }
    ~DialogResponse() {
        if (timer) KillTimer(nullptr, timer);
        responseButton = previous;
    }
};
} // namespace AsyncFileRegression

inline bool RunAsyncFileRegressions(std::string& error, std::ostream* log = nullptr, bool exportReviewOnly = false) {
    using namespace AsyncFileRegression;
    error.clear();
    // Native dialog checks must never put windows on the user's input desktop.
    struct PrivateDesktop {
        HDESK original = GetThreadDesktop(GetCurrentThreadId());
        HDESK isolated = nullptr;
        ~PrivateDesktop() {
            if (isolated) { SetThreadDesktop(original); CloseDesktop(isolated); }
        }
    } desktop;
    const auto desktopName = L"Voopoo_file_tests_" + std::to_wstring(GetCurrentProcessId());
    desktop.isolated = CreateDesktopW(desktopName.c_str(), nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    if (!desktop.isolated || !SetThreadDesktop(desktop.isolated)) {
        error = "Cannot isolate native file-dialog regressions from the user desktop";
        return false;
    }
    auto check = [&](bool ok, const char* reason) {
        if (!ok) { error += reason; error += "; "; }
        return ok;
    };
    std::error_code ec;
    auto root = std::filesystem::temp_directory_path(ec);
    if (ec) { error = "async-file temporary directory unavailable"; return false; }
    root /= L"Voopoo_async_files_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64());
    if (!std::filesystem::create_directory(root, ec)) { error = "async-file temporary directory creation failed"; return false; }
    struct RemoveFiles {
        std::filesystem::path path;
        ~RemoveFiles() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
    } removeFiles{root};
    auto utf8 = [](const std::filesystem::path& path) { return PathUtils::WideToUtf8(path.wstring()); };

    auto* previous = ImGui::GetCurrentContext();
    auto* context = ImGui::CreateContext();
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> gpu;
    struct RestoreContext {
        ImGuiContext* context;
        ImGuiContext* previous;
        bool renderer = false;
        ~RestoreContext() {
            if (renderer) ImGui_ImplDX11_Shutdown();
            ImGui::DestroyContext(context);
            ImGui::SetCurrentContext(previous);
        }
    } restoreContext{context, previous};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, gpu.GetAddressOf()))) {
        error = "Async file UI could not create offscreen WARP device"; return false;
    }
    if (!ImGui_ImplDX11_Init(device.Get(), gpu.Get())) {
        error = "Async file UI renderer initialization failed"; return false;
    }
    restoreContext.renderer = true;
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigInputTrickleEventQueue = false;
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 16, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    UiMarkdown::InitializeFonts(io.Fonts->GetGlyphRangesCyrillic());
    char capturePath[MAX_PATH]{};
    const DWORD captureLength = GetEnvironmentVariableA("VFS_FILE_UI_CAPTURE", capturePath, MAX_PATH);
    if (!check(captureLength < MAX_PATH, "file UI capture path exceeds MAX_PATH")) return false;
    bool captured = false;
    std::string renderedText;
    StudioState studio;
    StudioState* renderedDocument = &studio;
    auto frame = [&] {
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Async file regressions", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::LogToBuffer();
        UiResourceStudio::RenderFileProgress(*renderedDocument);
        renderedText = context->LogBuffer.c_str();
        ImGui::LogFinish();
        ImGui::End();
        ImGui::Render();
    };
    auto hasStage = [](const std::vector<std::string>& stages, const char* text) {
        return std::any_of(stages.begin(), stages.end(), [&](const auto& stage) {
            return stage.find(text) != std::string::npos;
        });
    };
    auto ordered = [&](const std::vector<std::string>& stages, std::initializer_list<const char*> expected) {
        auto next = stages.begin();
        for (const auto* text : expected) {
            next = std::find_if(next, stages.end(), [&](const auto& stage) {
                return stage.find(text) != std::string::npos;
            });
            if (next == stages.end()) return false;
            ++next;
        }
        return true;
    };
    UiResourceStudio::FileProgressSnapshot lastProgress;
    size_t operation = exportReviewOnly ? 100 : 0;
    auto drain = [&](StudioState& studio) {
        renderedDocument = &studio;
        ++operation;
        // Exercise the first countdown at 60 Hz; advance later review periods
        // with a synthetic clock instead of waiting five wall-clock seconds.
        io.DeltaTime = operation == 1 ? 1.0f / 60.0f : 0.25f;
        lastProgress = {};
        if (log) *log << "ASYNC FILE OPERATION " << operation << " started\n" << std::flush;
        DialogResponse dismissErrors(IDOK);
        if (!check(dismissErrors.timer != 0, "cannot automate error dialog input")) return false;
        const auto started = std::chrono::steady_clock::now();
        size_t frames = 0;
        size_t modalFrames = 0;
        double finishedAt = -1;
        bool dismissalChecked = false;
        bool sawFadeIn = false;
        const std::string countdownFormat = Tr(i18n::Str::FileTaskClosing);
        const auto countdownPrefix = countdownFormat.substr(0, countdownFormat.find('%'));
        while (UiResourceStudio::IsFileActionActive()) {
            UiResourceStudio::FirmwareReviewSnapshot review;
            if (UiResourceStudio::GetFirmwareReview(review)) {
                if (!check(review.prepared->Valid() && UiResourceStudio::ConfirmFirmwareExport(studio),
                    "queued firmware review was invalid or explicit confirmation failed")) return false;
            }
            UiResourceStudio::FileProgressSnapshot progress;
            const bool hasProgress = UiResourceStudio::GetFileProgress(progress);
            if (hasProgress) {
                if (progress.title == lastProgress.title && progress.filename == lastProgress.filename &&
                    lastProgress.log.size() <= progress.log.size()) {
                    check(std::equal(lastProgress.log.begin(), lastProgress.log.end(), progress.log.begin()),
                        "live job log snapshot changed an already-published entry");
                }
                lastProgress = std::move(progress);
                for (const auto& entry : lastProgress.log)
                    if (!check(entry.size() > 15 && entry[0] == '[' && entry[3] == ':' &&
                        entry[6] == ':' && entry[9] == '.' && entry[13] == ']' && entry[14] == ' ',
                        "log entry omitted its wall-clock millisecond timestamp")) return false;
            }
            frame();
            const auto* modal = ImGui::FindWindowByName("###FileOperationProgress");
            const bool visible = modal && modal->Active && !modal->Hidden;
            if (hasProgress && visible) {
                ++modalFrames;
                check(renderedText.find(Tr(lastProgress.titleLabel)) != std::string::npos &&
                      !(modal->Flags & ImGuiWindowFlags_NoTitleBar),
                      "modal omitted its localized title bar");
                if (context->DimBgRatio > 0 && context->DimBgRatio < 1) sawFadeIn = true;
                check(!lastProgress.log.empty() && renderedText.find(lastProgress.log.front()) != std::string::npos &&
                      renderedText.find("Show operation log") == std::string::npos,
                      "modal omitted its live operation log");
                if (!captured && lastProgress.title == "Save project" &&
                    hasStage(lastProgress.log, "Compress project body")) {
                    const auto path = captureLength ? std::filesystem::path(capturePath) : root / L"file-progress.png";
                    if (!CaptureUiDrawData(device.Get(), gpu.Get(), path.string().c_str(), error)) return false;
                    captured = true;
                }
            }
            if (visible && renderedText.find(countdownPrefix) != std::string::npos && finishedAt < 0) {
                finishedAt = ImGui::GetTime();
                if (operation == 1) {
                    const auto base = captureLength ? std::filesystem::path(capturePath) : root / L"file-progress.png";
                    const auto completedCapture = base.parent_path() /
                        (base.stem().wstring() + L"-completed.png");
                    if (!CaptureUiDrawData(device.Get(), gpu.Get(), completedCapture.string().c_str(), error)) return false;
                }
                if (operation == 3 || operation == 4) {
                    io.DeltaTime = 1.0f / 60.0f;
                    auto click = [&](ImVec2 position) {
                        io.AddMousePosEvent(position.x, position.y);
                        io.AddMouseButtonEvent(0, true); frame();
                        io.AddMouseButtonEvent(0, false); frame();
                    };
                    if (operation == 3) {
                        click(ImVec2(modal->Pos.x + 20, modal->Pos.y + 12));
                        io.DeltaTime = 6.0f; frame(); io.DeltaTime = 1.0f / 60.0f;
                        UiResourceStudio::FileProgressSnapshot retained;
                        if (!check(UiResourceStudio::GetFileProgress(retained) &&
                            UiResourceStudio::IsFileActionActive() &&
                            renderedText.find(countdownPrefix) == std::string::npos,
                            "clicking the log did not stop its completion timer")) return false;
                        ImGuiWindow* logEditor = nullptr;
                        for (auto* window : context->Windows)
                            if (window->Active && std::strstr(window->Name, "##FileOperationLog")) logEditor = window;
                        if (!check(logEditor != nullptr, "selectable multiline log is missing")) return false;
                        click(ImVec2(logEditor->Pos.x + 20, logEditor->Pos.y + 15));
                        io.AddKeyEvent(ImGuiMod_Ctrl, true);
                        io.AddKeyEvent(ImGuiKey_A, true); frame();
                        io.AddKeyEvent(ImGuiKey_A, false); frame();
                        io.AddKeyEvent(ImGuiKey_C, true); frame();
                        io.AddKeyEvent(ImGuiKey_C, false);
                        io.AddKeyEvent(ImGuiMod_Ctrl, false); frame();
                        std::string expectedLog;
                        for (const auto& entry : retained.log) expectedLog += entry + '\n';
                        if (!check(std::string(ImGui::GetClipboardText()) == expectedLog,
                            "select-all and Ctrl+C did not copy the timestamped log")) return false;
                        click(ImVec2(modal->Pos.x + modal->Size.x / 2,
                            modal->Pos.y + modal->Size.y - ImGui::GetStyle().WindowPadding.y -
                            ImGui::GetFrameHeight() / 2));
                    } else {
                        click(ImVec2(2, 2));
                    }
                    // Buttons request closing on release; inspect a later frame,
                    // not the instant at which the transition starts.
                    io.DeltaTime = 0.06f; frame(); io.DeltaTime = 1.0f / 60.0f;
                    if (!check(UiResourceStudio::IsFileActionActive() &&
                        context->DimBgRatio > 0 && context->DimBgRatio < 1,
                        "completion dialog background did not fade out with the window")) return false;
                    io.DeltaTime = 0.2f; frame(); io.DeltaTime = 1.0f / 60.0f;
                    frame();
                    if (!check(context->DimBgRatio == 0, "modal left a dim background after closing")) return false;
                    if (!check(!UiResourceStudio::IsFileActionActive(),
                        operation == 3 ? "pinned log did not close manually" :
                        "outside click did not close the completed log")) return false;
                    dismissalChecked = true;
                }
            }
            if (!hasProgress || (lastProgress.done && visible)) UiResourceStudio::PollFileActions(studio, nullptr);
            ++frames;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (log) *log << "ASYNC FILE OPERATION " << operation << " completed in " <<
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count()
            << " ms; frames=" << frames << "; error=" << studio.statusIsErr << '\n' << std::flush;
        if (operation == 1 && !check(sawFadeIn, "modal background did not fade in")) return false;
        if (operation == 1 && !check(finishedAt >= 0 && ImGui::GetTime() - finishedAt >= 3.0,
            "completed log closed before its three-second review period")) return false;
        if ((operation == 3 || operation == 4) && !check(dismissalChecked,
            "completion interaction scenario did not run")) return false;
        return check(frames > 0 && modalFrames > 0, "queued action did not render a visible modal during UI polling") &&
            check(std::chrono::steady_clock::now() - started < std::chrono::seconds(60),
                  "async operation exceeded the regression deadline") && error.empty();
    };

    FirmwarePackage package;
    // studio was declared before the frame closure so the modal can validate it.
    if (!LoadEmbeddedFirmware(package, error) ||
        !studio.LoadPackage(std::move(package), "original-source.zwdf", error)) return false;
    studio.isBuiltin = true;
    studio.EnableProjectDocument();
    if (!check(studio.catalog.size() >= 2, "async fixture has too few assets")) return false;
    if (exportReviewOnly) {
        io.DisplaySize = ImVec2(1000, 800);
        io.DeltaTime = 1.0f / 60.0f;
        auto changed = studio.originals.front();
        changed.front() ^= 1;
        studio.SetBitmapReplacement(0, std::move(changed));
        auto menu = studio.menuPatch;
        menu.enabled = true;
        studio.SetMenuPatch(menu);
        auto screen = studio.screenPatches[0];
        screen.enabled = true;
        screen.pixels.assign(size_t(FirmwareLogo::Width) * FirmwareLogo::Height * 2, 0x31);
        studio.SetScreenPatch(FirmwareLogo::Target::Boot, screen);
        if (!studio.palette.empty())
            studio.SetPaletteColor(0, Rgb8::FromRgb565(studio.palette[0].current.ToRgb565() ^ 0x07e0));
        const auto path = root / L"review.zwdf";
        auto awaitReview = [&](UiResourceStudio::FirmwareReviewSnapshot& review) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
            while (!UiResourceStudio::GetFirmwareReview(review)) {
                UiResourceStudio::PollFileActions(studio, nullptr);
                frame();
                if (!check(std::chrono::steady_clock::now() < deadline, "export review preparation timed out")) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            for (int i = 0; i < 20; ++i) frame();
            return true;
        };
        UiResourceStudio::FirmwareReviewSnapshot review;
        if (!check(UiResourceStudio::SaveFirmware(studio, nullptr, false, utf8(path)), "review was not queued") ||
            !awaitReview(review)) return false;
        if (!check(review.prepared->Valid() && std::filesystem::is_empty(root) && studio.HasUnsavedChanges(),
            "review wrote files or marked the project saved before confirmation")) return false;
        if (!check(renderedText.find(Tr(i18n::Str::ExportReviewTitle)) != std::string::npos &&
            renderedText.find(Tr(i18n::Str::ExportReviewChanges)) != std::string::npos &&
            renderedText.find(Tr(i18n::Str::ExportReviewCapacity)) != std::string::npos &&
            renderedText.find(Tr(i18n::Str::ExportReviewConfirm)) != std::string::npos &&
            renderedText.find("%zu") == std::string::npos,
            "review modal omitted its localized title, changes, capacity, or confirmation")) return false;
        const auto* modal = ImGui::FindWindowByName("###FileOperationProgress");
        if (!check(modal && modal->Active && context->DimBgRatio == 1,
            "review modal was not visible with its dim background")) return false;
        char reviewCapture[MAX_PATH]{};
        const DWORD length = GetEnvironmentVariableA("VFS_EXPORT_REVIEW_CAPTURE", reviewCapture, MAX_PATH);
        if (!check(length < MAX_PATH, "export review capture path exceeds MAX_PATH")) return false;
        if (length && !CaptureUiDrawData(device.Get(), gpu.Get(), reviewCapture, error)) return false;
        io.DeltaTime = 10.0f;
        frame();
        io.DeltaTime = 1.0f / 60.0f;
        check(UiResourceStudio::IsFileActionActive() && std::filesystem::is_empty(root),
            "mandatory review auto-closed or wrote output without approval");
        check(!UiResourceStudio::SaveProject(studio, nullptr) &&
            !UiResourceStudio::OpenFirmware(studio, nullptr, utf8(path)),
            "review allowed conflicting file actions");
        auto changedAgain = studio.originals.front();
        changedAgain.front() ^= 2;
        studio.SetBitmapReplacement(0, std::move(changedAgain));
        frame();
        if (!check(!UiResourceStudio::ConfirmFirmwareExport(studio) && std::filesystem::is_empty(root) &&
            renderedText.find(Tr(i18n::Str::ExportReviewStale)) != std::string::npos,
            "stale approval escaped validation or its warning was invisible")) return false;
        if (!check(UiResourceStudio::CancelFirmwareExport(), "review cancellation failed") || !drain(studio)) return false;
        if (!check(std::filesystem::is_empty(root) && studio.HasUnsavedChanges(), "review cancellation wrote files or lost changes"))
            return false;
        if (!UiResourceStudio::SaveFirmware(studio, nullptr, false, utf8(path)) || !awaitReview(review)) return false;
        const auto approved = review.prepared;
        if (!check(UiResourceStudio::ConfirmFirmwareExport(studio), "valid reviewed output could not be confirmed") || !drain(studio))
            return false;
        auto input = PathUtils::OpenReadStream(utf8(path), std::ios::binary);
        const std::vector<uint8_t> written(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>{});
        check(!studio.statusIsErr && written == approved->Bytes() && studio.HasUnsavedChanges(),
            "confirmed review wrote different bytes or marked a project saved");
        check(ordered(lastProgress.log, {"All hardware revisions passed export validation", "Export explicitly confirmed",
            "Write temporary file", "Atomically replace destination", "Serialize firmware build report"}),
            "review confirmation lost the meaningful operation log or bypassed prepared output");
        if (log) *log << "EXPORT REVIEW UI REGRESSIONS OK (mandatory modal, dim background, localized review, no preconfirm files, cancellation, stale approval, prepared-byte write)\n";
        return error.empty();
    }
    FirmwareLogo::EditableSource source;
    source.width = 7;
    source.height = 5;
    source.fitMode = 1;
    source.scalePercent = 137.5f;
    source.x = -13;
    source.y = 29;
    source.alignX = 2;
    source.alignY = 0;
    source.resampling = FirmwareBitmap::Resampling::Bicubic;
    auto pixels = std::make_shared<std::vector<uint8_t>>(size_t(source.width) * source.height * 4);
    for (size_t i = 0; i < pixels->size(); ++i) (*pixels)[i] = static_cast<uint8_t>(i * 37 + 11);
    source.rgba = pixels;
    pixels.reset();
    studio.PushUndo("Async source fixture");
    studio.SetScreenPatchSource(FirmwareLogo::Target::Boot, source);
    studio.SetAssetPaintColor(studio.catalog.front().id, {17, 31, 53});
    studio.EndUndoAction();
    const auto snapshot = studio.CreateFileSnapshot();
    if (!check(snapshot->screenPatchSources[0] == source && snapshot->screenPatchSources[0].rgba == source.rgba &&
               snapshot->firmwarePath == studio.firmwarePath && snapshot->isBuiltin &&
               snapshot->firmware.source_bytes == studio.firmware.source_bytes &&
               snapshot->assetPaintColors == studio.assetPaintColors && snapshot->undoStack.empty() &&
               !snapshot->activePreviewTexture.IsValid(), "full project snapshot lost original source/filename or retained UI state")) return false;
    std::vector<uint8_t> snapshotBytes, expectedBytes;
    std::vector<std::string> buildStages;
    if (!FirmwareProject::Build(*snapshot, expectedBytes, error,
            [&](const char* stage) { buildStages.emplace_back(stage); })) return false;
    if (!check(ordered(buildStages, {"Validate project edits", "Serialize original firmware",
            "Calculate original firmware SHA-256", "Serialize project body", "Compress project body",
            "Use compressed project body", "Calculate project CRC-32"}),
            "project build did not report its real serialization/compression/checksum sequence")) return false;
    ++source.x;
    studio.PushUndo("Change live source after snapshot");
    studio.SetScreenPatchSource(FirmwareLogo::Target::Boot, source);
    studio.EndUndoAction();
    if (!FirmwareProject::Build(*snapshot, snapshotBytes, error)) return false;
    if (!check(snapshotBytes == expectedBytes && snapshot->screenPatchSources[0].x != source.x,
               "immutable snapshot followed live placement mutation")) return false;
    const auto cancelledPath = root / L"cancelled.zwdf";
    bool writeStage = false;
    std::vector<std::string> cancelledStages;
    const auto cancelledPrepared = FirmwareSave::Prepare(snapshot,
        [&](const char* stage) { cancelledStages.emplace_back(stage); });
    const auto cancelled = FirmwareSave::Write(*cancelledPrepared, *snapshot, FirmwareSave::Approval::Confirmed,
        utf8(cancelledPath), [&](FirmwareSave::Phase) { writeStage = true; }, [] { return false; },
        [&](const char* stage) { cancelledStages.emplace_back(stage); });
    if (!check(cancelled.cancelled && !cancelled.FirmwareSaved() && !writeStage &&
               !std::filesystem::exists(cancelledPath) && studio.HasUnsavedChanges(),
               "precommit cancellation wrote a file or changed the live baseline")) return false;
    if (!check(hasStage(cancelledStages, "Pack firmware container") && !hasStage(cancelledStages, "temporary file"),
            "cancelled firmware build reported safe-write stages it never executed")) return false;

    const auto projectPath = root / L"source-project.vfwp";
    studio.projectPath = utf8(projectPath);
    const auto identity = studio.DocumentIdentity();
    const auto historySize = studio.undoStack.size();
    if (!check(UiResourceStudio::SaveProject(studio, nullptr), "project save was not queued")) return false;
    const bool remainedLive = studio.DocumentIdentity() == identity && studio.HasUnsavedChanges();
    const bool refusedConflict = !UiResourceStudio::OpenFirmware(studio, nullptr, studio.projectPath);
    if (!drain(studio)) return false;
    if (!check(remainedLive && refusedConflict && !studio.statusIsErr && !studio.HasUnsavedChanges() &&
               studio.undoStack.size() == historySize, "queued save blocked adoption ordering, admitted a conflict, or lost history")) return false;
    StudioState disk;
    if (!FirmwareProject::Open(disk, utf8(projectPath), error)) return false;
    if (!check(lastProgress.title == "Save project" && lastProgress.filename == "source-project.vfwp" &&
            ordered(lastProgress.log, {"Compress project body", "Calculate project CRC-32",
                "Create same-directory temporary file", "Write temporary file", "Flush temporary file",
                "Close temporary file", "Atomically replace destination", "Completed"}) && captured,
            "async project save omitted operation title, filename, packing/safe-write stages, or WARP capture")) return false;
    if (!check(disk.screenPatchSources[0] == source && disk.firmwarePath == "original-source.zwdf",
               "async project save lost full RGBA/placement/source filename")) return false;

    ++source.x;
    studio.PushUndo("Save before open");
    studio.SetScreenPatchSource(FirmwareLogo::Target::Boot, source);
    studio.EndUndoAction();
    {
        DialogResponse chooseSave(IDYES);
        if (!check(chooseSave.timer != 0, "cannot automate save-before-open prompt")) return false;
        if (!check(UiResourceStudio::OpenFirmware(studio, nullptr, utf8(projectPath)),
                   "save-before-open did not queue the save")) return false;
    }
    const bool openDeferred = studio.DocumentIdentity() == identity && studio.HasUnsavedChanges();
    if (!drain(studio)) return false;
    if (!check(openDeferred && studio.DocumentIdentity() != identity && !studio.HasUnsavedChanges() &&
               studio.screenPatchSources[0] == source && studio.undoStack.empty(),
               "open continuation ran before successful save or dropped original editable image")) return false;
    if (!check(lastProgress.title == "Open project" && ordered(lastProgress.log,
            {"Read project file", "Validate project header", "Verify project CRC-32",
             "Decompress project body", "Calculate firmware SHA-256", "Verify original firmware SHA-256",
             "Restore project resources", "Validate restored project edits", "Completed"}),
            "project open did not report real checksum/decompression/restore stages")) return false;

    const auto failedIdentity = studio.DocumentIdentity();
    const auto failedSources = studio.screenPatchSources;
    const auto failedFirmware = studio.firmware.source_bytes;
    if (!check(UiResourceStudio::OpenFirmware(studio, nullptr, utf8(root / L"missing.vfwp")),
               "failing open was not queued")) return false;
    if (!drain(studio)) return false;
    if (!check(studio.statusIsErr && studio.DocumentIdentity() == failedIdentity &&
               studio.screenPatchSources == failedSources && studio.firmware.source_bytes == failedFirmware &&
               !studio.HasUnsavedChanges(), "failed open changed the current document/baseline")) return false;
    if (!check(lastProgress.title == "Open project" && !hasStage(lastProgress.log, "Decompress") &&
               hasStage(lastProgress.log, "Failed:"), "failed open reported stages it never reached")) return false;
    const auto corruptPath = root / L"bad-checksum.vfwp";
    {
        auto corrupt = expectedBytes;
        corrupt.back() ^= 0x80;
        auto output = PathUtils::OpenWriteStream(utf8(corruptPath), std::ios::binary);
        output.write(reinterpret_cast<const char*>(corrupt.data()), static_cast<std::streamsize>(corrupt.size()));
        if (!check(static_cast<bool>(output), "cannot write bad-checksum project fixture")) return false;
    }
    if (!check(UiResourceStudio::OpenFirmware(studio, nullptr, utf8(corruptPath)),
            "bad-checksum project open was not queued")) return false;
    if (!drain(studio)) return false;
    if (!check(studio.statusIsErr && studio.DocumentIdentity() == failedIdentity &&
            ordered(lastProgress.log, {"Read project file", "Validate project header", "Verify project CRC-32", "Failed:"}) &&
            !hasStage(lastProgress.log, "Decompress") && !hasStage(lastProgress.log, "Restore project"),
            "bad-checksum project proceeded into decompression/restore or changed the live document")) return false;

    ++source.x;
    studio.PushUndo("Failed save before open");
    studio.SetScreenPatchSource(FirmwareLogo::Target::Boot, source);
    studio.EndUndoAction();
    studio.projectPath = utf8(root / L"missing-directory" / L"cannot-save.vfwp");
    {
        DialogResponse chooseSave(IDYES);
        if (!check(chooseSave.timer != 0, "cannot automate failed-save prompt")) return false;
        if (!check(UiResourceStudio::OpenFirmware(studio, nullptr, utf8(projectPath)),
                   "failing save-before-open was not queued")) return false;
    }
    if (!drain(studio)) return false;
    if (!check(studio.statusIsErr && studio.DocumentIdentity() == failedIdentity && studio.HasUnsavedChanges() &&
               studio.screenPatchSources[0] == source, "failed save continued opening or discarded unsaved edits")) return false;
    {
        DialogResponse chooseCancel(IDCANCEL);
        if (!check(chooseCancel.timer != 0, "cannot automate cancelled prompt")) return false;
        if (!check(!UiResourceStudio::OpenFirmware(studio, nullptr, utf8(projectPath)) &&
                   !UiResourceStudio::IsFileActionActive() && studio.DocumentIdentity() == failedIdentity &&
                   studio.screenPatchSources[0] == source, "cancelled prompt changed the document or queued I/O")) return false;
    }
    studio.projectPath = utf8(projectPath);
    MSG closeMessage{};
    {
        DialogResponse chooseSave(IDYES);
        if (!check(chooseSave.timer != 0, "cannot automate deferred-close prompt")) return false;
        if (!check(!UiResourceStudio::RequestClose(studio, nullptr) && UiResourceStudio::IsFileActionActive() &&
                   !PeekMessageW(&closeMessage, nullptr, WM_CLOSE, WM_CLOSE, PM_REMOVE),
                   "close escaped before the queued save completed")) return false;
    }
    if (!drain(studio)) return false;
    if (!check(!studio.HasUnsavedChanges() && PeekMessageW(&closeMessage, nullptr, WM_CLOSE, WM_CLOSE, PM_REMOVE),
               "successful deferred save did not request close")) return false;

    const size_t index = 0;
    const auto& spec = studio.catalog[index];
    const auto pngPath = root / PathUtils::ToFsPath(spec.id + ".png");
    std::vector<uint8_t> gray(size_t(spec.width) * spec.height, 255), expectedRaw;
    auto encode = [&] {
        if (spec.format == AssetFormat::Rgb565) {
            std::vector<uint32_t> rgba(gray.size());
            for (size_t i = 0; i < rgba.size(); ++i) rgba[i] = 0xff000000u | uint32_t(gray[i]) * 0x010101u;
            return FirmwareBitmap::EncodeRgb565(rgba.data(), spec.width, spec.height, expectedRaw);
        }
        return FirmwareBitmap::EncodeMaskPreserving(gray.data(), spec.width, spec.height,
            studio.originals[index].data(), spec.byte_len(), expectedRaw, spec.format);
    };
    if (!encode() || !FirmwareBitmap::SavePng(utf8(pngPath), gray.data(), spec.width, spec.height, error)) return false;
    if (!check(UiResourceStudio::ImportFolder(studio, nullptr, utf8(root)), "folder PNG import was not queued")) return false;
    if (!drain(studio)) return false;
    if (!check(!studio.statusIsErr && std::memcmp(studio.GetCurrentRaw(index), expectedRaw.data(), expectedRaw.size()) == 0,
               "queued folder PNG import did not adopt encoded bytes")) return false;
    if (!check(lastProgress.title == "Import images" && ordered(lastProgress.log,
            {"Read, decode and fit image", "Encode", "Completed"}),
            "image import title or actual decode/encode stages are missing")) return false;
    std::fill(gray.begin(), gray.end(), 0);
    if (!encode() || !FirmwareBitmap::SavePng(utf8(pngPath), gray.data(), spec.width, spec.height, error)) return false;
    studio.selectedVisibleIdx = studio.visibleAssetIndices.size() > 1 ? 1 : 0;
    const int selection = studio.selectedVisibleIdx;
    if (!check(UiResourceStudio::ImportImage(studio, nullptr, utf8(pngPath), static_cast<int>(index)),
               "explicit-target PNG import was not queued")) return false;
    if (!drain(studio)) return false;
    if (!check(!studio.statusIsErr && studio.selectedVisibleIdx == selection &&
               std::memcmp(studio.GetCurrentRaw(index), expectedRaw.data(), expectedRaw.size()) == 0,
               "explicit PNG target changed selection or imported into the wrong resource")) return false;
    const auto beforeFailedImport = expectedRaw;
    const auto beforeFailedHistory = studio.undoStack.size();
    std::fill(gray.begin(), gray.end(), 255);
    if (!FirmwareBitmap::SavePng(utf8(pngPath), gray.data(), spec.width, spec.height, error)) return false;
    {
        auto invalid = PathUtils::OpenWriteStream(utf8(root / PathUtils::ToFsPath(studio.catalog[1].id + ".png")));
        invalid << "not a PNG";
    }
    if (!check(UiResourceStudio::ImportFolder(studio, nullptr, utf8(root)), "failing folder import was not queued")) return false;
    if (!drain(studio)) return false;
    if (!check(studio.statusIsErr && studio.undoStack.size() == beforeFailedHistory &&
               std::memcmp(studio.GetCurrentRaw(index), beforeFailedImport.data(), beforeFailedImport.size()) == 0,
               "failed folder import adopted a partial result or changed history")) return false;
    // Discard the import edits before opening the exported firmware below.
    studio.MarkSaved();
    const auto imagesPath = root / L"exported-images";
    if (!check(UiResourceStudio::ExportImages(studio, nullptr, utf8(imagesPath)), "image export was not queued")) return false;
    if (!drain(studio)) return false;
    if (!check(!studio.statusIsErr && lastProgress.title == "Export images" &&
            ordered(lastProgress.log, {"Create image export directory", "Decode resource pixels",
                "Encode and write PNG", "Finish writing asset manifest", "Completed"}),
            "image export title or real PNG/manifest stages are missing")) return false;
    const auto firmwarePath = root / L"exported.zwdf";
    if (!check(UiResourceStudio::SaveFirmware(studio, nullptr, false, utf8(firmwarePath)),
            "firmware export was not queued")) return false;
    if (!drain(studio)) return false;
    if (!check(!studio.statusIsErr && lastProgress.title == "Export firmware" &&
            ordered(lastProgress.log, {"Build palette", "Apply resource", "Pack firmware container",
                "Validate repacked firmware", "Verify patched images", "Calculate output firmware SHA-256",
                "Write temporary file", "Atomically replace destination", "Serialize firmware build report",
                "Write temporary file", "Atomically replace destination", "Completed"}),
            "firmware export did not report actual packing/validation/firmware/report writes")) return false;
    if (!check(UiResourceStudio::OpenFirmware(studio, nullptr, utf8(firmwarePath)), "firmware open was not queued")) return false;
    if (!drain(studio)) return false;
    if (!check(!studio.statusIsErr && lastProgress.title == "Open firmware" &&
            ordered(lastProgress.log, {"Read firmware file", "Calculate firmware SHA-256",
                "Parse firmware container", "Load firmware resources", "Completed"}),
            "firmware open title or actual read/validate/load stages are missing")) return false;
    {
        StudioState draftDocument;
        if (!FirmwareProject::Open(draftDocument, utf8(projectPath), error)) return false;
        UiFirmwarePatches::State editor;
        editor.initialized = true;
        editor.documentIdentity = draftDocument.DocumentIdentity();
        editor.observed = editor.draft = draftDocument.screenPatches[0];
        editor.observedSource = draftDocument.screenPatchSources[0];
        const auto& original = editor.observedSource;
        editor.sourceImage = original.rgba;
        editor.sourceWidth = original.width; editor.sourceHeight = original.height;
        editor.fitMode = original.fitMode; editor.scalePercent = original.scalePercent;
        editor.x = original.x; editor.y = original.y;
        editor.alignX = original.alignX; editor.alignY = original.alignY;
        editor.resampling = original.resampling;
        editor.adjustments = original.adjustments;
        editor.adjustments.brightness = 27;
        editor.sourceModified = true;
        UiResourceStudio::BindPatchEditor(&editor);
        struct Unbind { ~Unbind() { UiResourceStudio::BindPatchEditor(nullptr); } } unbind;
        if (!check(!draftDocument.HasUnsavedChanges() && UiResourceStudio::HasUnsavedChanges(draftDocument),
            "correction-only draft was omitted from unsaved-change detection")) return false;
        {
            DialogResponse cancelClose(IDCANCEL);
            if (!check(!UiResourceStudio::RequestClose(draftDocument, nullptr),
                "closing a correction-only draft skipped the save prompt")) return false;
        }
        if (!UiResourceStudio::SaveProject(draftDocument, nullptr) || !drain(draftDocument)) return false;
        StudioState reopened;
        if (!FirmwareProject::Open(reopened, utf8(projectPath), error)) return false;
        if (!check(reopened.screenPatchSources[0].adjustments.brightness == 27 &&
            !UiResourceStudio::HasUnsavedChanges(draftDocument) &&
            !UiFirmwarePatches::HasDraftChanges(editor, draftDocument),
            "saving an unapplied correction lost settings or left a dirty draft")) return false;
        editor.draft.enabled = true;
        editor.draft.pixels.assign(size_t(FirmwareLogo::Width) * FirmwareLogo::Height * 2, 0x31);
        const auto exportPath = utf8(root / L"current-screen.zwdf");
        if (!UiResourceStudio::SaveFirmware(draftDocument, nullptr, false, exportPath) || !drain(draftDocument))
            return false;
        FirmwarePackage exported;
        FirmwareLogo::Patches exportedScreens;
        if (!FirmwarePackage::Open(exportPath, exported, error) ||
            !FirmwareLogo::Canonicalize(exported, exportedScreens, error)) return false;
        if (!check(exportedScreens[0] == editor.draft, "firmware export omitted current screen edits")) return false;
        uint32_t random = 0x7ab391du;
        for (size_t target = 0; target < FirmwareLogo::TargetCount; ++target) {
            auto screen = draftDocument.screenPatches[0];
            screen.enabled = true;
            for (auto& byte : screen.pixels) {
                random ^= random << 13; random ^= random >> 17; random ^= random << 5;
                byte = uint8_t(random);
            }
            draftDocument.SetScreenPatch(static_cast<FirmwareLogo::Target>(target), screen);
        }
        std::vector<uint8_t> projectBytes, firmwareBytes;
        std::string capacityError;
        if (!check(FirmwareProject::Build(draftDocument, projectBytes, error) &&
            !draftDocument.BuildToBuffer(firmwareBytes, capacityError) && !capacityError.empty(),
            "Flash overflow blocked project preservation or escaped firmware export validation")) return false;
    }
    {
        StudioState builtin;
        if (!UiResourceStudio::OpenBuiltin(builtin, nullptr, FirmwareModel::DragS, FirmwareEdition::Score))
            return check(false, "built-in firmware was not queued");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (UiResourceStudio::IsFileActionActive()) {
            UiResourceStudio::PollFileActions(builtin, nullptr);
            frame();
            if (!check(context->OpenPopupStack.empty(),
                "built-in firmware selection showed a file-operation window")) return false;
            if (std::chrono::steady_clock::now() >= deadline) return check(false, "built-in load timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!check(builtin.hasFirmware && builtin.isBuiltin, "silent built-in load failed")) return false;
    }
    if (log) *log << "ASYNC FILE REGRESSIONS OK (immutable RGBA snapshot, cancellation, queued save/open, deferred save/open/close, failure preservation, transactional PNG import, real pipeline stage sequences, operation titles/log, offscreen WARP capture)\n";
    return error.empty();
}

inline bool RunExportReviewUiRegressions(std::string& error, std::ostream* log = nullptr) {
    return RunAsyncFileRegressions(error, log, true);
}
