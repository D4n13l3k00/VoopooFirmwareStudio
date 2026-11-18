#pragma once
#include "layout_edit_selection.h"
#include "firmware_emulator.h"
#include "ui_firmware_layout.h"
#include <array>
#include <set>

inline bool RunLayoutSelectionRegressions(std::string& error) {
    using FirmwareLayout::Point;
    using LayoutEdit::Constraints;
    error.clear();
    auto fail = [&](const std::string& reason) { error = "Layout selection: " + reason; return false; };
    auto offset = [](const StudioState& studio, size_t index) {
        return LayoutEdit::Detail::Offset(studio.layoutOffsets, studio.layoutElements[index]);
    };
    auto move = [&](StudioState& studio, const Constraints& constraints, Point desired) {
        studio.PushUndo("Move selected layout scope");
        const bool result = LayoutEdit::Apply(studio, constraints, desired, error);
        studio.EndUndoAction();
        return result;
    };
    // Different boxes, native grids and current offsets are compatible by stable
    // semantic identity, not title, geometry or absolute position equality.
    StudioState synthetic;
    const std::array<uint16_t, 3> syntheticCodes{3, 2, 1};
    for (int i = 0; i < 3; ++i) {
        FirmwareLayout::Element element;
        element.id = "hw000" + std::to_string(i + 1) + ".battery";
        element.title = "Battery " + std::to_string(i);
        element.hardwareCode = uint16_t(i + 1);
        element.screenWidth = 80; element.screenHeight = 160;
        element.position = {50 + i, 0};
        element.width = 24 + i; element.height = 9 + i;
        element.minimumOffset = {-48, -24}; element.maximumOffset = {48, 48};
        element.stepX = std::array{2, 3, 8}[i];
        element.stepY = std::array{1, 8, 4}[i];
        synthetic.layoutElements.push_back(std::move(element));
    }
    synthetic.layoutOffsets = {{"hw0001.battery", {2, 0}}, {"hw0002.battery", {3, 8}},
        {"hw0003.battery", {8, 16}}};
    Constraints common;
    if (!LayoutEdit::Resolve(synthetic.layoutElements, synthetic.layoutOffsets, 0, syntheticCodes, common) ||
        common.targets != std::vector<size_t>{0, 2, 1} || common.x.nativeStep != 24 || common.x.phase != 2 ||
        common.x.minimum != -46 || common.x.maximum != 26 || common.y.nativeStep != 8 ||
        common.y.minimum != -24 || common.y.maximum != 32)
        return fail("mixed-grid semantic cohort did not resolve its selected-relative delta intersection");
    const auto syntheticBefore = synthetic.layoutOffsets;
    if (!move(synthetic, common, {1000, 1000}) || !error.empty() ||
        offset(synthetic, 0) != Point{26, 32} || offset(synthetic, 1) != Point{27, 40} ||
        offset(synthetic, 2) != Point{32, 48} || synthetic.undoStack.size() != 1 ||
        !synthetic.Undo() || synthetic.layoutOffsets != syntheticBefore)
        return fail("common phase snap/clamp did not preserve offsets or one-action Undo");
    // A stale or otherwise invalid cohort must fail before touching any member.
    if (!LayoutEdit::Resolve(synthetic.layoutElements, synthetic.layoutOffsets, 0, syntheticCodes, common))
        return fail("synthetic cohort failed to resolve after Undo");
    synthetic.layoutOffsets["hw0002.battery"].x = 48;
    const auto staleBefore = synthetic.layoutOffsets;
    const auto staleRevision = synthetic.ResourceRevision();
    const auto staleHistory = synthetic.undoStack.size();
    if (LayoutEdit::Apply(synthetic, common, {26, 0}, error) || error.empty() ||
        synthetic.layoutOffsets != staleBefore || synthetic.ResourceRevision() != staleRevision ||
        synthetic.undoStack.size() != staleHistory)
        return fail("invalid full-target prevalidation partially mutated the document");
    error.clear();
    synthetic.layoutOffsets = syntheticBefore;
    const auto originalId = synthetic.layoutElements[1].id;
    synthetic.layoutElements[1].id = "hw0002.other";
    if (LayoutEdit::Resolve(synthetic.layoutElements, synthetic.layoutOffsets, 0, syntheticCodes, common))
        return fail("same-looking but semantically missing element was accepted");
    synthetic.layoutElements[1].id = originalId;
    synthetic.layoutElements[1].screenWidth = 81;
    if (!LayoutEdit::Resolve(synthetic.layoutElements, synthetic.layoutOffsets, 0, syntheticCodes, common))
        return fail("different native display dimensions unnecessarily blocked a valid common delta");
    synthetic.layoutElements[1].screenWidth = 80;
    synthetic.layoutElements.push_back(synthetic.layoutElements[1]);
    if (LayoutEdit::Resolve(synthetic.layoutElements, synthetic.layoutOffsets, 0, syntheticCodes, common))
        return fail("duplicate semantic element was accepted");
    synthetic.layoutElements.pop_back();
    const std::array<uint16_t, 2> duplicateCodes{1, 1};
    const std::array<uint16_t, 1> missingSelected{2};
    if (LayoutEdit::Resolve(synthetic.layoutElements, synthetic.layoutOffsets, 0, duplicateCodes, common) ||
        LayoutEdit::Resolve(synthetic.layoutElements, synthetic.layoutOffsets, 0, missingSelected, common))
        return fail("duplicate scope codes or a missing selected hardware record were accepted");
    FirmwareEmulator::DeviceState tcView;
    tcView.screen = 30;
    tcView.temperatureControl = true;
    synthetic.layoutElements[0].sharedScreens = {30};
    synthetic.layoutElements[1].screen = 30;
    synthetic.layoutElements[1].sharedScreens = {0};
    synthetic.layoutElements[1].temperatureMode = 1;
    synthetic.layoutElements[2].sharedScreens = {30};
    if (!LayoutEdit::Resolve(synthetic.layoutElements, synthetic.layoutOffsets, 0, syntheticCodes, common, &tcView) ||
        common.targets.size() != 3)
        return fail("shared native blocks failed to match their actual active screen");
    tcView.temperatureControl = false;
    if (LayoutEdit::Resolve(synthetic.layoutElements, synthetic.layoutOffsets, 0, syntheticCodes, common, &tcView))
        return fail("bulk move crossed into a different native temperature view");
    tcView.temperatureControl = true;
    synthetic.layoutElements[2].orientation = 1;
    if (LayoutEdit::Resolve(synthetic.layoutElements, synthetic.layoutOffsets, 0, syntheticCodes, common, &tcView))
        return fail("bulk move crossed into a different native orientation");
    for (auto& element : synthetic.layoutElements) {
        element.screen = 0;
        element.sharedScreens.clear();
        element.temperatureMode = element.orientation = -1;
    }
    synthetic.layoutElements[1].id = "hw0002.native_call_08001234";
    for (auto& element : synthetic.layoutElements) element.semanticId = "battery";
    if (!LayoutEdit::Resolve(synthetic.layoutElements, synthetic.layoutOffsets, 0, syntheticCodes, common))
        return fail("relocated native calls did not use their proved shared semantic identity");
    synthetic.layoutElements[2].semanticId = "unrelated";
    if (LayoutEdit::Resolve(synthetic.layoutElements, synthetic.layoutOffsets, 0, syntheticCodes, common))
        return fail("record-local ID overrode an incompatible proved semantic identity");
    synthetic.layoutElements[1].id = originalId;
    for (auto& element : synthetic.layoutElements) element.semanticId.clear();
    synthetic.layoutOffsets.clear();
    synthetic.layoutElements[0].minimumOffset = {-3, -3};
    synthetic.layoutElements[0].maximumOffset = {3, 3};
    synthetic.layoutElements[0].stepX = synthetic.layoutElements[0].stepY = 8;
    const std::array<uint16_t, 1> fixedCode{1};
    if (!LayoutEdit::Resolve(synthetic.layoutElements, synthetic.layoutOffsets, 0, fixedCode, common) ||
        common.x.minimum != 0 || common.x.maximum != 0 || common.y.minimum != 0 || common.y.maximum != 0)
        return fail("a single legal common coordinate was not exposed as a fixed UI axis");

    FirmwarePackage package;
    StudioState studio;
    if (!LoadEmbeddedFirmware(package, error, FirmwareModel::DragS, FirmwareEdition::Score) ||
        !studio.LoadPackage(std::move(package), "S Score selection regression", error)) return false;
    if (!studio.layoutError.empty()) return fail(studio.layoutError);
    std::vector<uint16_t> codes;
    for (const auto& record : studio.firmware.records)
        codes.push_back(uint16_t(record.device_code[0]) << 8 | record.device_code[1]);
    if (codes.size() != 7) return fail("S Score fixture does not contain all seven hardware records");
    std::vector<size_t> batteries, ranks;
    std::set<std::pair<int, int>> batterySizes;
    for (size_t i = 0; i < studio.layoutElements.size(); ++i) {
        const auto& element = studio.layoutElements[i];
        if (LayoutEdit::Detail::SemanticId(element) == "battery") {
            batteries.push_back(i); batterySizes.emplace(element.width, element.height);
        }
        if (LayoutEdit::Detail::SemanticId(element) == "rank") ranks.push_back(i);
    }
    if (batteries.size() != codes.size() || ranks.size() != codes.size() ||
        !batterySizes.contains({27, 10}) || !batterySizes.contains({24, 9}) || !batterySizes.contains({25, 9}))
        return fail("S Score battery size variants or rank records are missing");
    const size_t selected = batteries.back();
    if (!LayoutEdit::Resolve(studio.layoutElements, studio.layoutOffsets, selected, codes, common) ||
        common.targets.front() != selected || common.targets.size() != 7 || common.x.minimum >= 0)
        return fail("all seven real S Score battery variants are not jointly editable");
    FirmwareEmulator::Renderer renderer;
    std::vector<FirmwareEmulator::Frame> stock(codes.size());
    for (size_t i = 0; i < codes.size(); ++i) {
        FirmwareEmulator::DeviceState device;
        device.hardwareCode = codes[i]; device.score = 21400;
        if (!renderer.Render(studio, device, stock[i])) return fail(stock[i].error);
    }
    // All -> individual -> All: the second common movement must preserve the
    // individual record's newly divergent offset instead of replacing it.
    if (!move(studio, common, {-1, 0}) || !error.empty()) return fail("all-battery movement: " + error);
    const std::array<uint16_t, 1> individual{studio.layoutElements[selected].hardwareCode};
    Constraints one;
    if (!LayoutEdit::Resolve(studio.layoutElements, studio.layoutOffsets, selected, individual, one) ||
        !move(studio, one, {-2, 0})) return fail("individual battery movement: " + error);
    for (size_t i : batteries)
        if (offset(studio, i) != (i == selected ? Point{-2, 0} : Point{-1, 0}))
            return fail("individual selection modified another hardware record");
    if (!LayoutEdit::Resolve(studio.layoutElements, studio.layoutOffsets, selected, codes, common) ||
        !move(studio, common, {-3, 0})) return fail("divergent all-battery movement: " + error);
    for (size_t i : batteries)
        if (offset(studio, i) != (i == selected ? Point{-3, 0} : Point{-2, 0}))
            return fail("all movement flattened individual hardware offsets");
    if (!LayoutEdit::Resolve(studio.layoutElements, studio.layoutOffsets, selected, individual, one) ||
        !move(studio, one, {-4, 0})) return fail("second individual battery movement: " + error);
    for (size_t i : batteries)
        if (offset(studio, i) != (i == selected ? Point{-4, 0} : Point{-2, 0}))
            return fail("individual selection after the second All movement escaped its scope");
    // Native execution is checked only for the affected seven records, not a
    // full grouping movement matrix. Pixels outside old/new boxes stay intact.
    for (size_t i = 0; i < codes.size(); ++i) {
        FirmwareEmulator::DeviceState device;
        device.hardwareCode = codes[i]; device.score = 21400;
        FirmwareEmulator::Frame moved;
        if (!renderer.Render(studio, device, moved) || moved.rgba == stock[i].rgba)
            return fail("battery common movement has no native effect: " + moved.error);
        const auto match = std::find_if(batteries.begin(), batteries.end(), [&](size_t index) {
            return studio.layoutElements[index].hardwareCode == codes[i];
        });
        if (match == batteries.end()) return fail("native battery record was lost");
        const auto& element = studio.layoutElements[*match];
        const auto delta = offset(studio, *match);
        if (moved.width != stock[i].width || moved.height != stock[i].height)
            return fail("common movement changed native frame dimensions");
        auto inside = [&](int x, int y, Point shift) {
            return x >= element.position.x + shift.x && x < element.position.x + shift.x + element.width &&
                y >= element.position.y + shift.y && y < element.position.y + shift.y + element.height;
        };
        for (int y = 0; y < moved.height; ++y)
            for (int x = 0; x < moved.width; ++x)
                if (!inside(x, y, {}) && !inside(x, y, delta) &&
                    stock[i].rgba[size_t(y) * moved.width + x] != moved.rgba[size_t(y) * moved.width + x])
                    return fail("battery common movement changed pixels outside native boxes");
    }
    // Exercise the public UI routing seam with a known identical-UI pair. A
    // discovered matching group must never turn an explicit revision into a group.
    UiFirmwareLayout::State ui;
    using Scope = UiFirmwareLayout::SelectionScope;
    const std::array<uint16_t, 2> groupCodes{0x3014, 0x3013};
    const auto groupBattery = std::find_if(batteries.begin(), batteries.end(), [&](size_t index) {
        return studio.layoutElements[index].hardwareCode == groupCodes.front();
    });
    if (groupBattery == batteries.end()) return fail("known identical-UI pair is missing");
    const size_t groupSelected = *groupBattery;
    auto select = [&](Scope scope, std::span<const uint16_t> selectedCodes = {}) {
        UiFirmwareLayout::SelectScope(ui, studio, scope, selectedCodes);
        ui.selected = groupSelected;
    };
    auto uiMove = [&](Point desired) {
        studio.PushUndo("UI selected scope movement");
        const bool result = UiFirmwareLayout::ApplySelectionOffset(ui, studio, desired);
        studio.EndUndoAction();
        if (!result) error = ui.editError;
        return result;
    };
    auto expectDelta = [&](const FirmwareLayout::Offsets& before, int delta, bool all) {
        for (size_t index : batteries) {
            const auto old = LayoutEdit::Detail::Offset(before, studio.layoutElements[index]);
            const auto actual = offset(studio, index);
            if (actual != Point{old.x + ((all || index == groupSelected) ? delta : 0), old.y}) return false;
        }
        return true;
    };
    select(Scope::All);
    UiFirmwareLayout::RevisionGroup knownGroup;
    knownGroup.hardwareCodes.assign(groupCodes.begin(), groupCodes.end());
    knownGroup.elements.resize(1);
    for (size_t index : batteries)
        if (studio.layoutElements[index].hardwareCode == groupCodes[0] ||
            studio.layoutElements[index].hardwareCode == groupCodes[1])
            knownGroup.elements.front().push_back(index);
    knownGroup.title = "3014 / 3013";
    ui.revisionGroups.push_back(std::move(knownGroup));
    ui.revisionGroupsReady = true;
    auto beforeUi = studio.layoutOffsets;
    if (!uiMove({offset(studio, groupSelected).x - 1, 0}) || !expectDelta(beforeUi, -1, true))
        return fail("UI All movement did not edit the captured seven-record scope: " + error);
    const std::array<uint16_t, 1> groupIndividual{groupCodes.front()};
    select(Scope::Revision, groupIndividual);
    beforeUi = studio.layoutOffsets;
    if (!uiMove({offset(studio, groupSelected).x - 1, 0}) || !expectDelta(beforeUi, -1, false) ||
        ui.selectionScope != Scope::Revision || ui.device.hardwareCode != groupCodes.front())
        return fail("explicit UI revision was silently promoted into its discovered group");
    select(Scope::All);
    beforeUi = studio.layoutOffsets;
    if (!uiMove({offset(studio, groupSelected).x - 1, 0}) || !expectDelta(beforeUi, -1, true))
        return fail("UI All movement flattened an individual revision's divergent position");
    select(Scope::Revision, groupIndividual);
    beforeUi = studio.layoutOffsets;
    if (!uiMove({offset(studio, groupSelected).x - 1, 0}) || !expectDelta(beforeUi, -1, false))
        return fail("UI individual scope after the second All edit escaped its record");
    select(Scope::Group, groupCodes);
    const auto beforeGroup = studio.layoutOffsets;
    if (!uiMove({offset(studio, groupSelected).x - 1, 0})) return fail("UI group movement: " + error);
    for (size_t index : batteries) {
        const auto old = LayoutEdit::Detail::Offset(beforeGroup, studio.layoutElements[index]);
        const uint16_t code = studio.layoutElements[index].hardwareCode;
        const bool member = code == groupCodes.front() || code == groupCodes.back();
        if (offset(studio, index) != Point{old.x - (member ? 1 : 0), old.y})
            return fail("UI group movement flattened divergent positions or edited an unrelated revision");
    }
    const auto afterGroup = studio.layoutOffsets;
    const auto groupHistory = studio.undoStack.size();
    select(Scope::Revision, groupIndividual);
    if (!uiMove({offset(studio, groupSelected).x - 1, 0}) || !expectDelta(afterGroup, -1, false) ||
        studio.undoStack.size() != groupHistory + 1 || !studio.Undo() || studio.layoutOffsets != afterGroup)
        return fail("individual edit after UI group was not isolated in one Undo action");
    // The originally reported selected label was Score. Full-width native
    // boxes must still resolve, with fixed X and a usable common Y axis.
    const auto score = std::find_if(studio.layoutElements.begin(), studio.layoutElements.end(),
        [](const auto& element) { return LayoutEdit::Detail::SemanticId(element) == "score"; });
    if (score == studio.layoutElements.end()) return fail("real S Score element is missing");
    const size_t scoreIndex = static_cast<size_t>(score - studio.layoutElements.begin());
    if (!LayoutEdit::Resolve(studio.layoutElements, studio.layoutOffsets, scoreIndex, codes, common) ||
        common.targets.size() != 7 || common.x.minimum != 0 || common.x.maximum != 0 ||
        common.y.minimum >= common.y.maximum)
        return fail("all seven Score elements did not resolve fixed X and movable Y");
    const int scoreY = common.y.maximum > 0 ? 1 : -1;
    if (!move(studio, common, {10000, scoreY}) || !error.empty())
        return fail("all Score fixed-X/legal-Y movement: " + error);
    for (size_t i : common.targets)
        if (offset(studio, i) != Point{0, scoreY}) return fail("Score movement lost a hardware record");
    // Move one rank to its lower boundary, then use another selected record.
    // The common intersection must prevent any further negative movement.
    const size_t boundaryRank = ranks.front(), selectedRank = ranks.back();
    if (!LayoutEdit::Resolve(studio.layoutElements, studio.layoutOffsets, selectedRank, codes, common) ||
        !move(studio, common, {0, -1})) return fail("all-rank setup: " + error);
    const std::array<uint16_t, 1> rankIndividual{studio.layoutElements[boundaryRank].hardwareCode};
    if (!LayoutEdit::Resolve(studio.layoutElements, studio.layoutOffsets, boundaryRank, rankIndividual, one) ||
        !move(studio, one, {0, one.y.minimum})) return fail("rank boundary setup: " + error);
    if (!LayoutEdit::Resolve(studio.layoutElements, studio.layoutOffsets, selectedRank, codes, common) ||
        common.y.minimum != -1) return fail("rank common bounds ignored an already divergent boundary record");
    const auto beforeBoundary = studio.layoutOffsets;
    const auto boundaryHistory = studio.undoStack.size();
    if (!move(studio, common, {0, -10000}) || !error.empty() || studio.layoutOffsets != beforeBoundary ||
        studio.undoStack.size() != boundaryHistory)
        return fail("ordinary common rank boundary movement produced an error or mutation");
    if (!move(studio, common, {0, 10000}) || !error.empty()) return fail("rank upper clamp: " + error);
    const auto atUpper = studio.layoutOffsets;
    if (!LayoutEdit::Resolve(studio.layoutElements, studio.layoutOffsets, selectedRank, codes, common) ||
        !move(studio, common, {0, 10000}) || !error.empty() || studio.layoutOffsets != atUpper)
        return fail("rank common upper boundary was not a harmless clamp");
    // Export actual guarded firmware bytes and reopen: every record's native
    // positions and framebuffer must match, including divergent offsets.
    std::vector<uint8_t> bytes;
    BuildReport report;
    if (!studio.BuildToBuffer(bytes, error, &report)) return fail("export: " + error);
    if (report.changed_layout_elements != studio.layoutOffsets.size() || bytes.size() != studio.firmware.source_bytes.size())
        return fail("export lost selected layout elements or changed image length");
    FirmwarePackage rebuilt;
    StudioState reopened;
    if (!FirmwarePackage::Parse(bytes, rebuilt, error) ||
        !reopened.LoadPackage(std::move(rebuilt), "reopened selection regression", error)) return fail("reopen: " + error);
    if (!reopened.layoutError.empty() || reopened.HasUnsavedChanges()) return fail("reopened layout is unavailable or dirty");
    for (size_t i = 0; i < studio.layoutElements.size(); ++i) {
        const auto& original = studio.layoutElements[i];
        const auto found = std::find_if(reopened.layoutElements.begin(), reopened.layoutElements.end(),
            [&](const auto& candidate) { return candidate.id == original.id; });
        const auto delta = offset(studio, i);
        if (found == reopened.layoutElements.end() ||
            found->position != Point{original.position.x + delta.x, original.position.y + delta.y})
            return fail(original.id + " position did not survive guarded export/reopen");
    }
    for (uint16_t code : codes) {
        FirmwareEmulator::DeviceState device;
        device.hardwareCode = code; device.score = 21400;
        FirmwareEmulator::Frame editedFrame, reopenedFrame;
        if (!renderer.Render(studio, device, editedFrame) || !renderer.Render(reopened, device, reopenedFrame) ||
            editedFrame.width != reopenedFrame.width || editedFrame.height != reopenedFrame.height ||
            editedFrame.hardwareCode != code || reopenedFrame.hardwareCode != code || editedFrame.rgba != reopenedFrame.rgba)
            return fail("native rank/battery frame differs after export/reopen: " + editedFrame.error + reopenedFrame.error);
    }
    return true;
}
