#pragma once

#include "atomic_file.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct StudioState;

namespace FirmwareProject {

inline constexpr uint32_t Version = 1;
inline constexpr size_t MaximumFileBytes = 128 * 1024 * 1024;
inline constexpr size_t MaximumDecodedBodyBytes = MaximumFileBytes - 112;

struct SaveResult {
    bool saved = false;
    AtomicFile::Error fileError;
    std::string error;
};

bool IsProjectPath(const std::string& utf8Path);
// Serializes original firmware, staged edits, and original decoded RGBA images
// with placement and adjustments using XZ preset 6/CRC64 when smaller; never builds firmware.
// Optional observers run synchronously on the caller's thread at real stage boundaries.
bool Build(const StudioState& studio, std::vector<uint8_t>& bytes, std::string& error,
           const std::function<void(const char*)>& progress = {});
SaveResult Save(StudioState& studio, const std::string& utf8Path,
                const std::function<void(const char*)>& progress = {});
// Failure never changes the live document, its history, or its saved baseline.
bool Open(StudioState& studio, const std::string& utf8Path, std::string& error,
          const std::function<void(const char*)>& progress = {});

} // namespace FirmwareProject
