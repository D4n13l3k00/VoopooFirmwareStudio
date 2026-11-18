#pragma once
#include "../src/core/app_state.h"
#include "../src/core/embedded_firmware.h"
#include "../src/core/firmware_emulator.h"
#include "../src/core/firmware_emulator_screens.h"
#include "../src/core/firmware_profiles.h"
#include "../src/core/layout_edit_selection.h"
#include <algorithm>
#include <array>
#include <ostream>
#include <set>

namespace ProPlusCoverageDetail {
inline bool ServiceMoves(StudioState& editor, FirmwareEmulator::Renderer& renderer,
    const FirmwareEmulator::DeviceState& device, const FirmwareEmulator::Frame& stock,
    size_t& moves, std::string& error) {
    using FirmwareLayout::Point;
    std::vector<size_t> targets;
    for (size_t i = 0; i < editor.layoutElements.size(); ++i) {
        const auto& element = editor.layoutElements[i];
        if (element.hardwareCode == device.hardwareCode &&
            FirmwareLayout::InView(element, device.screen, device.temperatureControl, device.landscape))
            targets.push_back(i);
    }
    if (targets.size() != 3) { error = "Service page must expose header, complete UID panel, and level-code panel"; return false; }
    for (const size_t index : targets) {
        const auto& element = editor.layoutElements[index];
        const auto semantic = LayoutEdit::Detail::SemanticId(element);
        const bool uid = semantic.find(".uid_panel.") != std::string_view::npos;
        const bool level = semantic.find(".level_code.") != std::string_view::npos;
        const auto fail = [&](const std::string& why) {
            error = element.id + ": " + why;
            return false;
        };
        if (uid || level) {
            // The actual emulated identity is nonzero. Check every native
            // 7x13 hex cell, with the ROM's 9px column / 17px row strides.
            // A collapsed row or column cannot pass merely by staying in bounds.
            for (int row = 0; row < (uid ? 2 : 1); ++row) {
                for (int column = 0; column < 8; ++column) {
                    bool ink = false;
                    for (int y = 0; y < 13; ++y)
                        for (int x = 0; x < 7; ++x)
                            ink = ink || stock.rgba[size_t((uid ? 59 + row * 17 : 135) + y) * stock.width +
                                5 + column * 9 + x] == 0xffffffffu;
                    if (!ink) return fail("Missing native identity glyph before movement");
                }
            }
        }
        int left = stock.width, top = stock.height, right = 0, bottom = 0;
        for (int y = element.position.y; y < element.position.y + element.height; ++y) {
            for (int x = element.position.x; x < element.position.x + element.width; ++x) {
                if ((stock.rgba[size_t(y) * stock.width + x] & 0x00ffffffu) == 0) continue;
                left = (std::min)(left, x); top = (std::min)(top, y);
                right = (std::max)(right, x + 1); bottom = (std::max)(bottom, y + 1);
            }
        }
        if (left >= right || top >= bottom) return fail("Editable service block has no native content");
        for (int axis = 0; axis < 2; ++axis) {
            const int step = axis ? element.stepY : element.stepX;
            const int minimum = axis ? element.minimumOffset.y : element.minimumOffset.x;
            const int maximum = axis ? element.maximumOffset.y : element.maximumOffset.x;
            if (minimum == 0 && maximum == 0) continue;
            // Move UID away from the header and level code away from the
            // lower glass edge. The complete nonblack content has free space;
            // its surrounding correlated clear may meet the adjacent panel.
            const int shift = level && minimum <= -step ? -step :
                maximum >= step ? step : minimum <= -step ? -step : 0;
            if (!shift) return fail("Advertised service axis has no native step");
            const Point offset = axis ? Point{0, shift} : Point{shift, 0};
            editor.PushUndo("Move complete native service content");
            if (!editor.SetLayoutOffset(index, offset, error)) return false;
            editor.EndUndoAction();
            FirmwareEmulator::Frame changed;
            if (!renderer.Render(editor, device, changed)) return fail(changed.error);
            if (changed.rgba == stock.rgba) return fail("Service move did not execute native coordinate edits");
            // Compare every pixel of the entire native content rectangle,
            // including blank gaps between all 16 UID glyphs. Constant aliases
            // that freeze a loop's row/column fail this correspondence.
            for (int y = top; y < bottom; ++y)
                for (int x = left; x < right; ++x)
                    if (stock.rgba[size_t(y) * stock.width + x] !=
                        changed.rgba[size_t(y + offset.y) * changed.width + x + offset.x])
                        return fail("Native service content was not translated intact");
            std::vector<uint8_t> packed;
            FirmwarePackage exported;
            StudioState reopened;
            if (!editor.BuildToBuffer(packed, error) || !FirmwarePackage::Parse(packed, exported, error) ||
                !reopened.LoadPackage(std::move(exported), "native service move", error)) return false;
            const auto restored = std::find_if(reopened.layoutElements.begin(), reopened.layoutElements.end(),
                [&](const auto& candidate) { return candidate.id == element.id; });
            if (!reopened.layoutError.empty() || restored == reopened.layoutElements.end() ||
                restored->position != Point{element.position.x + offset.x, element.position.y + offset.y})
                return fail("Service origin seeds did not survive export/reopen");
            FirmwareEmulator::Frame persisted;
            if (!renderer.Render(reopened, device, persisted) || persisted.rgba != changed.rgba)
                return fail("Exported native service content differs after reopening");
            editor.Undo();
            ++moves;
        }
    }
    return true;
}
} // namespace ProPlusCoverageDetail

inline bool RunProPlusCoverageRegressions(std::string& error, std::ostream* proof = nullptr) {
    using namespace FirmwareEmulator;
    using FirmwareLayout::Point;
    error.clear();
    size_t records = 0, views = 0, moves = 0, serviceMoves = 0;
    for (const auto& profile : FirmwareProfiles()) {
        if (profile.model != FirmwareModel::DragSPro && profile.model != FirmwareModel::DragXPro &&
            profile.model != FirmwareModel::DragXPlus && profile.model != FirmwareModel::DragXPlusPro) continue;
        const auto fail = [&](const std::string& why) {
            error = std::string(profile.filename) + ": " + why;
            return false;
        };
        FirmwarePackage package;
        StudioState editor;
        if (!LoadEmbeddedFirmware(package, error, profile.model, profile.edition) ||
            !editor.LoadPackage(std::move(package), profile.filename, error)) return fail(error);
        if (!editor.layoutError.empty() || editor.layoutElements.empty()) return fail("Native layout discovery: " + editor.layoutError);
        Renderer renderer;
        std::set<std::string> identifiers;
        for (const auto& element : editor.layoutElements) {
            if (!identifiers.insert(element.id).second || element.sites.empty()) return fail("Duplicate or synthetic native element");
            if (element.minimumOffset == Point{} && element.maximumOffset == Point{}) return fail("Fake immovable element");
        }
        if (editor.firmware.records.size() > 1) {
            std::vector<uint16_t> codes;
            std::vector<Frame> before(editor.firmware.records.size());
            for (size_t i = 0; i < editor.firmware.records.size(); ++i) {
                const auto& record = editor.firmware.records[i];
                codes.push_back(uint16_t(record.device_code[0]) << 8 | record.device_code[1]);
                DeviceState device;
                device.hardwareCode = codes.back();
                if (!renderer.Render(editor, device, before[i])) return fail("Group baseline: " + before[i].error);
            }
            const auto selected = std::find_if(editor.layoutElements.begin(), editor.layoutElements.end(),
                [&](const auto& element) {
                    return element.hardwareCode == codes.front() &&
                        LayoutEdit::Detail::SemanticId(element) == "main.logo.vw.portrait";
                });
            LayoutEdit::Constraints group;
            DeviceState view;
            view.hardwareCode = codes.front();
            if (selected == editor.layoutElements.end() ||
                !LayoutEdit::Resolve(editor.layoutElements, editor.layoutOffsets,
                    static_cast<size_t>(selected - editor.layoutElements.begin()), codes, group, &view) ||
                group.targets.size() != codes.size())
                return fail("Equivalent boards do not resolve the same native semantic component");
            editor.PushUndo("Shared native Pro/Plus logo move");
            if (!LayoutEdit::Apply(editor, group, {1, 0}, error)) return fail("Group move: " + error);
            editor.EndUndoAction();
            for (size_t i = 0; i < codes.size(); ++i) {
                DeviceState device;
                device.hardwareCode = codes[i];
                Frame moved;
                if (!renderer.Render(editor, device, moved) || moved.hardwareCode != codes[i] ||
                    moved.rgba == before[i].rgba) return fail("Shared move missed a native hardware record: " + moved.error);
            }
            std::vector<uint8_t> packed;
            FirmwarePackage exported;
            StudioState reopened;
            if (!editor.BuildToBuffer(packed, error) || !FirmwarePackage::Parse(packed, exported, error) ||
                !reopened.LoadPackage(std::move(exported), profile.filename, error)) return fail("Group export/reopen: " + error);
            for (const size_t target : group.targets) {
                const auto& element = editor.layoutElements[target];
                const auto persisted = std::find_if(reopened.layoutElements.begin(), reopened.layoutElements.end(),
                    [&](const auto& candidate) { return candidate.id == element.id; });
                if (persisted == reopened.layoutElements.end() ||
                    persisted->position != Point{element.position.x + 1, element.position.y})
                    return fail("Shared semantic movement did not persist for every native board");
            }
            editor.Undo();
        }
        for (const auto& record : editor.firmware.records) {
            ++records;
            const uint16_t code = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
            const auto screens = AvailableScreens(editor, code);
            if (screens.empty()) return fail("No native pages for hardware " + std::to_string(code));
            if (std::none_of(editor.layoutElements.begin(), editor.layoutElements.end(),
                [code](const auto& element) { return element.hardwareCode == code; })) return fail("Silently skipped native hardware record");
            const bool hasVoopoo = std::any_of(screens.begin(), screens.end(), [](const auto& screen) { return screen.id == 2; });
            if ((code == 0x3139 || code == 0x3132 || code == 0x3133) && hasVoopoo)
                return fail("A missing native VOOPOO page was replaced by another record");
            if (!hasVoopoo) {
                DeviceState absent;
                absent.hardwareCode = code; absent.screen = 2;
                Frame frame;
                if (renderer.Render(editor, absent, frame) || frame.hardwareCode || !frame.rgba.empty())
                    return fail("Missing native page executed a substitute image");
            }
            for (const auto& screen : screens) {
                std::array<DeviceState, 3> states{};
                size_t stateCount = 1;
                states[0].hardwareCode = code; states[0].screen = screen.id;
                states[0].temperatureControl = screen.temperatureMode == 1;
                if (screen.id == 0) {
                    if (profile.edition == FirmwareEdition::Tc) {
                        states[stateCount] = states[0]; states[stateCount++].temperatureControl = true;
                    }
                    if (profile.model == FirmwareModel::DragXPlus || profile.model == FirmwareModel::DragXPlusPro) {
                        states[stateCount] = states[0]; states[stateCount++].landscape = true;
                    }
                }
                for (size_t view = 0; view < stateCount; ++view) {
                    DeviceState device = states[view];
                    Frame stock;
                    if (!renderer.Render(editor, device, stock)) return fail("Native render: " + stock.error);
                    if (stock.hardwareCode != code || stock.rgba.size() != 12800 || !stock.spiBytes ||
                        !stock.pixelWrites || stock.width * stock.height != 12800)
                        return fail("Wrong native LCD stream or hardware record");
                    if (stock.emulatedUid != (screen.id == 20)) return fail("Wrong service UID provenance");
                    ++views;
                    // Exercise native selection/confirmation and TC material/unit
                    // banks, both power-width branches, lock and charging paths.
                    const unsigned variants = (std::max)(3u, screen.menuItems);
                    for (unsigned choice = 0; choice < variants; ++choice) {
                        auto alternate = device;
                        alternate.menuSelection = static_cast<int>(choice);
                        alternate.menuConfirmed = choice % 2 != 0;
                        alternate.temperatureMaterial = static_cast<int>(choice % 3);
                        alternate.fahrenheit = choice % 2 != 0;
                        alternate.temperature = alternate.fahrenheit ? 600 : 315;
                        alternate.watts = choice ? 100 : 5;
                        alternate.puffs = choice ? 99999 : 0;
                        alternate.battery = static_cast<int>(choice % 6);
                        alternate.locked = choice % 2 != 0;
                        alternate.charging = choice % 2 != 0;
                        alternate.rba = choice % 2 != 0;
                        Frame changed;
                        if (!renderer.Render(editor, alternate, changed) || changed.hardwareCode != code)
                            return fail("Native state transition: " + changed.error);
                    }
                    if (screen.id == 20 &&
                        !ProPlusCoverageDetail::ServiceMoves(editor, renderer, device, stock, serviceMoves, error))
                        return fail("Service content translation: " + error);
                    const auto target = std::find_if(editor.layoutElements.begin(), editor.layoutElements.end(), [&](const auto& element) {
                        return element.hardwareCode == code && FirmwareLayout::OnScreen(element, device.screen) &&
                            (element.temperatureMode < 0 || element.temperatureMode == int(device.temperatureControl)) &&
                            (element.orientation < 0 || element.orientation == int(device.landscape));
                    });
                    if (target == editor.layoutElements.end()) return fail("Native view has no genuine editable block");
                    const size_t index = static_cast<size_t>(target - editor.layoutElements.begin());
                    const std::string id = target->id;
                    const Point position = target->position;
                    Point offset;
                    if (target->maximumOffset.x >= target->stepX) offset.x = target->stepX;
                    else if (target->minimumOffset.x <= -target->stepX) offset.x = -target->stepX;
                    else if (target->maximumOffset.y >= target->stepY) offset.y = target->stepY;
                    else if (target->minimumOffset.y <= -target->stepY) offset.y = -target->stepY;
                    else return fail("Native block advertises movement without a usable axis");
                    editor.PushUndo("Native Pro/Plus coverage move");
                    if (!editor.SetLayoutOffset(index, offset, error)) return fail(error);
                    Frame moved;
                    if (!renderer.Render(editor, device, moved)) return fail("Moved native render: " + moved.error);
                    if (moved.rgba == stock.rgba) return fail("Native coordinate edit did not change the LCD stream: " + id);
                    if (!moved.resources.empty()) {
                        const size_t resource = moved.resources.front();
                        auto raw = editor.originals[resource];
                        if (!raw.empty()) { raw[0] ^= 1; editor.SetBitmapReplacement(resource, std::move(raw)); }
                    }
                    if (!editor.palette.empty()) {
                        const auto color = editor.palette.front().current.ToRgb565() ^ 1;
                        editor.SetPaletteColor(0, Rgb8::FromRgb565(static_cast<uint16_t>(color)));
                    }
                    editor.EndUndoAction();
                    Frame combined;
                    if (!renderer.Render(editor, device, combined)) return fail("Layout/resource/palette interaction: " + combined.error);
                    std::vector<uint8_t> packed;
                    FirmwarePackage exported;
                    StudioState reopened;
                    if (!editor.BuildToBuffer(packed, error) || !FirmwarePackage::Parse(packed, exported, error) ||
                        !reopened.LoadPackage(std::move(exported), profile.filename, error)) return fail("Export/reopen: " + error);
                    const auto restored = std::find_if(reopened.layoutElements.begin(), reopened.layoutElements.end(),
                        [&](const auto& element) { return element.id == id; });
                    if (!reopened.layoutError.empty() || restored == reopened.layoutElements.end() ||
                        restored->position != Point{position.x + offset.x, position.y + offset.y})
                        return fail("Native coordinates did not survive resource/palette export");
                    Frame persisted;
                    if (!renderer.Render(reopened, device, persisted) || persisted.hardwareCode != code || persisted.rgba != combined.rgba)
                        return fail("Exported native LCD execution differs after reopening: " + persisted.error);
                    std::vector<uint8_t> exact;
                    if (!reopened.BuildToBuffer(exact, error) || exact != packed) return fail("Reopened native export is not byte-exact");
                    editor.Undo();
                    ++moves;
                }
            }
        }
        DeviceState invalid;
        invalid.hardwareCode = 0xffff;
        Frame frame;
        if (renderer.Render(editor, invalid, frame) || !AvailableScreens(editor, invalid.hardwareCode).empty())
            return fail("Unknown hardware silently selected a canonical record");
    }
    if (records != 23 || !views || moves != views || serviceMoves < records * 3) {
        error = "Incomplete bundled Pro/Plus native coverage"; return false;
    }
    if (proof) *proof << "Pro/Plus native records=" << records << " views=" << views <<
        " persisted_moves=" << moves << " service_axes=" << serviceMoves << '\n';
    return true;
}
