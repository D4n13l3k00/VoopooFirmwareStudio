#include "ui_tabs.h"
#include "ui_modal_transition.h"
#include "ui_markdown.h"
#include "imgui_internal.h"
#include "ui_firmware_patches.h"
#include "ui_resource_studio.h"
#include "ui_pixel_editor.h"
#include "ui_palette.h"
#include "path_utils.h"
#include "ui_controls.h"
#include "firmware_save.h"
#include "firmware_project.h"
#include <commctrl.h>
#include <cstring>
#include <atomic>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>
#include "../core/firmware_profiles.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include "../imgui/imgui.h"

namespace UiResourceStudio {
namespace { int activeFrame = -1; }
bool IsActive() { return activeFrame == ImGui::GetFrameCount(); }

bool UpdateReferenceTexture(StudioState& studio, size_t catIdx, ID3D11Device* device, ID3D11DeviceContext* context) {
    const auto& spec = studio.catalog[catIdx];
    const auto* refRaw = studio.GetReferenceRaw(catIdx);
    if (!refRaw) { studio.referencePreviewTexture.Release(); return false; }
    const auto fgCol = studio.GetForegroundForAsset(spec).ScreenColor();
    std::vector<uint32_t> refRgba;
    if (spec.format == AssetFormat::Rgb565) {
        FirmwareBitmap::DecodeRgb565(refRaw, spec.byte_len(), spec.width, spec.height, refRgba);
    } else {
        std::vector<uint8_t> gray;
        FirmwareBitmap::DecodeMask(refRaw, spec.byte_len(), spec.width, spec.height, gray, spec.format);
        FirmwareBitmap::ColorizeMask(gray.data(), spec.width, spec.height, fgCol, studio.GetBackgroundForAsset(spec).ScreenColor(), refRgba);
    }
    // Reference content, dimensions and palette can all change while the SRV stays valid.
    return studio.referencePreviewTexture.Update(device, context, refRgba.data(), spec.width, spec.height);
}



namespace {
enum class FileAction { Open, Builtin, Project, Review, Firmware, ExportPng, ImportPng };
enum class Cancellation { Available, Requested, CommitLocked };

std::string TimestampLog(std::string message) {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    char prefix[24];
    snprintf(prefix, sizeof(prefix), "[%02u:%02u:%02u.%03u] ",
        unsigned(time.wHour), unsigned(time.wMinute), unsigned(time.wSecond), unsigned(time.wMilliseconds));
    return prefix + message;
}

struct FileJob {
    FileAction action = FileAction::Open;
    std::string path;
    FirmwareModel model = FirmwareModel::DragS;
    FirmwareEdition edition = FirmwareEdition::Score;
    std::shared_ptr<const StudioState> snapshot;
    std::unique_ptr<StudioState> document;
    std::atomic<Cancellation> cancellation{Cancellation::Available};
    std::atomic<bool> done{false};
    std::atomic<size_t> completed{0};
    size_t total = 0;
    size_t assetIndex = SIZE_MAX; // SIZE_MAX means a complete folder.
    bool horizontal = false;
    bool showSuccessModal = false;
    bool success = false;
    std::string error;
    AtomicFile::Error fileError;
    FirmwareSave::Result firmwareResult;
    std::shared_ptr<const FirmwareSave::Prepared> prepared;
    std::vector<std::pair<size_t, std::vector<uint8_t>>> imported;
    size_t changed = 0;
    size_t unchanged = 0;
    std::function<void()> afterSuccess; // Accessed only on the UI thread.
    mutable std::mutex logMutex;
    std::vector<std::string> log;

    void Log(std::string message) {
        std::lock_guard lock(logMutex);
        log.emplace_back(TimestampLog(std::move(message)));
    }

    bool Cancelled() const {
        return cancellation.load(std::memory_order_relaxed) == Cancellation::Requested;
    }
    bool BeginCommit() {
        auto expected = Cancellation::Available;
        return cancellation.compare_exchange_strong(expected, Cancellation::CommitLocked);
    }
};

std::shared_ptr<FileJob> activeJob;
bool nativeDialogActive = false;
bool closeAfterSuccess = false;
bool completionPending = false;
std::function<void()> pendingContinuation;
FileProgressSnapshot completedProgress;
bool hasCompletedProgress = false;
bool progressPinned = false;
double completedVisibleAt = -1;
bool completedSuccessfully = false, completedCancelled = false;
UiFirmwarePatches::State* patchEditor = nullptr;

std::pair<const char*, i18n::Str> FileTitle(const FileJob& job) {
    using enum i18n::Str;
    switch (job.action) {
    case FileAction::Open: return FirmwareProject::IsProjectPath(job.path) ?
        std::pair{"Open project", FileTaskOpenProject} : std::pair{"Open firmware", FileTaskOpenFirmware};
    case FileAction::Builtin: return {"Open firmware", FileTaskOpenFirmware};
    case FileAction::Project: return {"Save project", FileTaskSaveProject};
    case FileAction::Review: return {"Review firmware export", ExportReviewTitle};
    case FileAction::Firmware: return {"Export firmware", FileTaskExportFirmware};
    case FileAction::ImportPng: return {"Import images", FileTaskImportImages};
    case FileAction::ExportPng: return {"Export images", FileTaskExportImages};
    }
    return {"", FileTaskOpenFirmware};
}
struct FileActionScope {
    bool previous = nativeDialogActive;
    FileActionScope() { nativeDialogActive = true; }
    ~FileActionScope() { nativeDialogActive = previous; }
};

bool ExportPng(FileJob& job, size_t index, const std::string& path, bool horizontal) {
    const auto& snapshot = *job.snapshot;
    auto& error = job.error;
    job.Log("Decode resource pixels: " + snapshot.catalog[index].id);
    const auto& spec = snapshot.catalog[index];
    const uint8_t* raw = snapshot.GetCurrentRaw(index);
    if (spec.format == AssetFormat::Rgb565) {
        std::vector<uint32_t> rgba;
        if (!FirmwareBitmap::DecodeRgb565(raw, spec.byte_len(), spec.width, spec.height, rgba)) {
            error = "Cannot decode RGB565 resource: " + spec.id;
            return false;
        }
        job.Log("Encode and write PNG: " + PathUtils::WideToUtf8(PathUtils::ToFsPath(path).filename().wstring()));
        return horizontal
            ? FirmwareBitmap::SavePngRgb565Rotated(path, rgba.data(), spec.width, spec.height, true, error)
            : FirmwareBitmap::SavePngRgb565(path, rgba.data(), spec.width, spec.height, error);
    }
    std::vector<uint8_t> gray;
    if (!FirmwareBitmap::DecodeMask(raw, spec.byte_len(), spec.width, spec.height, gray, spec.format)) {
        error = "Cannot decode resource: " + spec.id;
        return false;
    }
    job.Log("Encode and write PNG: " + PathUtils::WideToUtf8(PathUtils::ToFsPath(path).filename().wstring()));
    return FirmwareBitmap::SavePng(path, gray.data(), spec.width, spec.height, error);
}

bool ImportPng(FileJob& job, size_t index, const std::string& path) {
    const auto& snapshot = *job.snapshot;
    const auto& spec = snapshot.catalog[index];
    job.Log("Read, decode and fit image: " + PathUtils::WideToUtf8(PathUtils::ToFsPath(path).filename().wstring()));
    std::vector<uint8_t> raw;
    bool encoded = false;
    if (spec.format == AssetFormat::Rgb565) {
        std::vector<uint32_t> rgba;
        if (!FirmwareBitmap::LoadAndFitRgb565(path, spec.width, spec.height, rgba, job.error)) return false;
        job.Log("Encode RGB565 resource: " + spec.id);
        encoded = FirmwareBitmap::EncodeRgb565(rgba.data(), spec.width, spec.height, raw);
    } else {
        std::vector<uint8_t> gray;
        if (!FirmwareBitmap::LoadAndFit(path, spec.width, spec.height, gray, job.error)) return false;
        job.Log("Encode mask resource: " + spec.id);
        encoded = FirmwareBitmap::EncodeMaskPreserving(gray.data(), spec.width, spec.height,
            snapshot.originals[index].data(), spec.byte_len(), raw, spec.format);
    }
    if (!encoded) { job.error = "Cannot encode resource: " + spec.id; return false; }
    if (raw == snapshot.originals[index]) ++job.unchanged;
    else ++job.changed;
    job.imported.emplace_back(index, std::move(raw));
    return true;
}

void RunPngJob(FileJob& job) {
    if (job.assetIndex != SIZE_MAX) {
        if (job.Cancelled()) return;
        job.success = job.action == FileAction::ExportPng
            ? ExportPng(job, job.assetIndex, job.path, job.horizontal)
            : ImportPng(job, job.assetIndex, job.path);
        job.completed.store(1);
        return;
    }
    const auto directory = PathUtils::ToFsPath(job.path);
    std::ofstream manifest;
    if (job.action == FileAction::ExportPng) {
        job.Log("Create image export directory and asset manifest");
        fs::create_directories(directory);
        manifest = PathUtils::OpenWriteStream(PathUtils::WideToUtf8((directory / L"asset_manifest.json").wstring()));
        if (!manifest) { job.error = "Cannot create asset manifest"; return; }
        manifest << "{\n  \"format\": \"" << FirmwareModelName(job.snapshot->model) << " Resource Studio export\",\n"
                 << "  \"edition\": \"" << FirmwareEditionName(job.snapshot->edition) << "\",\n  \"resources\": [\n";
    }
    for (size_t i = 0; i < job.snapshot->catalog.size(); ++i) {
        if (job.Cancelled()) return;
        const auto& spec = job.snapshot->catalog[i];
        auto path = directory / PathUtils::ToFsPath(spec.id + ".png");
        const auto horizontal = directory / L"boot_voopoo_animated_horizontal_136x22.png";
        if (job.action == FileAction::ImportPng) {
            if (!fs::exists(path)) {
                if (spec.id != "boot_voopoo_animated" || !fs::exists(horizontal)) {
                    job.completed.store(i + 1);
                    continue;
                }
                path = horizontal;
            }
            if (!ImportPng(job, i, PathUtils::WideToUtf8(path.wstring()))) return;
        } else {
            if (!ExportPng(job, i, PathUtils::WideToUtf8(path.wstring()), false)) return;
            if (spec.id == "boot_voopoo_animated" &&
                !ExportPng(job, i, PathUtils::WideToUtf8(horizontal.wstring()), true)) return;
            manifest << "    { \"id\": \"" << spec.id << "\", \"title\": \"" << spec.title
                     << "\", \"width\": " << spec.width << ", \"height\": " << spec.height
                     << ", \"address\": \"0x" << std::hex << spec.address << std::dec << "\" }"
                     << (i + 1 < job.snapshot->catalog.size() ? ",\n" : "\n");
        }
        job.completed.store(i + 1);
    }
    if (manifest.is_open()) {
        job.Log("Finish writing asset manifest");
        manifest << "  ]\n}\n";
        manifest.close();
        if (!manifest) { job.error = "Cannot write asset manifest"; return; }
    }
    job.success = true;
}

void RunFileJob(FileJob& job) {
    if (job.Cancelled()) return;
    const auto progress = [&job](const char* message) { job.Log(message); };
    switch (job.action) {
    case FileAction::Open:
    case FileAction::Builtin: {
        job.document = std::make_unique<StudioState>();
        // Fresh CPU-only states have an empty filter, so loading never enters
        // the localized resource-search branch or owns a D3D texture.
        if (job.action == FileAction::Builtin) {
            job.Log("Load embedded firmware");
            FirmwarePackage package;
            job.success = LoadEmbeddedFirmware(package, job.error, job.model, job.edition);
            if (job.success) {
                job.Log("Load firmware resources, palette and layout");
                job.success = job.document->LoadPackage(std::move(package), job.path, job.error);
            }
            job.document->isBuiltin = true;
        } else {
            job.success = FirmwareProject::IsProjectPath(job.path)
                ? FirmwareProject::Open(*job.document, job.path, job.error, progress)
                : job.document->OpenFirmware(job.path, job.error, progress);
        }
        if (job.success) job.document->EnableProjectDocument();
        break;
    }
    case FileAction::Project: {
        std::vector<uint8_t> bytes;
        if (!FirmwareProject::Build(*job.snapshot, bytes, job.error, progress) || !job.BeginCommit()) break;
        fs::path protectedSource;
        if (!job.snapshot->isBuiltin && !job.snapshot->firmwareSourceEmbedded) {
            const auto source = PathUtils::ToFsPath(job.snapshot->firmwarePath);
            std::error_code ec;
            if (fs::exists(source, ec) && !ec) protectedSource = source;
        }
        job.success = AtomicFile::Write(PathUtils::ToFsPath(job.path), bytes.data(), bytes.size(),
            true, job.fileError, protectedSource, {}, progress);
        break;
    }
    case FileAction::Review:
        job.prepared = FirmwareSave::Prepare(job.snapshot, progress, [&job] { return job.Cancelled(); });
        job.success = true; // A completed review may contain blocking validation errors.
        break;
    case FileAction::Firmware:
        job.firmwareResult = FirmwareSave::Write(*job.prepared, *job.snapshot, FirmwareSave::Approval::Confirmed,
            job.path, {}, [&job] { return job.BeginCommit(); }, progress);
        job.success = job.firmwareResult.FirmwareSaved();
        break;
    case FileAction::ExportPng:
    case FileAction::ImportPng:
        RunPngJob(job);
        break;
    }
}

bool StartFileJob(StudioState& studio, const std::shared_ptr<FileJob>& job) {
    if (activeJob || completionPending) return false;
    studio.EndUndoAction();
    studio.isPixelStrokeActive = false;
    activeJob = job;
    progressPinned = false;
    completedVisibleAt = -1;
    hasCompletedProgress = false;
    completedSuccessfully = completedCancelled = false;
    try {
        job->Log("Prepare file operation");
        if (!job->snapshot && studio.hasFirmware && job->action != FileAction::Open && job->action != FileAction::Builtin)
            job->snapshot = studio.CreateFileSnapshot();
        // Detached jobs own all inputs/results. Poll consumes only published
        // results; neither the next action nor shutdown ever joins a worker.
        std::thread([job] {
            try { RunFileJob(*job); }
            catch (const std::exception& error) { job->error = error.what(); job->success = false; }
            catch (...) { job->error = "Unexpected file operation failure"; job->success = false; }
            if (job->Cancelled()) job->Log("Cancelled; no document changes adopted");
            else if (!job->error.empty()) job->Log("Failed: " + job->error);
            else if (!job->success) {
                const auto& buildError = job->firmwareResult.buildError;
                const auto code = job->action == FileAction::Firmware
                    ? job->firmwareResult.fileError.code : job->fileError.code;
                job->Log(buildError.empty() ? "Failed; Windows error " + std::to_string(code) : "Failed: " + buildError);
            } else if (job->action == FileAction::Firmware &&
                       job->firmwareResult.outcome == FirmwareSave::Outcome::FirmwareOnly) {
                job->Log("Firmware saved, but report write failed; Windows error " +
                    std::to_string(job->firmwareResult.fileError.code));
            } else if (job->action == FileAction::Review) job->Log("Review ready; no files written");
            else job->Log("Completed");
            job->done.store(true, std::memory_order_release);
        }).detach();
    } catch (const std::exception& error) {
        job->error = error.what();
        job->done.store(true, std::memory_order_release);
    }
    return true;
}

bool QueueOpen(StudioState& studio, const std::string& path,
               bool builtin = false, FirmwareModel model = FirmwareModel::DragS,
               FirmwareEdition edition = FirmwareEdition::Score) {
    auto job = std::make_shared<FileJob>();
    job->action = builtin ? FileAction::Builtin : FileAction::Open;
    job->path = path;
    job->model = model;
    job->edition = edition;
    return StartFileJob(studio, job);
}

bool QueuePng(StudioState& studio, FileAction action, const std::string& path,
              size_t index = SIZE_MAX, bool horizontal = false) {
    if (activeJob || !studio.hasFirmware) return false;
    auto job = std::make_shared<FileJob>();
    job->action = action;
    job->path = path;
    job->assetIndex = index;
    job->horizontal = horizontal;
    job->total = index == SIZE_MAX ? studio.catalog.size() : 1;
    return StartFileJob(studio, job);
}

bool QueueProjectSave(StudioState& studio, HWND hwnd, bool saveAs, std::function<void()> afterSuccess) {
    if (activeJob || !studio.hasFirmware) return false;
    FileActionScope scope;
    std::string path = studio.projectPath;
    if (saveAs || path.empty()) {
        const auto defaultPath = path.empty()
            ? PathUtils::ToFsPath(studio.firmwarePath).stem().wstring() + L".vfwp"
            : PathUtils::Utf8ToWide(path);
        wchar_t filename[MAX_PATH]{};
        wcsncpy_s(filename, defaultPath.c_str(), MAX_PATH - 1);
        OPENFILENAMEW dialog{sizeof(dialog)};
        dialog.hwndOwner = hwnd;
        dialog.lpstrFilter = L"Voopoo Firmware Project (*.vfwp)\0*.vfwp\0";
        dialog.lpstrDefExt = L"vfwp";
        dialog.lpstrFile = filename;
        dialog.nMaxFile = MAX_PATH;
        dialog.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&dialog)) return false;
        path = PathUtils::WideToUtf8(filename);
    }
    CommitPatchDrafts(studio);
    auto job = std::make_shared<FileJob>();
    job->action = FileAction::Project;
    job->path = std::move(path);
    job->afterSuccess = std::move(afterSuccess);
    return StartFileJob(studio, job);
}
} // namespace

void BindPatchEditor(UiFirmwarePatches::State* editor) { patchEditor = editor; }

void CommitPatchDrafts(StudioState& studio, bool retainHistory) {
    if (patchEditor) UiFirmwarePatches::CommitDrafts(*patchEditor, studio, retainHistory);
}

bool HasUnsavedChanges(const StudioState& studio) {
    return studio.HasUnsavedChanges() ||
        (patchEditor && UiFirmwarePatches::HasDraftChanges(*patchEditor, studio));
}

bool IsFileActionActive() {
    return nativeDialogActive || completionPending || static_cast<bool>(activeJob);
}

bool GetFileProgress(FileProgressSnapshot& snapshot) {
    if (!activeJob) {
        if (!hasCompletedProgress) return false;
        snapshot = completedProgress;
        return true;
    }
    const auto& job = *activeJob;
    snapshot.done = job.done.load(std::memory_order_acquire);
    const auto [title, titleLabel] = FileTitle(job);
    snapshot.title = title;
    snapshot.titleLabel = titleLabel;
    snapshot.filename = PathUtils::WideToUtf8(PathUtils::ToFsPath(job.path).filename().wstring());
    {
        std::lock_guard lock(job.logMutex);
        snapshot.log = job.log;
    }
    snapshot.completed = job.completed.load(std::memory_order_relaxed);
    snapshot.total = job.total;
    const auto cancellation = job.cancellation.load(std::memory_order_relaxed);
    snapshot.cancelling = cancellation == Cancellation::Requested;
    snapshot.cancellable = cancellation == Cancellation::Available;
    return true;
}

static std::string DescribeSaveError(const AtomicFile::Error& error) {
    i18n::Str message = i18n::Str::SaveIdentityError;
    switch (error.stage) {
    case AtomicFile::Stage::Temporary: message = i18n::Str::SaveTemporaryError; break;
    case AtomicFile::Stage::Write: message = i18n::Str::SaveWriteError; break;
    case AtomicFile::Stage::Flush: message = i18n::Str::SaveFlushError; break;
    case AtomicFile::Stage::Close: message = i18n::Str::SaveCloseError; break;
    case AtomicFile::Stage::Backup: message = i18n::Str::SaveBackupError; break;
    case AtomicFile::Stage::Commit: message = i18n::Str::SaveCommitError; break;
    default: break;
    }
    char text[1024];
    snprintf(text, sizeof(text), Tr(i18n::Str::MsgWindowsErrorCode), Tr(message), error.code);
    return text;
}

bool SaveFirmware(StudioState& studio, HWND hwnd, bool showSuccessModal, const std::string& requestedPath) {
    if (IsFileActionActive() || !studio.hasFirmware) return false;
    FileActionScope scope;
    const auto extension = PathUtils::ToFsPath(FirmwareDefaultFilename(studio.model, studio.edition)).extension().wstring();
    const std::string defaultName = PathUtils::WideToUtf8(
        PathUtils::ToFsPath(studio.firmwarePath).stem().wstring() + L"_custom" + extension);
    const wchar_t* filter = studio.firmware.format == FirmwareFormat::Xwdf
        ? L"VOOPOO Firmware (*.xwdf)\0*.xwdf\0"
        : studio.firmware.format == FirmwareFormat::Swdf
            ? L"VOOPOO Firmware (*.swdf)\0*.swdf\0"
            : L"VOOPOO Firmware (*.zwdf)\0*.zwdf\0";
    std::string path = requestedPath;
    if (path.empty() && !PathUtils::SaveFileDialog(hwnd, filter, defaultName, path))
        return false;
    CommitPatchDrafts(studio);
    auto job = std::make_shared<FileJob>();
    job->action = FileAction::Review;
    job->path = std::move(path);
    job->showSuccessModal = showSuccessModal;
    return StartFileJob(studio, job);
}

bool GetFirmwareReview(FirmwareReviewSnapshot& snapshot) {
    if (!activeJob || activeJob->action != FileAction::Review ||
        !activeJob->done.load(std::memory_order_acquire) || activeJob->Cancelled() || !activeJob->prepared)
        return false;
    snapshot.prepared = activeJob->prepared;
    snapshot.path = activeJob->path;
    return true;
}

bool ConfirmFirmwareExport(StudioState& studio) {
    FirmwareReviewSnapshot review;
    if (!GetFirmwareReview(review) || !review.prepared->Valid() || !review.prepared->Matches(studio) ||
        (patchEditor && UiFirmwarePatches::HasDraftChanges(*patchEditor, studio))) return false;
    auto job = std::move(activeJob);
    job->action = FileAction::Firmware;
    job->done.store(false, std::memory_order_release);
    job->success = false;
    job->Log("Export explicitly confirmed; write the reviewed immutable firmware bytes");
    // Keep the original snapshot and log; approval never rebuilds or recaptures.
    return StartFileJob(studio, job);
}

bool CancelFirmwareExport() {
    if (!activeJob || activeJob->action != FileAction::Review) return false;
    auto expected = Cancellation::Available;
    return activeJob->cancellation.compare_exchange_strong(expected, Cancellation::Requested);
}

bool SaveProject(StudioState& studio, HWND hwnd, bool saveAs) {
    if (IsFileActionActive()) return false;
    return QueueProjectSave(studio, hwnd, saveAs, {});
}

void PollFileActions(StudioState& studio, HWND hwnd) {
    if (!activeJob || !activeJob->done.load(std::memory_order_acquire)) return;
    if (activeJob->action == FileAction::Review && !activeJob->Cancelled() && activeJob->error.empty()) return;
    completionPending = true;
    GetFileProgress(completedProgress);
    auto job = std::move(activeJob);
    char text[1536]{};
    bool successful = job->success && !job->Cancelled();
    if (successful && job->snapshot &&
        (job->action == FileAction::Project || job->action == FileAction::Firmware ||
         job->action == FileAction::ImportPng) &&
        (studio.DocumentIdentity() != job->snapshot->DocumentIdentity() ||
         studio.ResourceRevision() != job->snapshot->ResourceRevision() ||
         studio.assetPaintColors != job->snapshot->assetPaintColors)) {
        job->error = "The document changed while the file operation was running";
        successful = false;
    }
    if (job->Cancelled()) {
        studio.statusMsg = Tr(i18n::Str::FileProgressCancelled);
        studio.statusIsErr = false;
    } else if (!job->error.empty()) {
        snprintf(text, sizeof(text), Tr(i18n::Str::FileProgressFailed), job->error.c_str());
        studio.statusMsg = text;
        studio.statusIsErr = true;
        successful = false;
    } else if (job->action == FileAction::Open || job->action == FileAction::Builtin) {
        if (successful) {
            // Only this UI-thread adoption touches live document/GPU ownership.
            studio.AdoptLoadedDocument(*job->document);
            studio.activePreviewTexture.Release();
            studio.referencePreviewTexture.Release();
            studio.horizontalPreviewTexture.Release();
            studio.showStartupChooser = false;
            snprintf(text, sizeof(text),
                Tr(job->action == FileAction::Builtin ? i18n::Str::MsgLoadedBuiltinFw : i18n::Str::MsgSuccessOpened),
                PathUtils::WideToUtf8(PathUtils::ToFsPath(job->path).filename().wstring()).c_str());
        } else {
            snprintf(text, sizeof(text), Tr(i18n::Str::MsgOpenError), job->error.c_str());
        }
        studio.statusMsg = text;
        studio.statusIsErr = !successful;
    } else if (job->action == FileAction::Project) {
        if (successful) {
            studio.projectPath = job->path;
            studio.EnableProjectDocument();
            studio.MarkSaved();
            snprintf(text, sizeof(text), Tr(i18n::Str::MsgProjectSaved),
                PathUtils::WideToUtf8(PathUtils::ToFsPath(job->path).filename().wstring()).c_str());
        } else {
            snprintf(text, sizeof(text), Tr(i18n::Str::MsgSaveError), DescribeSaveError(job->fileError).c_str());
        }
        studio.statusMsg = text;
        studio.statusIsErr = !successful;
    } else if (job->action == FileAction::Firmware) {
        const auto& result = job->firmwareResult;
        if (!result.FirmwareSaved()) {
            const auto error = result.buildError.empty() ? DescribeSaveError(result.fileError) : result.buildError;
            snprintf(text, sizeof(text), Tr(i18n::Str::MsgSaveError), error.c_str());
            studio.statusIsErr = true;
        } else {
            studio.lastReport = std::move(job->firmwareResult.report);
            studio.lastBuildPath = job->path;
            studio.showBuildModal = false;
            if (!studio.projectDocument) studio.MarkSaved();
            if (result.outcome == FirmwareSave::Outcome::FirmwareOnly) {
                snprintf(text, sizeof(text), Tr(i18n::Str::MsgFirmwareOnlySaved), DescribeSaveError(result.fileError).c_str());
                studio.statusIsErr = true;
                successful = false;
            } else {
                snprintf(text, sizeof(text), Tr(i18n::Str::MsgBuildSuccess),
                    PathUtils::WideToUtf8(PathUtils::ToFsPath(job->path).filename().wstring()).c_str());
                studio.statusIsErr = false;
                studio.showBuildModal = job->showSuccessModal;
            }
        }
        studio.statusMsg = text;
    } else if (job->action == FileAction::ImportPng && successful) {
        studio.PushUndo("Import PNG: " + job->path);
        for (auto& [index, raw] : job->imported) studio.SetBitmapReplacement(index, std::move(raw));
        studio.EndUndoAction();
        studio.SynchronizeEditorState();
        snprintf(text, sizeof(text), Tr(i18n::Str::MsgImportedChangedPngs), job->changed, job->unchanged);
        studio.statusMsg = text;
        studio.statusIsErr = false;
    } else if (job->action == FileAction::ExportPng && successful) {
        if (job->assetIndex == SIZE_MAX) {
            snprintf(text, sizeof(text), Tr(i18n::Str::MsgExportSuccess), static_cast<int>(job->total), job->path.c_str());
        } else if (job->horizontal) {
            snprintf(text, sizeof(text), Tr(i18n::Str::MsgSavedBootLogo), job->path.c_str());
        } else {
            snprintf(text, sizeof(text), Tr(i18n::Str::MsgSavedAssetPng),
                job->snapshot->catalog[job->assetIndex].id.c_str(), job->path.c_str());
        }
        studio.statusMsg = text;
        studio.statusIsErr = false;
    }
    completedProgress.done = true;
    completedProgress.cancellable = false;
    completedProgress.cancelling = false;
    completedProgress.log.push_back(TimestampLog(job->Cancelled() ? "Cancelled" :
        successful ? "Completed successfully" : "Failed: " + job->error));
    hasCompletedProgress = job->action != FileAction::Builtin;
    completedVisibleAt = -1;
    completedSuccessfully = successful;
    completedCancelled = job->Cancelled();
    const bool close = std::exchange(closeAfterSuccess, false);
    if (successful) {
        pendingContinuation = [after = std::move(job->afterSuccess), close, hwnd] {
            if (after) after();
            if (close) PostMessage(hwnd, WM_CLOSE, 0, 0);
        };
    }
}

bool ConfirmUnsavedChanges(StudioState& studio, HWND hwnd, std::function<void()> afterSave) {
    if (IsFileActionActive()) return false;
    if (!HasUnsavedChanges(studio)) return true;
    FileActionScope scope;
    const auto title = PathUtils::Utf8ToWide(Tr(i18n::Str::UnsavedTitle));
    const auto prompt = PathUtils::Utf8ToWide(Tr(i18n::Str::UnsavedPrompt));
    const auto save = PathUtils::Utf8ToWide(Tr(i18n::Str::BtnSaveChanges));
    const auto discard = PathUtils::Utf8ToWide(Tr(i18n::Str::BtnDiscardChanges));
    const auto cancel = PathUtils::Utf8ToWide(Tr(i18n::Str::BtnCancelAction));
    const TASKDIALOG_BUTTON buttons[] = {{IDYES, save.c_str()}, {IDNO, discard.c_str()}, {IDCANCEL, cancel.c_str()}};
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = hwnd;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    config.pszWindowTitle = title.c_str();
    config.pszMainInstruction = prompt.c_str();
    config.pszMainIcon = TD_WARNING_ICON;
    config.cButtons = static_cast<UINT>(std::size(buttons));
    config.pButtons = buttons;
    config.nDefaultButton = IDCANCEL;
    int choice = IDCANCEL;
    if (FAILED(TaskDialogIndirect(&config, &choice, nullptr, nullptr))) return false;
    if (choice == IDNO) return true;
    if (choice == IDYES) QueueProjectSave(studio, hwnd, false, std::move(afterSave));
    return false;
}

bool RequestClose(StudioState& studio, HWND hwnd) {
    if (activeJob) {
        closeAfterSuccess = true;
        return false;
    }
    return ConfirmUnsavedChanges(studio, hwnd, [hwnd] { PostMessage(hwnd, WM_CLOSE, 0, 0); });
}

bool OpenFirmware(StudioState& studio, HWND hwnd, const std::string& requestedPath) {
    if (IsFileActionActive()) return false;
    std::string path = requestedPath;
    {
        FileActionScope scope;
        if (path.empty() && !PathUtils::PickFileDialog(hwnd,
                L"Firmware or project (*.vfwp;*.zwdf;*.swdf;*.xwdf)\0*.vfwp;*.zwdf;*.swdf;*.xwdf\0Voopoo Firmware Project (*.vfwp)\0*.vfwp\0All Files (*.*)\0*.*\0", path))
            return false;
    }
    if (!ConfirmUnsavedChanges(studio, hwnd, [&studio, path] { QueueOpen(studio, path); }))
        return static_cast<bool>(activeJob);
    return QueueOpen(studio, path);
}

bool ImportFolder(StudioState& studio, HWND, const std::string& path) {
    if (IsFileActionActive()) return false;
    return QueuePng(studio, FileAction::ImportPng, path);
}

bool ExportImages(StudioState& studio, HWND, const std::string& path) {
    if (IsFileActionActive()) return false;
    return QueuePng(studio, FileAction::ExportPng, path);
}

bool ImportImage(StudioState& studio, HWND, const std::string& path, int catalogIndex) {
    if (IsFileActionActive() || !studio.hasFirmware) return false;
    if (catalogIndex < 0) {
        if (studio.selectedVisibleIdx < 0 ||
            studio.selectedVisibleIdx >= static_cast<int>(studio.visibleAssetIndices.size())) return false;
        catalogIndex = static_cast<int>(studio.visibleAssetIndices[studio.selectedVisibleIdx]);
    }
    if (catalogIndex >= static_cast<int>(studio.catalog.size())) return false;
    return QueuePng(studio, FileAction::ImportPng, path, static_cast<size_t>(catalogIndex));
}

bool OpenBuiltin(StudioState& studio, HWND hwnd, FirmwareModel model, FirmwareEdition edition) {
    if (IsFileActionActive()) return false;
    const std::string path = FirmwareDefaultFilename(model, edition);
    if (!ConfirmUnsavedChanges(studio, hwnd,
            [&studio, path, model, edition] { QueueOpen(studio, path, true, model, edition); }))
        return static_cast<bool>(activeJob);
    return QueueOpen(studio, path, true, model, edition);
}
bool IsLoadingBuiltin() { return activeJob && activeJob->action == FileAction::Builtin; }


static void RenderPreparedReview(const FirmwareSave::Prepared& prepared, bool stale) {
    ImGui::TextWrapped("%s", Tr(i18n::Str::ExportReviewIntro));
    ImGui::SeparatorText(Tr(i18n::Str::ExportReviewChanges));
    const auto& document = prepared.Snapshot();
    if (prepared.Changes().empty()) ImGui::TextDisabled("%s", Tr(i18n::Str::ExportReviewNoChanges));
    for (size_t i = 0; i < prepared.Changes().size(); ++i) {
        const auto& change = prepared.Changes()[i];
        ImGui::PushID(static_cast<int>(i));
        switch (change.kind) {
        case FirmwareSave::ChangeKind::Screen: {
            const i18n::Str labels[] = {i18n::Str::PatchBootLogo, i18n::Str::PatchShutdownLogo, i18n::Str::PatchDeviceInfo};
            bool enabled = document.screenPatches[change.target].enabled;
            ImGui::BeginDisabled();
            ImGui::Checkbox(Tr(labels[change.target]), &enabled);
            ImGui::EndDisabled();
            break;
        }
        case FirmwareSave::ChangeKind::Menu: {
            bool enabled = document.menuPatch.enabled;
            ImGui::BeginDisabled();
            ImGui::Checkbox(Tr(i18n::Str::PatchDeviceMenu), &enabled);
            if (enabled) {
                const std::pair<uint32_t, i18n::Str> modules[] = {
                    {FirmwareMenu::InformationMask, i18n::Str::PatchMenuInformation},
                    {FirmwareMenu::VoltageMask, i18n::Str::PatchMenuVoltage},
                    {FirmwareMenu::PuffEditorMask, i18n::Str::PatchMenuPuffEditor},
                    {FirmwareMenu::ScoreEditorMask, i18n::Str::PatchMenuScoreEditor},
                    {FirmwareMenu::RankSelectorMask, i18n::Str::PatchMenuRankSelector}};
                ImGui::Indent();
                for (const auto& [mask, label] : modules)
                    if (document.menuPatch.enabledItems & mask) ImGui::BulletText("%s", Tr(label));
                ImGui::Unindent();
            }
            ImGui::EndDisabled();
            break;
        }
        case FirmwareSave::ChangeKind::Layout: {
            const auto element = std::find_if(document.layoutElements.begin(), document.layoutElements.end(),
                [&](const auto& item) { return item.id == change.id; });
            const auto position = document.layoutOffsets.find(change.id);
            ImGui::BulletText("%s: %s (%+d, %+d)", Tr(i18n::Str::TabFirmwareLayout),
                i18n::ResourceText(element == document.layoutElements.end() ? change.id : element->title),
                position->second.x, position->second.y);
            break;
        }
        case FirmwareSave::ChangeKind::Resource: {
            const int asset = document.FindAssetIndex(change.id);
            ImGui::BulletText("%s: %s", Tr(i18n::Str::PanelResourcesHeader),
                i18n::ResourceText(asset < 0 ? change.id : document.catalog[size_t(asset)].title));
            break;
        }
        case FirmwareSave::ChangeKind::Palette: {
            const auto color = std::find_if(document.palette.begin(), document.palette.end(),
                [&](const auto& item) { return item.spec.id == change.id; });
            ImGui::BulletText("%s: %s", Tr(i18n::Str::SupportPalette),
                i18n::ResourceText(color == document.palette.end() ?
                    std::string_view(change.id) : std::string_view(color->spec.label)));
            break;
        }
        }
        ImGui::PopID();
    }
    ImGui::SeparatorText(Tr(i18n::Str::PatchMemoryTitle));
    const auto unavailable = [] {
        ImGui::TextDisabled("%s", Tr(i18n::Str::ExportReviewUnavailable));
        ImGui::SetItemTooltip("%s", Tr(i18n::Str::ExportReviewCapacityNote));
    };
    if (ImGui::BeginTable("ExportReviewRevisions", 4, ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("HW");
        ImGui::TableSetupColumn(Tr(i18n::Str::ExportReviewCapacity));
        ImGui::TableSetupColumn(Tr(i18n::Str::PatchMemoryUsed));
        ImGui::TableSetupColumn(Tr(i18n::Str::PatchMemoryFree));
        ImGui::TableHeadersRow();
        for (const auto& revision : prepared.Revisions()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%04X", unsigned(revision.hardwareCode));
            ImGui::TableNextColumn();
            if (revision.capacityKnown) ImGui::Text(Tr(i18n::Str::PatchMemoryBytes), revision.capacityBytes);
            else unavailable();
            ImGui::TableNextColumn();
            if (revision.capacityKnown)
                ImGui::Text("%zu B (%.1f%%)", revision.usedBytes, 100.0 * revision.usedBytes / revision.capacityBytes);
            else ImGui::Text(Tr(i18n::Str::PatchMemoryBytes), revision.usedBytes);
            ImGui::TableNextColumn();
            if (revision.capacityKnown) {
                const auto free = int64_t(revision.capacityBytes) - int64_t(revision.usedBytes);
                ImGui::Text("%lld B (%.1f%%)", static_cast<long long>(free), 100.0 * free / revision.capacityBytes);
            } else unavailable();
        }
        ImGui::EndTable();
    }
    if (stale || !prepared.Valid()) {
        ImGui::SeparatorText(Tr(i18n::Str::ExportReviewIssues));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.45f, 0.35f, 1));
        if (stale) ImGui::TextWrapped("%s", Tr(i18n::Str::ExportReviewStale));
        for (const auto& issue : prepared.Issues()) ImGui::TextWrapped("%s", issue.c_str());
        for (const auto& revision : prepared.Revisions())
            for (const auto& issue : revision.issues) ImGui::TextWrapped("HW %04X: %s", unsigned(revision.hardwareCode), issue.c_str());
        ImGui::PopStyleColor();
    }
}

void RenderFileProgress(StudioState& studio) {
    static bool wasOpen = false;
    static Ui::ModalTransition transition;
    static bool closing = false;
    const char* popup = "###FileOperationProgress";
    if (activeJob && activeJob->action == FileAction::Builtin) return;
    FirmwareReviewSnapshot review;
    const bool reviewing = GetFirmwareReview(review);
    const bool reviewStale = reviewing && (!review.prepared->Matches(studio) ||
        (patchEditor && UiFirmwarePatches::HasDraftChanges(*patchEditor, studio)));
    if (!activeJob && !hasCompletedProgress) {
        if (wasOpen && ImGui::BeginPopupModal(popup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        wasOpen = false;
        completionPending = false;
        auto continuation = std::move(pendingContinuation);
        pendingContinuation = {};
        if (continuation) continuation();
        return;
    }
    FileProgressSnapshot progress;
    GetFileProgress(progress);
    const std::string windowTitle = std::string(Tr(progress.titleLabel)) + popup;
    if (!ImGui::IsPopupOpen(popup)) ImGui::OpenPopup(popup);
    if (!wasOpen) {
        transition = {};
        closing = false;
    }
    wasOpen = true;
    const auto center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (reviewing) {
        const auto size = ImGui::GetMainViewport()->WorkSize;
        ImGui::SetNextWindowSize(ImVec2((std::min)(780.0f, size.x - 32), (std::min)(760.0f, size.y - 32)));
    } else ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f));
    transition.Update(!closing, ImGui::GetTime());
    const float opacity = transition.Opacity();
    const auto requestClose = [&] {
        closing = true;
        transition.Update(false, ImGui::GetTime());
    };
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * opacity);
    if (!ImGui::BeginPopupModal(windowTitle.c_str(), nullptr, ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | (reviewing ? 0 : ImGuiWindowFlags_AlwaysAutoResize))) {
        ImGui::PopStyleVar();
        return;
    }
    // ImGui otherwise animates dimming independently and keeps it opaque on close.
    ImGui::GetCurrentContext()->DimBgRatio = opacity;
    // Review/close starts only after UI-thread adoption and continuation setup.
    progress.done = !activeJob && hasCompletedProgress;
    const ImVec2 windowMin = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const bool inside = ImGui::IsMouseHoveringRect(windowMin,
        ImVec2(windowMin.x + windowSize.x, windowMin.y + windowSize.y), false);
    if (!closing && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (inside) progressPinned = true;
        else if (progress.done) requestClose();
    }
    if (progress.done && completedVisibleAt < 0) completedVisibleAt = ImGui::GetTime();
    if (!progress.filename.empty()) ImGui::TextWrapped("%s", progress.filename.c_str());
    ImGui::Spacing();
    if (reviewing) {
        const bool stale = !review.prepared->Matches(studio) ||
            (patchEditor && UiFirmwarePatches::HasDraftChanges(*patchEditor, studio));
        ImGui::BeginChild("ExportReviewBody", ImVec2(0, -170));
        RenderPreparedReview(*review.prepared, stale);
        ImGui::EndChild();
    }
    const ImVec2 start = ImGui::GetCursorScreenPos();
    constexpr float radius = 9.0f;
    auto* draw = ImGui::GetWindowDrawList();
    const ImU32 resultColor = ImGui::GetColorU32(completedCancelled ? ImGuiCol_PlotHistogram : ImGuiCol_CheckMark);
    if (progress.done || reviewing) {
        if (reviewing ? review.prepared->Valid() && !reviewStale : completedSuccessfully) {
            draw->AddLine(ImVec2(start.x + 3, start.y + 11), ImVec2(start.x + 8, start.y + 16), resultColor, 2.5f);
            draw->AddLine(ImVec2(start.x + 8, start.y + 16), ImVec2(start.x + 20, start.y + 4), resultColor, 2.5f);
        } else if (completedCancelled) {
            draw->AddLine(ImVec2(start.x + 3, start.y + 11), ImVec2(start.x + 20, start.y + 11), resultColor, 2.5f);
        } else {
            const ImU32 errorColor = ImGui::GetColorU32(ImVec4(1, 0.35f, 0.3f, 1));
            draw->AddLine(ImVec2(start.x + 4, start.y + 4), ImVec2(start.x + 19, start.y + 19), errorColor, 2.5f);
            draw->AddLine(ImVec2(start.x + 19, start.y + 4), ImVec2(start.x + 4, start.y + 19), errorColor, 2.5f);
        }
    } else {
        const float angle = static_cast<float>(ImGui::GetTime() * 5.0);
        for (int segment = 0; segment < 12; ++segment) {
            const float a = angle + static_cast<float>(segment) * 0.52359878f;
            const ImVec2 position(start.x + radius + std::cos(a) * radius,
                start.y + radius + std::sin(a) * radius);
            draw->AddCircleFilled(position, 1.8f,
                ImGui::GetColorU32(ImGuiCol_CheckMark, (segment + 1) / 12.0f));
        }
    }
    ImGui::Dummy(ImVec2(24, 22));
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextWrapped("%s", reviewing ?
        Tr(reviewStale ? i18n::Str::ExportReviewStale :
            review.prepared->Valid() ? i18n::Str::ExportReviewReady : i18n::Str::ExportReviewIssues) :
        progress.done ? Tr(completedSuccessfully ? i18n::Str::FileTaskComplete :
            completedCancelled ? i18n::Str::FileTaskCancelled : i18n::Str::FileTaskFailed) :
        progress.cancelling ? Tr(i18n::Str::FileTaskCancelling) :
        progress.log.empty() ? Tr(i18n::Str::FileTaskPreparing) : progress.log.back().c_str());
    if (progress.total != 0) {
        const auto completed = progress.completed;
        char count[64];
        snprintf(count, sizeof(count), "%zu / %zu", completed, progress.total);
        ImGui::ProgressBar(static_cast<float>(completed) / static_cast<float>(progress.total),
            ImVec2(-1, 0), count);
    }
    static std::vector<std::string> displayedLog;
    static std::string logText;
    if (displayedLog != progress.log) {
        displayedLog = std::move(progress.log);
        logText.clear();
        for (const auto& entry : displayedLog) {
            logText += entry;
            logText += '\n';
        }
    }
    ImGui::PushFont(UiMarkdown::MonospaceFont(), 15.0f);
    ImGui::InputTextMultiline("##FileOperationLog", logText.data(), logText.size() + 1,
        ImVec2(-1, reviewing ? 90.0f : 140.0f), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);
    ImGui::PopFont();
    if (ImGui::SmallButton(Tr(i18n::Str::FileTaskCopyLog)))
        ImGui::SetClipboardText(logText.c_str());
    ImGui::Spacing();
    if (reviewing) {
        const bool stale = !review.prepared->Matches(studio) ||
            (patchEditor && UiFirmwarePatches::HasDraftChanges(*patchEditor, studio));
        ImGui::BeginDisabled(stale || !review.prepared->Valid());
        if (ImGui::Button(Tr(i18n::Str::ExportReviewConfirm), ImVec2(ImGui::GetContentRegionAvail().x * 0.6f, 0)))
            ConfirmFirmwareExport(studio);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(Tr(i18n::Str::BtnCancelAction), ImVec2(-1, 0))) CancelFirmwareExport();
    } else if (progress.done) {
        const double remaining = (std::max)(0.0, double(StudioPreferences::Get().logReviewSeconds) - (ImGui::GetTime() - completedVisibleAt));
        if (!progressPinned)
            ImGui::Text(Tr(i18n::Str::FileTaskClosing), std::ceil(remaining));
        else
            ImGui::TextUnformatted(Tr(i18n::Str::FileTaskPinned));
        if ((ImGui::Button(Tr(i18n::Str::FileTaskClose), ImVec2(-1, 0)) || (!progressPinned && remaining == 0)) && !closing)
            requestClose();
    } else {
        ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
        ImGui::BeginDisabled(!progress.cancellable);
        if (ImGui::Button(Tr(i18n::Str::BtnCancelAction), ImVec2(-1, 0))) {
            auto expected = Cancellation::Available;
            activeJob->cancellation.compare_exchange_strong(expected, Cancellation::Requested);
        }
        ImGui::EndDisabled();
    }
    if (closing && transition.FullyHidden()) {
        hasCompletedProgress = false;
        completedProgress = {};
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
    ImGui::PopStyleVar();
}


void RenderMenuBar(StudioState& studio, HWND hwnd) {
    // Selectable highlights extend by half ItemSpacing: match the popup padding
    // so menu rows reach both edges and the first menu starts flush left.
    ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, ImGui::GetStyle().WindowPadding.x * 2.0f);
    if (!ImGui::BeginMenuBar()) {
        ImGui::PopStyleVar();
        return;
    }
    if (ImGui::BeginMenu(Tr(i18n::Str::MenuFile))) {
        if (ImGui::MenuItem(Tr(i18n::Str::MenuCreate)))
            studio.showStartupChooser = true;
        if (ImGui::MenuItem(Tr(i18n::Str::MenuOpen))) OpenFirmware(studio, hwnd);
        if (ImGui::MenuItem(Tr(i18n::Str::MenuSaveProject), "Ctrl+S", false, studio.hasFirmware))
            SaveProject(studio, hwnd);
        if (ImGui::MenuItem(Tr(i18n::Str::MenuSaveProjectAs), "Ctrl+Shift+S", false, studio.hasFirmware))
            SaveProject(studio, hwnd, true);
        ImGui::Separator();
        if (ImGui::MenuItem(Tr(i18n::Str::MenuExport), nullptr, false, studio.hasFirmware)) {
            FileActionScope scope;
            std::string dir;
            if (PathUtils::PickFolderDialog(hwnd, dir))
                ExportImages(studio, hwnd, dir);
        }
        if (ImGui::MenuItem(Tr(i18n::Str::MenuImport), nullptr, false, studio.hasFirmware)) {
            FileActionScope scope;
            std::string dir;
            if (PathUtils::PickFolderDialog(hwnd, dir))
                QueuePng(studio, FileAction::ImportPng, dir);
        }
        ImGui::Separator();
        if (ImGui::MenuItem(Tr(i18n::Str::MenuBuild), nullptr, false, studio.hasFirmware))
            SaveFirmware(studio, hwnd);
        ImGui::Separator();
        if (ImGui::MenuItem(Tr(i18n::Str::MenuExit))) PostMessage(hwnd, WM_CLOSE, 0, 0);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(Tr(i18n::Str::MenuEdit))) {
        if (ImGui::MenuItem(Tr(i18n::Str::BtnUndo), "Ctrl+Z", false, studio.hasFirmware && studio.CanUndo())) {
            const std::string description = studio.undoStack.back().desc;
            studio.Undo();
            char message[256];
            snprintf(message, sizeof(message), Tr(i18n::Str::MsgUndoPerformed), description.c_str());
            studio.statusMsg = message;
            studio.statusIsErr = false;
        }
        if (ImGui::MenuItem(Tr(i18n::Str::BtnRedo), "Ctrl+Y", false, studio.hasFirmware && studio.CanRedo())) {
            const std::string description = studio.redoStack.back().desc;
            studio.Redo();
            char message[256];
            snprintf(message, sizeof(message), Tr(i18n::Str::MsgRedoPerformed), description.c_str());
            studio.statusMsg = message;
            studio.statusIsErr = false;
        }
        ImGui::Separator();
        if (ImGui::MenuItem(Tr(i18n::Str::BtnRevertAllStaged), nullptr, false,
                studio.hasFirmware && (!studio.replacements.empty() || !studio.layoutOffsets.empty() ||
                    studio.screenPatches != studio.originalScreenPatches ||
                    studio.menuPatch != studio.originalMenuPatch ||
                    studio.screenPatchSources != studio.originalScreenPatchSources))) {
            studio.RevertAllAssets();
            studio.statusMsg = Tr(i18n::Str::MsgAllReverted);
            studio.statusIsErr = false;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu(Tr(i18n::Str::MenuLanguage))) {
        for (const auto& language : i18n::GetSupportedLanguages()) {
            if (ImGui::MenuItem(language.displayName, nullptr, language.lang == i18n::GetCurrentLanguage())) {
                i18n::SetLanguage(language.lang);
                studio.SynchronizeEditorState();
                i18n::SaveSettings();
            }
        }
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
    ImGui::PopStyleVar();
}


void RenderTab(
    StudioState& studio,
    HWND hwnd,
    ID3D11Device* device,
    ID3D11DeviceContext* context
) {
    char _tb[128];
    snprintf(_tb, sizeof(_tb), "%s###TabResStudio", Tr(i18n::Str::TabResourceStudio));
    if (Ui::BeginTabItem(_tb)) {
        activeFrame = ImGui::GetFrameCount();
    // Two Column Layout
    float leftWidth = 430.0f;
    ImGui::BeginChild("ResourceListPanel", ImVec2(leftWidth, 0.0f), true);
    {
        size_t nonStockCount = studio.CountTotalNonStockAssets();
        char resHdr[128];
        if (nonStockCount > 0) {
            snprintf(resHdr, sizeof(resHdr), "%s (%zu | %zu custom)", Tr(i18n::Str::PanelResourcesHeader), studio.catalog.size(), nonStockCount);
        } else {
            snprintf(resHdr, sizeof(resHdr), "%s (%zu | stock)", Tr(i18n::Str::PanelResourcesHeader), studio.catalog.size());
        }
        ImGui::SeparatorText(resHdr);

        const char* groupsCombo[] = {
            Tr(i18n::Str::GroupAll),
            Tr(i18n::Str::GroupAlerts),
            Tr(i18n::Str::GroupFonts),
            Tr(i18n::Str::GroupScore),
            Tr(i18n::Str::GroupStatusLabels),
            Tr(i18n::Str::GroupControls),
            i18n::ResourceText("Units"),
            Tr(i18n::Str::GroupScreens),
            Tr(i18n::Str::GroupAnimations)
        };
        if (Ui::BeginControlTable("ResourceFilters")) {
        Ui::ControlRow(Tr(i18n::Str::LabelCategory));
        if (ImGui::Combo("##Category", &studio.groupFilterIdx, groupsCombo, IM_ARRAYSIZE(groupsCombo))) {
            studio.UpdateVisible();
        }

        Ui::ControlRow(Tr(i18n::Str::LabelSearch));
        if (ImGui::InputTextWithHint("##Search", Tr(i18n::Str::SearchPlaceholder), studio.filterBuf, sizeof(studio.filterBuf))) {
            studio.UpdateVisible();
        }
        ImGui::EndTable();
        }
        if (ImGui::Checkbox(Tr(i18n::Str::FilterOnlyCustom), &studio.filterOnlyCustom)) {
            studio.UpdateVisible();
        }

        ImGui::Spacing();

        float listHeight = ImGui::GetContentRegionAvail().y - (FirmwareHasPalette(studio.model) ? 290.0f : 0.0f);
        if (listHeight < 120.0f) listHeight = 120.0f;
        ImGui::BeginChild("ListScrollArea", ImVec2(0.0f, listHeight), true);
        {
            if (!studio.hasFirmware) {
                ImGui::TextDisabled("%s", Tr(i18n::Str::NoFirmwareLoadedList));
            } else if (studio.visibleAssetIndices.empty()) {
                ImGui::TextDisabled("%s", Tr(i18n::Str::NoResourcesMatchFilter));
            } else {
                for (size_t v = 0; v < studio.visibleAssetIndices.size(); ++v) {
                    size_t catIdx = studio.visibleAssetIndices[v];
                    const auto& spec = studio.catalog[catIdx];
                    bool selected = ((int)v == studio.selectedVisibleIdx);

                    bool isModified = (studio.replacements.find(spec.id) != studio.replacements.end());
                    bool isStock = studio.IsActiveMatchingReference(catIdx);

                    if (isModified) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                    } else if (!isStock) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
                    }

                    char itemLabel[256];
                    const char* tag = isModified ? "[M] " : (!isStock ? "[C] " : "");
                    snprintf(itemLabel, sizeof(itemLabel), "%s%s (%ux%u)###Item_%s", tag, i18n::ResourceText(spec.title), spec.width, spec.height, spec.id.c_str());

                    if (ImGui::Selectable(itemLabel, selected)) {
                        studio.selectedVisibleIdx = (int)v;
                        Rgb8 c = studio.GetForegroundForAsset(spec);
                        strncpy_s(studio.assetHexEditBuf, c.ToHex().c_str(), sizeof(studio.assetHexEditBuf));
                        studio.textureNeedsUpdate = true;
                    }
                    if (isModified || !isStock) {
                        ImGui::PopStyleColor();
                    }
                }
            }
        }
        ImGui::EndChild();

        if (FirmwareHasPalette(studio.model)) {
            ImGui::Spacing();
            UiPalette::RenderPaletteEditor(studio);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right Panel: Selected Resource Inspector & Preview
    ImGui::BeginChild("ResourcePreviewPanel", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
    {
        if (!studio.hasFirmware || studio.selectedVisibleIdx < 0 || studio.selectedVisibleIdx >= (int)studio.visibleAssetIndices.size()) {
            ImGui::TextDisabled("%s", Tr(i18n::Str::SelectResourcePrompt));
        } else {
            size_t catIdx = studio.visibleAssetIndices[studio.selectedVisibleIdx];
            const auto& spec = studio.catalog[catIdx];
            bool isModified = studio.replacements.find(spec.id) != studio.replacements.end();

            ImGui::SeparatorText(i18n::ResourceText(spec.title));

            if (AssetUsesModeColor(spec, studio.model)) {
                if (ImGui::Checkbox(Tr(i18n::Str::LabelPreviewCyanMode), &studio.previewCyanMode))
                    studio.textureNeedsUpdate = true;
            }
            if (spec.id.rfind("meter_", 0) == 0) {
                if (ImGui::SliderInt(Tr(i18n::Str::LabelPreviewRank), &studio.previewRank, 0, 7))
                    studio.textureNeedsUpdate = true;
            }
            Rgb8 fgCol = studio.GetForegroundForAsset(spec).ScreenColor();
            const char* pal = PaletteIdForAsset(spec, studio.previewRank, studio.previewCyanMode, studio.edition, studio.model);
            const std::string fixedColorName = fgCol.ToHex();
            const char* colName = spec.format == AssetFormat::Rgb565 ? "RGB565" : (pal ? GetRoleLabelTr(pal) : fixedColorName.c_str());
            if (pal && (studio.edition == FirmwareEdition::Factory || FirmwareExtensionFor(studio.model))) {
                for (const auto& entry : studio.palette) {
                    if (std::strcmp(entry.spec.id, pal) == 0) { colName = entry.spec.label; break; }
                }
            }
            ImGui::Text(Tr(i18n::Str::LabelMetaDetails), spec.id.c_str(), i18n::ResourceText(spec.group), colName);
            if (spec.format != AssetFormat::Rgb565) {
                ImGui::SameLine();
                ImGui::ColorButton("##AssetColorSwatch", ImVec4(fgCol.r / 255.0f, fgCol.g / 255.0f, fgCol.b / 255.0f, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
            }
            ImGui::Text(Tr(i18n::Str::LabelMetaHardware),
                spec.address, spec.width, spec.height, spec.byte_len(),
                static_cast<size_t>(std::count_if(studio.locations[catIdx].begin(), studio.locations[catIdx].end(),
                    [](size_t offset) { return offset != SIZE_MAX; })));
            if (!spec.recordAddresses.empty()) {
                ImGui::Text("Source: %04X | HW:", spec.sourceCode ? spec.sourceCode : FirmwareCanonicalCode(studio.model));
                for (size_t r = 0; r < studio.firmware.records.size(); ++r) {
                    if (studio.locations[catIdx][r] == SIZE_MAX) continue;
                    ImGui::SameLine();
                    const auto& record = studio.firmware.records[r];
                    ImGui::Text("%02X%02X", record.device_code[0], record.device_code[1]);
                }
            }

            bool matchesRef = studio.IsActiveMatchingReference(catIdx);
            if (isModified) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", Tr(i18n::Str::StatusModifiedStaged));
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "%s", Tr(i18n::Str::StatusOriginal));
            }
            ImGui::SameLine();
            if (matchesRef) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "|  %s", Tr(i18n::Str::StatusStockOriginal));
            } else {
                size_t diffBytes = studio.CountDifferencesFromReference(catIdx);
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "|  %s (%zu B diff)", Tr(i18n::Str::StatusCustomDiffers), diffBytes);
            }

            // Actions for this resource
            const float actionWidth = (std::min)(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 32);
            const int actionColumns = actionWidth >= ImGui::GetFontSize() * 32 ? 2 : 1;
            if (ImGui::BeginTable("ResourceActions", actionColumns, ImGuiTableFlags_SizingStretchSame,
                ImVec2(actionWidth, 0))) {
            ImGui::TableNextColumn();
            if (ImGui::Button(Tr(i18n::Str::BtnCopyClipboard), ImVec2(-1, 0))) {
                CopyCurrentSelectedAsset(studio, hwnd);
            }
            ImGui::TableNextColumn();
            if (ImGui::Button(Tr(i18n::Str::BtnPasteClipboard), ImVec2(-1, 0))) {
                PasteIntoCurrentSelectedAsset(studio, hwnd);
            }
            ImGui::TableNextColumn();
            if (ImGui::Button(Tr(i18n::Str::BtnReplaceFromPng), ImVec2(-1, 0))) {
                FileActionScope scope;
                std::string picked;
                if (PathUtils::PickFileDialog(hwnd, L"PNG Image (*.png)\0*.png\0All Files (*.*)\0*.*\0", picked))
                    QueuePng(studio, FileAction::ImportPng, picked, catIdx);
            }
            if (isModified) {
                ImGui::TableNextColumn();
                if (ImGui::Button(Tr(i18n::Str::BtnRestoreOriginal), ImVec2(-1, 0))) {
                    studio.PushUndo("Restore " + spec.id);
                    studio.SetBitmapReplacement(catIdx, studio.originals[catIdx]);
                    char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgRestoredOriginal), spec.id.c_str());
                    studio.statusMsg = _m;
                    studio.statusIsErr = false;
                    studio.textureNeedsUpdate = true;
                }
            }
            if (!matchesRef) {
                ImGui::TableNextColumn();
                if (ImGui::Button(Tr(i18n::Str::BtnRestoreStock), ImVec2(-1, 0))) {
                    studio.RevertAssetToStock(catIdx);
                    char _buf[256];
                    snprintf(_buf, sizeof(_buf), Tr(i18n::Str::MsgRestoredToStock), spec.id.c_str());
                    studio.statusMsg = _buf;
                    studio.statusIsErr = false;
                    studio.textureNeedsUpdate = true;
                }
            }
            ImGui::TableNextColumn();
            if (ImGui::Button(Tr(i18n::Str::BtnExportThisPng), ImVec2(-1, 0))) {
                FileActionScope scope;
                std::string def = spec.id + ".png";
                std::string outPath;
                if (PathUtils::SaveFileDialog(hwnd, L"PNG Image (*.png)\0*.png\0", def, outPath))
                    QueuePng(studio, FileAction::ExportPng, outPath, catIdx);
            }
            if (spec.id == "boot_voopoo_animated") {
                ImGui::TableNextColumn();
                if (ImGui::Button(Tr(i18n::Str::BtnExportHorizontal), ImVec2(-1, 0))) {
                    FileActionScope scope;
                    std::string def = "boot_voopoo_animated_horizontal_136x22.png";
                    std::string outPath;
                    if (PathUtils::SaveFileDialog(hwnd, L"PNG Image (*.png)\0*.png\0", def, outPath))
                        QueuePng(studio, FileAction::ExportPng, outPath, catIdx, true);
                }
            }
            ImGui::EndTable();
            }

            ImGui::Spacing();
            if (spec.format == AssetFormat::Rgb565) {
                ImGui::SeparatorText(Tr(i18n::Str::LabelBrushColor));
                const Rgb8 brush = studio.GetPaintColor(spec).ScreenColor();
                float color[3] = {brush.r / 255.0f, brush.g / 255.0f, brush.b / 255.0f};
                ImGui::SetNextItemWidth((std::min)(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 22));
                if (ImGui::ColorEdit3("##Rgb565Brush", color)) {
                    studio.PushUndo(std::string(Tr(i18n::Str::LabelBrushColor)) + ": " + spec.id);
                    studio.SetAssetPaintColor(spec.id, {
                        (uint8_t)(color[0] * 255 + 0.5f), (uint8_t)(color[1] * 255 + 0.5f),
                        (uint8_t)(color[2] * 255 + 0.5f)});
                }
                ImGui::TextDisabled("%s", Tr(i18n::Str::InfoNativeRgb565));
            } else {
                ImGui::TextDisabled("%s", Tr(i18n::Str::InfoFirmwareColors));
                const std::string bgHex = studio.GetBackgroundForAsset(spec).ScreenColor().ToHex();
                ImGui::Text(Tr(i18n::Str::LabelPreviewBackground), bgHex.c_str());
            }

            // View Controls
            ImGui::Spacing();
            if (Ui::BeginControlTable("ResourcePreviewControls",
                (std::min)(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 30))) {
                Ui::ControlRow(Tr(i18n::Str::LabelZoom));
                Ui::ZoomSlider("##Zoom", studio.previewZoom, StudioPreferences::ResourceMaximumZoom);
                ImGui::EndTable();
            }
            ImGui::Checkbox(Tr(i18n::Str::LabelPixelGrid), &studio.previewShowGrid);

            static bool s_showStockCompare = true;
            if (!matchesRef) {
                ImGui::SameLine();
                ImGui::Checkbox(Tr(i18n::Str::LabelCompareWithStock), &s_showStockCompare);
            }

            ImGui::Separator();

            // Pixel Touch-up Toolbar (4.4 with Brush Size, Eyedropper, FloodFill)
            UiPixelEditor::RenderToolbar(studio, catIdx);

            ImGui::Spacing();

            // Interactive Hardware Texture Canvas (2.1)
            UiPixelEditor::RenderCanvas(studio, catIdx, device, context);

            // Stock Reference Comparison
            if (!matchesRef && s_showStockCompare) {
                const uint8_t* refRaw = studio.GetReferenceRaw(catIdx);
                if (refRaw) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "%s", Tr(i18n::Str::LabelStockReferencePreview));
                    float pZoom = (float)studio.previewZoom;
                    float canvasW = spec.width * pZoom;
                    float canvasH = spec.height * pZoom;

                    UpdateReferenceTexture(studio, catIdx, device, context);
                    if (studio.referencePreviewTexture.IsValid()) {
                        ImGui::Image((ImTextureID)studio.referencePreviewTexture.GetSRV(), ImVec2(canvasW, canvasH));
                        if (studio.previewShowGrid && studio.previewZoom >= 4) {
                            const ImVec2 origin = ImGui::GetItemRectMin();
                            ImDrawList* drawList = ImGui::GetWindowDrawList();
                            const ImU32 gridColor = IM_COL32(80, 80, 90, 110);
                            for (uint32_t x = 1; x < spec.width; ++x) {
                                const float screenX = origin.x + x * pZoom;
                                drawList->AddLine(ImVec2(screenX, origin.y), ImVec2(screenX, origin.y + canvasH), gridColor);
                            }
                            for (uint32_t y = 1; y < spec.height; ++y) {
                                const float screenY = origin.y + y * pZoom;
                                drawList->AddLine(ImVec2(origin.x, screenY), ImVec2(origin.x + canvasW, screenY), gridColor);
                            }
                        }
                    }
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::EndTabItem();
    }
}

} // namespace UiResourceStudio

