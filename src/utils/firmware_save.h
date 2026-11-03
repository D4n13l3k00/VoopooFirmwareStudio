#pragma once

#include "app_state.h"
#include "atomic_file.h"

namespace FirmwareSave {

enum class Outcome { Failed, FirmwareOnly, Complete };
struct Result {
    Outcome outcome = Outcome::Failed;
    AtomicFile::Error fileError;
    std::string buildError;
    bool FirmwareSaved() const { return outcome != Outcome::Failed; }
};

inline Result Save(StudioState& studio, const std::string& utf8Path) {
    Result result;
    std::vector<uint8_t> bytes;
    BuildReport report;
    if (!studio.BuildToBuffer(bytes, result.buildError, &report)) return result;
    const auto destination = PathUtils::ToFsPath(utf8Path);
    const auto source = studio.isBuiltin ? std::filesystem::path{} : PathUtils::ToFsPath(studio.firmwarePath);
    if (!AtomicFile::Write(destination, bytes.data(), bytes.size(), true, result.fileError, source))
        return result;

    // The firmware is already committed: a sidecar failure is partial success,
    // never a failed firmware save and never grounds for retaining a dirty flag.
    studio.MarkSaved();
    studio.lastReport = std::move(report);
    studio.lastBuildPath = utf8Path;
    studio.showBuildModal = false;
    result.outcome = Outcome::FirmwareOnly;
    auto reportPath = destination;
    reportPath.replace_extension(L".report.json");
    const std::string json = studio.lastReport.ToJson();
    if (!AtomicFile::Write(reportPath, json.data(), json.size(), false, result.fileError, source, destination))
        return result;
    result.outcome = Outcome::Complete;
    return result;
}

} // namespace FirmwareSave
