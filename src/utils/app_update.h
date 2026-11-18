#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#ifndef STUDIO_VERSION
#define STUDIO_VERSION "2.0.0"
#endif

namespace AppUpdate {
inline constexpr std::string_view Version = STUDIO_VERSION;
struct Release {
    std::string tag;
    std::string notes;
    std::string url;
    std::string assetUrl;
    std::string sha256;
    uint64_t size = 0;
};
enum class Phase { Idle, Checking, Current, Available, Downloading, Ready, Failed };
struct Status {
    Phase phase = Phase::Idle;
    Release release;
    std::filesystem::path stagedFile;
    std::string error;
};
// Strict public-release parsing and semantic numeric version comparison.
bool IsNewerVersion(std::string_view candidate, std::string_view current);
bool ParseRelease(std::string_view json, Release& release, std::string& error);
bool DownloadRelease(const Release&, std::filesystem::path& stagedFile, std::string& error);
bool VerifyFile(const std::filesystem::path&, std::string_view sha256, uint64_t size, std::string& error);
class Client {
public:
    Client();
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    bool Check();
    bool Download();
    void Poll();
    const Status& Get() const;
    // Installer already owns a verified private copy: remove the original stage.
    void ReleaseStage();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
