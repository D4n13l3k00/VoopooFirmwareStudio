#include "studio_preferences.h"
#include "i18n.h"
#include <windows.h>
#include <charconv>
#include <string_view>
#include <vector>

namespace StudioPreferences {
namespace {
Preferences current;
constexpr const char* SectionName = "StudioPreferences";
struct IntegerField { const char* key; int Preferences::*member; int minimum, maximum; };
constexpr IntegerField Integers[]{
    {"LogReviewSeconds", &Preferences::logReviewSeconds, 1, 30},
    {"DefaultPatchTarget", &Preferences::defaultPatchTarget, 0, 2},
    {"DefaultBootStages", &Preferences::defaultBootStages, 1, 3},
    {"PatchZoom", &Preferences::patchZoom, MinimumZoom, MaximumZoom},
    {"EmulatorZoom", &Preferences::emulatorZoom, MinimumZoom, MaximumZoom},
    {"DeviceZoom", &Preferences::deviceZoom, MinimumZoom, MaximumZoom},
    {"LayoutZoom", &Preferences::layoutZoom, MinimumZoom, MaximumZoom},
    {"MenuZoom", &Preferences::menuZoom, MinimumZoom, MaximumZoom},
    {"ComparisonZoom", &Preferences::comparisonZoom, MinimumZoom, MaximumZoom},
    {"ResourceZoom", &Preferences::resourceZoom, MinimumZoom, ResourceMaximumZoom},
    {"FontZoom", &Preferences::fontZoom, MinimumZoom, MaximumZoom}
};
int ReadInteger(const std::string& path, const char* key, int fallback, int minimum, int maximum) {
    char buffer[128]{};
    const DWORD count = GetPrivateProfileStringA(SectionName, key, "", buffer, sizeof(buffer), path.c_str());
    if (!count || count == sizeof(buffer) - 1) return fallback;
    std::string_view text(buffer, count);
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) text.remove_suffix(1);
    int value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
        value >= minimum && value <= maximum ? value : fallback;
}
Preferences Validated(Preferences value) {
    const Preferences defaults;
    for (const auto& field : Integers)
        if (value.*field.member < field.minimum || value.*field.member > field.maximum)
            value.*field.member = defaults.*field.member;
    return value;
}
}
Preferences& Get() { return current; }
void Reset() { current = {}; }
void Load(const std::string& requestedPath) {
    const auto path = requestedPath.empty() ? i18n::GetConfigPath() : requestedPath;
    Preferences loaded;
    loaded.smoothAnimations = ReadInteger(path, "SmoothAnimations", loaded.smoothAnimations, 0, 1) != 0;
    for (const auto& field : Integers)
        loaded.*field.member = ReadInteger(path, field.key, loaded.*field.member, field.minimum, field.maximum);
    for (size_t index = 0; index < SectionIds.size(); ++index)
        loaded.sectionOpen[index] = ReadInteger(path, SectionIds[index], loaded.sectionOpen[index], 0, 1) != 0;
    current = loaded;
}
bool Save(const std::string& requestedPath) {
    const auto requested = requestedPath.empty() ? i18n::GetConfigPath() : requestedPath;
    const DWORD length = GetFullPathNameA(requested.c_str(), 0, nullptr, nullptr);
    if (!length) return false;
    std::vector<char> full(length);
    char* name = nullptr;
    if (!GetFullPathNameA(requested.c_str(), length, full.data(), &name) || !name) return false;
    const std::string path(full.data());
    const std::string directory(full.data(), name);
    char temporary[MAX_PATH]{};
    if (!GetTempFileNameA(directory.c_str(), "vfp", 0, temporary)) return false;
    struct Cleanup {
        const char* path;
        ~Cleanup() { DeleteFileA(path); }
    } cleanup{temporary};
    const DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        if ((attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY)) ||
            !CopyFileA(path.c_str(), temporary, FALSE)) return false;
    } else if (GetLastError() != ERROR_FILE_NOT_FOUND) return false;
    const Preferences value = Validated(current);
    const auto put = [&](const char* key, int number) {
        const auto text = std::to_string(number);
        return WritePrivateProfileStringA(SectionName, key, text.c_str(), temporary) != FALSE;
    };
    if (!put("SmoothAnimations", value.smoothAnimations)) return false;
    for (const auto& field : Integers)
        if (!put(field.key, value.*field.member)) return false;
    for (size_t index = 0; index < SectionIds.size(); ++index)
        if (!put(SectionIds[index], value.sectionOpen[index])) return false;
    // The cache-flush form has no meaningful success return. Durability is
    // checked explicitly below with FlushFileBuffers before the atomic replace.
    WritePrivateProfileStringA(nullptr, nullptr, nullptr, temporary);
    const HANDLE file = CreateFileA(temporary, GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    const bool flushed = FlushFileBuffers(file) != FALSE;
    const bool closed = CloseHandle(file) != FALSE;
    if (!flushed || !closed) return false;
    return MoveFileExA(temporary, path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}
} // namespace StudioPreferences
