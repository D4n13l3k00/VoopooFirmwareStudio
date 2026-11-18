#pragma once

#include "app_state.h"
#include "atomic_file.h"
#include <functional>
#include <cstdio>

namespace FirmwareSave {

enum class Outcome { Failed, FirmwareOnly, Complete };
enum class Phase { Write, Report };
enum class Approval { Confirmed };
struct Result {
    Outcome outcome = Outcome::Failed;
    AtomicFile::Error fileError;
    std::string buildError;
    BuildReport report;
    bool cancelled = false;
    bool FirmwareSaved() const { return outcome != Outcome::Failed; }
};

struct Revision {
    uint16_t hardwareCode = 0;
    size_t usedBytes = 0, capacityBytes = 0;
    bool capacityKnown = false;
    std::vector<std::string> issues;
};
enum class ChangeKind { Screen, Menu, Layout, Resource, Palette };
struct Change {
    ChangeKind kind;
    std::string id;
    size_t target = 0;
};

// Only Prepare can publish exportable bytes. Consumers cannot mutate either the
// snapshot or the approved output, and Write never invokes a builder.
class Prepared {
public:
    const StudioState& Snapshot() const { return *snapshot; }
    const std::vector<uint8_t>& Bytes() const { return bytes; }
    const BuildReport& Report() const { return report; }
    const std::vector<Revision>& Revisions() const { return revisions; }
    const std::vector<Change>& Changes() const { return changes; }
    const std::vector<std::string>& Issues() const { return issues; }
    bool Valid() const { return !bytes.empty() && issues.empty() &&
        std::all_of(revisions.begin(), revisions.end(), [](const Revision& r) { return r.issues.empty(); }); }
    bool Matches(const StudioState& live) const {
        return snapshot && live.hasFirmware &&
            live.DocumentIdentity() == snapshot->DocumentIdentity() &&
            live.ResourceRevision() == snapshot->ResourceRevision() &&
            live.assetPaintColors == snapshot->assetPaintColors &&
            live.firmwarePath == snapshot->firmwarePath && live.projectPath == snapshot->projectPath &&
            live.isBuiltin == snapshot->isBuiltin && live.firmwareSourceEmbedded == snapshot->firmwareSourceEmbedded;
    }
private:
    std::shared_ptr<const StudioState> snapshot;
    std::vector<uint8_t> bytes;
    BuildReport report;
    std::vector<Revision> revisions;
    std::vector<Change> changes;
    std::vector<std::string> issues;
    friend std::shared_ptr<const Prepared> Prepare(std::shared_ptr<const StudioState>,
        const std::function<void(const char*)>&, const std::function<bool()>&);
};

inline std::shared_ptr<const Prepared> Prepare(std::shared_ptr<const StudioState> snapshot,
        const std::function<void(const char*)>& observer = {},
        const std::function<bool()>& cancelled = {}) {
    auto prepared = std::make_shared<Prepared>();
    prepared->snapshot = std::move(snapshot);
    if (!prepared->snapshot || !prepared->snapshot->hasFirmware) {
        prepared->issues.emplace_back("No firmware loaded.");
        return prepared;
    }
    const auto& studio = *prepared->snapshot;
    for (size_t i = 0; i < studio.screenPatches.size(); ++i)
        if (studio.screenPatches[i].enabled || studio.screenPatches[i] != studio.originalScreenPatches[i])
            prepared->changes.push_back({ChangeKind::Screen, {}, i});
    if (studio.menuPatch.enabled || studio.menuPatch != studio.originalMenuPatch)
        prepared->changes.push_back({ChangeKind::Menu, {}});
    for (const auto& [id, offset] : studio.layoutOffsets)
        if (offset != FirmwareLayout::Point{}) prepared->changes.push_back({ChangeKind::Layout, id});
    for (size_t i = 0; i < studio.catalog.size(); ++i) {
        const auto& id = studio.catalog[i].id;
        if (studio.replacements.contains(id) ||
            (i < studio.referenceBitmaps.size() && i < studio.originals.size() &&
                !studio.referenceBitmaps[i].empty() && studio.originals[i] != studio.referenceBitmaps[i]))
            prepared->changes.push_back({ChangeKind::Resource, id});
    }
    for (const auto& color : studio.palette) {
        const auto stock = studio.referencePalette.find(color.spec.id);
        if (color.current != (stock == studio.referencePalette.end() ? color.spec.original : stock->second))
            prepared->changes.push_back({ChangeKind::Palette, color.spec.id});
    }
    if (studio.backgroundColor != studio.referenceBackground)
        prepared->changes.push_back({ChangeKind::Palette, "background"});

    std::string error;
    if (!studio.BuildToBuffer(prepared->bytes, error, &prepared->report, observer)) {
        prepared->bytes.clear();
        prepared->issues.push_back(error.empty() ? "Firmware build failed" : error);
    }
    if (cancelled && cancelled()) { prepared->bytes.clear(); return prepared; }
    if (observer) observer("Measure exact Flash usage for every hardware revision");
    std::vector<FirmwareLogo::FlashUsage> usage;
    const bool measured = FirmwareLogo::MeasureFlash(studio.firmware, studio.screenPatches, usage, error);
    FirmwarePackage baseline;
    // A damaged record must not hide accounting for other recognized revisions.
    // The fallback still calls the same strict accounting on a complete inventory.
    if (!measured && studio.model == FirmwareModel::DragS)
        LoadEmbeddedFirmware(baseline, error, studio.model, studio.edition);
    prepared->revisions.reserve(studio.firmware.records.size());
    for (size_t r = 0; r < studio.firmware.records.size(); ++r) {
        if (cancelled && cancelled()) { prepared->bytes.clear(); return prepared; }
        const auto& record = studio.firmware.records[r];
        Revision revision;
        revision.hardwareCode = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
        const FirmwareLogo::FlashUsage* measuredUsage = measured && r < usage.size() ? &usage[r] : nullptr;
        std::vector<FirmwareLogo::FlashUsage> recovered;
        if (!measured && !baseline.records.empty()) {
            const auto found = std::find_if(baseline.records.begin(), baseline.records.end(), [&](const HardwareRecord& item) {
                return item.device_code[0] == record.device_code[0] && item.device_code[1] == record.device_code[1];
            });
            if (found != baseline.records.end()) {
                const size_t index = size_t(found - baseline.records.begin());
                auto candidate = baseline;
                candidate.records[index] = record;
                if (FirmwareLogo::MeasureFlash(candidate, studio.screenPatches, recovered, error) && index < recovered.size())
                    measuredUsage = &recovered[index];
            }
        }
        revision.usedBytes = record.image.size();
        if (measuredUsage) {
            revision.capacityKnown = true;
            revision.capacityBytes = measuredUsage->capacityBytes;
            revision.usedBytes = measuredUsage->Used() +
                FirmwareMenu::FlashBytes(record, studio.menuPatch, measuredUsage->Used());
            if (revision.usedBytes > revision.capacityBytes)
                revision.issues.push_back("Flash capacity exceeded by " +
                    std::to_string(revision.usedBytes - revision.capacityBytes) + " bytes");
        }
        prepared->revisions.push_back(std::move(revision));
    }
    // Normal builds remain the sole authority for approved bytes. On failure,
    // run independent real patch builders for every revision as well, so an
    // earlier broken revision cannot conceal later layout/resource conflicts.
    if (!prepared->issues.empty()) {
        for (size_t r = 0; r < prepared->revisions.size(); ++r) {
            if (cancelled && cancelled()) return prepared;
            auto& revision = prepared->revisions[r];
            if (observer) {
                char stage[96];
                snprintf(stage, sizeof(stage), "Validate resource and layout patches for hardware %04X", unsigned(revision.hardwareCode));
                observer(stage);
            }
            FirmwareLayout::Offsets offsets;
            for (const auto& [id, delta] : studio.layoutOffsets) {
                const auto element = std::find_if(studio.layoutElements.begin(), studio.layoutElements.end(),
                    [&](const auto& item) { return item.id == id; });
                if (element != studio.layoutElements.end() && element->hardwareCode == revision.hardwareCode)
                    offsets.emplace(id, delta);
            }
            std::vector<CodePatch> patches;
            if (!FirmwareLayout::BuildPatches(studio.firmware, studio.layoutElements, offsets, patches, error))
                revision.issues.push_back(error);
            std::map<std::string, std::pair<std::vector<size_t>, std::vector<uint8_t>>> replacements;
            for (size_t i = 0; i < studio.catalog.size(); ++i) {
                const auto replacement = studio.replacements.find(studio.catalog[i].id);
                if (replacement == studio.replacements.end()) continue;
                if (i >= studio.locations.size() || studio.locations[i].size() != studio.firmware.records.size()) {
                    revision.issues.push_back(studio.catalog[i].id + ": resource location map must cover every hardware record");
                    continue;
                }
                if (studio.locations[i][r] == SIZE_MAX) continue;
                std::vector<size_t> locations(studio.firmware.records.size(), SIZE_MAX);
                locations[r] = studio.locations[i][r];
                replacements.emplace(replacement->first, std::make_pair(std::move(locations), replacement->second));
            }
            std::vector<uint8_t> diagnosticBytes;
            BuildReport diagnosticReport;
            if (!studio.firmware.PackWith(replacements, patches, diagnosticBytes, diagnosticReport, error))
                revision.issues.push_back(error);
        }
    }
    if (observer) observer(prepared->Valid() ? "All hardware revisions passed export validation; awaiting confirmation" :
        "Export validation failed; no files written");
    return prepared;
}

// The caller must obtain explicit approval for this exact prepared review.
// A stale document is rejected before even creating a temporary file.
inline Result Write(const Prepared& prepared, const StudioState& live, Approval,
                     const std::string& utf8Path,
                     const std::function<void(Phase)>& progress = {},
                     const std::function<bool()>& beforeCommit = {},
                     const std::function<void(const char*)>& observer = {}) {
    Result result;
    if (!prepared.Valid()) { result.buildError = "Firmware export review did not pass validation"; return result; }
    if (!prepared.Matches(live)) { result.buildError = "The document changed after firmware export review"; return result; }
    result.report = prepared.Report();
    const auto& studio = prepared.Snapshot();
    const auto& bytes = prepared.Bytes();
    if (beforeCommit && !beforeCommit()) { result.cancelled = true; return result; }
    if (progress) progress(Phase::Write);
    const auto destination = PathUtils::ToFsPath(utf8Path);
    const auto source = studio.isBuiltin || studio.firmwareSourceEmbedded
        ? std::filesystem::path{} : PathUtils::ToFsPath(studio.firmwarePath);
    const auto project = PathUtils::ToFsPath(studio.projectPath);
    if (!AtomicFile::Write(destination, bytes.data(), bytes.size(), true, result.fileError, source, project, observer))
        return result;
    result.outcome = Outcome::FirmwareOnly;
    auto reportPath = destination;
    reportPath.replace_extension(L".report.json");
    if (observer) observer("Serialize firmware build report");
    const std::string json = result.report.ToJson();
    if (progress) progress(Phase::Report);
    if (!AtomicFile::Write(reportPath, json.data(), json.size(), false, result.fileError,
                           source.empty() ? project : source, destination, observer))
        return result;
    result.outcome = Outcome::Complete;
    return result;
}

} // namespace FirmwareSave
