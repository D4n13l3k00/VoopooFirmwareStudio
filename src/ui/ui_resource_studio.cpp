#include "ui_resource_studio.h"
#include "ui_pixel_editor.h"
#include "ui_palette.h"
#include "path_utils.h"
#include "../imgui/imgui.h"

namespace UiResourceStudio {

static std::string FormatTimestampNow() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &tt);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    return std::string(buf);
}

void RenderTab(
    StudioState& studio,
    HWND hwnd,
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    bool usbConnected,
    bool isFlashing,
    bool& requestOpenFlasherTab
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
        std::string picked;
        if (PathUtils::PickFileDialog(hwnd, L"VOOPOO Firmware (*.zwdf;*.swdf)\0*.zwdf;*.swdf\0All Files (*.*)\0*.*\0", picked)) {
            std::string err;
            if (studio.OpenFirmware(picked, err)) {
                char _m[512];
                snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgSuccessOpened), fs::path(picked).filename().string().c_str());
                studio.statusMsg = _m;
                studio.statusIsErr = false;
            } else {
                char _m[512];
                snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgOpenError), err.c_str());
                studio.statusMsg = _m;
                studio.statusIsErr = true;
            }
        }
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
                                studio.replacements.erase(spec.id);
                                unchanged++;
                            } else {
                                studio.replacements[spec.id] = std::move(newRaw);
                                changed++;
                            }
                        }
                    }
                } else {
                    std::vector<uint8_t> gray;
                    std::string err;
                    if (FirmwareBitmap::LoadAndFit(pngPath, spec.width, spec.height, gray, err)) {
                        std::vector<uint8_t> newRaw;
                        if (FirmwareBitmap::EncodeMaskPreserving(gray.data(), spec.width, spec.height, studio.originals[i].data(), spec.byte_len(), newRaw)) {
                            if (newRaw == studio.originals[i]) {
                                studio.replacements.erase(spec.id);
                                unchanged++;
                            } else {
                                studio.replacements[spec.id] = std::move(newRaw);
                                changed++;
                            }
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
        std::string defName = fs::path(studio.firmwarePath).stem().string() + "_custom.zwdf";
        std::string savePath;
        if (PathUtils::SaveFileDialog(hwnd, L"VOOPOO Firmware (*.zwdf)\0*.zwdf\0", defName, savePath)) {
            if (savePath == studio.firmwarePath) {
                studio.statusMsg = Tr(i18n::Str::MsgRefuseOverwrite);
                studio.statusIsErr = true;
            } else {
                std::string err;
                std::vector<CodePatch> codePatches;
                if (!BuildColorPatches(studio.firmware, studio.palette, studio.backgroundColor, codePatches, err)) {
                    char _m[512];
                    snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgColorPatchError), err.c_str());
                    studio.statusMsg = _m;
                    studio.statusIsErr = true;
                } else {
                    std::map<std::string, std::pair<std::vector<size_t>, std::vector<uint8_t>>> repMap;
                    for (size_t i = 0; i < studio.catalog.size(); ++i) {
                        const auto& spec = studio.catalog[i];
                        auto it = studio.replacements.find(spec.id);
                        if (it != studio.replacements.end()) {
                            repMap[spec.id] = { studio.locations[i], it->second };
                        }
                    }

                    std::vector<uint8_t> outBytes;
                    BuildReport rep;
                    if (!studio.firmware.PackWith(repMap, codePatches, outBytes, rep, err)) {
                        char _m[512];
                        snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgPackError), err.c_str());
                        studio.statusMsg = _m;
                        studio.statusIsErr = true;
                    } else {
                        if (fs::exists(PathUtils::ToFsPath(savePath))) {
                            std::string bak = savePath + ".bak." + FormatTimestampNow();
                            fs::copy_file(PathUtils::ToFsPath(savePath), PathUtils::ToFsPath(bak), fs::copy_options::overwrite_existing);
                        }

                        auto outF = PathUtils::OpenWriteStream(savePath);
                        outF.write((const char*)outBytes.data(), outBytes.size());
                        outF.close();

                        std::string repPath = fs::path(savePath).replace_extension(".report.json").string();
                        auto repF = PathUtils::OpenWriteStream(repPath);
                        repF << rep.ToJson();
                        repF.close();

                        studio.lastReport = rep;
                        studio.lastBuildPath = savePath;
                        studio.showBuildModal = true;
                        char _m[512];
                        snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgBuildSuccess), fs::path(savePath).filename().string().c_str());
                        studio.statusMsg = _m;
                        studio.statusIsErr = false;
                    }
                }
            }
        }
    }

#if STUDIO_FLASHER
    bool canQuickFlash = studio.hasFirmware && usbConnected && !isFlashing;
    AutoSameLine(Tr(i18n::Str::BtnFlashToDevice));
    if (!canQuickFlash) ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.55f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.70f, 0.38f, 1.0f));
    if (ImGui::Button(Tr(i18n::Str::BtnFlashToDevice), ImVec2(0, 0))) {
        requestOpenFlasherTab = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (!studio.hasFirmware) ImGui::SetTooltip("%s", Tr(i18n::Str::TooltipOpenOrLoad));
        else if (!usbConnected) ImGui::SetTooltip("%s", Tr(i18n::Str::TooltipNoDeviceUsb));
        else if (isFlashing) ImGui::SetTooltip("%s", Tr(i18n::Str::TooltipFlashingInProgress));
        else ImGui::SetTooltip("%s", Tr(i18n::Str::TooltipOpenDeviceFlasher));
    }
    ImGui::PopStyleColor(2);
    if (!canQuickFlash) ImGui::EndDisabled();
#endif

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
                    bool hasCustomCol = (studio.assetColorOverrides.find(spec.id) != studio.assetColorOverrides.end());

                    if (isModified) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                    } else if (!isStock) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
                    } else if (hasCustomCol) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 1.0f, 1.0f));
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
                    if (isModified || !isStock || hasCustomCol) {
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

            Rgb8 fgCol = studio.GetForegroundForAsset(spec).ScreenColor();
            bool hasCustomColor = (studio.assetColorOverrides.find(spec.id) != studio.assetColorOverrides.end());
            if (hasCustomColor) {
                ImGui::Text(Tr(i18n::Str::LabelMetaDetailsCustom), spec.id.c_str(), spec.group.c_str(), fgCol.ToHex().c_str());
            } else {
                const char* pal = PaletteIdForAsset(spec);
                const char* colName = pal ? GetRoleLabelTr(pal) : Tr(i18n::Str::ColorPresetWhite);
                ImGui::Text(Tr(i18n::Str::LabelMetaDetails), spec.id.c_str(), spec.group.c_str(), colName);
            }
            ImGui::SameLine();
            ImGui::ColorButton("##AssetColorSwatch", ImVec4(fgCol.r / 255.0f, fgCol.g / 255.0f, fgCol.b / 255.0f, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
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
                                    studio.replacements.erase(spec.id);
                                    char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgReplacedMatchedOriginal), spec.id.c_str());
                                    studio.statusMsg = _m;
                                } else {
                                    studio.replacements[spec.id] = std::move(newRaw);
                                    char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgStagedRgbReplacement), spec.id.c_str());
                                    studio.statusMsg = _m;
                                }
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
                                    studio.replacements.erase(spec.id);
                                    char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgReplacedMatchedOriginal), spec.id.c_str());
                                    studio.statusMsg = _m;
                                } else {
                                    studio.replacements[spec.id] = std::move(newRaw);
                                    char _m[256]; snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgStagedReplacementCustom), spec.id.c_str());
                                    studio.statusMsg = _m;
                                }
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
                    studio.replacements.erase(spec.id);
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
                        if (hasCustomColor) {
                            std::vector<uint32_t> recolored;
                            FirmwareBitmap::RecolorRgb565(rgba.data(), spec.width, spec.height, fgCol, recolored);
                            ok = FirmwareBitmap::SavePngRgb565(outPath, recolored.data(), spec.width, spec.height, err);
                        } else {
                            ok = FirmwareBitmap::SavePngRgb565(outPath, rgba.data(), spec.width, spec.height, err);
                        }
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

            // Color Override Section
            ImGui::Spacing();
            ImGui::SeparatorText(Tr(i18n::Str::LabelPreviewTint));
            const bool isFontMask = spec.id.rfind("font_", 0) == 0;
            if (isFontMask) {
                ImGui::TextDisabled("%s", Tr(i18n::Str::InfoDigitsMonochrome));
            } else {
                ImGui::TextDisabled("%s", Tr(i18n::Str::InfoPreviewNotSaved));
            }

            float customColFloat[3] = { fgCol.r / 255.0f, fgCol.g / 255.0f, fgCol.b / 255.0f };
            ImGui::Text("%s:", Tr(i18n::Str::LabelPreviewOnly));
            ImGui::SameLine();
            if (ImGui::ColorEdit3("##AssetCustomCol", customColFloat, ImGuiColorEditFlags_NoInputs)) {
                Rgb8 picked((uint8_t)(customColFloat[0] * 255.0f + 0.5f), (uint8_t)(customColFloat[1] * 255.0f + 0.5f), (uint8_t)(customColFloat[2] * 255.0f + 0.5f));
                studio.assetColorOverrides[spec.id] = picked;
                strncpy_s(studio.assetHexEditBuf, picked.ToHex().c_str(), sizeof(studio.assetHexEditBuf));
                studio.textureNeedsUpdate = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::InputText("##AssetCustomHex", studio.assetHexEditBuf, sizeof(studio.assetHexEditBuf), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase)) {
                Rgb8 parsed;
                if (Rgb8::ParseHex(studio.assetHexEditBuf, parsed)) {
                    studio.assetColorOverrides[spec.id] = parsed;
                    studio.textureNeedsUpdate = true;
                }
            }
            ImGui::SameLine();
            if (hasCustomColor) {
                if (ImGui::Button(Tr(i18n::Str::BtnResetColor))) {
                    studio.assetColorOverrides.erase(spec.id);
                    Rgb8 c = studio.GetForegroundForAsset(spec);
                    strncpy_s(studio.assetHexEditBuf, c.ToHex().c_str(), sizeof(studio.assetHexEditBuf));
                    studio.textureNeedsUpdate = true;
                }
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

                    std::vector<uint32_t> refRgba;
                    if (spec.format == AssetFormat::Rgb565) {
                        FirmwareBitmap::DecodeRgb565(refRaw, spec.byte_len(), spec.width, spec.height, refRgba);
                    } else {
                        std::vector<uint8_t> gray;
                        FirmwareBitmap::DecodeMask(refRaw, spec.byte_len(), spec.width, spec.height, gray);
                        FirmwareBitmap::ColorizeMask(gray.data(), spec.width, spec.height, fgCol, studio.backgroundColor.ScreenColor(), refRgba);
                    }
                    if (!studio.referencePreviewTexture.IsValid()) {
                        studio.referencePreviewTexture.Update(device, context, refRgba.data(), spec.width, spec.height);
                    }
                    if (studio.referencePreviewTexture.IsValid()) {
                        ImGui::Image((ImTextureID)studio.referencePreviewTexture.GetSRV(), ImVec2(canvasW, canvasH));
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

