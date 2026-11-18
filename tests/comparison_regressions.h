#pragma once
#include "app_state.h"
#include "firmware_comparison.h"

inline bool RunComparisonRegressions(std::string& error) {
    using namespace FirmwareComparison;
    error.clear();
    auto check = [&](bool ok, const char* message) {
        if (!ok) { error += message; error += "; "; }
    };
    StudioState editor;
    FirmwarePackage package;
    if (!LoadEmbeddedFirmware(package, error, FirmwareModel::DragS, FirmwareEdition::Score) ||
        !editor.LoadPackage(std::move(package), "comparison baseline", error)) return false;
    Snapshot baseline, current;
    baseline.Update(editor);
    current.Update(editor);
    auto equal = [](const std::vector<Entry>& entries) {
        return !entries.empty() && std::all_of(entries.begin(), entries.end(),
            [](const Entry& row) { return row.status == Status::Equal; });
    };
    check(equal(Compare(baseline, current)), "same firmware differs");
    const size_t index = static_cast<size_t>(editor.FindAssetIndex("font_6x10_07"));
    if (index >= editor.catalog.size()) { error = "comparison fixture missing digit"; return false; }
    const auto& spec = editor.catalog[index];
    std::vector<uint8_t> gray, replacement;
    FirmwareBitmap::DecodeMask(editor.GetCurrentRaw(index), spec.byte_len(), spec.width, spec.height, gray, spec.format);
    gray[0] = gray[0] ? 0 : 255;
    if (!FirmwareBitmap::EncodeMaskPreserving(gray.data(), spec.width, spec.height,
        editor.GetCurrentRaw(index), spec.byte_len(), replacement, spec.format)) {
        error = "comparison fixture encoding failed"; return false;
    }
    editor.PushUndo("Comparison pixel regression");
    editor.SetBitmapReplacement(index, std::move(replacement));
    editor.EndUndoAction();
    current.Update(editor);
    auto entries = Compare(baseline, current);
    const auto changed = std::find_if(entries.begin(), entries.end(),
        [index](const Entry& row) { return row.left == index; });
    check(changed != entries.end() && changed->status == Status::Changed && changed->differentPixels == 1,
        "unsaved one-pixel edit was not isolated");
    check(std::count_if(entries.begin(), entries.end(), [](const Entry& row) {
        return row.status == Status::Changed;
    }) == 1, "one pixel changed unrelated resources");
    std::vector<uint32_t> diff;
    size_t differing = 0;
    check(BuildDifference(baseline.resources[index], current.resources[index], 0, false, diff, differing) &&
        differing == 1 && diff[0] == 0xffff00ffu, "pixel difference highlight incorrect");
    check(editor.replacements.size() == 1 && editor.CanUndo(), "comparison mutated editor edits or history");
    editor.Undo();
    current.Update(editor);
    check(equal(Compare(baseline, current)), "comparison retained stale edit after Undo");
    const auto accent = std::find_if(editor.palette.begin(), editor.palette.end(),
        [](const PaletteState& color) { return std::string_view(color.spec.id) == "accent_cyan"; });
    if (accent == editor.palette.end()) { error = "comparison fixture missing cyan accent"; return false; }
    editor.PushUndo("Comparison palette regression");
    editor.SetPaletteColor(static_cast<size_t>(accent - editor.palette.begin()), {255, 0, 0});
    editor.EndUndoAction();
    current.Update(editor);
    entries = Compare(baseline, current);
    const size_t puff = static_cast<size_t>(editor.FindAssetIndex("font_11x20_00"));
    check(std::any_of(entries.begin(), entries.end(), [puff](const Entry& row) {
        return row.left == puff && row.status == Status::Changed && row.differentPixels > 0;
    }), "cyan-only palette change was missed");
    editor.Undo();
    current.Update(editor);
    check(equal(Compare(baseline, current)), "palette Undo retained stale colors");

    Snapshot left, right;
    left.loaded = right.loaded = true;
    left.resources.push_back(baseline.resources[index]);
    right.resources.push_back(baseline.resources[index]);
    right.resources[0].spec.address += 0x100;
    check(equal(Compare(left, right)), "address relocation changed matching");
    right.resources[0].spec.width++;
    entries = Compare(left, right);
    check(entries.size() == 1 && entries[0].status == Status::Incompatible,
        "different dimensions treated as equal pixels");
    right.resources[0].spec.width--;
    right.model = FirmwareModel::Drag2;
    entries = Compare(left, right);
    check(entries.size() == 2 && std::any_of(entries.begin(), entries.end(), [](const Entry& row) {
        return row.status == Status::Removed;
    }) && std::any_of(entries.begin(), entries.end(), [](const Entry& row) {
        return row.status == Status::Added;
    }), "unrelated model resource IDs incorrectly paired");
    right.model = left.model;
    entries = Compare(left, right);
    const auto language = i18n::GetCurrentLanguage();
    i18n::SetLanguage(i18n::Language::German);
    check(MatchesQuery(entries[0], left, right, L"ЦИФРА 7") &&
        MatchesQuery(entries[0], left, right, L"DIGIT 7") &&
        MatchesQuery(entries[0], left, right, L"FONT_6X10_07"),
        "comparison search depends on interface language");
    i18n::SetLanguage(language);
    right.resources.clear();
    entries = Compare(left, right);
    check(entries.size() == 1 && entries[0].status == Status::Removed && entries[0].right == MissingIndex,
        "missing right resource not reported");
    entries = Compare(right, left);
    check(entries.size() == 1 && entries[0].status == Status::Added && entries[0].left == MissingIndex,
        "missing left resource not reported");
    std::string loadError;
    const size_t originalCount = baseline.resources.size();
    check(!LoadFile("", baseline, loadError) && !loadError.empty() &&
        baseline.loaded && baseline.path == "comparison baseline" &&
        baseline.resources.size() == originalCount &&
        baseline.resources[index].raw == editor.originals[index],
        "failed file load discarded the previous comparison source");
    return error.empty();
}
