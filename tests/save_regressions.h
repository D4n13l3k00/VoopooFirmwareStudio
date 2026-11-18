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
    auto save = [&](const std::string& path) {
        const auto prepared = FirmwareSave::Prepare(studio.CreateFileSnapshot());
        auto saved = FirmwareSave::Write(*prepared, studio, FirmwareSave::Approval::Confirmed, path);
        if (saved.FirmwareSaved()) {
            if (!studio.projectDocument) studio.MarkSaved();
            studio.lastReport = saved.report;
            studio.lastBuildPath = path;
            studio.showBuildModal = false;
        }
        return saved;
    };
    change(1);
    check(studio.HasUnsavedChanges(), "save fixture edit is not dirty");

    // Source identity must reject exact names, dot aliases, case aliases and hard links.
    const auto original = readBytes(source);
    auto result = save(utf8(source));
    check(!result.FirmwareSaved() && studio.HasUnsavedChanges(), "source overwrite accepted or edits lost");
    result = save(utf8(root / L"." / source.filename()));
    check(!result.FirmwareSaved(), "dot source alias accepted");
    auto upper = source.wstring();
    CharUpperBuffW(upper.data(), static_cast<DWORD>(upper.size()));
    result = save(PathUtils::WideToUtf8(upper));
    check(!result.FirmwareSaved(), "case source alias accepted");
    const auto alias = root / L"hardlink.zwdf";
    if (check(CreateHardLinkW(alias.c_str(), source.c_str(), nullptr) != FALSE, "source hardlink setup failed")) {
        result = save(utf8(alias));
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
        result = save(utf8(destination));
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
    result = save(utf8(backupFailureTarget));
    check(!result.FirmwareSaved() && result.fileError.stage == AtomicFile::Stage::Backup &&
          studio.HasUnsavedChanges(), "backup failure did not abort firmware save");
    check(readBytes(backupFailureTarget) == previous, "backup failure changed destination");
    std::filesystem::remove(backupFailureTarget, ec);
    result = save(utf8(root / L"missing" / L"output.zwdf"));
    check(!result.FirmwareSaved() && studio.HasUnsavedChanges(), "missing directory save marked saved");

    std::vector<uint8_t> expected;
    if (!studio.BuildToBuffer(expected, loadError)) { error += loadError; return false; }
    result = save(utf8(destination));
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
    result = save(utf8(partial));
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

inline bool RunExportReviewRegressions(std::string& error) {
    error.clear();
    const auto check = [&](bool ok, const char* message) {
        if (!ok) { error += message; error += "; "; }
        return ok;
    };
    std::error_code ec;
    auto root = std::filesystem::temp_directory_path(ec);
    if (ec) { error = "export review temporary directory unavailable"; return false; }
    root /= L"Voopoo_export_review_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64());
    if (!std::filesystem::create_directory(root, ec) || ec) {
        error = "export review temporary directory creation failed"; return false;
    }
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    } cleanup{root};
    const auto utf8 = [](const auto& path) { return PathUtils::WideToUtf8(path.wstring()); };
    const auto readBytes = [](const auto& path) {
        std::ifstream input(path, std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    };
    FirmwarePackage package;
    StudioState studio;
    if (!LoadEmbeddedFirmware(package, error) ||
        !studio.LoadPackage(std::move(package), "review-source.zwdf", error)) return false;
    studio.isBuiltin = true;
    auto resource = studio.originals.front();
    resource.front() ^= 1;
    studio.SetBitmapReplacement(0, std::move(resource));
    auto menu = studio.menuPatch;
    menu.enabled = true;
    studio.SetMenuPatch(menu);
    auto screen = studio.screenPatches[0];
    screen.enabled = true;
    screen.pixels.assign(size_t(FirmwareLogo::Width) * FirmwareLogo::Height * 2, 0x31);
    studio.SetScreenPatch(FirmwareLogo::Target::Boot, screen);
    const auto prepared = FirmwareSave::Prepare(studio.CreateFileSnapshot());
    if (!check(prepared->Valid(), "valid review failed") ||
        !check(prepared->Revisions().size() == studio.firmware.records.size(), "review omitted hardware revisions") ||
        !check(std::filesystem::is_empty(root), "preparation created output/report/backup/temp files")) return false;
    FirmwarePackage built;
    if (!FirmwarePackage::Parse(prepared->Bytes(), built, error)) return false;
    for (size_t r = 0; r < built.records.size(); ++r)
        check(prepared->Revisions()[r].capacityKnown &&
            prepared->Revisions()[r].usedBytes == built.records[r].image.size() &&
            prepared->Revisions()[r].usedBytes < prepared->Revisions()[r].capacityBytes,
            "review Flash usage differs from actual prepared native output");
    const auto cancelledPath = root / L"cancelled.zwdf";
    bool writeStage = false;
    auto result = FirmwareSave::Write(*prepared, studio, FirmwareSave::Approval::Confirmed, utf8(cancelledPath),
        [&](auto) { writeStage = true; }, [] { return false; });
    check(result.cancelled && !result.FirmwareSaved() && !writeStage && std::filesystem::is_empty(root),
        "cancelled approval wrote files or entered the writer");
    auto nextResource = studio.originals.front();
    nextResource.front() ^= 2;
    studio.SetBitmapReplacement(0, std::move(nextResource));
    const auto stalePath = root / L"stale.zwdf";
    result = FirmwareSave::Write(*prepared, studio, FirmwareSave::Approval::Confirmed, utf8(stalePath));
    check(!result.FirmwareSaved() && !result.buildError.empty() && std::filesystem::is_empty(root),
        "stale review approval wrote output");
    check(prepared->Bytes() == built.source_bytes, "live edit mutated immutable prepared bytes");
    const auto refreshed = FirmwareSave::Prepare(studio.CreateFileSnapshot());
    const auto output = root / L"approved.zwdf";
    std::vector<std::string> stages;
    result = FirmwareSave::Write(*refreshed, studio, FirmwareSave::Approval::Confirmed, utf8(output), {}, {},
        [&](const char* stage) { stages.emplace_back(stage); });
    check(result.outcome == FirmwareSave::Outcome::Complete && readBytes(output) == refreshed->Bytes() &&
        readBytes(output) != prepared->Bytes(), "writer did not use the exact newly approved bytes");
    check(std::none_of(stages.begin(), stages.end(), [](const auto& stage) {
        return stage.find("Build") != std::string::npos || stage.find("Pack firmware") != std::string::npos;
    }), "confirmation rebuilt instead of writing prepared bytes");
    auto reportPath = output;
    reportPath.replace_extension(L".report.json");
    const auto report = readBytes(reportPath);
    check(std::string(report.begin(), report.end()) == refreshed->Report().ToJson(), "approved report differs from prepared report");
    {
        // Give every revision two contradictory writes to the very same byte.
        // The real layout builder, not a UI overlap heuristic, diagnoses them.
        FirmwarePackage conflictPackage;
        StudioState conflicts;
        if (!LoadEmbeddedFirmware(conflictPackage, error) ||
            !conflicts.LoadPackage(std::move(conflictPackage), "conflicts.zwdf", error)) return false;
        conflicts.isBuiltin = true;
        for (size_t r = 0; r < conflicts.firmware.records.size(); ++r) {
            const auto& record = conflicts.firmware.records[r];
            const auto code = uint16_t(uint16_t(record.device_code[0]) << 8 | record.device_code[1]);
            for (int shift : {1, 2}) {
                FirmwareLayout::Element element;
                element.id = "review_conflict_" + std::to_string(code) + "_" + std::to_string(shift);
                element.hardwareCode = code;
                element.maximumOffset = {2, 0};
                element.sites.push_back({record.linked_base, FirmwareLayout::Encoding::UInt8, false, record.image.front(), 1});
                conflicts.layoutOffsets.emplace(element.id, FirmwareLayout::Point{shift, 0});
                conflicts.layoutElements.push_back(std::move(element));
            }
        }
        auto replacement = conflicts.originals.front();
        conflicts.SetBitmapReplacement(0, std::move(replacement));
        // A separate resource failure must remain visible after a layout failure.
        conflicts.replacements[conflicts.catalog.front().id] = {0xff};
        for (size_t r = 0; r < conflicts.firmware.records.size(); ++r)
            conflicts.locations.front()[r] = conflicts.firmware.records[r].image.size();
        const auto review = FirmwareSave::Prepare(conflicts.CreateFileSnapshot());
        check(!review->Valid() && !review->Issues().empty() && review->Bytes().empty(),
            "conflicting review retained exportable bytes");
        check(review->Revisions().size() == conflicts.firmware.records.size(), "failed build omitted later revisions");
        for (const auto& revision : review->Revisions())
            check(revision.issues.size() >= 2 &&
                std::any_of(revision.issues.begin(), revision.issues.end(), [](const auto& issue) {
                    return issue.find("conflicting coordinate sites") != std::string::npos;
                }) && std::any_of(revision.issues.begin(), revision.issues.end(), [](const auto& issue) {
                    return issue.find("out of bounds") != std::string::npos;
                }), "fail-fast build hid a later revision layout/resource diagnostic");
        result = FirmwareSave::Write(*review, conflicts, FirmwareSave::Approval::Confirmed, utf8(root / L"conflicts.zwdf"));
        check(!result.FirmwareSaved() && !std::filesystem::exists(root / L"conflicts.zwdf"),
            "invalid review could be confirmed");
    }
    uint32_t random = 0x7ab391du;
    for (size_t target = 0; target < FirmwareLogo::TargetCount; ++target) {
        auto large = screen;
        for (auto& byte : large.pixels) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            byte = uint8_t(random);
        }
        studio.SetScreenPatch(static_cast<FirmwareLogo::Target>(target), std::move(large));
    }
    const auto overflow = FirmwareSave::Prepare(studio.CreateFileSnapshot());
    check(!overflow->Valid() && overflow->Bytes().empty() &&
        overflow->Revisions().size() == studio.firmware.records.size(), "overflow review omitted revisions or allowed approval");
    for (const auto& revision : overflow->Revisions())
        check(revision.capacityKnown && revision.usedBytes > revision.capacityBytes && !revision.issues.empty(),
            "overflow did not retain exact capacity and reason for every revision");
    {
        FirmwarePackage damagedPackage;
        StudioState damaged;
        if (!LoadEmbeddedFirmware(damagedPackage, error) ||
            !damaged.LoadPackage(std::move(damagedPackage), "foreign-inventory.zwdf", error)) return false;
        damaged.isBuiltin = true;
        damaged.firmware.records.back().device_code[0] = 0xff;
        damaged.firmware.records.back().device_code[1] = 0xff;
        const auto review = FirmwareSave::Prepare(damaged.CreateFileSnapshot());
        check(!review->Valid() && !review->Issues().empty() && review->Bytes().empty() &&
            review->Revisions().size() == damaged.firmware.records.size(),
            "foreign hardware inventory bypassed the ordinary full-build guard");
        for (size_t r = 0; r + 1 < review->Revisions().size(); ++r)
            check(review->Revisions()[r].capacityKnown, "one unknown record hid recognized revision Flash accounting");
        check(!review->Revisions().back().capacityKnown, "unknown hardware received fabricated Flash capacity");
    }
    {
        FirmwarePackage otherPackage;
        StudioState other;
        if (!LoadEmbeddedFirmware(otherPackage, error, FirmwareModel::DragX, FirmwareEdition::Score) ||
            !other.LoadPackage(std::move(otherPackage), "unknown-capacity.zwdf", error)) return false;
        other.isBuiltin = true;
        const auto review = FirmwareSave::Prepare(other.CreateFileSnapshot());
        check(review->Valid() && review->Revisions().size() == other.firmware.records.size(),
            "unknown Flash map prevented an otherwise valid guarded export");
        for (const auto& revision : review->Revisions())
            check(!revision.capacityKnown && revision.capacityBytes == 0, "unknown Flash map invented capacity");
    }
    return error.empty();
}
