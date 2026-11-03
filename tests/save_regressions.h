#pragma once

#include "firmware_save.h"
#include <iterator>

// Real filesystem failures, with no dialogs and no fault-injection production code.
inline bool RunSaveRegressions(std::string& error) {
    error.clear();
    auto check = [&](bool ok, const char* message) {
        if (!ok) { error += message; error += "; "; }
        return ok;
    };
    std::error_code ec;
    auto root = std::filesystem::temp_directory_path(ec);
    if (ec) { error = "save regression temp directory unavailable"; return false; }
    root /= L"Voopoo_save_тест_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64());
    if (!std::filesystem::create_directory(root, ec) || ec) {
        error = "save regression directory creation failed";
        return false;
    }
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    } cleanup{root};
    auto utf8 = [](const std::filesystem::path& path) { return PathUtils::WideToUtf8(path.wstring()); };
    auto readBytes = [](const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };
    FirmwarePackage package;
    std::string loadError;
    if (!LoadEmbeddedFirmware(package, loadError)) { error = loadError; return false; }
    const auto source = root / L"исходная.zwdf";
    AtomicFile::Error fileError;
    if (!AtomicFile::Write(source, package.source_bytes.data(), package.source_bytes.size(), false, fileError)) {
        error = "save regression source creation failed";
        return false;
    }
    StudioState studio;
    if (!studio.LoadPackage(std::move(package), utf8(source), loadError)) { error = loadError; return false; }
    auto change = [&](uint8_t mask) {
        auto bytes = studio.originals.front();
        bytes.front() ^= mask;
        studio.PushUndo("Save regression edit");
        studio.SetBitmapReplacement(0, std::move(bytes));
    };
    change(1);
    check(studio.HasUnsavedChanges(), "save fixture edit is not dirty");

    // Source identity must reject exact names, dot aliases, case aliases and hard links.
    const auto original = readBytes(source);
    auto result = FirmwareSave::Save(studio, utf8(source));
    check(!result.FirmwareSaved() && studio.HasUnsavedChanges(), "source overwrite accepted or edits lost");
    result = FirmwareSave::Save(studio, utf8(root / L"." / source.filename()));
    check(!result.FirmwareSaved(), "dot source alias accepted");
    auto upper = source.wstring();
    CharUpperBuffW(upper.data(), static_cast<DWORD>(upper.size()));
    result = FirmwareSave::Save(studio, PathUtils::WideToUtf8(upper));
    check(!result.FirmwareSaved(), "case source alias accepted");
    const auto alias = root / L"hardlink.zwdf";
    if (check(CreateHardLinkW(alias.c_str(), source.c_str(), nullptr) != FALSE, "source hardlink setup failed")) {
        result = FirmwareSave::Save(studio, utf8(alias));
        check(!result.FirmwareSaved(), "hardlink source alias accepted");
    }
    check(readBytes(source) == original, "source contents changed after refused saves");

    const auto destination = root / L"результат.zwdf";
    const std::vector<uint8_t> previous{1, 3, 5, 7};
    if (!check(AtomicFile::Write(destination, previous.data(), previous.size(), false, fileError),
               "existing destination setup failed")) return false;
    {
        AtomicFile::Handle lock;
        lock.value = CreateFileW(destination.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!check(lock.value != INVALID_HANDLE_VALUE, "destination lock setup failed")) return false;
        result = FirmwareSave::Save(studio, utf8(destination));
        check(!result.FirmwareSaved() && studio.HasUnsavedChanges(), "locked target marked saved");
        check(readBytes(destination) == previous, "locked target was changed");
    }
    // The temporary filename fits NTFS's component limit, but the longer
    // backup suffix does not. Extended paths avoid the unrelated MAX_PATH limit.
    const auto rootName = root.wstring();
    const auto extendedRoot = rootName.rfind(L"\\\\", 0) == 0
        ? L"\\\\?\\UNC\\" + rootName.substr(2) : L"\\\\?\\" + rootName;
    const auto backupFailureTarget = std::filesystem::path(extendedRoot) / std::wstring(235, L'x');
    if (!check(AtomicFile::Write(backupFailureTarget, previous.data(), previous.size(), false, fileError),
               "backup failure target setup failed")) return false;
    result = FirmwareSave::Save(studio, utf8(backupFailureTarget));
    check(!result.FirmwareSaved() && result.fileError.stage == AtomicFile::Stage::Backup &&
          studio.HasUnsavedChanges(), "backup failure did not abort firmware save");
    check(readBytes(backupFailureTarget) == previous, "backup failure changed destination");
    std::filesystem::remove(backupFailureTarget, ec);
    result = FirmwareSave::Save(studio, utf8(root / L"missing" / L"output.zwdf"));
    check(!result.FirmwareSaved() && studio.HasUnsavedChanges(), "missing directory save marked saved");

    std::vector<uint8_t> expected;
    if (!studio.BuildToBuffer(expected, loadError)) { error += loadError; return false; }
    result = FirmwareSave::Save(studio, utf8(destination));
    check(result.outcome == FirmwareSave::Outcome::Complete, "Unicode replacement save failed");
    check(!studio.HasUnsavedChanges(), "successful save remains dirty");
    check(readBytes(destination) == expected, "saved firmware bytes differ from build");
    auto reportPath = destination;
    reportPath.replace_extension(L".report.json");
    const auto reportBytes = readBytes(reportPath);
    const auto expectedReport = studio.lastReport.ToJson();
    check(std::string(reportBytes.begin(), reportBytes.end()) == expectedReport, "saved report mismatch");
    bool backupFound = false;
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.path().filename().wstring().find(L"результат.zwdf.bak.") == 0 && readBytes(entry.path()) == previous)
            backupFound = true;
    }
    check(backupFound, "previous destination backup missing");

    studio.Undo();
    check(studio.HasUnsavedChanges(), "undo away from saved state stays clean");
    studio.Redo();
    check(!studio.HasUnsavedChanges(), "redo to saved state stays dirty");
    change(2);
    const auto partial = root / L"partial.zwdf";
    auto blockedReport = partial;
    blockedReport.replace_extension(L".report.json");
    if (!check(std::filesystem::create_directory(blockedReport, ec) && !ec, "report failure setup failed")) return false;
    if (!studio.BuildToBuffer(expected, loadError)) { error += loadError; return false; }
    result = FirmwareSave::Save(studio, utf8(partial));
    check(result.outcome == FirmwareSave::Outcome::FirmwareOnly && result.FirmwareSaved(),
          "report failure not reported as partial firmware success");
    check(!studio.HasUnsavedChanges() && !studio.showBuildModal,
          "report failure retained dirty state or enabled complete-success modal");
    check(readBytes(partial) == expected, "report failure lost committed firmware");
    check(std::filesystem::is_directory(blockedReport), "blocked report destination was changed");
    for (const auto& entry : std::filesystem::directory_iterator(root))
        check(entry.path().filename().wstring().find(L".tmp.") == std::wstring::npos, "temporary file leaked");
    return error.empty();
}
