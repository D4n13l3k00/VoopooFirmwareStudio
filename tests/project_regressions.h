#pragma once

#include "firmware_project.h"
#include "firmware_save.h"
#include "firmware_profiles.h"
#include <array>
#include <bit>
#include <iterator>
#include <limits>
#include <cstdio>
#include <lzma.h>
#include <ostream>

inline bool RunProjectRegressions(std::string& error) {
    error.clear();
    auto check = [&](bool ok, const char* message) {
        if (!ok) { error += message; error += "; "; }
        return ok;
    };
    std::error_code ec;
    auto root = std::filesystem::temp_directory_path(ec);
    if (ec) { error = "project regression temp directory unavailable"; return false; }
    root /= L"Voopoo_project_тест_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64());
    if (!std::filesystem::create_directory(root, ec) || ec) {
        error = "project regression directory creation failed";
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
    AtomicFile::Error fileError;
    auto writeBytes = [&](const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
        return check(AtomicFile::Write(path, bytes.data(), bytes.size(), false, fileError),
                     "project regression fixture write failed");
    };
    FirmwarePackage package;
    std::string detail;
    if (!LoadEmbeddedFirmware(package, detail)) { error = detail; return false; }
    const auto originalSource = package.source_bytes;
    const auto sourcePath = root / L"исходная.zwdf";
    if (!writeBytes(sourcePath, originalSource)) return false;
    StudioState studio;
    if (!studio.LoadPackage(std::move(package), utf8(sourcePath), detail)) { error = detail; return false; }
    studio.EnableProjectDocument();
    if (!check(studio.catalog.size() >= 2 && !studio.palette.empty() && !studio.layoutElements.empty(),
               "project fixture lacks bitmap/palette/native layout")) return false;
    for (size_t i = 0; i < 2; ++i) {
        auto bitmap = studio.originals[i];
        bitmap.front() ^= uint8_t(i + 1);
        studio.PushUndo("Project bitmap edit");
        studio.SetBitmapReplacement(i, std::move(bitmap));
        studio.SetAssetPaintColor(studio.catalog[i].id, {uint8_t(51 + i), 102, 153});
    }
    studio.PushUndo("Project exact palette edit");
    studio.SetPaletteColor(0, {17, 33, 49});
    // Project persistence must not depend on whether staged edits can export.
    studio.SetBackgroundColor({9, 18, 27});
    bool moved = false;
    for (size_t i = 0; i < studio.layoutElements.size(); ++i) {
        const auto& element = studio.layoutElements[i];
        FirmwareLayout::Point offset;
        if (element.maximumOffset.x >= element.stepX) offset.x = element.stepX;
        else if (element.minimumOffset.x <= -element.stepX) offset.x = -element.stepX;
        else if (element.maximumOffset.y >= element.stepY) offset.y = element.stepY;
        else if (element.minimumOffset.y <= -element.stepY) offset.y = -element.stepY;
        else continue;
        studio.PushUndo("Project native layout edit");
        if (!studio.SetLayoutOffset(i, offset, detail)) { error += detail; return false; }
        moved = true;
        break;
    }
    if (!check(moved && studio.HasUnsavedChanges(), "project fixture layout did not move")) return false;
    const FirmwareMenu::Settings disabledMenuPreferences{false, true, true,
        {FirmwareMenu::Item::RankSelector, FirmwareMenu::Item::Exit, FirmwareMenu::Item::PuffEditor,
         FirmwareMenu::Item::Information, FirmwareMenu::Item::ScoreEditor, FirmwareMenu::Item::Voltage},
        FirmwareMenu::InformationMask | FirmwareMenu::PuffEditorMask |
            FirmwareMenu::ScoreEditorMask | FirmwareMenu::RankSelectorMask, FirmwareMenu::InfoResistanceMask};
    studio.PushUndo("Project disabled menu preferences");
    studio.SetMenuPatch(disabledMenuPreferences);
    const auto expectedReplacements = studio.replacements;
    const auto expectedLayout = studio.layoutOffsets;
    const auto expectedBrushes = studio.assetPaintColors;
    const auto expectedPalette = studio.palette;
    const auto expectedBackground = studio.backgroundColor;
    const auto initialHistory = studio.undoStack.size();
    const auto initialFirmwarePath = studio.firmwarePath;
    auto saved = FirmwareProject::Save(studio, utf8(sourcePath));
    check(!saved.saved && studio.projectPath.empty() && studio.HasUnsavedChanges(),
          "project save overwrote original source or changed failed-save identity");
    check(readBytes(sourcePath) == originalSource, "project save changed original firmware file");
    const auto projectPath = root / L"проект.vfwp";
    saved = FirmwareProject::Save(studio, utf8(projectPath));
    if (!check(saved.saved, "Unicode project save failed")) { error += saved.error; return false; }
    check(!studio.HasUnsavedChanges() && studio.projectDocument && studio.projectPath == utf8(projectPath),
          "project save did not establish clean independent project identity");
    check(studio.firmwarePath == initialFirmwarePath && studio.undoStack.size() == initialHistory,
          "project save changed source identity or dropped live undo history");
    check(studio.firmware.source_bytes == originalSource && readBytes(sourcePath) == originalSource,
          "project save repacked original firmware");
    const auto valid = readBytes(projectPath);
    std::vector<uint8_t> rebuilt;
    if (!FirmwareProject::Build(studio, rebuilt, detail)) { error += detail; return false; }
    check(valid == rebuilt, "project serialization is not deterministic");
    studio.Undo();
    check(studio.HasUnsavedChanges(), "undo away from saved project stayed clean");
    studio.Redo();
    check(!studio.HasUnsavedChanges(), "redo to saved project stayed dirty");

    const auto portableDirectory = root / L"переносной";
    if (!check(std::filesystem::create_directory(portableDirectory, ec) && !ec,
               "portable project directory creation failed")) return false;
    const auto portablePath = portableDirectory / L"автономный.VFWP";
    std::filesystem::rename(projectPath, portablePath, ec);
    if (!check(!ec, "project relocation failed")) return false;
    std::filesystem::remove(sourcePath, ec);
    if (!check(!ec && !std::filesystem::exists(sourcePath), "original source removal failed")) return false;
    StudioState reopened;
    if (!FirmwareProject::Open(reopened, utf8(portablePath), detail)) { error += detail; return false; }
    check(FirmwareProject::IsProjectPath(utf8(portablePath)), "project extension is not case-insensitive");
    check(reopened.firmware.source_bytes == originalSource, "project did not preserve exact original source bytes");
    check(reopened.replacements == expectedReplacements && reopened.layoutOffsets == expectedLayout &&
          reopened.assetPaintColors == expectedBrushes, "project lost staged bitmap/layout/brush changes");
    check(reopened.palette.size() == expectedPalette.size(), "project lost palette roles");
    for (size_t i = 0; i < (std::min)(reopened.palette.size(), expectedPalette.size()); ++i)
        check(reopened.palette[i].current == expectedPalette[i].current, "project rounded or lost exact RGB palette values");
    check(reopened.backgroundColor == expectedBackground, "project lost staged background color");
    check(reopened.screenPatches == studio.screenPatches, "project changed disabled screen patches");
    check(reopened.menuPatch == studio.menuPatch && reopened.menuPatch.batteryVoltage &&
          reopened.menuPatch.reverseNavigation, "project lost disabled menu voltage/navigation/item preferences");
    check(reopened.projectPath == utf8(portablePath) && reopened.firmwarePath == "исходная.zwdf" &&
          reopened.firmwareSourceEmbedded && !reopened.isBuiltin && reopened.projectDocument,
          "portable project source/project identities are incorrect");
    check(!reopened.HasUnsavedChanges() && !reopened.CanUndo() && !reopened.CanRedo() &&
          reopened.HistoryMemoryBytes() == 0 && !reopened.activePreviewTexture.IsValid(),
          "reopened project retained dirty state/history/GPU resources");
    {
        StudioState emptyMenu;
        if (!FirmwareProject::Open(emptyMenu, utf8(portablePath), detail)) { error += detail; return false; }
        auto emptyPreferences = disabledMenuPreferences;
        emptyPreferences.enabled = true;
        emptyPreferences.enabledItems = FirmwareMenu::PuffEditorMask;
        emptyPreferences.infoFields = 0;
        emptyMenu.PushUndo("Select only puff editor");
        emptyMenu.SetMenuPatch(emptyPreferences);
        emptyMenu.EndUndoAction();
        const auto emptyMenuPath = root / L"disabled-empty-menu.vfwp";
        const auto emptyMenuSaved = FirmwareProject::Save(emptyMenu, utf8(emptyMenuPath));
        if (!emptyMenuSaved.saved) { error += emptyMenuSaved.error; return false; }
        StudioState emptyMenuReopened;
        if (!FirmwareProject::Open(emptyMenuReopened, utf8(emptyMenuPath), detail)) { error += detail; return false; }
        if (!check(emptyMenuReopened.menuPatch == emptyPreferences && !emptyMenuReopened.HasUnsavedChanges(),
                   "project lost puff-editor-only menu with no INFO fields")) return false;
    }

    // Exact editor metadata is a project change even if its RGB565 value is unchanged.
    reopened.PushUndo("Project RGB precision");
    reopened.SetPaletteColor(0, {16, 32, 48});
    check(reopened.HasUnsavedChanges(), "project ignored RGB precision-only edit");
    reopened.Undo();
    check(!reopened.HasUnsavedChanges(), "project exact palette undo did not restore baseline");
    reopened.PushUndo("Project brush metadata");
    reopened.SetAssetPaintColor(reopened.catalog[0].id, {12, 34, 56});
    check(reopened.HasUnsavedChanges(), "project ignored brush-only edit");
    reopened.SetBackgroundColor({0, 0, 0});
    const auto exportPath = root / L"экспорт.zwdf";
    const auto preparedExport = FirmwareSave::Prepare(reopened.CreateFileSnapshot());
    const auto exported = FirmwareSave::Write(*preparedExport, reopened, FirmwareSave::Approval::Confirmed, utf8(exportPath));
    check(exported.outcome == FirmwareSave::Outcome::Complete && reopened.HasUnsavedChanges(),
          "portable project export failed or marked project saved");
    check(readBytes(portablePath) == valid, "firmware export changed saved project");
    const auto refusedExport = FirmwareSave::Write(*preparedExport, reopened, FirmwareSave::Approval::Confirmed, utf8(portablePath));
    check(!refusedExport.FirmwareSaved() && readBytes(portablePath) == valid && reopened.HasUnsavedChanges(),
          "firmware export overwrote project or cleared dirty state");
    const auto partialPath = root / L"partial.zwdf";
    auto blockedReport = partialPath;
    blockedReport.replace_extension(L".report.json");
    if (!check(std::filesystem::create_directory(blockedReport, ec) && !ec,
               "project partial-export fixture setup failed")) return false;
    const auto partial = FirmwareSave::Write(*preparedExport, reopened, FirmwareSave::Approval::Confirmed, utf8(partialPath));
    check(partial.outcome == FirmwareSave::Outcome::FirmwareOnly && reopened.HasUnsavedChanges(),
          "firmware-only export marked project saved");
    const auto saveAsPath = root / L"копия.vfwp";
    saved = FirmwareProject::Save(reopened, utf8(saveAsPath));
    check(saved.saved && reopened.projectPath == utf8(saveAsPath) && !reopened.HasUnsavedChanges(),
          "project Save As failed to change path/baseline");
    reopened.Undo();
    check(reopened.HasUnsavedChanges(), "brush undo away from saved project stayed clean");
    reopened.Redo();
    check(!reopened.HasUnsavedChanges(), "brush redo to saved project stayed dirty");
    reopened.PushUndo("Unsaved project before failure");
    auto bitmap = reopened.replacements.at(reopened.catalog[0].id);
    bitmap.front() ^= 4;
    reopened.SetBitmapReplacement(0, std::move(bitmap));
    reopened.PushUndo("Redo history must survive failed project load");
    reopened.SetAssetPaintColor(reopened.catalog[1].id, {1, 2, 3});
    reopened.Undo();
    const auto undoSize = reopened.undoStack.size();
    const auto redoSize = reopened.redoStack.size();
    const auto historyBytes = reopened.HistoryMemoryBytes();
    const auto revision = reopened.ResourceRevision();
    const auto documentIdentity = reopened.DocumentIdentity();
    const auto identity = reopened.projectPath;
    std::vector<uint8_t> beforeFailure;
    if (!FirmwareProject::Build(reopened, beforeFailure, detail)) { error += detail; return false; }
    {
        AtomicFile::Handle lock;
        lock.value = CreateFileW(saveAsPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!check(lock.value != INVALID_HANDLE_VALUE, "project destination lock failed")) return false;
        const auto previous = readBytes(saveAsPath);
        saved = FirmwareProject::Save(reopened, utf8(saveAsPath));
        check(!saved.saved && reopened.HasUnsavedChanges() && reopened.projectPath == identity &&
              readBytes(saveAsPath) == previous, "failed atomic project save changed file/baseline/identity");
    }

    // Decode the actual XZ payload once, then mutate the existing body tables.
    // Semantic fixtures are recompressed to exercise both decoder and parser.
    auto u32 = [](const std::vector<uint8_t>& bytes, size_t offset) {
        uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i) value |= uint32_t(bytes[offset + i]) << (i * 8);
        return value;
    };
    auto u64 = [](const std::vector<uint8_t>& bytes, size_t offset) {
        uint64_t value = 0;
        for (unsigned i = 0; i < 8; ++i) value |= uint64_t(bytes[offset + i]) << (i * 8);
        return value;
    };
    auto patch32 = [](std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = uint8_t(value >> (i * 8));
    };
    auto patch64 = [](std::vector<uint8_t>& bytes, size_t offset, uint64_t value) {
        for (unsigned i = 0; i < 8; ++i) bytes[offset + i] = uint8_t(value >> (i * 8));
    };
    auto repair = [&](std::vector<uint8_t>& bytes) {
        patch64(bytes, 16, bytes.size());
        patch32(bytes, 104, 0);
        static const auto table = [] {
            std::array<uint32_t, 256> values{};
            for (uint32_t i = 0; i < values.size(); ++i) {
                uint32_t value = i;
                for (unsigned bit = 0; bit < 8; ++bit)
                    value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0u);
                values[i] = value;
            }
            return values;
        }();
        uint32_t crc = 0xffffffffu;
        for (const auto byte : bytes) crc = (crc >> 8) ^ table[(crc ^ byte) & 255];
        patch32(bytes, 104, ~crc);
    };
    const size_t decodedSize = static_cast<size_t>(u64(valid, 24) + u64(valid, 32));
    if (!check(u32(valid, 8) == FirmwareProject::Version && u32(valid, 108) == 1 && valid.size() < 112 + decodedSize,
               "project did not produce beneficial XZ compression")) return false;
    std::vector<uint8_t> rawValid(valid.begin(), valid.begin() + 112);
    rawValid.resize(112 + decodedSize);
    uint64_t memoryLimit = 64 * 1024 * 1024;
    size_t inputPosition = 112, outputPosition = 112;
    if (!check(lzma_stream_buffer_decode(&memoryLimit, 0, nullptr, valid.data(), &inputPosition, valid.size(),
                   rawValid.data(), &outputPosition, rawValid.size()) == LZMA_OK &&
               inputPosition == valid.size() && outputPosition == rawValid.size(),
               "project XZ payload is not independently decodable")) return false;
    patch32(rawValid, 108, 0);
    repair(rawValid);
    std::printf("VFWP XZ preset 6 proof: %zu stored bytes -> %zu bytes\n", rawValid.size(), valid.size());
    const auto storedPath = root / L"stored.vfwp";
    constexpr size_t screenSettingsBytes = 32 * FirmwareLogo::TargetCount;
    constexpr size_t sourceMetadataBytes = 112 * FirmwareLogo::TargetCount;
    constexpr size_t menuSettingsBytes = (5 + FirmwareMenu::ItemCount) * sizeof(uint32_t);
    if (!writeBytes(storedPath, rawValid)) return false;
    StudioState storedLoaded;
    if (!check(FirmwareProject::Open(storedLoaded, utf8(storedPath), detail),
               "stored current project roundtrip failed")) { error += detail; return false; }
    check(storedLoaded.firmware.source_bytes == originalSource &&
          storedLoaded.replacements == expectedReplacements && storedLoaded.layoutOffsets == expectedLayout &&
          storedLoaded.assetPaintColors == expectedBrushes && storedLoaded.backgroundColor == expectedBackground &&
          storedLoaded.palette.size() == expectedPalette.size() &&
          storedLoaded.screenPatches == studio.screenPatches &&
          storedLoaded.screenPatchSources == studio.screenPatchSources && storedLoaded.menuPatch == studio.menuPatch &&
          !storedLoaded.HasUnsavedChanges(), "stored current project lost document data");
    for (size_t i = 0; i < (std::min)(storedLoaded.palette.size(), expectedPalette.size()); ++i)
        check(storedLoaded.palette[i].current == expectedPalette[i].current, "stored project lost exact palette");
    auto compressFixture = [&](std::vector<uint8_t>& bytes, uint32_t preset = 6,
                               lzma_check frameCheck = LZMA_CHECK_CRC64) {
        const size_t bodySize = bytes.size() - 112;
        std::vector<uint8_t> encoded(112 + lzma_stream_buffer_bound(bodySize));
        std::copy_n(bytes.begin(), 112, encoded.begin());
        size_t position = 112;
        if (!check(lzma_easy_buffer_encode(preset, frameCheck, nullptr, bytes.data() + 112, bodySize,
                       encoded.data(), &position, encoded.size()) == LZMA_OK,
                   "malformed project fixture compression failed")) return false;
        encoded.resize(position);
        patch32(encoded, 108, 1);
        patch64(encoded, 32, bodySize - static_cast<size_t>(u64(bytes, 24)));
        repair(encoded);
        bytes = std::move(encoded);
        return true;
    };
    struct Entry { size_t start, id, idLength, value, end; };
    auto skipString = [&](size_t& cursor) { const auto length = u32(rawValid, cursor); cursor += 4 + length; };
    auto table = [&](size_t& cursor, bool bitmapTable) {
        std::vector<Entry> entries;
        const auto count = u32(rawValid, cursor); cursor += 4;
        for (uint32_t i = 0; i < count; ++i) {
            Entry item{cursor, cursor + 4, u32(rawValid, cursor), 0, 0};
            skipString(cursor); item.value = cursor;
            if (bitmapTable) { const auto length = u32(rawValid, cursor); cursor += 4 + length; }
            else cursor += 12;
            item.end = cursor;
            entries.push_back(item);
        }
        return entries;
    };
    size_t cursor = 112 + originalSource.size();
    skipString(cursor);
    const auto bitmapEntries = table(cursor, true);
    const auto paletteEntries = table(cursor, false);
    cursor += 12; // Background RGB.
    const size_t layoutCountOffset = cursor;
    const auto layoutCount = u32(rawValid, cursor); cursor += 4;
    std::vector<Entry> layoutEntries;
    for (uint32_t i = 0; i < layoutCount; ++i) {
        Entry item{cursor, cursor + 4, u32(rawValid, cursor), 0, 0};
        skipString(cursor); item.value = cursor; cursor += 8; item.end = cursor;
        layoutEntries.push_back(item);
    }
    const auto brushEntries = table(cursor, false);
    if (!check(bitmapEntries.size() == 2 && !paletteEntries.empty() && !layoutEntries.empty() && brushEntries.size() == 2,
               "project malformed-fixture table discovery failed")) return false;
    const auto invalidPath = root / L"повреждённый.vfwp";
    auto reject = [&](std::vector<uint8_t> bytes, const char* message, bool fix = true,
                      const char* expectedError = nullptr) {
        if (fix) repair(bytes);
        if (!writeBytes(invalidPath, bytes)) return;
        std::string failure;
        check(!FirmwareProject::Open(reopened, utf8(invalidPath), failure) && !failure.empty() &&
              (!expectedError || failure.find(expectedError) != std::string::npos), message);
    };
    auto rejectBody = [&](std::vector<uint8_t> bytes, const char* message) {
        if (compressFixture(bytes)) reject(std::move(bytes), message, false);
    };
    auto unknownId = [&](const Entry& entry, const char* message) {
        auto bytes = rawValid;
        std::fill_n(bytes.begin() + entry.id, entry.idLength, uint8_t('?'));
        rejectBody(std::move(bytes), message);
    };
    auto duplicate = [&](const Entry& first, const Entry& second, const char* message) {
        auto bytes = rawValid;
        const std::vector<uint8_t> record(rawValid.begin() + first.start, rawValid.begin() + first.end);
        bytes.erase(bytes.begin() + second.start, bytes.begin() + second.end);
        bytes.insert(bytes.begin() + second.start, record.begin(), record.end());
        rejectBody(std::move(bytes), message);
    };
    for (const uint32_t version : {0u, 2u, 13u, 14u}) {
        auto unknownVersion = valid;
        patch32(unknownVersion, 8, version);
        reject(std::move(unknownVersion), "unknown project version accepted");
    }
    auto invalid = valid;
    invalid[0] ^= 1;
    reject(std::move(invalid), "invalid project magic accepted");
    invalid = valid; invalid.back() ^= 1;
    reject(std::move(invalid), "project corruption accepted", false, "checksum");
    invalid = valid; invalid[40] = invalid[40] == '0' ? '1' : '0';
    reject(std::move(invalid), "incorrect original source hash accepted");
    invalid = rawValid; invalid[112] ^= 1;
    rejectBody(std::move(invalid), "modified original source bytes accepted");
    invalid = valid; patch32(invalid, 108, 2);
    reject(std::move(invalid), "unknown project codec accepted");
    invalid = valid; patch32(invalid, 108, 0x80000001u);
    reject(std::move(invalid), "unknown project codec flags accepted");
    invalid = valid; patch64(invalid, 24, (std::numeric_limits<uint64_t>::max)());
    reject(std::move(invalid), "unbounded project source size accepted");
    invalid = valid; patch64(invalid, 24, 64 * 1024 * 1024 + 1);
    reject(std::move(invalid), "oversize decoded source accepted");
    invalid = valid; patch64(invalid, 32, FirmwareProject::MaximumDecodedBodyBytes - originalSource.size() + 1);
    reject(std::move(invalid), "oversize decoded body accepted");
    invalid = valid; patch64(invalid, 32, u64(valid, 32) - 1);
    reject(std::move(invalid), "decoded output exceeding declared length accepted");
    invalid = valid; patch64(invalid, 32, u64(valid, 32) + 1);
    reject(std::move(invalid), "short decoded output accepted");
    invalid = valid; invalid.resize(valid.size() - 1);
    reject(std::move(invalid), "truncated project accepted", false);
    invalid = valid; invalid.resize(valid.size() - 1);
    reject(std::move(invalid), "truncated XZ footer accepted");
    reject(std::vector<uint8_t>(valid.begin(), valid.begin() + 20), "truncated project header accepted", false);
    invalid = valid; invalid[112 + (valid.size() - 112) / 2] ^= 0x80;
    reject(std::move(invalid), "invalid compressed data accepted");
    invalid = valid; invalid[valid.size() - 12] ^= 1;
    reject(std::move(invalid), "invalid XZ footer checksum accepted");
    invalid = valid;
    const size_t indexBytes = (static_cast<size_t>(u32(valid, valid.size() - 8)) + 1) * 4;
    invalid[valid.size() - 12 - indexBytes - 1] ^= 1;
    reject(std::move(invalid), "invalid XZ CRC64 payload checksum accepted");
    invalid = valid; invalid.push_back(0);
    reject(std::move(invalid), "trailing XZ byte accepted");
    invalid = valid; invalid.insert(invalid.end(), 4, 0);
    reject(std::move(invalid), "XZ stream padding accepted");
    invalid = valid; invalid.insert(invalid.end(), valid.begin() + 112, valid.end());
    reject(std::move(invalid), "concatenated XZ streams accepted");
    invalid = rawValid; invalid.push_back(0);
    rejectBody(std::move(invalid), "trailing decoded project bytes accepted");
    invalid = rawValid; patch32(invalid, invalid.size() - screenSettingsBytes - sourceMetadataBytes - menuSettingsBytes, 2);
    rejectBody(std::move(invalid), "unknown project screen patch flag accepted");
    invalid = rawValid;
    invalid.erase(invalid.end() - menuSettingsBytes - 1);
    rejectBody(std::move(invalid), "truncated project screen patch settings accepted");
    invalid = rawValid;
    invalid.erase(invalid.end() - menuSettingsBytes - screenSettingsBytes - sourceMetadataBytes,
                  invalid.end() - menuSettingsBytes);
    rejectBody(std::move(invalid), "missing project screen patch settings accepted");
    invalid = rawValid; patch32(invalid, invalid.size() - menuSettingsBytes, 2);
    rejectBody(std::move(invalid), "unknown project device menu enabled flag accepted");
    invalid = rawValid; patch32(invalid, invalid.size() - menuSettingsBytes + 4, 2);
    rejectBody(std::move(invalid), "unknown project device menu voltage flag accepted");
    invalid = rawValid; patch32(invalid, invalid.size() - menuSettingsBytes + 8, 2);
    rejectBody(std::move(invalid), "unknown project device menu navigation flag accepted");
    invalid = rawValid; patch32(invalid, invalid.size() - menuSettingsBytes + 12, FirmwareMenu::FunctionalMask + 1);
    rejectBody(std::move(invalid), "unknown project device menu item mask accepted");
    invalid = rawValid; patch32(invalid, invalid.size() - menuSettingsBytes, 1);
    patch32(invalid, invalid.size() - menuSettingsBytes + 12, 0);
    rejectBody(std::move(invalid), "enabled project menu without functional items accepted");
    invalid = rawValid; patch32(invalid, invalid.size() - 4, FirmwareMenu::AllInfoFields | 8u);
    rejectBody(std::move(invalid), "unknown project INFO field bits accepted");
    invalid = rawValid; patch32(invalid, invalid.size() - 4, 0);
    rejectBody(std::move(invalid), "enabled INFO module without fields accepted while menu disabled");
    invalid = rawValid; patch32(invalid, invalid.size() - menuSettingsBytes, 1);
    patch32(invalid, invalid.size() - 4, 0);
    rejectBody(std::move(invalid), "enabled INFO module without fields accepted while menu enabled");
    for (size_t index = 0; index < FirmwareMenu::ItemCount; ++index) {
        invalid = rawValid;
        patch32(invalid, invalid.size() - menuSettingsBytes + 16 + index * 4, uint32_t(FirmwareMenu::ItemCount));
        rejectBody(std::move(invalid), "unknown project device menu item ID accepted");
    }
    invalid = rawValid;
    patch32(invalid, invalid.size() - menuSettingsBytes + 16,
            u32(invalid, invalid.size() - menuSettingsBytes + 20));
    rejectBody(std::move(invalid), "duplicate project device menu item accepted");
    for (size_t missing = 1; missing <= menuSettingsBytes; ++missing) {
        invalid = rawValid;
        invalid.resize(invalid.size() - missing);
        rejectBody(std::move(invalid), "truncated project device menu settings accepted");
    }
    invalid = rawValid;
    if (compressFixture(invalid, 6, LZMA_CHECK_CRC32))
        reject(std::move(invalid), "non-CRC64 XZ frame accepted");
    invalid = rawValid;
    if (compressFixture(invalid, 6, LZMA_CHECK_NONE))
        reject(std::move(invalid), "unchecked XZ frame accepted");
    // Preset 9's 64 MiB dictionary plus decoder overhead exceeds the cap.
    invalid = rawValid;
    if (compressFixture(invalid, 9)) {
        auto corruptChecksum = invalid;
        corruptChecksum[104] ^= 1;
        reject(std::move(corruptChecksum), "decoder ran before outer checksum validation", false, "checksum");
        reject(std::move(invalid), "oversize XZ decoder memory window accepted", false, "memory limit");
    }
    const size_t sourceMetadataOffset = rawValid.size() - sourceMetadataBytes - menuSettingsBytes;
    invalid = rawValid; patch32(invalid, sourceMetadataOffset, 1);
    rejectBody(std::move(invalid), "source dimensions without original RGBA accepted");
    invalid = rawValid; patch32(invalid, sourceMetadataOffset + 8, 3);
    rejectBody(std::move(invalid), "unknown original-image fit mode accepted");
    invalid = rawValid; patch32(invalid, sourceMetadataOffset + 12, 0x7fc00000);
    rejectBody(std::move(invalid), "nonfinite original-image scale accepted");
    invalid = rawValid; patch32(invalid, sourceMetadataOffset + 16, 65537);
    rejectBody(std::move(invalid), "out-of-range original-image position accepted");
    invalid = rawValid; patch32(invalid, sourceMetadataOffset + 24, 3);
    rejectBody(std::move(invalid), "unknown original-image alignment accepted");
    invalid = rawValid; patch32(invalid, sourceMetadataOffset + 32, 4);
    rejectBody(std::move(invalid), "unknown original-image filter accepted");
    invalid = rawValid; patch32(invalid, sourceMetadataOffset + 36, 0xffffffff);
    rejectBody(std::move(invalid), "unbounded original RGBA payload accepted");
    invalid = rawValid; patch32(invalid, sourceMetadataOffset + 40, 0x7fc00000);
    rejectBody(std::move(invalid), "nonfinite image correction accepted");
    invalid = rawValid; patch32(invalid, sourceMetadataOffset + 72, 17);
    rejectBody(std::move(invalid), "unsupported image palette size accepted");
    // Mutate every appended channel point in an otherwise valid compressed body.
    for (size_t point = 0; point < 9; ++point)
        for (float value : {-0.01f, 1.01f, std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::quiet_NaN()}) {
            invalid = rawValid;
            patch32(invalid, sourceMetadataOffset + 76 + point * 4, std::bit_cast<uint32_t>(value));
            rejectBody(std::move(invalid), "invalid project channel curve accepted");
        }
    for (size_t missing = 1; missing <= 36; ++missing) {
        invalid = rawValid;
        invalid.erase(invalid.end() - menuSettingsBytes - missing, invalid.end() - menuSettingsBytes);
        rejectBody(std::move(invalid), "truncated project channel curves accepted");
    }
    unknownId(bitmapEntries.front(), "unknown project bitmap ID accepted");
    unknownId(paletteEntries.front(), "unknown project palette ID accepted");
    unknownId(layoutEntries.front(), "unknown project layout ID accepted");
    unknownId(brushEntries.front(), "unknown project brush ID accepted");
    duplicate(bitmapEntries[0], bitmapEntries[1], "duplicate project bitmap ID accepted");
    if (paletteEntries.size() >= 2)
        duplicate(paletteEntries[0], paletteEntries[1], "duplicate project palette ID accepted");
    duplicate(brushEntries[0], brushEntries[1], "duplicate project brush ID accepted");
    invalid = rawValid;
    const auto& layout = layoutEntries.front();
    const std::vector<uint8_t> layoutRecord(rawValid.begin() + layout.start, rawValid.begin() + layout.end);
    invalid.insert(invalid.begin() + layout.end, layoutRecord.begin(), layoutRecord.end());
    patch32(invalid, layoutCountOffset, layoutCount + 1);
    rejectBody(std::move(invalid), "duplicate project layout ID accepted");
    invalid = rawValid; patch32(invalid, bitmapEntries.front().value, u32(rawValid, bitmapEntries.front().value) + 1);
    rejectBody(std::move(invalid), "invalid project bitmap length accepted");
    invalid = rawValid; patch32(invalid, bitmapEntries.front().start, 0xffffffffu);
    rejectBody(std::move(invalid), "unbounded project identifier length accepted");
    invalid = rawValid; patch32(invalid, paletteEntries.front().value, 256);
    rejectBody(std::move(invalid), "invalid project RGB palette channel accepted");
    invalid = rawValid; patch32(invalid, brushEntries.front().value, 256);
    rejectBody(std::move(invalid), "invalid project RGB brush channel accepted");
    invalid = rawValid; patch32(invalid, layoutEntries.front().value, 0x7fffffffu);
    rejectBody(std::move(invalid), "out-of-bounds project layout accepted");
    std::vector<uint8_t> afterFailure;
    if (!FirmwareProject::Build(reopened, afterFailure, detail)) { error += detail; return false; }
    check(beforeFailure == afterFailure && reopened.projectPath == identity && reopened.HasUnsavedChanges() &&
          reopened.undoStack.size() == undoSize && reopened.redoStack.size() == redoSize &&
          reopened.HistoryMemoryBytes() == historyBytes && reopened.ResourceRevision() == revision &&
          reopened.DocumentIdentity() == documentIdentity,
          "failed project load changed live document/history/saved baseline");
    FirmwarePackage ordinary;
    if (!LoadEmbeddedFirmware(ordinary, detail) || !reopened.LoadPackage(std::move(ordinary), "ordinary.zwdf", detail)) {
        error += detail; return false;
    }
    check(reopened.projectPath.empty() && !reopened.projectDocument && !reopened.firmwareSourceEmbedded &&
          !reopened.HasUnsavedChanges(), "ordinary firmware load retained project identity");

    // Every embedded profile traverses its real package codec (ZWDF, start/end,
    // SWDF, XWDF). Project roundtrips must not change exported bytes.
    size_t profileCount = 0;
    for (const auto& profile : FirmwareProfiles()) {
        ++profileCount;
        auto fail = [&](const std::string& why) {
            error += std::string(profile.filename) + ": " + why + "; ";
            return false;
        };
        FirmwarePackage original;
        StudioState current;
        if (!LoadEmbeddedFirmware(original, detail, profile.model, profile.edition) ||
            !current.LoadPackage(std::move(original), profile.filename, detail)) return fail(detail);
        current.isBuiltin = true;
        current.EnableProjectDocument();
        if (current.catalog.empty() || current.originals.front().empty()) return fail("missing editable bitmap");
        auto raw = current.originals.front();
        raw.front() ^= 1;
        current.PushUndo("Cross-format project edit");
        current.SetBitmapReplacement(0, std::move(raw));
        current.SetAssetPaintColor(current.catalog.front().id, {7, 81, 239});
        if (!current.palette.empty()) current.SetPaletteColor(0, {24, 80, 232});
        for (size_t i = 0; i < current.layoutElements.size(); ++i) {
            const auto& element = current.layoutElements[i];
            FirmwareLayout::Point offset;
            if (element.maximumOffset.x >= element.stepX) offset.x = element.stepX;
            else if (element.minimumOffset.x <= -element.stepX) offset.x = -element.stepX;
            else if (element.maximumOffset.y >= element.stepY) offset.y = element.stepY;
            else if (element.minimumOffset.y <= -element.stepY) offset.y = -element.stepY;
            else continue;
            if (!current.SetLayoutOffset(i, offset, detail)) return fail(detail);
            break;
        }
        std::vector<uint8_t> expectedExport;
        if (!current.BuildToBuffer(expectedExport, detail)) return fail(detail);
        const auto path = root / (L"profile_" + std::to_wstring(profile.resourceId) + L".vfwp");
        const auto result = FirmwareProject::Save(current, utf8(path));
        if (!result.saved) return fail(result.error.empty() ? "atomic project save failed" : result.error);
        const auto encoded = readBytes(path);
        const size_t profileBodySize = static_cast<size_t>(u64(encoded, 24) + u64(encoded, 32));
        if (u32(encoded, 8) != FirmwareProject::Version || u32(encoded, 108) != 1 || encoded.size() >= 112 + profileBodySize)
            return fail("profile did not produce beneficial XZ compression");
        StudioState loaded;
        if (!FirmwareProject::Open(loaded, utf8(path), detail)) return fail(detail);
        std::vector<uint8_t> actualExport;
        if (!loaded.BuildToBuffer(actualExport, detail)) return fail(detail);
        if (loaded.firmware.source_bytes != current.firmware.source_bytes ||
            loaded.firmware.format != current.firmware.format || loaded.model != profile.model ||
            loaded.edition != profile.edition || loaded.replacements != current.replacements ||
            loaded.assetPaintColors != current.assetPaintColors || loaded.layoutOffsets != current.layoutOffsets ||
            loaded.palette.size() != current.palette.size() || loaded.backgroundColor != current.backgroundColor ||
            loaded.screenPatches != current.screenPatches ||
            loaded.screenPatchSources != current.screenPatchSources || loaded.menuPatch != current.menuPatch ||
            actualExport != expectedExport ||
            loaded.HasUnsavedChanges() || loaded.CanUndo() || loaded.CanRedo() ||
            loaded.DocumentIdentity() == current.DocumentIdentity()) return fail("roundtrip/source/export mismatch");
        for (size_t i = 0; i < current.palette.size(); ++i)
            if (loaded.palette[i].current != current.palette[i].current) return fail("palette metadata mismatch");
        // Decode the same complete current body without removing any tables.
        std::vector<uint8_t> stored(encoded.begin(), encoded.begin() + 112);
        stored.resize(112 + profileBodySize);
        uint64_t limit = 64 * 1024 * 1024;
        size_t in = 112, out = 112;
        if (lzma_stream_buffer_decode(&limit, 0, nullptr, encoded.data(), &in, encoded.size(),
                stored.data(), &out, stored.size()) != LZMA_OK ||
            in != encoded.size() || out != stored.size()) return fail("independent profile XZ decode failed");
        patch32(stored, 108, 0);
        repair(stored);
        if (!writeBytes(storedPath, stored)) return false;
        StudioState storedProfile;
        if (!FirmwareProject::Open(storedProfile, utf8(storedPath), detail) ||
            !storedProfile.BuildToBuffer(actualExport, detail)) return fail(detail);
        if (actualExport != expectedExport || storedProfile.firmware.source_bytes != current.firmware.source_bytes ||
            storedProfile.firmware.format != current.firmware.format || storedProfile.model != profile.model ||
            storedProfile.edition != profile.edition || storedProfile.layoutOffsets != current.layoutOffsets ||
            storedProfile.replacements != current.replacements ||
            storedProfile.assetPaintColors != current.assetPaintColors ||
            storedProfile.palette.size() != current.palette.size() ||
            storedProfile.backgroundColor != current.backgroundColor ||
            storedProfile.screenPatches != current.screenPatches ||
            storedProfile.screenPatchSources != current.screenPatchSources ||
            storedProfile.menuPatch != current.menuPatch ||
            storedProfile.HasUnsavedChanges() || storedProfile.CanUndo() || storedProfile.CanRedo())
            return fail("stored current profile source/document/export mismatch");
        for (size_t i = 0; i < current.palette.size(); ++i)
            if (storedProfile.palette[i].current != current.palette[i].current)
                return fail("stored current profile palette metadata mismatch");
    }
    check(profileCount == 21, "project regression did not cover all 21 embedded profiles");
    // CHIP ID uses the same native descriptor and persisted timing fields as
    // the other targets. Cover both static compatibility and the full sequence
    // across the 7 Score and 9 Factory hardware records.
    size_t serviceRecordCount = 0;
    for (const auto edition : {FirmwareEdition::Score, FirmwareEdition::Factory}) {
        FirmwarePackage original;
        StudioState current;
        if (!LoadEmbeddedFirmware(original, detail, FirmwareModel::DragS, edition) ||
            !current.LoadPackage(std::move(original), "service.zwdf", detail)) {
            error += detail; return false;
        }
        serviceRecordCount += current.firmware.records.size();
        constexpr size_t info = static_cast<size_t>(FirmwareLogo::Target::DeviceInfo);
        FirmwareLogo::Settings service;
        service.enabled = true;
        service.pixels.resize(size_t(FirmwareLogo::Width) * FirmwareLogo::Height * 2);
        for (size_t i = 0; i < service.pixels.size(); i += 2) {
            service.pixels[i] = 0xe0;
            service.pixels[i + 1] = 0x07;
        }
        for (const bool animated : {false, true}) {
            service.animation = animated ? FirmwareLogo::Animation::Dissolve : FirmwareLogo::Animation::None;
            service.exitAnimation = animated ? FirmwareLogo::Animation::Iris : FirmwareLogo::Animation::None;
            service.durationMs = 200;
            service.holdMs = animated ? 350 : 0;
            service.exitDurationMs = 2000;
            current.SetScreenPatch(FirmwareLogo::Target::DeviceInfo, service);
            if (!check(current.screenPatches[info] == service,
                       "CHIP ID setter lost valid static/animated settings")) return false;
            auto invalidService = service;
            invalidService.bootStages = FirmwareLogo::BootStages::Both;
            current.SetScreenPatch(FirmwareLogo::Target::DeviceInfo, invalidService);
            invalidService = service;
            invalidService.holdMs = 10001;
            current.SetScreenPatch(FirmwareLogo::Target::DeviceInfo, invalidService);
            if (!check(current.screenPatches[info] == service,
                       "CHIP ID setter accepted invalid stages or timing")) return false;
            std::vector<uint8_t> exported;
            if (!current.BuildToBuffer(exported, detail)) { error += detail; return false; }
            FirmwarePackage imported;
            if (!FirmwarePackage::Parse(exported, imported, detail)) { error += detail; return false; }
            std::vector<FirmwareLogo::FlashUsage> usage;
            if (!FirmwareLogo::MeasureFlash(current.firmware, current.screenPatches, usage, detail)) {
                error += detail; return false;
            }
            if (!check(usage.size() == imported.records.size(), "Flash accounting missed hardware revisions"))
                return false;
            for (size_t i = 0; i < usage.size(); ++i) {
                const auto& record = imported.records[i];
                if (!check(usage[i].Used() == record.image.size() &&
                    usage[i].hardwareCode == uint16_t((record.device_code[0] << 8) | record.device_code[1]) &&
                    usage[i].capacityBytes + record.linked_base + 1 == 0x0801F000,
                    "Flash usage differs from exported bytes or protected boundary")) return false;
            }
            for (const auto& record : imported.records)
                if (!check(FirmwareLogo::FrameBoundary(record, FirmwareLogo::Target::DeviceInfo) != 0,
                           "CHIP ID exported descriptor failed native validation")) return false;
            StudioState importedDocument;
            if (!importedDocument.LoadPackage(std::move(imported), "imported.zwdf", detail)) {
                error += detail; return false;
            }
            if (!check(importedDocument.screenPatches == current.screenPatches,
                       "CHIP ID firmware import lost static/animated settings")) return false;
            const auto path = root / L"service.vfwp";
            const auto result = FirmwareProject::Save(current, utf8(path));
            if (!result.saved) { error += result.error; return false; }
            StudioState loaded;
            if (!FirmwareProject::Open(loaded, utf8(path), detail)) { error += detail; return false; }
            std::vector<uint8_t> roundtrip;
            if (!loaded.BuildToBuffer(roundtrip, detail)) { error += detail; return false; }
            if (!check(loaded.screenPatches == current.screenPatches && roundtrip == exported,
                       "CHIP ID project roundtrip lost settings or changed firmware")) return false;
        }
        auto crowded = current.screenPatches;
        uint32_t random = 0x12345678;
        for (auto& screen : crowded) {
            screen = service;
            for (auto& byte : screen.pixels) {
                random ^= random << 13; random ^= random >> 17; random ^= random << 5;
                byte = uint8_t(random);
            }
        }
        std::vector<FirmwareLogo::FlashUsage> crowdedUsage;
        if (!FirmwareLogo::MeasureFlash(current.firmware, crowded, crowdedUsage, detail)) {
            error += detail; return false;
        }
        bool overflow = false;
        for (const auto& usage : crowdedUsage) overflow |= usage.Used() > usage.capacityBytes;
        std::vector<uint8_t> rejected;
        BuildReport rejectedReport;
        if (!check(overflow && !FirmwareLogo::Build(current.firmware, crowded, {}, {}, rejected, rejectedReport, detail),
            "Flash indicator failed to predict rejected overflowing screens")) return false;
        // Menu is an outer native envelope: importing must first restore its
        // hooks, then decode the unchanged strict screen-image envelope.
        current.PushUndo("Enable device menu beside CHIP ID image");
        auto nativeMenuPreferences = disabledMenuPreferences;
        nativeMenuPreferences.enabled = true;
        current.SetMenuPatch(nativeMenuPreferences);
        current.EndUndoAction();
        std::vector<uint8_t> menuExport;
        if (!current.BuildToBuffer(menuExport, detail)) { error += detail; return false; }
        FirmwarePackage menuPackage;
        StudioState menuImported;
        if (!FirmwarePackage::Parse(menuExport, menuPackage, detail) ||
            !menuImported.LoadPackage(std::move(menuPackage), "menu-and-screen.zwdf", detail)) {
            error += detail; return false;
        }
        if (!check(menuImported.menuPatch == current.menuPatch &&
                   menuImported.originalMenuPatch == current.menuPatch &&
                   menuImported.screenPatches == current.screenPatches &&
                   menuImported.firmware.source_bytes == menuExport &&
                   !menuImported.HasUnsavedChanges() && !menuImported.CanUndo(),
                   "native menu import lost source identity, baseline, or inner screen image")) return false;
        std::vector<uint8_t> menuRoundtrip;
        if (!menuImported.BuildZwdfBytes(menuRoundtrip, detail)) { error += detail; return false; }
        if (!check(menuRoundtrip == menuExport, "menu-plus-screen import was not byte exact")) return false;
        const auto enabledMenuPath = root / L"menu-reversed.vfwp";
        const auto enabledMenuSaved = FirmwareProject::Save(menuImported, utf8(enabledMenuPath));
        if (!enabledMenuSaved.saved) { error += enabledMenuSaved.error; return false; }
        StudioState enabledMenuReopened;
        if (!FirmwareProject::Open(enabledMenuReopened, utf8(enabledMenuPath), detail) ||
            !enabledMenuReopened.BuildToBuffer(menuRoundtrip, detail)) { error += detail; return false; }
        if (!check(enabledMenuReopened.menuPatch == current.menuPatch &&
                   enabledMenuReopened.menuPatch.reverseNavigation && menuRoundtrip == menuExport &&
                   !enabledMenuReopened.HasUnsavedChanges(),
                   "project lost reversed native menu settings or exact exported bytes")) return false;
        menuImported.PushUndo("Disable imported device menu");
        menuImported.SetMenuPatch({false, false});
        menuImported.EndUndoAction();
        const auto menuPath = root / L"menu-and-screen.vfwp";
        const auto menuSaved = FirmwareProject::Save(menuImported, utf8(menuPath));
        if (!menuSaved.saved) { error += menuSaved.error; return false; }
        if (!check(menuImported.Undo() && menuImported.HasUnsavedChanges() &&
                   menuImported.menuPatch == current.menuPatch &&
                   menuImported.Redo() && !menuImported.HasUnsavedChanges(),
                   "menu-only save/undo/redo did not track the saved project baseline")) return false;
        StudioState menuReopened;
        if (!FirmwareProject::Open(menuReopened, utf8(menuPath), detail) ||
            !menuReopened.BuildToBuffer(menuRoundtrip, detail)) { error += detail; return false; }
        if (!check(menuReopened.menuPatch == FirmwareMenu::Settings{} &&
                   menuReopened.originalMenuPatch == current.menuPatch &&
                   menuReopened.screenPatches == current.screenPatches &&
                   menuReopened.firmware.source_bytes == menuExport &&
                   !menuReopened.HasUnsavedChanges(),
                   "project menu override was replaced by its imported source envelope")) return false;
        FirmwarePackage disabledMenuPackage;
        if (!FirmwarePackage::Parse(menuRoundtrip, disabledMenuPackage, detail)) { error += detail; return false; }
        for (const auto& record : disabledMenuPackage.records)
            if (!check(FirmwareMenu::StateAddress(record) == 0 &&
                       FirmwareLogo::FrameBoundary(record, FirmwareLogo::Target::DeviceInfo) != 0,
                       "disabling imported menu damaged inner screen envelope or retained native menu")) return false;
        FirmwarePackage menuOuterPackage;
        if (!FirmwarePackage::Parse(menuExport, menuOuterPackage, detail)) { error += detail; return false; }
        if (!check(disabledMenuPackage.records.size() == menuOuterPackage.records.size(),
                   "menu accounting lost a hardware record")) return false;
        for (size_t i = 0; i < disabledMenuPackage.records.size(); ++i) {
            const auto& inner = disabledMenuPackage.records[i];
            const size_t cost = FirmwareMenu::FlashBytes(inner, current.menuPatch);
            if (!check(cost != 0 && inner.image.size() + cost == menuOuterPackage.records[i].image.size() &&
                       FirmwareMenu::FlashBytes(inner, {}) == 0 &&
                       inner.linked_base + inner.image.size() + cost < 0x0801F000,
                       "menu FlashBytes differs from exact export size or protected capacity")) return false;
        }
        menuReopened.RevertAllAssets();
        if (!check(menuReopened.menuPatch == current.menuPatch && menuReopened.HasUnsavedChanges() &&
                   menuReopened.Undo() && menuReopened.menuPatch == FirmwareMenu::Settings{} &&
                   !menuReopened.HasUnsavedChanges(),
                   "restore-original/undo ignored imported menu baseline")) return false;
    }
    check(serviceRecordCount == 16, "CHIP ID project regression missed DRAG S hardware records");
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        check(entry.path().filename().wstring().find(L".tmp.") == std::wstring::npos, "project temporary file leaked");
    return error.empty();
}

// Focused consumer regression: project originals survive cropping, and metadata
// never changes firmware bytes in either current project storage codec.
inline bool RunEditableSourceRegressions(std::string& error, std::ostream* log) {
    error.clear();
    auto fail = [&](const char* message) { error = message; return false; };
    {
        const std::vector<uint32_t> original{0xff302010, 0xff906040, 0xffd0c0b0};
        auto adjusted = original;
        FirmwareBitmap::ImageAdjustments parameters;
        FirmwareBitmap::ApplyAdjustments(adjusted, 3, 1, parameters);
        if (!parameters.Valid()) return fail("Identity image corrections are invalid");
        if (adjusted != original) return fail("Neutral corrections changed image pixels");
        const auto originalStorage = adjusted.data();
        const auto originalCapacity = adjusted.capacity();
        FirmwareBitmap::ApplyAdjustments(adjusted, 3, 1, parameters);
        if (adjusted.data() != originalStorage || adjusted.capacity() != originalCapacity)
            return fail("Neutral corrections replaced image storage");
        for (size_t channel = 0; channel < 3; ++channel) {
            parameters = {};
            parameters.channelCurves[channel] = {0.05f, 0.25f, 0.55f};
            if (parameters == FirmwareBitmap::ImageAdjustments{})
                return fail("Channel curve was omitted from adjustment equality");
            adjusted = original;
            FirmwareBitmap::ApplyAdjustments(adjusted, 3, 1, parameters);
            const uint32_t mask = 255u << (channel * 8);
            bool changed = false;
            for (size_t i = 0; i < original.size(); ++i) {
                if ((adjusted[i] & ~mask) != (original[i] & ~mask))
                    return fail("Channel-only curve changed another channel or alpha");
                changed |= (adjusted[i] & mask) != (original[i] & mask);
            }
            if (!changed) return fail("Channel-only curve did not affect pixels");
            for (size_t point = 0; point < 3; ++point)
                for (float invalid : {-0.01f, 1.01f, std::numeric_limits<float>::infinity(),
                                     std::numeric_limits<float>::quiet_NaN()}) {
                    auto rejected = parameters;
                    rejected.channelCurves[channel][point] = invalid;
                    if (rejected.Valid()) return fail("Invalid channel curve point accepted");
                    adjusted = original;
                    FirmwareBitmap::ApplyAdjustments(adjusted, 3, 1, rejected);
                    if (adjusted != original) return fail("Invalid channel curve changed image");
                }
        }
        parameters = {};
        adjusted = original;
        FirmwareBitmap::ApplyAdjustments(adjusted, 3, 1, parameters);
        if (adjusted != original || parameters != FirmwareBitmap::ImageAdjustments{})
            return fail("Reset corrections did not restore all identity curves");
        // Check every input byte against scalar master/channel composition.
        auto curveAt = [](float value, const std::array<float, 3>& curve) {
            const float points[]{0, curve[0], curve[1], curve[2], 1};
            const int segment = (std::min)(int(value * 4), 3);
            return points[segment] + (points[segment + 1] - points[segment]) * (value * 4 - segment);
        };
        parameters.brightness = 7;
        parameters.contrast = -13;
        parameters.gamma = 1.4f;
        parameters.curve = {0.12f, 0.61f, 0.88f};
        for (bool channelCurves : {false, true}) {
            if (channelCurves)
                parameters.channelCurves = {{{0.1f, 0.3f, 0.7f}, {0.4f, 0.6f, 0.9f}, {0.2f, 0.4f, 0.6f}}};
            adjusted.resize(256);
            for (uint32_t value = 0; value < 256; ++value)
                adjusted[value] = 0x7f000000u | value | value << 8 | value << 16;
            FirmwareBitmap::ApplyAdjustments(adjusted, 256, 1, parameters);
            for (uint32_t value = 0; value < 256; ++value) {
                float master = (std::clamp)((float(value) / 255 - 0.5f) *
                    std::exp2(parameters.contrast / 50.0f) + 0.5f + parameters.brightness / 100, 0.0f, 1.0f);
                master = curveAt(std::pow(master, 1 / parameters.gamma), parameters.curve);
                uint32_t expected = 0x7f000000u;
                for (size_t channel = 0; channel < 3; ++channel) {
                    const float output = channelCurves ? curveAt(master, parameters.channelCurves[channel]) : master;
                    expected |= uint32_t(std::lround(output * 255)) << (channel * 8);
                }
                if (adjusted[value] != expected)
                    return fail("Master/channel composition or identity output changed");
            }
        }
        adjusted = original;
        parameters = {};
        parameters.brightness = 20;
        FirmwareBitmap::ApplyAdjustments(adjusted, 3, 1, parameters);
        if ((adjusted[0] & 255) <= (original[0] & 255))
            return fail("Brightness did not lift image shadows");
        parameters = {};
        parameters.saturation = -100;
        adjusted = original;
        FirmwareBitmap::ApplyAdjustments(adjusted, 3, 1, parameters);
        for (auto pixel : adjusted)
            if ((pixel & 255) != ((pixel >> 8) & 255) || (pixel & 255) != ((pixel >> 16) & 255))
                return fail("Desaturation did not produce neutral gray");
        for (uint32_t count : {16u, 64u, 256u}) {
            adjusted.resize(80 * 160);
            for (size_t i = 0; i < adjusted.size(); ++i)
                adjusted[i] = 0xff000000u | (uint32_t(i * 619) & 0xffffffu);
            parameters = {};
            parameters.paletteColors = count;
            FirmwareBitmap::ApplyAdjustments(adjusted, 80, 160, parameters);
            auto palette = adjusted;
            std::sort(palette.begin(), palette.end());
            const auto unique = std::unique(palette.begin(), palette.end()) - palette.begin();
            if (unique > count) return fail("Palette reduction exceeded requested color count");
        }
    }
    std::error_code ec;
    auto root = std::filesystem::temp_directory_path(ec);
    if (ec) return fail("editable source temp directory unavailable");
    root /= L"Voopoo_editable_sources_" + std::to_wstring(GetCurrentProcessId()) +
        L"_" + std::to_wstring(GetTickCount64());
    if (!std::filesystem::create_directory(root, ec) || ec)
        return fail("editable source temp directory creation failed");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
    } cleanup{root};
    auto utf8 = [](const std::filesystem::path& path) { return PathUtils::WideToUtf8(path.wstring()); };
    FirmwarePackage package;
    StudioState studio;
    if (!LoadEmbeddedFirmware(package, error) ||
        !studio.LoadPackage(std::move(package), "editable-source.zwdf", error)) return false;
    studio.EnableProjectDocument();
    FirmwareLogo::EditableSource source;
    source.width = 173; source.height = 217;
    source.fitMode = 2; source.scalePercent = 150;
    source.x = -17; source.y = -29;
    source.alignX = source.alignY = -1;
    source.resampling = FirmwareBitmap::Resampling::Bicubic;
    source.adjustments.brightness = 13;
    source.adjustments.contrast = -9;
    source.adjustments.gamma = 1.3f;
    source.adjustments.saturation = 17;
    source.adjustments.sharpness = 42;
    source.adjustments.curve = {0.19f, 0.57f, 0.83f};
    source.adjustments.channelCurves = {{{0.12f, 0.43f, 0.71f}, {0.27f, 0.61f, 0.91f}, {0.08f, 0.39f, 0.69f}}};
    source.adjustments.paletteColors = 64;
    std::vector<uint8_t> original(size_t(source.width) * source.height * 4);
    for (int y = 0; y < source.height; ++y)
        for (int x = 0; x < source.width; ++x) {
            const size_t pixel = (size_t(y) * source.width + x) * 4;
            original[pixel] = uint8_t(x * 7 + y);
            original[pixel + 1] = uint8_t(x + y * 3);
            original[pixel + 2] = uint8_t(x * 11 + y * 5);
            original[pixel + 3] = uint8_t(55 + (x + y) % 201);
        }
    source.rgba = std::make_shared<const std::vector<uint8_t>>(std::move(original));
    std::vector<uint32_t> crop;
    FirmwareBitmap::ResizeRgbaOnBlack(source.rgba->data(), source.width, source.height,
        120, 240, source.resampling, crop, -source.x, -source.y,
        FirmwareLogo::Width, FirmwareLogo::Height);
    FirmwareBitmap::ApplyAdjustments(crop, FirmwareLogo::Width, FirmwareLogo::Height, source.adjustments);
    FirmwareLogo::Settings settings;
    settings.enabled = true;
    if (!FirmwareBitmap::EncodeRgb565(crop.data(), FirmwareLogo::Width, FirmwareLogo::Height, settings.pixels))
        return fail("editable source crop encoding failed");
    studio.PushUndo("Apply original with cropped placement");
    studio.SetScreenPatch(FirmwareLogo::Target::Boot, settings);
    studio.SetScreenPatchSource(FirmwareLogo::Target::Boot, source);
    for (size_t i = 1; i < FirmwareLogo::TargetCount; ++i) {
        auto other = source;
        other.fitMode = int(i) - 1;
        other.scalePercent = 75.5f + float(i);
        other.x = int(i) * 13; other.y = -int(i) * 7;
        other.alignX = int(i) - 1; other.alignY = 2;
        other.resampling = static_cast<FirmwareBitmap::Resampling>(i - 1);
        studio.SetScreenPatchSource(static_cast<FirmwareLogo::Target>(i), std::move(other));
    }
    studio.EndUndoAction();
    const auto expectedSources = studio.screenPatchSources;
    if (!studio.HasUnsavedChanges() || studio.screenPatches[0] != settings ||
        studio.screenPatchSources[0] != source)
        return fail("editable source apply did not commit original and placement");
    std::vector<uint8_t> expectedFirmware;
    if (!studio.BuildToBuffer(expectedFirmware, error)) return false;
    const auto path = root / L"originals.vfwp";
    const auto result = FirmwareProject::Save(studio, utf8(path));
    if (!result.saved) { error = result.error; return false; }
    StudioState reopened;
    if (!FirmwareProject::Open(reopened, utf8(path), error)) return false;
    std::vector<uint8_t> actualFirmware;
    if (!reopened.BuildToBuffer(actualFirmware, error)) return false;
    if (reopened.screenPatchSources != expectedSources || reopened.screenPatches != studio.screenPatches ||
        !reopened.screenPatchSources[0].rgba || *reopened.screenPatchSources[0].rgba != *source.rgba ||
        reopened.HasUnsavedChanges() || reopened.CanUndo() || actualFirmware != expectedFirmware)
        return fail("editable source reopen lost full RGBA/placement or changed firmware bytes");
    // Native frame snapshots deliberately exclude the original project images.
    if (reopened.CreateEmulationSnapshot()->screenPatchSources != FirmwareLogo::EditableSources{})
        return fail("native snapshot retained unnecessary original images");
    auto moved = reopened.screenPatchSources[0];
    moved.x -= 3; moved.scalePercent = 183.25f;
    moved.adjustments.channelCurves[1][1] = 0.37f;
    const auto savedRevision = reopened.ResourceRevision();
    reopened.PushUndo("Original placement metadata only");
    reopened.SetScreenPatchSource(FirmwareLogo::Target::Boot, moved);
    reopened.EndUndoAction();
    if (!reopened.HasUnsavedChanges() || reopened.ResourceRevision() == savedRevision ||
        reopened.screenPatches != studio.screenPatches ||
        reopened.screenPatchSources[0].rgba != moved.rgba)
        return fail("metadata-only edit did not update dirty/revision or retained pixels incorrectly");
    if (!reopened.BuildToBuffer(actualFirmware, error)) return false;
    if (actualFirmware != expectedFirmware)
        return fail("editable metadata changed exported firmware");
    if (!reopened.Undo() || reopened.HasUnsavedChanges() || reopened.screenPatchSources != expectedSources ||
        !reopened.Redo() || !reopened.HasUnsavedChanges() || reopened.screenPatchSources[0] != moved ||
        !reopened.Undo() || reopened.HasUnsavedChanges())
        return fail("editable source undo/redo did not restore saved baseline");
    auto changedPixels = std::make_shared<std::vector<uint8_t>>(*source.rgba);
    changedPixels->back() ^= 1; // Original pixels outside the composed 80x160 crop.
    auto changedSource = reopened.screenPatchSources[0];
    changedSource.rgba = changedPixels;
    reopened.PushUndo("Change off-canvas original pixel");
    reopened.SetScreenPatchSource(FirmwareLogo::Target::Boot, changedSource);
    reopened.EndUndoAction();
    if (!reopened.HasUnsavedChanges() || !reopened.Undo() || reopened.HasUnsavedChanges() ||
        !reopened.Redo() || !reopened.HasUnsavedChanges() || !reopened.Undo())
        return fail("off-canvas original bytes were omitted from dirty/history");
    reopened.RevertAllAssets();
    if (reopened.screenPatchSources != FirmwareLogo::EditableSources{} ||
        !reopened.Undo() || reopened.screenPatchSources != expectedSources || reopened.HasUnsavedChanges())
        return fail("restore original did not undo editable sources as one action");
    // Capture a nonempty source in an inverse delta: its shared payload must
    // still count against the history budget, even for a placement-only edit.
    reopened.PushUndo("Capture source payload for history accounting");
    reopened.SetScreenPatchSource(FirmwareLogo::Target::Boot, moved);
    reopened.EndUndoAction();
    if (reopened.HistoryMemoryBytes() < source.rgba->size())
        return fail("history accounting omitted original RGBA payload");
    if (!reopened.Undo() || reopened.HasUnsavedChanges())
        return fail("source accounting probe did not restore saved placement");

    // Independently decode the full current body and reopen it stored, retaining
    // all three original images, corrections, channel curves, screens, and menu.
    std::vector<uint8_t> encoded;
    if (!FirmwareProject::Build(studio, encoded, error)) return false;
    auto u32 = [](const std::vector<uint8_t>& bytes, size_t at) {
        uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i) value |= uint32_t(bytes[at + i]) << (i * 8);
        return value;
    };
    auto u64 = [](const std::vector<uint8_t>& bytes, size_t at) {
        uint64_t value = 0;
        for (unsigned i = 0; i < 8; ++i) value |= uint64_t(bytes[at + i]) << (i * 8);
        return value;
    };
    auto patch32 = [](std::vector<uint8_t>& bytes, size_t at, uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) bytes[at + i] = uint8_t(value >> (i * 8));
    };
    auto patch64 = [](std::vector<uint8_t>& bytes, size_t at, uint64_t value) {
        for (unsigned i = 0; i < 8; ++i) bytes[at + i] = uint8_t(value >> (i * 8));
    };
    const size_t bodySize = static_cast<size_t>(u64(encoded, 24) + u64(encoded, 32));
    std::vector<uint8_t> stored(encoded.begin(), encoded.begin() + 112);
    stored.resize(112 + bodySize);
    if (u32(encoded, 108) == 1) {
        uint64_t limit = 64 * 1024 * 1024;
        size_t in = 112, out = 112;
        if (lzma_stream_buffer_decode(&limit, 0, nullptr, encoded.data(), &in, encoded.size(),
                stored.data(), &out, stored.size()) != LZMA_OK ||
            in != encoded.size() || out != stored.size())
            return fail("editable source independent XZ decode failed");
    } else std::copy(encoded.begin() + 112, encoded.end(), stored.begin() + 112);
    auto repairStored = [&](std::vector<uint8_t>& bytes) {
        patch32(bytes, 108, 0);
        patch64(bytes, 16, bytes.size());
        patch64(bytes, 32, bytes.size() - 112 - static_cast<size_t>(u64(bytes, 24)));
        patch32(bytes, 104, 0);
        uint32_t crc = 0xffffffffu;
        for (const uint8_t byte : bytes) {
            crc ^= byte;
            for (unsigned i = 0; i < 8; ++i) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
        }
        patch32(bytes, 104, ~crc);
    };
    repairStored(stored);
    AtomicFile::Error fileError;
    const auto storedPath = root / L"originals-stored.vfwp";
    if (!AtomicFile::Write(storedPath, stored.data(), stored.size(), false, fileError))
        return fail("editable source stored fixture write failed");
    StudioState storedImported;
    if (!FirmwareProject::Open(storedImported, utf8(storedPath), error) ||
        !storedImported.BuildToBuffer(actualFirmware, error)) return false;
    if (storedImported.screenPatchSources != expectedSources ||
        storedImported.screenPatches != studio.screenPatches || storedImported.menuPatch != studio.menuPatch ||
        storedImported.firmware.source_bytes != studio.firmware.source_bytes ||
        actualFirmware != expectedFirmware || storedImported.HasUnsavedChanges() ||
        storedImported.CanUndo() || storedImported.CanRedo())
        return fail("stored current project lost originals, corrections, screens, menu, or firmware equivalence");
    if (log) *log << "Editable sources: compressed/stored current projects, full 173x217 RGBA, "
        "all placements, crop/firmware equivalence, master/R/G/B curves, channel validation, "
        "metadata/off-canvas undo and dirty state, source restore passed\n";
    return true;
}
