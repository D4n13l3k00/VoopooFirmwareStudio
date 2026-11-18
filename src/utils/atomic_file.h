#pragma once

#include "path_utils.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>

namespace AtomicFile {

enum class Stage { None, Identity, Temporary, Write, Flush, Close, Backup, Commit };
struct Error {
    Stage stage = Stage::None;
    DWORD code = ERROR_SUCCESS;
};

class Handle {
public:
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

inline bool SameFile(HANDLE a, HANDLE b, bool& same, Error& error) {
    BY_HANDLE_FILE_INFORMATION ai{}, bi{};
    if (!GetFileInformationByHandle(a, &ai) || !GetFileInformationByHandle(b, &bi)) {
        error = {Stage::Identity, GetLastError()};
        return false;
    }
    same = ai.dwVolumeSerialNumber == bi.dwVolumeSerialNumber &&
           ai.nFileIndexHigh == bi.nFileIndexHigh && ai.nFileIndexLow == bi.nFileIndexLow;
    return true;
}

// Protected handles stay open without write/delete sharing through the rename.
// This also prevents a source alias being substituted between check and commit.
inline bool Write(const std::filesystem::path& destination, const void* data, size_t size,
                  bool backup, Error& error, const std::filesystem::path& protectedSource = {},
                  const std::filesystem::path& protectedOutput = {},
                  const std::function<void(const char*)>& progress = {}) {
    error = {};
    if (progress) progress("Check safe-write destination and protected source identity");
    Handle source, output;
    const std::filesystem::path* protectedPaths[] = {&protectedSource, &protectedOutput};
    Handle* protectedHandles[] = {&source, &output};
    for (size_t i = 0; i < 2; ++i) {
        if (protectedPaths[i]->empty()) continue;
        protectedHandles[i]->value = CreateFileW(protectedPaths[i]->c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (protectedHandles[i]->value == INVALID_HANDLE_VALUE) {
            error = {Stage::Identity, GetLastError()};
            return false;
        }
    }

    Handle existing;
    existing.value = CreateFileW(destination.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    const bool exists = existing.value != INVALID_HANDLE_VALUE;
    if (!exists) {
        const DWORD code = GetLastError();
        if (code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND) {
            error = {Stage::Identity, code};
            return false;
        }
    } else {
        for (const auto* handle : protectedHandles) {
            if (handle->value == INVALID_HANDLE_VALUE) continue;
            bool same = false;
            if (!SameFile(existing.value, handle->value, same, error)) return false;
            if (same) {
                error = {Stage::Identity, ERROR_ACCESS_DENIED};
                return false;
            }
        }
    }

    if (progress) progress("Create same-directory temporary file");
    static std::atomic<unsigned long> sequence{0};
    std::filesystem::path temporary;
    Handle file;
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        temporary = destination;
        temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." +
                     std::to_wstring(sequence.fetch_add(1));
        file.value = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file.value != INVALID_HANDLE_VALUE) break;
        const DWORD code = GetLastError();
        if (code != ERROR_FILE_EXISTS && code != ERROR_ALREADY_EXISTS) {
            error = {Stage::Temporary, code};
            return false;
        }
    }
    if (file.value == INVALID_HANDLE_VALUE) {
        error = {Stage::Temporary, ERROR_FILE_EXISTS};
        return false;
    }
    struct RemoveTemporary {
        const std::filesystem::path& path;
        Handle& file;
        ~RemoveTemporary() {
            if (file.value != INVALID_HANDLE_VALUE) {
                CloseHandle(file.value);
                file.value = INVALID_HANDLE_VALUE;
            }
            DeleteFileW(path.c_str());
        }
    } removeTemporary{temporary, file};
    auto fail = [&](Stage stage, DWORD code) {
        if (file.value != INVALID_HANDLE_VALUE) {
            CloseHandle(file.value);
            file.value = INVALID_HANDLE_VALUE;
        }
        error = {stage, code};
        return false;
    };
    if (progress) progress("Write temporary file");
    const auto* bytes = static_cast<const uint8_t*>(data);
    size_t offset = 0;
    while (offset < size) {
        const DWORD chunk = static_cast<DWORD>((std::min)(size - offset,
            static_cast<size_t>((std::numeric_limits<DWORD>::max)())));
        DWORD written = 0;
        if (!WriteFile(file.value, bytes + offset, chunk, &written, nullptr))
            return fail(Stage::Write, GetLastError());
        if (written == 0) return fail(Stage::Write, ERROR_WRITE_FAULT);
        offset += written;
    }
    if (progress) progress("Flush temporary file to disk");
    if (!FlushFileBuffers(file.value)) return fail(Stage::Flush, GetLastError());
    if (progress) progress("Close temporary file");
    const HANDLE closing = file.value;
    file.value = INVALID_HANDLE_VALUE;
    if (!CloseHandle(closing)) return fail(Stage::Close, GetLastError());

    if (backup && exists) {
        if (progress) progress("Back up existing destination");
        auto backupPath = destination;
        backupPath += L".bak." + PathUtils::Utf8ToWide(PathUtils::FormatTimestampNow()) + L"." +
                      std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(sequence.fetch_add(1));
        // Never overwrite an earlier backup; any backup failure aborts the save.
        if (!CopyFileW(destination.c_str(), backupPath.c_str(), TRUE))
            return fail(Stage::Backup, GetLastError());
    }
    // Windows replacement requires closing our destination inspection handle.
    // Keep protected source/output handles open so their aliases remain locked.
    if (existing.value != INVALID_HANDLE_VALUE) {
        const HANDLE inspected = existing.value;
        existing.value = INVALID_HANDLE_VALUE;
        if (!CloseHandle(inspected)) return fail(Stage::Close, GetLastError());
    }
    // Same-directory rename never truncates the destination. Do not fall back to
    // copying/deleting if replacement is denied (locks, read-only files, etc.).
    if (progress) progress("Atomically replace destination");
    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                     (exists ? MOVEFILE_REPLACE_EXISTING : 0) | MOVEFILE_WRITE_THROUGH))
        return fail(Stage::Commit, GetLastError());
    return true;
}

} // namespace AtomicFile
