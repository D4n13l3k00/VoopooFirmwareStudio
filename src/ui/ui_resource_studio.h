#pragma once

#include "app_state.h"
#include <d3d11.h>
#include <functional>

namespace UiFirmwarePatches { struct State; }
namespace FirmwareSave { class Prepared; }
namespace UiResourceStudio {

// The UI owns this editor; pass nullptr before destroying a bound editor.
void BindPatchEditor(UiFirmwarePatches::State* editor);
// Publish editable screens without reloading the VM. Emulator entry retains
// target-local undo; save/export closes the draft history.
void CommitPatchDrafts(StudioState& studio, bool retainHistory = false);
bool HasUnsavedChanges(const StudioState& studio);
bool UpdateReferenceTexture(StudioState& studio, size_t catIdx, ID3D11Device* device, ID3D11DeviceContext* context);

// Queues an asynchronous, mandatory review, including when path is supplied.
// No firmware/report/temp/backup is written until ConfirmFirmwareExport.
bool SaveFirmware(StudioState& studio, HWND hwnd, bool showSuccessModal = true, const std::string& path = {});
struct FirmwareReviewSnapshot {
    std::shared_ptr<const FirmwareSave::Prepared> prepared;
    std::string path;
};
bool GetFirmwareReview(FirmwareReviewSnapshot& snapshot);
bool ConfirmFirmwareExport(StudioState& studio);
bool CancelFirmwareExport();
bool SaveProject(StudioState& studio, HWND hwnd, bool saveAs = false);
// True only for immediate permission (clean document / discard). Saving returns
// false and calls afterSave on the UI thread only after a successful commit.
bool ConfirmUnsavedChanges(StudioState& studio, HWND hwnd, std::function<void()> afterSave = {});
bool RequestClose(StudioState& studio, HWND hwnd);
bool OpenFirmware(StudioState& studio, HWND hwnd, const std::string& path = {});
bool OpenBuiltin(StudioState& studio, HWND hwnd, FirmwareModel model, FirmwareEdition edition);
bool ImportFolder(StudioState& studio, HWND hwnd, const std::string& path);
bool ExportImages(StudioState& studio, HWND hwnd, const std::string& path);
bool ImportImage(StudioState& studio, HWND hwnd, const std::string& path, int catalogIndex = -1);
bool IsFileActionActive();
bool IsLoadingBuiltin();
void PollFileActions(StudioState& studio, HWND hwnd);
struct FileProgressSnapshot {
    std::string title;
    i18n::Str titleLabel = i18n::Str::FileTaskOpenFirmware;
    std::string filename;
    std::vector<std::string> log;
    size_t completed = 0;
    size_t total = 0;
    bool cancelling = false;
    bool cancellable = false;
    bool done = false;
};
// UI-thread query; worker-written log entries are copied under their mutex.
bool GetFileProgress(FileProgressSnapshot& snapshot);
void RenderFileProgress(StudioState& studio);
void RenderMenuBar(StudioState& studio, HWND hwnd);
bool IsActive();

void RenderTab(
    StudioState& studio,
    HWND hwnd,
    ID3D11Device* device,
    ID3D11DeviceContext* context
);

} // namespace UiResourceStudio

