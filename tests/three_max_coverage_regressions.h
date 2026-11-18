#pragma once
#include "../src/core/app_state.h"
#include "../src/core/firmware_emulator.h"
#include "../src/core/firmware_emulator_screens.h"
#include "../src/core/firmware_profiles.h"
#include "../src/core/firmware_three_max_profiles.h"
#include "../src/core/firmware_three_max_layout_profiles.h"
#include "../src/core/layout_edit_selection.h"
#include <algorithm>
#include <ostream>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>

inline bool RunThreeMaxHexGlyphRegressions(StudioState& state, FirmwareEmulator::Renderer& renderer,
    std::string& error, std::ostream* proof) {
    using namespace FirmwareEmulator;
    constexpr std::array<uint16_t, 3> codes{0x3134, 0x3030, 0x311c};
    std::array<Frame, 3> stock;
    for (size_t r = 0; r < codes.size(); ++r) {
        DeviceState device;
        device.hardwareCode = codes[r];
        device.screen = 15;
        if (!renderer.Render(state, device, stock[r])) { error = stock[r].error; return false; }
    }
    for (unsigned digit = 10; digit < 16; ++digit) {
        char id[64];
        std::snprintf(id, sizeof(id), "font_native_9x18_302_%02u", digit);
        const auto fail = [&](const std::string& reason) {
            error = std::string("Native UID glyph ") + char('A' + digit - 10) + ": " + reason;
            return false;
        };
        const int index = state.FindAssetIndex(id);
        if (index < 0) return fail("missing editable hexadecimal slot");
        const auto& asset = state.catalog[index];
        const auto& original = state.originals[index];
        if (asset.width != 9 || asset.height != 18 || asset.format != AssetFormat::Mask1Bit ||
            asset.recordAddresses.size() != codes.size()) return fail("incorrect native glyph geometry or hardware coverage");
        for (size_t r = 0; r < codes.size(); ++r) {
            if (std::none_of(asset.recordAddresses.begin(), asset.recordAddresses.end(),
                    [&](const auto& location) { return location.first == codes[r]; }) ||
                std::find(stock[r].resources.begin(), stock[r].resources.end(), size_t(index)) == stock[r].resources.end())
                return fail("native service renderer does not read this glyph on hardware " + std::to_string(codes[r]));
        }
        auto bitmap = original;
        for (size_t y = 0; y < asset.height; ++y)
            for (size_t x = 0; x < asset.width; ++x)
                bitmap[y * asset.stride() + x / 8] ^= uint8_t(1u << (x % 8));
        state.PushUndo("Invert native UID hexadecimal glyph");
        state.SetBitmapReplacement(size_t(index), bitmap);
        state.EndUndoAction();
        std::array<Frame, 3> changed;
        for (size_t r = 0; r < codes.size(); ++r) {
            DeviceState device;
            device.hardwareCode = codes[r];
            device.screen = 15;
            if (!renderer.Render(state, device, changed[r]) || changed[r].hardwareCode != codes[r] ||
                changed[r].rgba == stock[r].rgba)
                return fail("bitmap edit has no native LCD effect: " + changed[r].error);
        }
        std::vector<uint8_t> bytes;
        FirmwarePackage rebuilt;
        StudioState reopened;
        if (!state.BuildToBuffer(bytes, error) || !FirmwarePackage::Parse(bytes, rebuilt, error)) return fail(error);
        for (const auto& [code, address] : asset.recordAddresses) {
            const auto record = std::find_if(rebuilt.records.begin(), rebuilt.records.end(), [&](const auto& candidate) {
                return (uint16_t(candidate.device_code[0]) << 8 | candidate.device_code[1]) == code;
            });
            if (record == rebuilt.records.end()) return fail("export lost a glyph hardware record");
            const auto* raw = rebuilt.BytesAt(static_cast<size_t>(record - rebuilt.records.begin()), address, bitmap.size());
            if (!raw || !std::equal(bitmap.begin(), bitmap.end(), raw)) return fail("export lost the hexadecimal bitmap bytes");
        }
        if (!reopened.LoadPackage(std::move(rebuilt), "edited native UID hex glyph", error) ||
            !reopened.layoutError.empty()) return fail("reopen: " + error + reopened.layoutError);
        const int reopenedIndex = reopened.FindAssetIndex(id);
        if (reopenedIndex < 0 || reopened.originals[reopenedIndex] != bitmap)
            return fail("reopened hexadecimal slot differs from the edited resource");
        for (size_t r = 0; r < codes.size(); ++r) {
            DeviceState device;
            device.hardwareCode = codes[r];
            device.screen = 15;
            Frame frame;
            if (!renderer.Render(reopened, device, frame) || frame.rgba != changed[r].rgba ||
                std::find(frame.resources.begin(), frame.resources.end(), size_t(reopenedIndex)) == frame.resources.end())
                return fail("export/reopen changed native UID glyph rendering: " + frame.error);
        }
        reopened.PushUndo("Restore original native UID hex glyph");
        reopened.SetBitmapReplacement(size_t(reopenedIndex), original);
        reopened.EndUndoAction();
        std::vector<uint8_t> restoredBytes;
        FirmwarePackage restoredPackage;
        StudioState restored;
        if (!reopened.BuildToBuffer(restoredBytes, error) || restoredBytes != state.firmware.source_bytes ||
            !FirmwarePackage::Parse(restoredBytes, restoredPackage, error) ||
            !restored.LoadPackage(std::move(restoredPackage), "restored native UID hex glyph", error))
            return fail("restoring the glyph does not recover byte-exact stock firmware: " + error);
        for (size_t r = 0; r < codes.size(); ++r) {
            DeviceState device;
            device.hardwareCode = codes[r];
            device.screen = 15;
            Frame frame;
            if (!renderer.Render(restored, device, frame) || frame.rgba != stock[r].rgba)
                return fail("restored native UID pixels differ from stock: " + frame.error);
        }
        if (!state.Undo() || state.HasUnsavedChanges()) return fail("glyph edit cannot return to the unchanged document");
        if (proof) *proof << "THREE/MAX UID HEX " << char('A' + digit - 10) << " nativeRecords=" << codes.size()
            << " edit/export/reopen/restore OK\n" << std::flush;
    }
    return true;
}

inline bool RunThreeMaxPaddingGlyphRegression(StudioState& state, FirmwareEmulator::Renderer& renderer,
    std::string& error, std::ostream* proof) {
    using namespace FirmwareEmulator;
    using FirmwareLayout::Point;
    const auto fail = [&](const std::string& reason) { error = "TC3151 painted glyph/padding: " + reason; return false; };
    const auto element = std::find_if(state.layoutElements.begin(), state.layoutElements.end(), [](const auto& candidate) {
        return candidate.id == "hw3151.main.block_080090D8.mode0.orientation1";
    });
    const int glyphIndex = state.FindAssetIndex("font_native_10x6_513_01");
    if (element == state.layoutElements.end() || glyphIndex < 0) return fail("missing native glyph or editable field");
    const auto& asset = state.catalog[glyphIndex];
    DeviceState device;
    device.hardwareCode = 0x3151;
    device.landscape = true;
    device.puffs = 11111;
    Frame original;
    if (!renderer.Render(state, device, original) ||
        std::find(original.resources.begin(), original.resources.end(), size_t(glyphIndex)) == original.resources.end())
        return fail("native service does not read the selected digit: " + original.error);
    auto bitmap = state.originals[glyphIndex];
    for (size_t y = 0; y < asset.height; ++y)
        for (size_t x = 0; x < asset.width; ++x)
            bitmap[y * asset.stride() + x / 8] |= uint8_t(1u << (x % 8));
    state.PushUndo("Paint the complete native glyph cell");
    state.SetBitmapReplacement(size_t(glyphIndex), bitmap);
    state.EndUndoAction();
    Frame painted;
    if (!renderer.Render(state, device, painted)) return fail(painted.error);
    const int glyphWidth = int(asset.height), glyphHeight = int(asset.width);
    const auto glyphPixels = [&](const Frame& frame, Point delta, const char* phase) {
        for (int y = 0; y < glyphHeight; ++y)
            for (int x = 0; x < glyphWidth; ++x) {
                const int px = element->position.x + delta.x + x, py = element->position.y + delta.y + y;
                const uint32_t actual = frame.rgba[size_t(py) * frame.width + px];
                if (actual != 0xffffffff) {
                    std::ostringstream details;
                    details << phase << " delta=(" << delta.x << ',' << delta.y << ") first=(" << px << ',' << py
                        << ") expected=0xffffffff actual=0x" << std::hex << actual << std::dec
                        << " state{HW=3151,screen=0,TC=0,landscape=1,puffs=" << device.puffs << '}';
                    return fail(details.str());
                }
            }
        return true;
    };
    if (!glyphPixels(painted, {}, "painted native glyph")) return false;
    for (int axis = 0; axis < 2; ++axis) {
        Point delta;
        int& shift = axis ? delta.y : delta.x;
        const int maximum = axis ? element->maximumOffset.y : element->maximumOffset.x;
        const int minimum = axis ? element->minimumOffset.y : element->minimumOffset.x;
        const int step = axis ? element->stepY : element->stepX;
        if (maximum >= step) shift = step;
        else if (minimum <= -step) shift = -step;
        else return fail("glyph has no valid movement direction");
        state.PushUndo("Move the painted native glyph and its trailing clear");
        if (!state.SetLayoutOffset(static_cast<size_t>(element - state.layoutElements.begin()), delta, error)) return fail(error);
        state.EndUndoAction();
        Frame moved;
        if (!renderer.Render(state, device, moved)) return fail(moved.error);
        if (!glyphPixels(moved, delta, "moved native glyph")) return false;
        for (int y = 0; y < painted.height; ++y)
            for (int x = 0; x < painted.width; ++x) {
                const auto inside = [&](Point offset) {
                    return x >= element->position.x + offset.x && y >= element->position.y + offset.y &&
                        x < element->position.x + element->width + offset.x &&
                        y < element->position.y + element->height + offset.y;
                };
                const size_t pixel = size_t(y) * painted.width + x;
                if (!inside({}) && !inside(delta) && painted.rgba[pixel] != moved.rgba[pixel])
                    return fail("painted glyph movement changes an unrelated native pixel at " +
                        std::to_string(x) + "," + std::to_string(y) + " axis=" + std::to_string(axis));
            }
        std::vector<uint8_t> bytes;
        FirmwarePackage package;
        StudioState reopened;
        if (!state.BuildToBuffer(bytes, error) || !FirmwarePackage::Parse(bytes, package, error) ||
            !reopened.LoadPackage(std::move(package), "painted native glyph and padding", error)) return fail(error);
        const int reopenedGlyph = reopened.FindAssetIndex(asset.id);
        Frame loaded;
        if (!reopened.layoutError.empty() || reopenedGlyph < 0 || reopened.originals[reopenedGlyph] != bitmap ||
            !renderer.Render(reopened, device, loaded) || loaded.rgba != moved.rgba)
            return fail("resource/layout export and reopen change the native glyph: " + reopened.layoutError + loaded.error);
        if (!state.Undo()) return fail("cannot undo the painted glyph move");
    }
    if (!state.Undo() || state.HasUnsavedChanges()) return fail("cannot restore the original glyph resource");
    std::vector<uint8_t> restoredBytes;
    Frame restored;
    if (!state.BuildToBuffer(restoredBytes, error) || restoredBytes != state.firmware.source_bytes ||
        !renderer.Render(state, device, restored) || restored.rgba != original.rgba)
        return fail("restoring the glyph changes stock firmware bytes or native pixels: " + error + restored.error);
    if (proof) *proof << "THREE/MAX TC3151 full-glyph padding edit/move/export/reopen/restore OK\n" << std::flush;
    return true;
}

inline bool RunThreeMaxCoverageRegressions(std::string& error, std::ostream* proof = nullptr) {
    using namespace FirmwareEmulator;
    using FirmwareLayout::Point;
    error.clear();
    uint16_t selectedHardware = 0;
    size_t expectedRecords = 11, expectedSharedEdits = 4;
    if (const char* digits = std::getenv("VFS_NATIVE_HW")) {
        if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) digits += 2;
        unsigned value = 0, count = 0;
        for (; *digits; ++digits) {
            const int nibble = *digits >= '0' && *digits <= '9' ? *digits - '0' :
                *digits >= 'a' && *digits <= 'f' ? *digits - 'a' + 10 :
                *digits >= 'A' && *digits <= 'F' ? *digits - 'A' + 10 : -1;
            if (nibble < 0 || ++count > 4) {
                error = "VFS_NATIVE_HW must be one to four hexadecimal digits (optional 0x prefix)";
                return false;
            }
            value = value * 16 + unsigned(nibble);
        }
        if (!count || !value) { error = "VFS_NATIVE_HW must name a nonzero hardware code"; return false; }
        selectedHardware = uint16_t(value);
        expectedRecords = expectedSharedEdits = 0;
        for (const auto& profile : ThreeMaxNative::Profiles) {
            if (profile.code != selectedHardware) continue;
            ++expectedRecords;
            if (profile.model == FirmwareModel::Drag3) expectedSharedEdits += 2;
        }
        if (!expectedRecords) { error = "VFS_NATIVE_HW is not a bundled DRAG3 / MAX hardware code"; return false; }
        (proof ? *proof : std::clog) << "THREE/MAX SELECTED HW " << std::hex << selectedHardware << std::dec
            << " (filtered diagnostic; final all-HW coverage still required)\n" << std::flush;
    }
    Renderer renderer;
    size_t records = 0, views = 0, moves = 0, sharedEdits = 0;
    for (const auto& release : FirmwareProfiles()) {
        if (release.model != FirmwareModel::Drag3 && release.model != FirmwareModel::DragMax) continue;
        if (selectedHardware && !ThreeMaxNative::Find(release.model, release.edition, selectedHardware)) continue;
        StudioState state;
        FirmwarePackage package;
        const auto fail = [&](const std::string& reason) {
            error = std::string(release.filename) + ": " + reason;
            return false;
        };
        if (!LoadEmbeddedFirmware(package, error, release.model, release.edition) ||
            !state.LoadPackage(std::move(package), release.filename, error)) return fail(error);
        if (!state.layoutError.empty() || state.layoutElements.empty()) return fail("layout discovery: " + state.layoutError);
        if (release.model == FirmwareModel::Drag3 && release.edition == FirmwareEdition::Factory) {
            if (!RunThreeMaxHexGlyphRegressions(state, renderer, error, proof)) return fail(error);
            const auto pointer = std::find_if(state.layoutElements.begin(), state.layoutElements.end(), [](const auto& element) {
                return element.id == "hw315C.main.block_08009056.mode0.orientation1";
            });
            if (pointer == state.layoutElements.end() || pointer->position != Point{106, 18} ||
                pointer->width != 20 || pointer->height != 16 ||
                std::none_of(pointer->sites.begin(), pointer->sites.end(), [](const auto& site) {
                    return site.address == 0x08009052 && site.original == 106;
                })) return fail("landscape redraw pointer inherits a skipped conditional update coordinate");
            // The exhaustive moves below retain their strict outside-bounds
            // assertion. Native 902C's #126 belongs only to the path that skips
            // draw call9056; the actual redraw call uses #106.
        }
        if (state.firmware.records.size() > 1) {
            std::vector<uint16_t> allCodes;
            for (const auto& hardware : state.firmware.records)
                allCodes.push_back(uint16_t(hardware.device_code[0]) << 8 | hardware.device_code[1]);
            const auto anchor = std::find_if(state.layoutElements.begin(), state.layoutElements.end(), [](const auto& element) {
                return element.hardwareCode == 0x3030 && element.screen == 0 && element.temperatureMode == 0 &&
                    element.orientation == 0 && element.title == "Header DRAG";
            });
            if (anchor == state.layoutElements.end() || anchor->semanticId.empty() ||
                anchor->semanticId.starts_with("three_max.local.")) return fail("missing resource-proven shared header identity");
            const size_t selected = static_cast<size_t>(anchor - state.layoutElements.begin());
            // The header is recovered from the actual DRAG bitmap pointer in
            // every record, including 96x160 and relocated 311C renderers.
            for (const std::vector<uint16_t>& codes : {std::vector<uint16_t>{0x3030, 0x3134}, allCodes}) {
                DeviceState view;
                view.hardwareCode = 0x3030;
                LayoutEdit::Constraints constraints;
                if (!LayoutEdit::Resolve(state.layoutElements, state.layoutOffsets, selected, codes, constraints, &view) ||
                    constraints.targets.size() != codes.size()) return fail("resource-equivalent hardware blocks cannot resolve");
                Point delta;
                if (constraints.y.maximum >= 1) delta.y = 1;
                else if (constraints.y.minimum <= -1) delta.y = -1;
                else return fail("shared native header has no common movement range");
                std::vector<Frame> before(allCodes.size());
                for (size_t r = 0; r < allCodes.size(); ++r) {
                    view.hardwareCode = allCodes[r];
                    if (!renderer.Render(state, view, before[r])) return fail(before[r].error);
                }
                state.PushUndo("Move resource-proven hardware revision selection");
                if (!LayoutEdit::Apply(state, constraints, delta, error)) return fail(error);
                state.EndUndoAction();
                std::vector<Frame> moved(allCodes.size());
                for (size_t r = 0; r < allCodes.size(); ++r) {
                    view.hardwareCode = allCodes[r];
                    if (!renderer.Render(state, view, moved[r])) return fail(moved[r].error);
                    const bool targeted = std::find(codes.begin(), codes.end(), allCodes[r]) != codes.end();
                    if ((moved[r].rgba != before[r].rgba) != targeted)
                        return fail("shared revision move affects the wrong native hardware records");
                }
                std::vector<uint8_t> exported;
                FirmwarePackage rebuilt;
                StudioState reopened;
                if (!state.BuildToBuffer(exported, error) || !FirmwarePackage::Parse(exported, rebuilt, error))
                    return fail("shared revision export: " + error);
                for (size_t r = 0; r < rebuilt.records.size(); ++r) {
                    const auto& hardware = rebuilt.records[r];
                    const uint16_t code = uint16_t(hardware.device_code[0]) << 8 | hardware.device_code[1];
                    if (std::find(codes.begin(), codes.end(), code) == codes.end() &&
                        hardware.image != state.firmware.records[r].image)
                        return fail("shared revision export modifies an unselected hardware record");
                }
                if (!reopened.LoadPackage(std::move(rebuilt), "shared native revision move", error) ||
                    !reopened.layoutError.empty()) return fail("shared revision reopen: " + error + reopened.layoutError);
                for (size_t target : constraints.targets) {
                    const auto& original = state.layoutElements[target];
                    const auto restored = std::find_if(reopened.layoutElements.begin(), reopened.layoutElements.end(),
                        [&](const auto& candidate) { return candidate.id == original.id; });
                    if (restored == reopened.layoutElements.end() || restored->semanticId != original.semanticId ||
                        restored->position != Point{original.position.x + delta.x, original.position.y + delta.y})
                        return fail("shared semantic identity or native position changed after reopening");
                }
                const auto reopenedAnchor = std::find_if(reopened.layoutElements.begin(), reopened.layoutElements.end(),
                    [&](const auto& candidate) { return candidate.id == anchor->id; });
                view.hardwareCode = 0x3030;
                LayoutEdit::Constraints reopenedConstraints;
                if (reopenedAnchor == reopened.layoutElements.end() ||
                    !LayoutEdit::Resolve(reopened.layoutElements, reopened.layoutOffsets,
                        static_cast<size_t>(reopenedAnchor - reopened.layoutElements.begin()), codes, reopenedConstraints, &view))
                    return fail("exported shared semantic group cannot be selected again");
                for (size_t r = 0; r < allCodes.size(); ++r) {
                    view.hardwareCode = allCodes[r];
                    Frame frame;
                    if (!renderer.Render(reopened, view, frame) || frame.rgba != moved[r].rgba)
                        return fail("shared revision native LCD stream changes after reopening: " + frame.error);
                }
                if (!state.Undo() || !state.layoutOffsets.empty()) return fail("cannot undo complete shared revision move");
                ++sharedEdits;
            }
            if (release.edition == FirmwareEdition::Tc) {
                DeviceState temperatureView;
                temperatureView.temperatureControl = true;
                const auto temperatureAnchor = std::find_if(state.layoutElements.begin(), state.layoutElements.end(),
                    [&](const auto& element) {
                        return element.hardwareCode == 0x3030 && element.title == "Header DRAG" &&
                            FirmwareLayout::InView(element, 0, true, false);
                    });
                LayoutEdit::Constraints temperatureConstraints;
                if (temperatureAnchor == state.layoutElements.end() || temperatureAnchor->semanticId != anchor->semanticId ||
                    !LayoutEdit::Resolve(state.layoutElements, {}, static_cast<size_t>(temperatureAnchor - state.layoutElements.begin()),
                        allCodes, temperatureConstraints, &temperatureView) || temperatureConstraints.targets.size() != allCodes.size())
                    return fail("same resource purpose cannot resolve across native temperature views");
            }
            auto ambiguous = state.layoutElements;
            auto duplicate = ambiguous[selected];
            duplicate.id += ".ambiguous";
            ambiguous.push_back(std::move(duplicate));
            LayoutEdit::Constraints rejected;
            DeviceState view;
            if (LayoutEdit::Resolve(ambiguous, {}, selected, allCodes, rejected, &view))
                return fail("ambiguous semantic candidates silently select an arbitrary block");
        }
        for (const auto& record : state.firmware.records) {
            const uint16_t code = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
            if (selectedHardware && code != selectedHardware) continue;
            const auto* native = ThreeMaxNative::Find(release.model, release.edition, code);
            if (!native) return fail("missing hardware ABI");
            if (release.edition == FirmwareEdition::Tc && code == 0x3151 &&
                !RunThreeMaxPaddingGlyphRegression(state, renderer, error, proof)) return fail(error);
            if (std::count_if(std::begin(ThreeMaxNative::LayoutBlocks), std::end(ThreeMaxNative::LayoutBlocks),
                [&](const auto& block) {
                    const auto& profile = ThreeMaxNative::Profiles[block.profile];
                    return profile.model == release.model && profile.edition == release.edition && profile.code == code &&
                        block.screen == 15 && block.loopCount == 2;
                }) != 1) return fail("UID and level code are not one complete two-field native service block");
            const auto off = std::find_if(state.layoutElements.begin(), state.layoutElements.end(),
                [&](const auto& element) { return element.hardwareCode == code && element.screen == 3; });
            if (off == state.layoutElements.end() ||
                std::count_if(state.layoutElements.begin(), state.layoutElements.end(),
                    [&](const auto& element) { return element.hardwareCode == code && element.screen == 3; }) != 1 ||
                std::count_if(off->sites.begin(), off->sites.end(), [](const auto& site) { return site.vertical; }) < 2 ||
                std::count_if(off->sites.begin(), off->sites.end(), [](const auto& site) { return !site.vertical; }) < 2)
                return fail("visible OFF mask and delayed erase do not share one native coordinate owner");
            if (release.model == FirmwareModel::Drag3 &&
                std::count_if(state.layoutElements.begin(), state.layoutElements.end(),
                    [&](const auto& element) { return element.hardwareCode == code && element.screen == 16; }) != 1)
                return fail("native animated startup logo does not have exactly one editable lifecycle owner");
            const auto screens = AvailableScreens(state, code);
            const size_t expected = release.model == FirmwareModel::DragMax ? 17 :
                release.edition == FirmwareEdition::Tc ? 20 : 18;
            if (screens.size() != expected) return fail("native pages were omitted");
            for (const auto& screen : screens) {
                const int variants = screen.id == 0 ? (release.edition == FirmwareEdition::Tc ? 3 : 2) : 1;
                for (int variant = 0; variant < variants; ++variant) {
                    DeviceState device;
                    device.hardwareCode = code;
                    device.screen = screen.id;
                    device.landscape = variant == 1;
                    device.temperatureControl = variant == 2;
                    ApplyScreenMode(device, screen);
                    for (unsigned selection = 0; selection < (std::max)(1u, screen.menuItems); ++selection) {
                        device.menuSelection = static_cast<int>(selection);
                        Frame frame;
                        if (!renderer.Render(state, device, frame)) return fail("native page " + std::to_string(screen.id) + ": " + frame.error);
                        const bool landscape = screen.id == 0 && device.landscape && !device.temperatureControl;
                        if (frame.hardwareCode != code || frame.width != (landscape ? native->height : native->width) ||
                            frame.height != (landscape ? native->width : native->height) || frame.rgba.empty() ||
                            !frame.spiBytes || frame.pixelWrites < size_t(native->width) * native->height)
                            return fail("wrong record-local native LCD stream");
                        ++views;
                    }
                }
            }
            DeviceState power;
            power.hardwareCode = code;
            Frame before, after;
            if (!renderer.Render(state, power, before)) return fail(before.error);
            power.watts = 42;
            power.puffs = 9876;
            power.battery = 1;
            power.resistanceMilliOhms = 567;
            power.timeCentiseconds = 456;
            if (!renderer.Render(state, power, after) || before.rgba == after.rgba)
                return fail("native device state does not affect display: " + after.error);
            size_t recordMoves = 0;
            for (size_t index = 0; index < state.layoutElements.size(); ++index) {
                const auto element = state.layoutElements[index];
                if (element.hardwareCode != code) continue;
                for (int axis = 0; axis < 2; ++axis) {
                    Point delta;
                    int& shift = axis ? delta.y : delta.x;
                    const int minimum = axis ? element.minimumOffset.y : element.minimumOffset.x;
                    const int maximum = axis ? element.maximumOffset.y : element.maximumOffset.x;
                    const int step = axis ? element.stepY : element.stepX;
                    if (maximum >= step) shift = step;
                    else if (minimum <= -step) shift = -step;
                    else continue;
                    const auto moveFail = [&](const std::string& reason) { return fail(element.id + ": " + reason); };
                    bool visible = false;
                    DeviceState selected;
                    Frame changed;
                    for (int variant = 0; variant < 12 && !visible; ++variant) {
                        DeviceState device;
                        device.hardwareCode = code;
                        device.screen = element.screen;
                        device.temperatureControl = element.temperatureMode == 1;
                        device.landscape = element.orientation == 1;
                        device.watts = variant & 1 ? 123 : 35;
                        device.rba = (variant & 2) != 0;
                        device.locked = (variant & 4) != 0;
                        device.charging = (variant & 1) != 0;
                        device.menuSelection = variant % 3;
                        device.temperatureMaterial = variant % 3;
                        device.battery = variant % 6;
                        device.fahrenheit = (variant & 1) != 0;
                        Frame stock;
                        if (!renderer.Render(state, device, stock)) return moveFail(stock.error);
                        state.PushUndo("Move native DRAG block");
                        if (!state.SetLayoutOffset(index, delta, error)) return moveFail(error);
                        state.EndUndoAction();
                        if (!renderer.Render(state, device, changed)) return moveFail(changed.error);
                        visible = changed.rgba != stock.rgba;
                        if (visible) {
                            selected = device;
                            const auto pixelFail = [&](const char* reason, int sx, int sy, int tx, int ty,
                                    uint32_t expected, uint32_t actual) {
                                std::ostringstream details;
                                details << reason << " axis=" << (axis ? 'y' : 'x') << " delta=(" << delta.x << ',' << delta.y
                                    << ") variant=" << variant << " source=(" << sx << ',' << sy << ") target=(" << tx << ',' << ty
                                    << ") expected=0x" << std::hex << expected << " actual=0x" << actual << std::dec
                                    << " box=(" << element.position.x << ',' << element.position.y << ',' << element.width << ',' << element.height
                                    << ") state{screen=" << device.screen << ",TC=" << device.temperatureControl << ",landscape=" << device.landscape
                                    << ",watts=" << device.watts << ",puffs=" << device.puffs << ",battery=" << device.battery
                                    << ",ohms=" << device.resistanceMilliOhms << ",time=" << device.timeCentiseconds << ",rba=" << device.rba
                                    << ",locked=" << device.locked << ",charging=" << device.charging << ",menu=" << device.menuSelection
                                    << ",material=" << device.temperatureMaterial << ",F=" << device.fahrenheit << '}';
                                return moveFail(details.str());
                            };
                            for (int y = 0; y < stock.height; ++y)
                                for (int x = 0; x < stock.width; ++x) {
                                    const auto inside = [&](Point offset) {
                                        return x >= element.position.x + offset.x && y >= element.position.y + offset.y &&
                                            x < element.position.x + element.width + offset.x && y < element.position.y + element.height + offset.y;
                                    };
                                    const size_t pixel = size_t(y) * stock.width + x;
                                    if (!inside({}) && !inside(delta) && stock.rgba[pixel] != changed.rgba[pixel])
                                        return pixelFail("native movement changes pixels outside its correlated block",
                                            x, y, x, y, stock.rgba[pixel], changed.rgba[pixel]);
                                }
                            if ((native->width == 128 && element.screen == 0 && element.temperatureMode == 0 &&
                                    element.orientation == 1 && element.position == Point{174, 29} &&
                                    element.width == 46 && element.height == 14) ||
                                (release.edition == FirmwareEdition::Tc &&
                                    element.id == "hw3151.main.block_080090D8.mode0.orientation1")) {
                                int contentWidth = element.width, contentHeight = element.height;
                                if (release.edition == FirmwareEdition::Tc &&
                                    element.id == "hw3151.main.block_080090D8.mode0.orientation1") {
                                    // Native glyph90D8 is followed by clear90E8.
                                    // After a +Y move, glyph9112 overwrites the
                                    // translated padding, not the glyph itself.
                                    const int font = state.FindAssetIndex("font_native_10x6_513_00");
                                    if (font < 0) return moveFail("missing native glyph-bank dimensions");
                                    contentWidth = int(state.catalog[font].height);
                                    contentHeight = int(state.catalog[font].width);
                                }
                                for (int y = 0; y < contentHeight; ++y)
                                    for (int x = 0; x < contentWidth; ++x) {
                                        const int sx = element.position.x + x, sy = element.position.y + y;
                                        const size_t source = size_t(sy) * stock.width + sx;
                                        const size_t target = size_t(sy + delta.y) * changed.width + sx + delta.x;
                                        if (stock.rgba[source] != changed.rgba[target])
                                            return pixelFail("native glyph content, size or clear padding do not translate intact",
                                                sx, sy, sx + delta.x, sy + delta.y, stock.rgba[source], changed.rgba[target]);
                                    }
                            }
                            if (element.screen == 15 && element.orientation == 0) {
                                const auto loopBlock = std::find_if(std::begin(ThreeMaxNative::LayoutBlocks),
                                    std::end(ThreeMaxNative::LayoutBlocks), [&](const auto& block) {
                                        const auto& profile = ThreeMaxNative::Profiles[block.profile];
                                        if (profile.model != release.model || profile.edition != release.edition ||
                                            profile.code != code || !block.loopCount || !block.siteCount) return false;
                                        const uint32_t address = ThreeMaxNative::LayoutSites[block.firstSite].address;
                                        return std::any_of(element.sites.begin(), element.sites.end(),
                                            [&](const auto& site) { return site.address == address; });
                                    });
                                if (loopBlock != std::end(ThreeMaxNative::LayoutBlocks)) {
                                    for (size_t field = 0; field < loopBlock->loopCount; ++field) {
                                        const auto& loop = ThreeMaxNative::GlyphLoops[loopBlock->firstLoop + field];
                                        for (unsigned row = 0; row < loop.rows; ++row)
                                            for (unsigned column = 0; column < loop.columns; ++column)
                                                for (int y = 0; y < loop.glyphHeight; ++y)
                                                    for (int x = 0; x < loop.glyphWidth; ++x) {
                                                        const int sx = element.position.x + loop.x + int(column) * loop.columnStride + x;
                                                        const int sy = element.position.y + loop.y + int(row) * loop.rowStride + y;
                                                        const int tx = sx + delta.x, ty = sy + delta.y;
                                                        if (sx < 0 || sy < 0 || tx < 0 || ty < 0 ||
                                                            sx >= stock.width || sy >= stock.height ||
                                                            tx >= changed.width || ty >= changed.height)
                                                            return moveFail("native service glyph slot is outside the LCD");
                                                        if (stock.rgba[size_t(sy) * stock.width + sx] !=
                                                            changed.rgba[size_t(ty) * changed.width + tx])
                                                            return moveFail("native UID/level glyph pixels or row/column strides changed during translation");
                                                    }
                                    }
                                }
                            }
                        } else if (!state.Undo()) return moveFail("cannot undo inactive native state");
                    }
                    if (!visible) return moveFail("movable native block never affects LCD output");
                    std::vector<uint8_t> bytes;
                    FirmwarePackage rebuilt;
                    StudioState reopened;
                    if (!state.BuildToBuffer(bytes, error) || !FirmwarePackage::Parse(bytes, rebuilt, error)) return moveFail(error);
                    for (size_t r = 0; r < rebuilt.records.size(); ++r)
                        if ((uint16_t(rebuilt.records[r].device_code[0]) << 8 | rebuilt.records[r].device_code[1]) != code &&
                            rebuilt.records[r].image != state.firmware.records[r].image)
                            return moveFail("editing one hardware record changed another");
                    if (!reopened.LoadPackage(std::move(rebuilt), "reopened DRAG layout", error)) return moveFail(error);
                    const auto restored = std::find_if(reopened.layoutElements.begin(), reopened.layoutElements.end(),
                        [&](const auto& candidate) { return candidate.id == element.id; });
                    if (!reopened.layoutError.empty() || restored == reopened.layoutElements.end() ||
                        restored->position != Point{element.position.x + delta.x, element.position.y + delta.y})
                        return moveFail("native exported position did not reopen: " + reopened.layoutError);
                    Frame loaded;
                    if (!renderer.Render(reopened, selected, loaded) || loaded.rgba != changed.rgba)
                        return moveFail("reopened native LCD stream differs: " + loaded.error);
                    if (!state.Undo()) return moveFail("cannot undo native move");
                    ++recordMoves;
                    ++moves;
                }
            }
            if (!recordMoves) return fail("hardware record has no genuine movable native blocks");
            ++records;
            if (proof) *proof << "THREE/MAX HW " << std::hex << code << std::dec << " moves=" << recordMoves << "\n" << std::flush;
        }
        DeviceState invalid;
        invalid.hardwareCode = 0xffff;
        Frame rejected;
        if (renderer.Render(state, invalid, rejected) || !AvailableScreens(state, invalid.hardwareCode).empty())
            return fail("unknown hardware code silently falls back");
        auto corrupt = state.firmware;
        const auto corruptRecord = std::find_if(corrupt.records.begin(), corrupt.records.end(), [&](const auto& record) {
            return !selectedHardware || (uint16_t(record.device_code[0]) << 8 | record.device_code[1]) == selectedHardware;
        });
        if (corruptRecord == corrupt.records.end()) return fail("selected hardware record is absent from its package");
        corruptRecord->image[(corruptRecord->reset & ~1u) - corruptRecord->linked_base] ^= 1;
        std::vector<FirmwareLayout::Element> unused;
        if (FirmwareLayout::DiscoverThreeMax(corrupt, unused, error)) return fail("unsupported executable mutation was accepted");
        error.clear();
        const auto mixedElement = std::find_if(state.layoutElements.begin(), state.layoutElements.end(), [&](const auto& element) {
            return !selectedHardware || element.hardwareCode == selectedHardware;
        });
        if (mixedElement == state.layoutElements.end()) return fail("selected hardware has no native layout for mixed edits");
        const auto& element = *mixedElement;
        Point delta;
        if (element.maximumOffset.y > 0) delta.y = element.stepY;
        else if (element.minimumOffset.y < 0) delta.y = -element.stepY;
        else if (element.maximumOffset.x > 0) delta.x = element.stepX;
        else delta.x = -element.stepX;
        state.PushUndo("Mixed DRAG resource, palette and native layout");
        if (!state.SetLayoutOffset(static_cast<size_t>(mixedElement - state.layoutElements.begin()), delta, error)) return fail(error);
        if (!state.palette.empty()) state.SetPaletteColor(0, Rgb8::FromRgb565(state.palette[0].current.ToRgb565() ^ 0x07e0));
        if (!state.originals.empty()) {
            auto bitmap = state.originals.front();
            bitmap.front() ^= 0x80;
            state.SetBitmapReplacement(0, std::move(bitmap));
        }
        state.EndUndoAction();
        std::vector<uint8_t> mixedBytes;
        FirmwarePackage mixed;
        StudioState mixedState;
        if (!state.BuildToBuffer(mixedBytes, error) || !FirmwarePackage::Parse(mixedBytes, mixed, error) ||
            !mixedState.LoadPackage(std::move(mixed), "mixed native DRAG edits", error)) return fail(error);
        if (!mixedState.layoutError.empty() || mixedState.layoutElements.empty()) return fail(mixedState.layoutError);
        DeviceState device;
        device.hardwareCode = element.hardwareCode;
        device.screen = element.screen;
        device.temperatureControl = element.temperatureMode == 1;
        device.landscape = element.orientation == 1;
        Frame live, reloaded;
        if (!renderer.Render(state, device, live) || !renderer.Render(mixedState, device, reloaded) || live.rgba != reloaded.rgba)
            return fail("mixed native edits changed after export/reopen: " + live.error + reloaded.error);
    }
    if (records != expectedRecords) {
        error = selectedHardware ? "Selected DRAG3 / MAX hardware edition coverage is incomplete" :
            "DRAG3 / MAX bundled inventory coverage is not eleven records";
        return false;
    }
    if (sharedEdits != expectedSharedEdits) { error = "DRAG3 Factory/TC group and all-record semantic coverage is incomplete"; return false; }
    if (proof) *proof << (selectedHardware ? "THREE/MAX FILTERED COVERAGE OK records=" : "THREE/MAX COVERAGE OK records=")
        << records << " views=" << views << " moves=" << moves
        << " sharedRevisionEdits=" << sharedEdits << "\n" << std::flush;
    return true;
}
