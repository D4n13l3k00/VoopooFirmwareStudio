#pragma once
#include "app_state.h"
#include "embedded_firmware.h"
#include "firmware_emulator.h"
#include "firmware_emulator_screens.h"
#include <ostream>
#include <cstdio>
#include <set>

inline bool RunSxPageCoverageRegressions(std::string& error, std::ostream* proof = nullptr) {
    using namespace FirmwareEmulator;
    using FirmwareLayout::Point;
    error.clear();
    Renderer renderer;
    size_t recordsChecked = 0, pagesChecked = 0;
    for (const auto [model, edition] : {
        std::pair{FirmwareModel::DragS, FirmwareEdition::Score},
        std::pair{FirmwareModel::DragS, FirmwareEdition::Factory},
        std::pair{FirmwareModel::DragX, FirmwareEdition::Score},
        std::pair{FirmwareModel::DragX, FirmwareEdition::Factory}}) {
        FirmwarePackage package;
        StudioState editor;
        if (!LoadEmbeddedFirmware(package, error, model, edition) ||
            !editor.LoadPackage(std::move(package), "S/X native page coverage", error)) return false;
        if (!editor.layoutError.empty()) { error = editor.layoutError; return false; }
        for (const auto& hardware : editor.firmware.records) {
            const uint16_t code = uint16_t(hardware.device_code[0]) << 8 | hardware.device_code[1];
            DeviceState mainDevice;
            mainDevice.hardwareCode = code;
            Frame mainStock;
            if (!renderer.Render(editor, mainDevice, mainStock)) { error = mainStock.error; return false; }
            std::set<uint32_t> coordinateSites;
            for (const auto& element : editor.layoutElements) {
                if (element.hardwareCode != code) continue;
                for (const auto& site : element.sites) {
                    if (!coordinateSites.insert(site.address).second) {
                        error = "S/X native coordinate has conflicting independent owners"; return false;
                    }
                }
            }
            const auto screens = AvailableScreens(editor, code);
            for (const auto& screen : screens) {
                if (screen.id == 0 || screen.id == 17) continue;
                const auto found = std::find_if(editor.layoutElements.begin(), editor.layoutElements.end(), [&](const auto& element) {
                    return element.hardwareCode == code && element.screen == screen.id;
                });
                auto fail = [&](const std::string& reason) {
                    error = std::string(FirmwareModelName(model)) + " / " + FirmwareEditionName(edition) +
                        " / hw" + std::to_string(code) + " / page" + std::to_string(screen.id) + ": " + reason;
                    return false;
                };
                if (found == editor.layoutElements.end() || found->sites.empty()) return fail("native page has no coordinate block");
                const auto& element = *found;
                const size_t index = static_cast<size_t>(found - editor.layoutElements.begin());
                const auto& firstSite = element.sites.front();
                const size_t instructionWidth = firstSite.encoding == FirmwareLayout::Encoding::ThumbImm8 ||
                    firstSite.encoding == FirmwareLayout::Encoding::ThumbMovAliasImm8 ? 2 : 4;
                auto unrelatedOpcode = hardware.image;
                unrelatedOpcode[firstSite.address - hardware.linked_base + instructionWidth - 1] ^= 0x80;
                if (FirmwareLayout::MatchesSxExecutable(editor.layoutElements, code, hardware.linked_base, hardware.sp,
                    hardware.image.data(), unrelatedOpcode.data(), hardware.image.size()))
                    return fail("page coordinate guard admits an unrelated opcode/register change");
                Point shift;
                if (element.maximumOffset.x >= 2) shift.x = 2;
                else if (element.minimumOffset.x <= -2) shift.x = -2;
                if (element.maximumOffset.y >= 1) shift.y = 1;
                else if (element.minimumOffset.y <= -1) shift.y = -1;
                if (shift == Point{}) return fail("fake immovable native element");
                DeviceState device = mainDevice;
                device.screen = screen.id;
                device.charging = true;
                device.battery = 3;
                device.historySelection = 5;
                device.puffHistory = {0, 987, 12, 420, 71, 65535, 42, 0, 36, 8, 975, 1, 243, 10};
                device.clockHour = 23; device.clockMinute = 59; device.menuSelection = 1;
                Frame stock;
                if (!renderer.Render(editor, device, stock)) return fail(stock.error);
                editor.PushUndo("Move real S/X native page block");
                if (!editor.SetLayoutOffset(index, shift, error)) return fail(error);
                editor.EndUndoAction();
                Frame moved, mainMoved;
                if (!renderer.Render(editor, device, moved)) return fail(moved.error);
                if (moved.rgba == stock.rgba || moved.hardwareCode != code || moved.pixelWrites < 80 * 160)
                    return fail("native coordinate edit has no LCD effect or incomplete stream");
                if (!renderer.Render(editor, mainDevice, mainMoved) || mainMoved.rgba != mainStock.rgba)
                    return fail("auxiliary-page edit changed native MAIN or its repaint cutover");
                // Every content pixel must translate intact, including native UID
                // row stepping, history variable-height bars and old clear bands.
                for (int y = 0; y < element.height; ++y) {
                    for (int x = 0; x < element.width; ++x) {
                        const size_t before = size_t(element.position.y + y) * 80 + element.position.x + x;
                        const size_t after = size_t(element.position.y + y + shift.y) * 80 + element.position.x + x + shift.x;
                        if (stock.rgba[before] != moved.rgba[after]) return fail("native clear/update path damaged translated pixels");
                    }
                }
                for (int y = 0; y < 160; ++y) {
                    for (int x = 0; x < 80; ++x) {
                        const auto inside = [&](Point offset) {
                            return x >= element.position.x + offset.x && x < element.position.x + offset.x + element.width &&
                                y >= element.position.y + offset.y && y < element.position.y + offset.y + element.height;
                        };
                        if (!inside({}) && !inside(shift) && stock.rgba[size_t(y) * 80 + x] != moved.rgba[size_t(y) * 80 + x])
                            return fail("coordinate patch changes pixels outside the native block");
                    }
                }
                std::vector<uint8_t> bytes;
                FirmwarePackage exported;
                if (!editor.BuildZwdfBytes(bytes, error) || !FirmwarePackage::Parse(bytes, exported, error)) return fail(error);
                for (size_t r = 0; r < exported.records.size(); ++r) {
                    const auto& original = editor.firmware.records[r];
                    const uint16_t other = uint16_t(original.device_code[0]) << 8 | original.device_code[1];
                    if (other != code && original.image != exported.records[r].image) return fail("page edit changed another hardware record");
                }
                StudioState reopened;
                if (!reopened.LoadPackage(std::move(exported), "reopened S/X native page", error) || !reopened.layoutError.empty())
                    return fail(error + reopened.layoutError);
                const auto restored = std::find_if(reopened.layoutElements.begin(), reopened.layoutElements.end(), [&](const auto& candidate) {
                    return candidate.id == element.id;
                });
                if (restored == reopened.layoutElements.end() || restored->position !=
                    Point{element.position.x + shift.x, element.position.y + shift.y} || restored->screen != screen.id)
                    return fail("exported coordinate cannot be rediscovered");
                Frame reloaded;
                if (!renderer.Render(reopened, device, reloaded) || reloaded.rgba != moved.rgba) return fail("export/reopen LCD mismatch: " + reloaded.error);
                // Exercise incremental inputs at the edited native coordinates.
                auto alternate = device;
                alternate.battery = 0; alternate.charging = false;
                alternate.historySelection = 13;
                alternate.puffHistory = {65535, 1, 0, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130};
                alternate.clockHour = 0; alternate.clockMinute = 0; alternate.menuSelection = 0;
                Frame stagedAlternate, reopenedAlternate;
                if (!renderer.Render(editor, alternate, stagedAlternate) || !renderer.Render(reopened, alternate, reopenedAlternate) ||
                    stagedAlternate.rgba != reopenedAlternate.rgba) return fail("edited native state interactions do not survive reopening");
                if ((screen.id == 4 || screen.id == 18 || screen.id == 19) && reloaded.rgba == reopenedAlternate.rgba)
                    return fail("native charging/history/clock input became a placeholder");
                if (screen.id == 15 && !reloaded.emulatedUid) return fail("UID identity bounds/label contract was lost");
                // Palette and actual read resources remain editable after moving
                // and reopening code sites, then survive a second native export.
                if (!reopened.palette.empty()) reopened.SetPaletteColor(0, {88, 164, 216});
                for (const size_t resource : reloaded.resources) {
                    if (resource < reopened.originals.size())
                        reopened.SetBitmapReplacement(resource, std::vector<uint8_t>(reopened.originals[resource].size(), 0));
                }
                Frame resourceEdited;
                if (!renderer.Render(reopened, device, resourceEdited)) return fail("layout plus palette/resource edit: " + resourceEdited.error);
                if (!reloaded.resources.empty() && resourceEdited.rgba == reloaded.rgba)
                    return fail("native resource read trace does not identify actual rendered bitmaps");
                FirmwarePackage editedPackage;
                if (!reopened.BuildZwdfBytes(bytes, error) || !FirmwarePackage::Parse(bytes, editedPackage, error)) return fail(error);
                StudioState editedReopened;
                if (!editedReopened.LoadPackage(std::move(editedPackage), "S/X page and resource reopen", error) ||
                    !editedReopened.layoutError.empty()) return fail(error + editedReopened.layoutError);
                // This fixture deliberately is not RGB565-aligned. Mixed native
                // RGB888/RGB565 sites must retain exact RGB instruction operands,
                // not replace them with an expanded 565 representative on reopen.
                if (edition == FirmwareEdition::Factory && !reopened.palette.empty() &&
                    (editedReopened.palette.empty() || editedReopened.palette.front().current != reopened.palette.front().current))
                    return fail("Factory reopen lost exact non-RGB565-aligned RGB888 palette operands");
                std::vector<uint8_t> repeatedBytes;
                if (!editedReopened.BuildZwdfBytes(repeatedBytes, error)) return fail("reopened package export: " + error);
                if (repeatedBytes != bytes) return fail("unchanged palette/resource/layout re-export changed native package bytes");
                Frame editedFrame;
                if (!renderer.Render(editedReopened, device, editedFrame))
                    return fail("layout plus palette/resource reopen native render: " + editedFrame.error);
                if (editedFrame.rgba != resourceEdited.rgba) {
                    const auto difference = std::mismatch(resourceEdited.rgba.begin(), resourceEdited.rgba.end(),
                        editedFrame.rgba.begin(), editedFrame.rgba.end());
                    const size_t pixel = static_cast<size_t>(difference.first - resourceEdited.rgba.begin());
                    char detail[160];
                    if (pixel < resourceEdited.rgba.size() && pixel < editedFrame.rgba.size())
                        std::snprintf(detail, sizeof(detail),
                            "layout plus palette/resource LCD mismatch at (%zu,%zu): staged=%08x reopened=%08x",
                            pixel % 80, pixel / 80, resourceEdited.rgba[pixel], editedFrame.rgba[pixel]);
                    else
                        std::snprintf(detail, sizeof(detail), "layout plus palette/resource LCD size mismatch: staged=%zu reopened=%zu",
                            resourceEdited.rgba.size(), editedFrame.rgba.size());
                    return fail(detail);
                }
                if (!editor.Undo()) return fail("native move cannot be undone");
                Frame undone;
                if (!renderer.Render(editor, device, undone) || undone.rgba != stock.rgba || editor.HasUnsavedChanges())
                    return fail("native move Undo did not restore stock code");
                Point invalid;
                invalid.y = element.maximumOffset.y + 1;
                if (editor.SetLayoutOffset(index, invalid, error) || editor.HasUnsavedChanges()) return fail("unsafe native bounds accepted");
                error.clear();
                ++pagesChecked;
                if (proof) *proof << "SX PAGE " << static_cast<int>(model) << '/' << static_cast<int>(edition)
                    << " hw=" << code << " screen=" << screen.id << " move=" << shift.x << ',' << shift.y << '\n' << std::flush;
            }
            ++recordsChecked;
        }
    }
    if (proof) *proof << "SX PAGES records=" << recordsChecked << " pages=" << pagesChecked << '\n' << std::flush;
    return true;
}
