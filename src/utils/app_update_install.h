#pragma once
#include "app_update.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace AppUpdate {
struct InstallRequest {
    std::filesystem::path target;
    std::filesystem::path stagedFile;
    std::string sha256;
    uint64_t size = 0;
    DWORD parentPid = 0;
    bool relaunch = true;
};
// Writes a fixed local PowerShell helper and a data-only manifest; no shell
// interpolation of release text or paths. Used by installation and isolated proofs.
bool PrepareInstall(const InstallRequest&, std::filesystem::path& script,
    std::filesystem::path& manifest, std::string& error);
// Caller confirms unsaved work first, releases Client's stage ownership only
// after successful launch, then exits through the normal message loop.
bool StartInstall(const Status&, std::string& error);
}
