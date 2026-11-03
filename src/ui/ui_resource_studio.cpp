#include "ui_resource_studio.h"
#include "ui_pixel_editor.h"
#include "ui_palette.h"
#include "path_utils.h"
#include "firmware_save.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include "../imgui/imgui.h"

namespace UiResourceStudio {

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
        FirmwareBitmap::DecodeMask(refRaw, spec.byte_len(), spec.width, spec.height, gray);
        FirmwareBitmap::ColorizeMask(gray.data(), spec.width, spec.height, fgCol, studio.GetBackgroundForAsset(spec).ScreenColor(), refRgba);
    }
    // Reference content, dimensions and palette can all change while the SRV stays valid.
    return studio.referencePreviewTexture.Update(device, context, refRgba.data(), spec.width, spec.height);
}



static bool fileActionActive = false;

bool IsFileActionActive() { return fileActionActive; }

struct FileActionScope {
    bool previous = fileActionActive;
    FileActionScope() { fileActionActive = true; }
    ~FileActionScope() { fileActionActive = previous; }
};

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

bool SaveFirmware(StudioState& studio, HWND hwnd, bool showSuccessModal) {
    if (!studio.hasFirmware) return false;
    FileActionScope scope;
    const std::string defaultName = PathUtils::WideToUtf8(
        PathUtils::ToFsPath(studio.firmwarePath).stem().wstring()) + "_custom.zwdf";
    std::string path;
    if (!PathUtils::SaveFileDialog(hwnd, L"VOOPOO Firmware (*.zwdf)\0*.zwdf\0", defaultName, path))
        return false;
    const auto result = FirmwareSave::Save(studio, path);
    char text[1536];
    if (!result.FirmwareSaved()) {
        const auto error = result.buildError.empty() ? DescribeSaveError(result.fileError) : result.buildError;
        snprintf(text, sizeof(text), Tr(i18n::Str::MsgSaveError), error.c_str());
        studio.statusIsErr = true;
    } else if (result.outcome == FirmwareSave::Outcome::FirmwareOnly) {
        snprintf(text, sizeof(text), Tr(i18n::Str::MsgFirmwareOnlySaved),
                 DescribeSaveError(result.fileError).c_str());
        studio.statusIsErr = true;
    } else {
        snprintf(text, sizeof(text), Tr(i18n::Str::MsgBuildSuccess),
                 PathUtils::WideToUtf8(PathUtils::ToFsPath(path).filename().wstring()).c_str());
        studio.statusIsErr = false;
        studio.showBuildModal = showSuccessModal;
    }
    studio.statusMsg = text;
    if (studio.statusIsErr) {
        // Also visible when a successful firmware-only save immediately opens
        // another document or closes the application.
        MessageBoxW(hwnd, PathUtils::Utf8ToWide(studio.statusMsg).c_str(),
                    PathUtils::Utf8ToWide(Tr(i18n::Str::AppTitle)).c_str(),
                    MB_OK | (result.FirmwareSaved() ? MB_ICONWARNING : MB_ICONERROR));
    }
    return result.FirmwareSaved();
}

bool ConfirmUnsavedChanges(StudioState& studio, HWND hwnd) {
    if (!studio.HasUnsavedChanges()) return true;
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
    return choice == IDYES && SaveFirmware(studio, hwnd, false);
}

bool OpenFirmware(StudioState& studio, HWND hwnd, const std::string& requestedPath) {
    FileActionScope scope;
    std::string path = requestedPath;
    if (path.empty() && !PathUtils::PickFileDialog(hwnd,
            L"VOOPOO Firmware (*.zwdf;*.swdf)\0*.zwdf;*.swdf\0All Files (*.*)\0*.*\0", path))
        return false;
    if (!ConfirmUnsavedChanges(studio, hwnd)) return false;
    std::string error;
    const bool opened = studio.OpenFirmware(path, error);
    char text[1024];
    if (opened) {
        snprintf(text, sizeof(text), Tr(i18n::Str::MsgSuccessOpened),
                 PathUtils::WideToUtf8(PathUtils::ToFsPath(path).filename().wstring()).c_str());
        studio.showBuildModal = false;
    } else {
        snprintf(text, sizeof(text), Tr(i18n::Str::MsgOpenError), error.c_str());
    }
    studio.statusMsg = text;
    studio.statusIsErr = !opened;
    return opened;
}

void RenderTab(
    StudioState& studio,
    HWND hwnd,
    ID3D11Device* device,
    ID3D11DeviceContext* context
) {
    char _tb[128];
    snprintf(_tb, sizeof(_tb), "%s###TabResStudio", Tr(i18n::Str::TabResourceStudio));
    if (ImGui::BeginTabItem(_tb)) {
        auto AutoSameLine = [](const char* nextLabel) {
        float needed = ImGui::CalcTextSize(nextLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float avail = ImGui::GetContentRegionAvail().x;
        if (avail >= needed + ImGui::GetStyle().ItemSpacing.x) {
            ImGui::SameLine();
        }
    };

    // Top Actions Bar
    if (ImGui::Button(Tr(i18n::Str::BtnOpenFirmware))) {
        OpenFirmware(studio, hwnd);
    }

    if (!studio.hasFirmware) ImGui::BeginDisabled();
    AutoSameLine(Tr(i18n::Str::BtnExportAll));
    if (ImGui::Button(Tr(i18n::Str::BtnExportAll))) {
        std::string dir;
        if (PathUtils::PickFolderDialog(hwnd, dir)) {
            fs::create_directories(PathUtils::ToFsPath(dir));
            size_t count = 0;
            std::string manifestPath = dir + "\\asset_manifest.json";
            auto mf = PathUtils::OpenWriteStream(manifestPath);
            mf << "{\n  \"format\": \"DRAG S Resource Studio export\",\n  \"resources\": [\n";

            for (size_t i = 0; i < studio.catalog.size(); ++i) {
                const auto& spec = studio.catalog[i];
                std::string pngFile = spec.id + ".png";
                std::string pngPath = dir + "\\" + pngFile;
                std::string err;

                if (spec.format == AssetFormat::Rgb565) {
                    const uint8_t* raw = studio.GetCurrentRaw(i);
                    std::vector<uint32_t> rgba;
                    FirmwareBitmap::DecodeRgb565(raw, spec.byte_len(), spec.width, spec.height, rgba);
                    FirmwareBitmap::SavePngRgb565(pngPath, rgba.data(), spec.width, spec.height, err);
                    if (spec.id == "boot_voopoo_animated") {
                        std::string rotPath = dir + "\\boot_voopoo_animated_horizontal_136x22.png";
                        FirmwareBitmap::SavePngRgb565Rotated(rotPath, rgba.data(), spec.width, spec.height, true, err);
                    }
                } else {
                    std::vector<uint8_t> gray;
                    const uint8_t* raw = studio.GetCurrentRaw(i);
                    FirmwareBitmap::DecodeMask(raw, spec.byte_len(), spec.width, spec.height, gray);
                    FirmwareBitmap::SavePng(pngPath, gray.data(), spec.width, spec.height, err);
                }
                count++;

                mf << "    { \"id\": \"" << spec.id << "\", \"title\": \"" << spec.title
                   << "\", \"width\": " << spec.width << ", \"height\": " << spec.height
                   << ", \"address\": \"0x" << std::hex << spec.address << std::dec << "\" }"
                   << (i + 1 < studio.catalog.size() ? ",\n" : "\n");
            }
            mf << "  ]\n}\n";
            char _m[512];
            snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgExportSuccess), (int)count, dir.c_str());
            studio.statusMsg = _m;
            studio.statusIsErr = false;
        }
    }
    AutoSameLine(Tr(i18n::Str::BtnImportFolder));
    if (ImGui::Button(Tr(i18n::Str::BtnImportFolder))) {
        std::string dir;
        if (PathUtils::PickFolderDialog(hwnd, dir)) {
            studio.PushUndo("Import folder: " + dir);
            size_t changed = 0, unchanged = 0;
            for (size_t i = 0; i < studio.catalog.size(); ++i) {
                const auto& spec = studio.catalog[i];
                std::string pngPath = dir + "\\" + spec.id + ".png";
                if (!fs::exists(PathUtils::ToFsPath(pngPath))) {
                    if (spec.id == "boot_voopoo_animated") {
                        std::string rotPath = dir + "\\boot_voopoo_animated_horizontal_136x22.png";
                        if (fs::exists(PathUtils::ToFsPath(rotPath))) pngPath = rotPath;
                        else continue;
                    } else {
                        continue;
                    }
                }

                if (spec.format == AssetFormat::Rgb565) {
                    std::vector<uint32_t> rgba;
                    std::string err;
                    if (FirmwareBitmap::LoadAndFitRgb565(pngPath, spec.width, spec.height, rgba, err)) {
                        std::vector<uint8_t> newRaw;
                        if (FirmwareBitmap::EncodeRgb565(rgba.data(), spec.width, spec.height, newRaw)) {
                            if (newRaw == studio.originals[i]) {
                                unchanged++;
                            } else {
                                changed++;
                            }
                            studio.SetBitmapReplacement(i, std::move(newRaw));
                        }
                    }
                } else {
                    std::vector<uint8_t> gray;
                    std::string err;
                    if (FirmwareBitmap::LoadAndFit(pngPath, spec.width, spec.height, gray, err)) {
                        std::vector<uint8_t> newRaw;
                        if (FirmwareBitmap::EncodeMaskPreserving(gray.data(), spec.width, spec.height, studio.originals[i].data(), spec.byte_len(), newRaw)) {
                            if (newRaw == studio.originals[i]) {
                                unchanged++;
                            } else {
                                changed++;
                            }
                            studio.SetBitmapReplacement(i, std::move(newRaw));
                        }
                    }
                }
            }
            char _m[512];
            snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgImportedChangedPngs), changed, unchanged);
            studio.statusMsg = _m;
            studio.statusIsErr = false;
            studio.textureNeedsUpdate = true;
        }
    }
    AutoSameLine(Tr(i18n::Str::BtnBuildFirmware));
    if (ImGui::Button(Tr(i18n::Str::BtnBuildFirmware), ImVec2(0, 0))) {
        SaveFirmware(studio, hwnd);
    }


    if (studio.hasFirmware) {
        AutoSameLine(Tr(i18n::Str::BtnUndo));
        bool canUndo = studio.CanUndo();
        if (!canUndo) ImGui::BeginDisabled();
        if (ImGui::Button(Tr(i18n::Str::BtnUndo))) {
            std::string desc = studio.undoStack.back().desc;
            studio.Undo();
            char buf[256];
            snprintf(buf, sizeof(buf), Tr(i18n::Str::MsgUndoPerformed), desc.c_str());
            studio.statusMsg = buf;
            studio.statusIsErr = false;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (canUndo) {
                char buf[256];
                snprintf(buf, sizeof(buf), Tr(i18n::Str::TooltipUndo), studio.undoStack.back().desc.c_str());
                ImGui::SetTooltip("%s", buf);
            } else {
                ImGui::SetTooltip("%s", Tr(i18n::Str::TooltipUndoEmpty));
            }
        }
        if (!canUndo) ImGui::EndDisabled();

        AutoSameLine(Tr(i18n::Str::BtnRedo));
        bool canRedo = studio.CanRedo();
        if (!canRedo) ImGui::BeginDisabled();
        if (ImGui::Button(Tr(i18n::Str::BtnRedo))) {
            std::string desc = studio.redoStack.back().desc;
            studio.Redo();
            char buf[256];
            snprintf(buf, sizeof(buf), Tr(i18n::Str::MsgRedoPerformed), desc.c_str());
            studio.statusMsg = buf;
            studio.statusIsErr = false;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (canRedo) {
                char buf[256];
                snprintf(buf, sizeof(buf), Tr(i18n::Str::TooltipRedo), studio.redoStack.back().desc.c_str());
                ImGui::SetTooltip("%s", buf);
            } else {
                ImGui::SetTooltip("%s", Tr(i18n::Str::TooltipRedoEmpty));
            }
        }
        if (!canRedo) ImGui::EndDisabled();
    }

    if (studio.hasFirmware && !studio.replacements.empty()) {
        AutoSameLine(Tr(i18n::Str::BtnRevertAllStaged));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.25f, 0.25f, 1.0f));
        if (ImGui::Button(Tr(i18n::Str::BtnRevertAllStaged))) {
            studio.RevertAllAssets();
            studio.statusMsg = Tr(i18n::Str::MsgAllReverted);
            studio.statusIsErr = false;
        }
        ImGui::PopStyleColor(2);
    }
    if (!studio.hasFirmware) ImGui::EndDisabled();

    ImGui::Separator();

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
            "Units",
            Tr(i18n::Str::GroupScreens),
            Tr(i18n::Str::GroupAnimations)
        };
        if (ImGui::Combo(Tr(i18n::Str::LabelCategory), &studio.groupFilterIdx, groupsCombo, IM_ARRAYSIZE(groupsCombo))) {
            studio.UpdateVisible();
        }

        if (ImGui::InputTextWithHint(Tr(i18n::Str::LabelSearch), Tr(i18n::Str::SearchPlaceholder), studio.filterBuf, sizeof(studio.filterBuf))) {
            studio.UpdateVisible();
        }
        if (ImGui::Checkbox(Tr(i18n::Str::FilterOnlyCustom), &studio.filterOnlyCustom)) {
            studio.UpdateVisible();
        }

        ImGui::Spacing();

        float listHeight = ImGui::GetContentRegionAvail().y - 290.0f;
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
                    snprintf(itemLabel, sizeof(itemLabel), "%s%s (%ux%u)###Item_%s", tag, spec.title.c_str(), spec.width, spec.height, spec.id.c_str());

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

        ImGui::Spacing();
        UiPalette::RenderPaletteEditor(studio);
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

            ImGui::SeparatorText(spec.title.c_str());

            if (AssetUsesModeColor(spec)) {
                if (ImGui::Checkbox(Tr(i18n::Str::LabelPreviewCyanMode), &studio.previewCyanMode))
                    studio.textureNeedsUpdate = true;
            }
            if (spec.id.rfind("meter_", 0) == 0) {
                if (ImGui::SliderInt(Tr(i18n::Str::LabelPreviewRank), &studio.previewRank, 0, 7))
                    studio.textureNeedsUpdate = true;
            }
            Rgb8 fgCol = studio.GetForegroundForAsset(spec).ScreenColor();
            const char* pal = PaletteIdForAsset(spec, studio.previewRank, studio.previewCyanMode);
            const std::string fixedColorName = fgCol.ToHex();
            const char* colName = spec.format == AssetFormat::Rgb565 ? "RGB565" : (pal ? GetRoleLabelTr(pal) : fixedColorName.c_str());
            ImGui::Text(Tr(i18n::Str::LabelMetaDetails), spec.id.c_str(), spec.group.c_str(), colName);
            if (spec.format != AssetFormat::Rgb565) {
                ImGui::SameLine();
                ImGui::ColorButton("##AssetColorSwatch", ImVec4(fgCol.r / 255.0f, fgCol.g / 255.0f, fgCol.b / 255.0f, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
            }
            ImGui::Text(Tr(i18n::Str::LabelMetaHardware),
                spec.address, spec.width, spec.height, spec.byte_len(), studio.firmware.records.size());

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
            if (ImGui::Button(Tr(i18n::Str::BtnCopyClipboard))) {
                CopyCurrentSelectedAsset(studio, hwnd);
            }
            AutoSameLine(Tr(i18n::Str::BtnPasteClipboard));
            if (ImGui::Button(Tr(i18n::Str::BtnPasteClipboard))) {
                PasteIntoCurrentSelectedAsset(studio, hwnd);
            }
            AutoSameLine(Tr(i18n::Str::BtnReplaceFromPng));
            if (ImGui::Button(Tr(i18n::Str::BtnReplaceFromPng))) {
                std::string picked;
                if (PathUtils::PickFileDialog(hwnd, L"PNG Image (*.png)\0*.png\0All Files (*.*)\0*.*\0", picked)) {
                    if (spec.format == AssetFormat::Rgb565) {
                        std::vector<uint32_t> rgba;
                        std::string err;
                        if (FirmwareBitmap::LoadAndFitRgb565(picked, spec.width, spec.height, rgba, err)) {
                            std::vector<uint8_t> newRaw;
                            if (FirmwareBitmap::EncodeRgb565(rgba.data(), spec.width, spec.height, newRaw)) {
                                studio.PushUndo("Replace " + spec.id + " from PNG");
                                if (newRaw == studio.originals[catIdx]) {
                                    char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgReplacedMatchedOriginal), spec.id.c_str());
                                    studio.statusMsg = _m;
                                } else {
                                    char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgStagedRgbReplacement), spec.id.c_str());
                                    studio.statusMsg = _m;
                                }
                                studio.SetBitmapReplacement(catIdx, std::move(newRaw));
                                studio.statusIsErr = false;
                                studio.textureNeedsUpdate = true;
                            }
                        } else {
                            char _m[512]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgPngLoadError), err.c_str());
                            studio.statusMsg = _m;
                            studio.statusIsErr = true;
                        }
                    } else {
                        std::vector<uint8_t> gray;
                        std::string err;
                        if (FirmwareBitmap::LoadAndFit(picked, spec.width, spec.height, gray, err)) {
                            std::vector<uint8_t> newRaw;
                            if (FirmwareBitmap::EncodeMaskPreserving(gray.data(), spec.width, spec.height, studio.originals[catIdx].data(), spec.byte_len(), newRaw)) {
                                studio.PushUndo("Replace " + spec.id + " from PNG");
                                if (newRaw == studio.originals[catIdx]) {
                                    char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgReplacedMatchedOriginal), spec.id.c_str());
                                    studio.statusMsg = _m;
                                } else {
                                    char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgStagedReplacementCustom), spec.id.c_str());
                                    studio.statusMsg = _m;
                                }
                                studio.SetBitmapReplacement(catIdx, std::move(newRaw));
                                studio.statusIsErr = false;
                                studio.textureNeedsUpdate = true;
                            }
                        } else {
                            char _m[512]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgPngLoadError), err.c_str());
                            studio.statusMsg = _m;
                            studio.statusIsErr = true;
                        }
                    }
                }
            }
            if (isModified) {
                AutoSameLine(Tr(i18n::Str::BtnRestoreOriginal));
                if (ImGui::Button(Tr(i18n::Str::BtnRestoreOriginal))) {
                    studio.PushUndo("Restore " + spec.id);
                    studio.SetBitmapReplacement(catIdx, studio.originals[catIdx]);
                    char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgRestoredOriginal), spec.id.c_str());
                    studio.statusMsg = _m;
                    studio.statusIsErr = false;
                    studio.textureNeedsUpdate = true;
                }
            }
            if (!matchesRef) {
                AutoSameLine(Tr(i18n::Str::BtnRestoreStock));
                if (ImGui::Button(Tr(i18n::Str::BtnRestoreStock))) {
                    studio.RevertAssetToStock(catIdx);
                    char _buf[256];
                    snprintf(_buf, sizeof(_buf), Tr(i18n::Str::MsgRestoredToStock), spec.id.c_str());
                    studio.statusMsg = _buf;
                    studio.statusIsErr = false;
                    studio.textureNeedsUpdate = true;
                }
            }
            AutoSameLine(Tr(i18n::Str::BtnExportThisPng));
            if (ImGui::Button(Tr(i18n::Str::BtnExportThisPng))) {
                std::string def = spec.id + ".png";
                std::string outPath;
                if (PathUtils::SaveFileDialog(hwnd, L"PNG Image (*.png)\0*.png\0", def, outPath)) {
                    std::string err;
                    bool ok = false;
                    if (spec.format == AssetFormat::Rgb565) {
                        const uint8_t* raw = studio.GetCurrentRaw(catIdx);
                        std::vector<uint32_t> rgba;
                        FirmwareBitmap::DecodeRgb565(raw, spec.byte_len(), spec.width, spec.height, rgba);
                        ok = FirmwareBitmap::SavePngRgb565(outPath, rgba.data(), spec.width, spec.height, err);
                    } else {
                        std::vector<uint8_t> gray;
                        const uint8_t* raw = studio.GetCurrentRaw(catIdx);
                        FirmwareBitmap::DecodeMask(raw, spec.byte_len(), spec.width, spec.height, gray);
                        ok = FirmwareBitmap::SavePng(outPath, gray.data(), spec.width, spec.height, err);
                    }
                    if (ok) {
                        char _m[512]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgSavedAssetPng), spec.id.c_str(), outPath.c_str());
                        studio.statusMsg = _m;
                        studio.statusIsErr = false;
                    } else {
                        char _m[512]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgSaveError), err.c_str());
                        studio.statusMsg = _m;
                        studio.statusIsErr = true;
                    }
                }
            }
            if (spec.id == "boot_voopoo_animated") {
                AutoSameLine(Tr(i18n::Str::BtnExportHorizontal));
                if (ImGui::Button(Tr(i18n::Str::BtnExportHorizontal))) {
                    std::string def = "boot_voopoo_animated_horizontal_136x22.png";
                    std::string outPath;
                    if (PathUtils::SaveFileDialog(hwnd, L"PNG Image (*.png)\0*.png\0", def, outPath)) {
                        const uint8_t* raw = studio.GetCurrentRaw(catIdx);
                        std::vector<uint32_t> rgba;
                        FirmwareBitmap::DecodeRgb565(raw, spec.byte_len(), spec.width, spec.height, rgba);
                        std::string err;
                        if (FirmwareBitmap::SavePngRgb565Rotated(outPath, rgba.data(), spec.width, spec.height, true, err)) {
                            char _m[512]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgSavedBootLogo), outPath.c_str());
                            studio.statusMsg = _m;
                            studio.statusIsErr = false;
                        } else {
                            char _m[512]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgSaveError), err.c_str());
                            studio.statusMsg = _m;
                            studio.statusIsErr = true;
                        }
                    }
                }
            }

            ImGui::Spacing();
            if (spec.format == AssetFormat::Rgb565) {
                ImGui::SeparatorText(Tr(i18n::Str::LabelBrushColor));
                const Rgb8 brush = studio.GetPaintColor(spec).ScreenColor();
                float color[3] = {brush.r / 255.0f, brush.g / 255.0f, brush.b / 255.0f};
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
            ImGui::SetNextItemWidth(150.0f);
            ImGui::SliderInt(Tr(i18n::Str::LabelZoom), &studio.previewZoom, 1, 32, "%dx");
            ImGui::SameLine();
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

