#pragma once
#include "app_update.h"

inline bool RunAppUpdateRegressions(std::string& error) {
    using namespace AppUpdate;
    error.clear();
    auto check = [&](bool ok, const char* message) {
        if (!ok) { error += message; error += "; "; }
    };
    check(IsNewerVersion("v1.10.0", "1.9.99") &&
        IsNewerVersion("2.0.0", "v1.99.99") &&
        !IsNewerVersion("v1.1.0", "1.1.0") &&
        !IsNewerVersion("1.0.9", "1.1.0"), "numeric version ordering failed");
    for (const auto invalid : {"v1.2.0-beta", "1.02.0", "1.2", "1.2.3.4", "1.2.-3"})
        check(!IsNewerVersion(invalid, "1.0.0"), "unstable or malformed version accepted");
    const std::string prefix = R"({"tag_name":"v2.10.0","draft":false,"prerelease":false,"html_url":"https://github.com/D4n13l3k00/VoopooFirmwareStudio/releases/tag/v2.10.0","body":"Строка\n\"кавычки\"","assets":[)";
    const std::string asset = R"({"name":"VoopooFirmwareStudio-v2.10.0-Release.exe","browser_download_url":"https://github.com/D4n13l3k00/VoopooFirmwareStudio/releases/download/v2.10.0/VoopooFirmwareStudio-v2.10.0-Release.exe","digest":"sha256:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA","size":12345})";
    const auto valid = prefix + asset + "]}";
    Release release;
    std::string reason;
    if (!ParseRelease(valid, release, reason)) {
        error = "Valid official release rejected: " + reason;
        return false;
    }
    check(release.notes == "Строка\n\"кавычки\"" && release.sha256 == std::string(64, 'a'),
        "release notes Unicode/escaping or digest normalization failed");
    auto reject = [&](const std::string& json, const char* message) {
        const auto tag = release.tag;
        reason.clear();
        check(!ParseRelease(json, release, reason) && !reason.empty() && release.tag == tag, message);
    };
    auto change = [&](std::string_view from, std::string_view to) {
        auto json = valid;
        const auto at = json.find(from);
        json.replace(at, from.size(), to);
        return json;
    };
    reject(change("\"draft\":false", "\"draft\":true"), "draft release accepted");
    reject(change("\"prerelease\":false", "\"prerelease\":true"), "prerelease accepted");
    reject(change("https://github.com/", "https://github.com.evil.invalid/"), "unofficial release page accepted");
    reject(change("\"browser_download_url\":\"https://", "\"browser_download_url\":\"http://"), "insecure executable URL accepted");
    reject(change("sha256:", "sha512:"), "unsupported asset digest accepted");
    reject(change("\"size\":12345", "\"size\":268435457"), "oversized asset accepted");
    reject(change("\"size\":12345", "\"size\":1.5"), "fractional asset size accepted");
    reject(change("\"size\":12345", "\"size\":\"12345\""), "incorrect JSON field type accepted");
    reject(prefix + asset + "," + asset + "]}", "ambiguous duplicate executable accepted");
    reject("{", "malformed JSON accepted");
    return error.empty();
}
