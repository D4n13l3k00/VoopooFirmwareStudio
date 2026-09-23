#if STUDIO_FLASHER

#include "ui_device_flasher.h"
#include "path_utils.h"
#include "i18n.h"
#include "zwdf_package.h"
#include "../imgui/imgui.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace UiDeviceFlasher {

void RenderTab(StudioState& studio, voopoo::UsbFlasher& flasher, HWND hwnd, bool& requestOpenFlasherTab, bool smokeTest, int debugFrame) {
    ImGuiTabItemFlags flasherTabFlags = (requestOpenFlasherTab || (smokeTest && debugFrame == 3)) ? ImGuiTabItemFlags_SetSelected : 0;
    char _tb[128];
    snprintf(_tb, sizeof(_tb), "%s###TabFlasher", Tr(i18n::Str::TabDeviceFlasher));
    if (ImGui::BeginTabItem(_tb, nullptr, flasherTabFlags)) {
        requestOpenFlasherTab = false;

        auto devInfo = flasher.GetDeviceInfo();
        auto flashProg = flasher.GetFlashProgress();
        bool isFlashing = (flashProg.state != voopoo::FlashState::Idle &&
                           flashProg.state != voopoo::FlashState::FinishedSuccess &&
                           flashProg.state != voopoo::FlashState::Failed);

        // Left Column: Device Status & Real-time Telemetry Card
        float leftCardWidth = 430.0f;
        ImGui::BeginChild("FlasherDeviceCard", ImVec2(leftCardWidth, 0.0f), true);
        {
            ImGui::SeparatorText(Tr(i18n::Str::FlasherConnectedUsbTitle));

            if (devInfo.connected) {
                if (devInfo.in_bootloader) {
                    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.1f, 1.0f), "%s", Tr(i18n::Str::FlasherBootloaderMode));
                } else {
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "%s", Tr(i18n::Str::FlasherDeviceReady));
                }

                ImGui::Spacing();
                if (ImGui::BeginTable("DevInfoTbl", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", Tr(i18n::Str::FlasherPropProduct));
                    ImGui::TableSetColumnIndex(1); ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "%s", devInfo.product.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", Tr(i18n::Str::FlasherPropManufacturer));
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%s", devInfo.manufacturer.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", Tr(i18n::Str::FlasherPropSerial));
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%s", devInfo.serial.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", Tr(i18n::Str::FlasherPropHwId));
                    ImGui::TableSetColumnIndex(1); ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", devInfo.hardware_id.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", Tr(i18n::Str::FlasherPropFwVersion));
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%s", devInfo.firmware_version.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", Tr(i18n::Str::FlasherPropDevAddr));
                    ImGui::TableSetColumnIndex(1); ImGui::Text("0x%04X", devInfo.device_address);

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::TextDisabled("%s", Tr(i18n::Str::FlasherNoTelemetry));
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", Tr(i18n::Str::FlasherNoDeviceDetected));
                ImGui::Spacing();
                ImGui::TextWrapped("%s", Tr(i18n::Str::FlasherConnectPrompt));
                ImGui::Spacing();
                ImGui::BulletText("VID: 0x0483 (STMicroelectronics)");
                ImGui::BulletText("PID: 0x5750 (Custom HID)");
                ImGui::BulletText("Report ID: 0x02 (64-byte packets)");
            }

            ImGui::Spacing();
            ImGui::SeparatorText(Tr(i18n::Str::FlasherBuiltinTitle));
            ImGui::TextWrapped("%s", Tr(i18n::Str::FlasherBuiltinDesc1));
            ImGui::TextWrapped("%s", Tr(i18n::Str::FlasherBuiltinDesc2));
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Right Column: Firmware Selection, Safety Verification, Flash Action & Progress
        ImGui::BeginChild("FlasherActionCard", ImVec2(0.0f, 0.0f), true);
        {
            ImGui::SeparatorText(Tr(i18n::Str::FlasherPkgToFlashTitle));

            static int s_firmwareSourceChoice = 0; // 0: Current Studio Firmware, 1: Custom .zwdf file
            static char s_customFwPath[512] = "";

            ImGui::RadioButton(Tr(i18n::Str::FlasherRadioActiveFw), &s_firmwareSourceChoice, 0);
            ImGui::RadioButton(Tr(i18n::Str::FlasherRadioCustomZwdf), &s_firmwareSourceChoice, 1);

            if (s_firmwareSourceChoice == 1) {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110.0f);
                ImGui::InputText("##CustomFwPath", s_customFwPath, sizeof(s_customFwPath));
                ImGui::SameLine();
                if (ImGui::Button(Tr(i18n::Str::FlasherBtnBrowse))) {
                    std::string picked;
                    if (PathUtils::PickFileDialog(hwnd, L"VOOPOO Firmware (*.zwdf)\0*.zwdf\0All Files (*.*)\0*.*\0", picked)) {
                        strncpy_s(s_customFwPath, picked.c_str(), sizeof(s_customFwPath));
                    }
                }
            }

            ImGui::Spacing();
            ImGui::SeparatorText(Tr(i18n::Str::FlasherPkgMatchTitle));

            bool readyToFlash = false;
            size_t recordImageBytes = 0;
            uint32_t recordLinkedBase = 0;

            if (s_firmwareSourceChoice == 0) {
                if (!studio.hasFirmware) {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", Tr(i18n::Str::FlasherNoFwLoaded));
                } else {
                    const HardwareRecord* rec3103 = nullptr;
                    for (const auto& r : studio.firmware.records) {
                        if (r.device_code[0] == 0x31 && r.device_code[1] == 0x03) {
                            rec3103 = &r;
                            break;
                        }
                    }
                    if (rec3103) {
                        recordImageBytes = rec3103->image.size();
                        recordLinkedBase = rec3103->linked_base;
                        readyToFlash = true;

                        std::string fwName = fs::path(PathUtils::ToFsPath(studio.firmwarePath)).filename().string();
                        if (studio.isBuiltin) {
                            fwName += " (";
                            fwName += Tr(i18n::Str::LabelBuiltin);
                            fwName += ")";
                        }
                        ImGui::Text(Tr(i18n::Str::FlasherSourceFile), fwName.c_str());
                        ImGui::Text(Tr(i18n::Str::FlasherFwBinSize), recordImageBytes, (recordImageBytes + 511) / 512);
                        ImGui::Text(Tr(i18n::Str::FlasherFlashBase), recordLinkedBase);
                        ImGui::Text(Tr(i18n::Str::FlasherStagedMods), studio.replacements.size(), studio.assetColorOverrides.size());
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", Tr(i18n::Str::FlasherHwMismatchActive));
                    }
                }
            } else {
                if (strlen(s_customFwPath) == 0 || !fs::exists(PathUtils::ToFsPath(s_customFwPath))) {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", Tr(i18n::Str::FlasherPleaseSelectFile));
                } else {
                    std::string err;
                    FirmwarePackage diskPkg;
                    if (FirmwarePackage::Open(s_customFwPath, diskPkg, err)) {
                        const HardwareRecord* rec3103 = nullptr;
                        for (const auto& r : diskPkg.records) {
                            if (r.device_code[0] == 0x31 && r.device_code[1] == 0x03) {
                                rec3103 = &r;
                                break;
                            }
                        }
                        if (rec3103) {
                            recordImageBytes = rec3103->image.size();
                            recordLinkedBase = rec3103->linked_base;
                            readyToFlash = true;

                            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", Tr(i18n::Str::FlasherRecFoundSelected));
                            ImGui::Text(Tr(i18n::Str::FlasherSourceFile), fs::path(PathUtils::ToFsPath(s_customFwPath)).filename().string().c_str());
                            ImGui::Text(Tr(i18n::Str::FlasherFwBinSize), recordImageBytes, (recordImageBytes + 511) / 512);
                            ImGui::Text(Tr(i18n::Str::FlasherFlashBase), recordLinkedBase);
                        } else {
                            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", Tr(i18n::Str::FlasherHwMismatchSelected));
                        }
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), Tr(i18n::Str::FlasherParseError), err.c_str());
                    }
                }
            }

            ImGui::Spacing();
            ImGui::SeparatorText(Tr(i18n::Str::FlasherActionTitle));

            if (devInfo.connected && devInfo.device_address != 0x3103) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), Tr(i18n::Str::FlasherDevIdMismatch), devInfo.device_address);
            }
            bool canTriggerFlash = readyToFlash && devInfo.connected && devInfo.device_address == 0x3103 && !isFlashing;
            if (!canTriggerFlash) ImGui::BeginDisabled();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.65f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.80f, 0.38f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.50f, 0.22f, 1.0f));

            if (ImGui::Button(Tr(i18n::Str::FlasherBtnFlash1Click), ImVec2(-1.0f, 44.0f))) {
                std::vector<uint8_t> payload;
                std::string packErr;
                bool ok = false;
                if (s_firmwareSourceChoice == 0) {
                    ok = studio.BuildZwdfBytes(payload, packErr);
                } else {
                    auto f = PathUtils::OpenReadStream(s_customFwPath);
                    if (f.is_open()) {
                        payload.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                        ok = !payload.empty();
                    } else {
                        packErr = "Cannot open file";
                    }
                }

                if (ok && !payload.empty()) {
                    const bool started = flasher.StartFlashAsync(payload, 0x3103);
                    studio.statusMsg = started ? Tr(i18n::Str::MsgFlashStarted) : Tr(i18n::Str::MsgFlashCouldNotStart);
                    studio.statusIsErr = !started;
                } else {
                    char _m[512];
                    snprintf(_m, sizeof(_m), Tr(i18n::Str::MsgPreparePkgFailed), packErr.c_str());
                    studio.statusMsg = _m;
                    studio.statusIsErr = true;
                }
            }
            ImGui::PopStyleColor(3);

            if (!canTriggerFlash) ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::SeparatorText(Tr(i18n::Str::FlasherProgressTitle));

            if (isFlashing) {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), Tr(i18n::Str::FlasherFlashingInProgress), flashProg.status_text.c_str());
                if (flashProg.total_blocks > 0) {
                    ImGui::Text(Tr(i18n::Str::FlasherBlock), flashProg.current_block, flashProg.total_blocks);
                }
                char progBuf[32];
                snprintf(progBuf, sizeof(progBuf), "%.1f%%", flashProg.progress * 100.0f);
                ImGui::ProgressBar(flashProg.progress, ImVec2(-1.0f, 26.0f), progBuf);

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.2f, 1.0f), "%s", Tr(i18n::Str::FlasherWarningDisconnect));
            } else if (flashProg.state == voopoo::FlashState::FinishedSuccess) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.35f, 0.15f, 0.5f));
                ImGui::BeginChild("SuccessCard", ImVec2(-1.0f, 80.0f), true);
                {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "%s", Tr(i18n::Str::FlasherSuccessAck));
                    ImGui::Text("%s", Tr(i18n::Str::FlasherSuccessDesc1));
                    ImGui::Text("%s", Tr(i18n::Str::FlasherSuccessDesc2));
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
            } else if (flashProg.state == voopoo::FlashState::Failed) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.4f, 0.15f, 0.15f, 0.5f));
                ImGui::BeginChild("FailCard", ImVec2(-1.0f, 80.0f), true);
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", Tr(i18n::Str::FlasherFailedTitle));
                    ImGui::Text(Tr(i18n::Str::FlasherStatusLabel), flashProg.status_text.c_str());
                    ImGui::Text(Tr(i18n::Str::FlasherErrorLabel), flashProg.error_message.c_str());
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("%s", Tr(i18n::Str::FlasherIdleStatus));
            }
        }
        ImGui::EndChild();

        ImGui::EndTabItem();
    }
}

} // namespace UiDeviceFlasher

#endif // STUDIO_FLASHER

