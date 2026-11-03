#include "ui_firmware_info.h"
#include "i18n.h"
#include "../imgui/imgui.h"

namespace UiFirmwareInfo {

void RenderTab(StudioState& studio) {
    char _tb[128];
    snprintf(_tb, sizeof(_tb), "%s###TabFwInfo", Tr(i18n::Str::TabFirmwareInfo));
    if (ImGui::BeginTabItem(_tb)) {
        ImGui::SeparatorText(Tr(i18n::Str::FwInfoSpecTitle));

        if (!studio.hasFirmware) {
            ImGui::TextDisabled("%s", Tr(i18n::Str::FwInfoNoFirmware));
        } else {
            std::string displayFwPath = studio.firmwarePath;
            if (studio.isBuiltin) {
                displayFwPath += " (";
                displayFwPath += Tr(i18n::Str::LabelBuiltin);
                displayFwPath += ")";
            }
            ImGui::Text(Tr(i18n::Str::FwInfoFilePath), displayFwPath.c_str());
            ImGui::Text(Tr(i18n::Str::FwInfoSha256), studio.firmware.source_sha256.c_str());
            ImGui::Text(Tr(i18n::Str::FwInfoPackageSize), studio.firmware.source_bytes.size());
            ImGui::Text(Tr(i18n::Str::FwInfoHwRevisions), studio.firmware.records.size());
            ImGui::Text(Tr(i18n::Str::FwInfoRecognizedResources), studio.catalog.size());

            ImGui::Spacing();
            ImGui::SeparatorText(Tr(i18n::Str::FwInfoRecordsTable));

            if (ImGui::BeginTable("RecordsTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn(Tr(i18n::Str::FwInfoColRecord));
                ImGui::TableSetupColumn(Tr(i18n::Str::FwInfoColDevCode));
                ImGui::TableSetupColumn(Tr(i18n::Str::FwInfoColPayload));
                ImGui::TableSetupColumn(Tr(i18n::Str::FwInfoColImage));
                ImGui::TableSetupColumn(Tr(i18n::Str::FwInfoColFlashBase));
                ImGui::TableSetupColumn(Tr(i18n::Str::FwInfoColMSP));
                ImGui::TableSetupColumn(Tr(i18n::Str::FwInfoColResetVector));
                ImGui::TableHeadersRow();

                for (size_t r = 0; r < studio.firmware.records.size(); ++r) {
                    const auto& rec = studio.firmware.records[r];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%zu", r);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%02X%02X", rec.device_code[0], rec.device_code[1]);

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%zu B", rec.image.size() + 2);

                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%zu B", rec.image.size());

                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("0x%08X", rec.linked_base);

                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("0x%08X", rec.sp);

                    ImGui::TableSetColumnIndex(6);
                    ImGui::Text("0x%08X", rec.reset);
                }
                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::SeparatorText(Tr(i18n::Str::FwInfoInstructionsTitle));
            ImGui::BulletText("%s", Tr(i18n::Str::FwInfoBulletBuildZwdf));
        }

        ImGui::EndTabItem();
    }
}

} // namespace UiFirmwareInfo

