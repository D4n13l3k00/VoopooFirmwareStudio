#pragma once
#include "app_state.h"
#include "firmware_emulator.h"
#include "firmware_profiles.h"
#include "firmware_save.h"
#include "ui_firmware_layout.h"
#include <ostream>

inline bool RunLayoutRegressions(std::string& error, std::ostream* proof = nullptr,
    const FirmwareProfile* onlyProfile = nullptr) {
    using FirmwareLayout::Point;
    error.clear();
    std::error_code filesystemError;
    auto root = std::filesystem::temp_directory_path(filesystemError);
    if (filesystemError) { error = "Layout regression temporary directory unavailable"; return false; }
    root /= L"Voopoo_layout_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64());
    if (!std::filesystem::create_directory(root, filesystemError) || filesystemError) {
        error = "Layout regression temporary directory creation failed"; return false;
    }
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    } cleanup{root};
    FirmwareEmulator::Renderer renderer;
    size_t recordsChecked = 0, movesChecked = 0;
    for (const auto& profile : FirmwareProfiles()) {
        if (onlyProfile && (profile.model != onlyProfile->model || profile.edition != onlyProfile->edition)) continue;
        const std::string context = std::string(FirmwareModelName(profile.model)) + " / " + FirmwareEditionName(profile.edition);
        auto fail = [&](const std::string& reason) { error = context + ": " + reason; return false; };
        FirmwarePackage package;
        StudioState editor;
        if (!LoadEmbeddedFirmware(package, error, profile.model, profile.edition) ||
            !editor.LoadPackage(std::move(package), "layout regression", error)) return fail("load: " + error);
        editor.isBuiltin = true;
        if (!editor.layoutError.empty() || editor.layoutElements.empty()) return fail(editor.layoutError);
        UiFirmwareLayout::State grouping;
        UiFirmwareLayout::PrepareRevisionGroups(grouping, editor, true);
        size_t groupedRecords = 0;
        for (const auto& group : grouping.revisionGroups) {
            groupedRecords += group.hardwareCodes.size();
            if (group.hardwareCodes.size() > 1 && !group.nativeSamples)
                return fail("hardware revisions grouped without native movement evidence");
            if (proof) *proof << "LAYOUT GROUP " << context << " " << group.title
                << " nativeSamples=" << group.nativeSamples << "\n" << std::flush;
        }
        if (groupedRecords != editor.firmware.records.size()) return fail("grouping lost hardware records");
        for (const auto& element : editor.layoutElements) {
            for (const auto& site : element.sites) {
                if (site.encoding != FirmwareLayout::Encoding::ThumbMovAliasImm8) continue;
                const auto record = std::find_if(editor.firmware.records.begin(), editor.firmware.records.end(),
                    [&](const auto& candidate) {
                        return (uint16_t(candidate.device_code[0]) << 8 | candidate.device_code[1]) == element.hardwareCode;
                    });
                if (record == editor.firmware.records.end()) return fail("missing alias hardware record");
                const size_t recordIndex = static_cast<size_t>(record - editor.firmware.records.begin());
                const auto* original = editor.firmware.BytesAt(recordIndex, site.address, 2);
                if (!original) return fail("alias outside firmware");
                const unsigned word = original[0] | unsigned(original[1]) << 8;
                const unsigned destination = (word & 0xff80) == 0x4600 ? word & 7 : word >> 8 & 7;
                const unsigned wrong = 0x2000 | ((destination + 1) & 7) << 8 | (site.original & 255);
                const uint8_t corrupt[] = {uint8_t(wrong), uint8_t(wrong >> 8)};
                if (FirmwareLayout::MatchesCode(editor.layoutElements, element.hardwareCode,
                    site.address, original, corrupt, 2)) return fail("layout guard allows changing a destination register");
            }
        }
        size_t saveIndex = SIZE_MAX;
        Point saveOffset;
        for (const auto& record : editor.firmware.records) {
            const uint16_t code = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
            FirmwareEmulator::DeviceState device;
            device.hardwareCode = code;
            device.score = 21400;
            FirmwareEmulator::Frame stock;
            if (!renderer.Render(editor, device, stock)) return fail("stock hardware " + std::to_string(code) + ": " + stock.error);
            if (stock.hardwareCode != code || stock.rgba.empty()) return fail("wrong native hardware frame");
            auto watts = device;
            watts.watts = 42;
            FirmwareEmulator::Frame power;
            if (!renderer.Render(editor, watts, power) || power.rgba == stock.rgba)
                return fail("power input does not affect hardware " + std::to_string(code) + ": " + power.error);
            bool movable = false;
            const auto defaultDevice = device;
            const auto& standardStock = stock;
            for (size_t pass = 0; pass < editor.layoutElements.size() * 2; ++pass) {
                const size_t index = pass / 2;
                const auto& element = editor.layoutElements[index];
                if (element.hardwareCode != code || !FirmwareLayout::OnScreen(element, 0)) continue;
                Point offset;
                if (!(pass & 1)) {
                    if (element.maximumOffset.x >= element.stepX) offset.x = element.stepX;
                    else if (element.minimumOffset.x <= -element.stepX) offset.x = -element.stepX;
                } else {
                    if (element.maximumOffset.y >= element.stepY) offset.y = element.stepY;
                    else if (element.minimumOffset.y <= -element.stepY) offset.y = -element.stepY;
                }
                if (offset == Point{}) continue;
                movable = true;
                if (saveIndex == SIZE_MAX) { saveIndex = index; saveOffset = offset; }
                auto motionFail = [&](const std::string& reason) { return fail(element.id + ": " + reason); };
                auto device = defaultDevice;
                if (element.temperatureMode >= 0) device.temperatureControl = element.temperatureMode != 0;
                if (element.orientation >= 0) device.landscape = element.orientation != 0;
                FirmwareEmulator::Frame materialStock;
                const bool materialMode = element.title == "Mode" &&
                    (profile.model == FirmwareModel::Drag2 || profile.model == FirmwareModel::DragMini);
                const bool alternateView = materialMode || device.temperatureControl != defaultDevice.temperatureControl ||
                    device.landscape != defaultDevice.landscape;
                if (alternateView) {
                    if (materialMode) device.temperatureControl = true;
                    device.temperatureMaterial = 0;
                    if (!renderer.Render(editor, device, materialStock)) return motionFail(materialStock.error);
                }
                const auto* stockFrame = alternateView ? &materialStock : &standardStock;
                const auto beforeRevision = editor.ResourceRevision();
                editor.PushUndo("Move native UI regression");
                if (!editor.SetLayoutOffset(index, offset, error)) return motionFail("move: " + error);
                editor.EndUndoAction();
                if (!editor.HasUnsavedChanges() || editor.ResourceRevision() == beforeRevision)
                    return motionFail("position edit is not tracked as a document change");
                FirmwareEmulator::Frame changed;
                if (!renderer.Render(editor, device, changed)) return motionFail(changed.error);
                // Use the same conditional-state matrix as the exhaustive
                // THREE/MAX coverage. Never skip a block with no visible move.
                if (changed.rgba == stockFrame->rgba) {
                    const auto initialDevice = device;
                    for (int variant = 0; variant < 12 && changed.rgba == stockFrame->rgba; ++variant) {
                        auto candidate = initialDevice;
                        candidate.watts = variant & 1 ? 123 : 35;
                        candidate.rba = (variant & 2) != 0;
                        candidate.locked = (variant & 4) != 0;
                        candidate.charging = (variant & 1) != 0;
                        candidate.menuSelection = variant % 3;
                        candidate.temperatureMaterial = variant % 3;
                        candidate.battery = variant % 6;
                        candidate.fahrenheit = (variant & 1) != 0;
                        if (!editor.Undo()) return motionFail("conditional baseline cannot undo");
                        if (!renderer.Render(editor, candidate, materialStock)) return motionFail(materialStock.error);
                        if (!editor.Redo()) return motionFail("conditional edit cannot redo");
                        if (!renderer.Render(editor, candidate, changed)) return motionFail(changed.error);
                        device = candidate;
                        stockFrame = &materialStock;
                    }
                }
                const auto& stock = *stockFrame;
                if (changed.rgba == stock.rgba) return motionFail("native position edit has no visible effect");
                if (changed.width != stock.width || changed.height != stock.height)
                    return motionFail("position edit changes display geometry");
                auto inside = [&](int x, int y, Point delta) {
                    return x >= element.position.x + delta.x && x < element.position.x + delta.x + element.width &&
                        y >= element.position.y + delta.y && y < element.position.y + delta.y + element.height;
                };
                for (int y = 0; y < stock.height; ++y) {
                    for (int x = 0; x < stock.width; ++x) {
                        const size_t pixel = size_t(y) * stock.width + x;
                        if (!inside(x, y, {}) && !inside(x, y, offset) && stock.rgba[pixel] != changed.rgba[pixel])
                            return motionFail("position edit affects pixels outside its native element bounds");
                    }
                }
                std::vector<uint8_t> bytes;
                BuildReport report;
                if (!editor.BuildToBuffer(bytes, error, &report)) return motionFail("export: " + error);
                if (bytes.size() != editor.firmware.source_bytes.size() || report.changed_layout_elements != 1)
                    return motionFail("layout export report or image length incorrect");
                FirmwarePackage rebuilt;
                if (!FirmwarePackage::Parse(bytes, rebuilt, error)) return motionFail("parse export: " + error);
                for (size_t r = 0; r < rebuilt.records.size(); ++r) {
                    const auto& original = editor.firmware.records[r];
                    const uint16_t otherCode = uint16_t(original.device_code[0]) << 8 | original.device_code[1];
                    if (otherCode != code && rebuilt.records[r].image != original.image)
                        return motionFail("editing one revision changed another revision");
                }
                StudioState reopened;
                if (!reopened.LoadPackage(std::move(rebuilt), "reopened layout", error)) return motionFail("reopen: " + error);
                const auto restored = std::find_if(reopened.layoutElements.begin(), reopened.layoutElements.end(),
                    [&](const auto& candidate) { return candidate.id == element.id; });
                if (!reopened.layoutError.empty() || restored == reopened.layoutElements.end() ||
                    restored->position != Point{element.position.x + offset.x, element.position.y + offset.y} ||
                    reopened.HasUnsavedChanges()) return motionFail("saved position cannot be reopened: " + reopened.layoutError);
                FirmwareEmulator::Frame reloaded;
                if (!renderer.Render(reopened, device, reloaded) || reloaded.rgba != changed.rgba)
                    return motionFail("exported native frame differs after reopening: " + reloaded.error);
                if (!editor.Undo()) return motionFail("position edit cannot be undone");
                FirmwareEmulator::Frame undone;
                if (!renderer.Render(editor, device, undone) || undone.rgba != stock.rgba || editor.HasUnsavedChanges())
                    return motionFail("Undo does not restore native firmware position: " + undone.error);
                if (!editor.Redo()) return motionFail("position edit cannot be redone");
                FirmwareEmulator::Frame redone;
                if (!renderer.Render(editor, device, redone) || redone.rgba != changed.rgba)
                    return motionFail("Redo does not restore the native moved frame: " + redone.error);
                editor.Undo();
                const auto revision = editor.ResourceRevision();
                const auto history = editor.undoStack.size();
                Point invalid = offset;
                invalid.x = element.maximumOffset.x + element.stepX;
                if (editor.SetLayoutOffset(index, invalid, error) || editor.HasUnsavedChanges() ||
                    editor.ResourceRevision() != revision || editor.undoStack.size() != history)
                    return motionFail("invalid native position changes document or history");
                error.clear();
                ++movesChecked;
            }
            // Moving the header into the old separator must preserve every pixel.
            // Stock MAIN used to clear x33..54 and y9..15 after drawing it.
            if ((profile.model == FirmwareModel::DragS || profile.model == FirmwareModel::DragX) &&
                profile.edition == FirmwareEdition::Score) {
                for (size_t index = 0; index < editor.layoutElements.size(); ++index) {
                    const auto& element = editor.layoutElements[index];
                    if (element.hardwareCode != code || element.screen != 0 || element.title != "Mode") continue;
                    for (Point delta : {Point{10, 0}, Point{0, 6}, Point{10, 6}}) {
                        editor.PushUndo("Move header across native clear regions");
                        if (!editor.SetLayoutOffset(index, delta, error)) return fail(element.id + ": " + error);
                        std::vector<std::pair<size_t, Point>> movedBlocks{{index, delta}};
                        if (delta == Point{10, 6}) {
                            for (size_t other = 0; other < editor.layoutElements.size(); ++other) {
                                const auto& block = editor.layoutElements[other];
                                if (block.hardwareCode != code || block.screen != 0) continue;
                                Point shift;
                                if (block.title == "Power") shift.y = 4;
                                else if (block.title == "Score") shift.y = -4;
                                else if (block.title == "Puffs") shift.y = 4;
                                else continue;
                                if (!editor.SetLayoutOffset(other, shift, error)) return fail(block.id + ": " + error);
                                movedBlocks.emplace_back(other, shift);
                            }
                        }
                        editor.EndUndoAction();
                        FirmwareEmulator::Frame moved;
                        if (!renderer.Render(editor, device, moved)) return fail(moved.error);
                        for (const auto& [blockIndex, shift] : movedBlocks) {
                            const auto& block = editor.layoutElements[blockIndex];
                            for (int y = 0; y < block.height; ++y)
                                for (int x = 0; x < block.width; ++x) {
                                    const size_t original = size_t(block.position.y + y) * stock.width + block.position.x + x;
                                    const size_t target = size_t(block.position.y + y + shift.y) * stock.width + block.position.x + x + shift.x;
                                    if (stock.rgba[original] != moved.rgba[target])
                                        return fail(block.id + ": native clear erases jointly moved elements");
                                }
                        }
                        if (!editor.Undo()) return fail("header regression cannot undo");
                    }
                }
            }
            if (!movable) return fail("hardware record has no movable native element: " + std::to_string(code));
            ++recordsChecked;
        }
        // Exercise the real atomic writer and the saved-state/Undo boundary.
        if (saveIndex == SIZE_MAX || !editor.SetLayoutOffset(saveIndex, saveOffset, error)) return false;
        editor.EndUndoAction();
        const auto destination = root / PathUtils::ToFsPath(profile.filename).filename();
        const auto prepared = FirmwareSave::Prepare(editor.CreateFileSnapshot());
        const auto saved = FirmwareSave::Write(*prepared, editor, FirmwareSave::Approval::Confirmed,
            PathUtils::WideToUtf8(destination.wstring()));
        if (saved.FirmwareSaved()) editor.MarkSaved();
        if (saved.outcome != FirmwareSave::Outcome::Complete || editor.HasUnsavedChanges())
            return fail("native layout file was not saved: " + saved.buildError);
        FirmwarePackage disk;
        StudioState diskEditor;
        if (!FirmwarePackage::Open(PathUtils::WideToUtf8(destination.wstring()), disk, error) ||
            !diskEditor.LoadPackage(std::move(disk), "saved layout file", error)) return fail("reopen saved: " + error);
        if (!diskEditor.layoutError.empty()) return fail("written layout cannot be reopened: " + diskEditor.layoutError);
        editor.Undo();
        if (!editor.HasUnsavedChanges()) return fail("Undo after saving does not mark layout dirty");
        editor.Redo();
        if (editor.HasUnsavedChanges()) return fail("Redo to saved layout stays dirty");
        if (!editor.palette.empty()) {
            editor.PushUndo("Mixed resource, palette and layout regression");
            editor.SetPaletteColor(0, Rgb8::FromRgb565(editor.palette[0].current.ToRgb565() ^ 0x07e0));
            auto pixels = editor.originals.front();
            pixels.front() ^= 0x80;
            editor.SetBitmapReplacement(0, std::move(pixels));
            editor.EndUndoAction();
            std::vector<uint8_t> mixedBytes;
            FirmwarePackage mixedPackage;
            StudioState mixedEditor;
            if (!editor.BuildToBuffer(mixedBytes, error) || !FirmwarePackage::Parse(mixedBytes, mixedPackage, error) ||
                !mixedEditor.LoadPackage(std::move(mixedPackage), "mixed layout", error)) return fail("mixed edits: " + error);
            if (!mixedEditor.layoutError.empty()) return fail("mixed edits disable layout editing: " + mixedEditor.layoutError);
            FirmwareEmulator::DeviceState state;
            state.hardwareCode = editor.layoutElements[saveIndex].hardwareCode;
            FirmwareEmulator::Frame live, reopened;
            if (!renderer.Render(editor, state, live) || !renderer.Render(mixedEditor, state, reopened) ||
                live.rgba != reopened.rgba) return fail("mixed edits change after reopening: " + live.error + reopened.error);
        }
        if (proof) *proof << "LAYOUT " << context << " records=" << editor.firmware.records.size() << " OK\n" << std::flush;
    }
    if (proof) *proof << "LAYOUT REGRESSIONS OK records=" << recordsChecked << " moves=" << movesChecked << "\n" << std::flush;
    return true;
}

inline bool RunLandscapePowerWidthRegressions(std::string& error, std::ostream* proof = nullptr) {
    FirmwarePackage package;
    StudioState editor;
    if (!LoadEmbeddedFirmware(package, error, FirmwareModel::Drag3, FirmwareEdition::Factory) ||
        !editor.LoadPackage(std::move(package), "landscape power widths", error)) return false;
    const auto found = std::find_if(editor.layoutElements.begin(), editor.layoutElements.end(),
        [](const auto& element) { return element.id == "hw315C.main.block_08009728.mode0.orientation1"; });
    if (found == editor.layoutElements.end()) { error = "Landscape small-font block missing"; return false; }
    const size_t index = size_t(found - editor.layoutElements.begin());
    FirmwareEmulator::Renderer renderer;
    for (int watts : {35, 99, 100, 105, 177}) {
        FirmwareEmulator::DeviceState device;
        device.hardwareCode = 0x315c;
        device.landscape = true;
        device.watts = watts;
        FirmwareEmulator::Frame before, after;
        if (!renderer.Render(editor, device, before)) { error = before.error; return false; }
        editor.PushUndo("Landscape width probe");
        if (!editor.SetLayoutOffset(index, {found->stepX, 0}, error)) return false;
        editor.EndUndoAction();
        if (!renderer.Render(editor, device, after)) { error = after.error; return false; }
        size_t changed = 0;
        for (size_t pixel = 0; pixel < before.rgba.size(); ++pixel) changed += before.rgba[pixel] != after.rgba[pixel];
        if (proof) *proof << "LANDSCAPE WIDTH watts=" << watts << " changedPixels=" << changed << '\n' << std::flush;
        if ((changed != 0) != (watts >= 100)) {
            error = "Landscape power glyph movement does not follow its native digit-width branch";
            return false;
        }
        if (!editor.Undo()) { error = "Landscape probe could not restore the original"; return false; }
    }
    return true;
}
