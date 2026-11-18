#include "app_update.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <sddl.h>
#include <winhttp.h>
#include <roapi.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <limits>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace AppUpdate {
namespace {
constexpr uint64_t MaxAssetSize = 256ull * 1024 * 1024;
constexpr uint64_t MaxJsonSize = 4ull * 1024 * 1024;
constexpr int NetworkTimeoutMs = 5000;
constexpr std::string_view Repository = "https://github.com/D4n13l3k00/VoopooFirmwareStudio";
constexpr std::wstring_view LatestUrl = L"https://api.github.com/repos/D4n13l3k00/VoopooFirmwareStudio/releases/latest";
using Clock = std::chrono::steady_clock;
using VersionParts = std::array<std::string_view, 3>;

bool ParseVersion(std::string_view text, VersionParts& parts) {
    if (!text.empty() && text.front() == 'v') text.remove_prefix(1);
    for (size_t i = 0; i < parts.size(); ++i) {
        const size_t dot = text.find('.');
        if ((i < 2) != (dot != std::string_view::npos)) return false;
        const auto part = text.substr(0, dot);
        if (part.empty() || (part.size() > 1 && part.front() == '0')) return false;
        for (const char ch : part) if (ch < '0' || ch > '9') return false;
        parts[i] = part;
        if (i < 2) text.remove_prefix(dot + 1);
    }
    return true;
}

int HexDigit(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

bool ParseDigest(std::string_view text, std::array<UCHAR, 32>& bytes) {
    if (text.size() != bytes.size() * 2) return false;
    for (size_t i = 0; i < bytes.size(); ++i) {
        const int hi = HexDigit(text[i * 2]);
        const int lo = HexDigit(text[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        bytes[i] = static_cast<UCHAR>((hi << 4) | lo);
    }
    return true;
}

std::string AssetName(std::string_view tag) {
    return "VoopooFirmwareStudio-" + std::string(tag) + "-Release.exe";
}

bool ValidateRelease(const Release& release, std::string& error) {
    VersionParts parts;
    std::array<UCHAR, 32> digest;
    if (!ParseVersion(release.tag, parts)) error = "Release tag is not a stable numeric semantic version.";
    else if (release.url != std::string(Repository) + "/releases/tag/" + release.tag)
        error = "Release page is not the exact official repository/tag HTTPS URL.";
    else if (release.assetUrl != std::string(Repository) + "/releases/download/" + release.tag + "/" + AssetName(release.tag))
        error = "Release asset is not the exact official repository/tag HTTPS download URL.";
    else if (!ParseDigest(release.sha256, digest)) error = "Release asset requires a 64-digit SHA-256 digest.";
    else if (release.size == 0 || release.size > MaxAssetSize) error = "Release asset size must be between 1 byte and 256 MiB.";
    else return true;
    return false;
}

std::string WindowsError(std::string_view operation, DWORD code = GetLastError()) {
    return std::string(operation) + " (Windows error " + std::to_string(code) + ").";
}

std::wstring Wide(std::string_view text) {
    if (text.empty()) return {};
    if (text.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
        throw std::runtime_error("UTF-8 text is too long.");
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!length) throw std::runtime_error("Release JSON contains invalid UTF-8.");
    std::wstring result(static_cast<size_t>(length), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), length))
        throw std::runtime_error("Cannot decode release UTF-8.");
    return result;
}

std::string Utf8(std::wstring_view text) {
    if (text.empty()) return {};
    if (text.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
        throw std::runtime_error("Release string is too long.");
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (!length) throw std::runtime_error("Release JSON contains an invalid Unicode string.");
    std::string result(static_cast<size_t>(length), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), length, nullptr, nullptr))
        throw std::runtime_error("Cannot encode release UTF-8.");
    return result;
}

struct Apartment {
    HRESULT result = RoInitialize(RO_INIT_MULTITHREADED);
    Apartment() { if (result != RPC_E_CHANGED_MODE) winrt::check_hresult(result); }
    ~Apartment() { if (SUCCEEDED(result)) RoUninitialize(); }
};

using namespace winrt::Windows::Data::Json;
IJsonValue Required(const JsonObject& object, const wchar_t* key, JsonValueType type) {
    if (!object.HasKey(key)) throw std::runtime_error("Required release field is missing: " + Utf8(key) + ".");
    auto value = object.GetNamedValue(key);
    if (value.ValueType() != type) throw std::runtime_error("Release field has the wrong JSON type: " + Utf8(key) + ".");
    return value;
}

std::string StringField(const JsonObject& object, const wchar_t* key) {
    const auto value = Required(object, key, JsonValueType::String).GetString();
    return Utf8(std::wstring_view(value.c_str(), value.size()));
}

struct FileHandle {
    HANDLE value = INVALID_HANDLE_VALUE;
    FileHandle() = default;
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    ~FileHandle() { if (value != INVALID_HANDLE_VALUE && value != nullptr) CloseHandle(value); }
    bool Close() {
        const HANDLE closing = std::exchange(value, INVALID_HANDLE_VALUE);
        return CloseHandle(closing) != FALSE;
    }
};

struct InternetHandle {
    HINTERNET value = nullptr;
    explicit InternetHandle(HINTERNET handle) : value(handle) {}
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
};

void RemoveStage(const std::filesystem::path& file) noexcept {
    if (file.empty()) return;
    DeleteFileW(file.c_str());
    // Never recurse into a directory that another process has populated.
    try {
        RemoveDirectoryW(file.parent_path().c_str());
    } catch (...) {
        // Cleanup remains nonthrowing, including during allocation failure.
    }
}

struct StageGuard {
    std::filesystem::path file;
    bool owned = true;
    ~StageGuard() { if (owned) RemoveStage(file); }
};

bool Cancelled(std::stop_token stop, std::string& error) {
    if (!stop.stop_requested()) return false;
    error = "Update operation cancelled.";
    return true;
}

bool VerifyHandle(HANDLE file, std::string_view sha256, uint64_t size, std::stop_token stop, std::string& error) {
    std::array<UCHAR, 32> expected;
    if (!ParseDigest(sha256, expected) || size == 0 || size > MaxAssetSize) {
        error = "Invalid expected SHA-256 digest or asset size.";
        return false;
    }
    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(file, &information)) {
        error = WindowsError("Cannot inspect staged file");
        return false;
    }
    if ((information.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
        error = "Staged payload must be a regular file, not a directory or reparse point.";
        return false;
    }
    LARGE_INTEGER actualSize{};
    if (!GetFileSizeEx(file, &actualSize)) {
        error = WindowsError("Cannot read staged file size");
        return false;
    }
    if (actualSize.QuadPart < 0 || static_cast<uint64_t>(actualSize.QuadPart) != size) {
        error = "Downloaded file size does not match the release asset.";
        return false;
    }
    LARGE_INTEGER start{};
    if (!SetFilePointerEx(file, start, nullptr, FILE_BEGIN)) {
        error = WindowsError("Cannot rewind staged file");
        return false;
    }
    struct Algorithm {
        BCRYPT_ALG_HANDLE value = nullptr;
        ~Algorithm() { if (value) BCryptCloseAlgorithmProvider(value, 0); }
    } algorithm;
    if (BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        error = "Cannot open the Windows SHA-256 provider.";
        return false;
    }
    DWORD objectSize = 0, returned = 0;
    if (BCryptGetProperty(algorithm.value, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &returned, 0) < 0 ||
        returned != sizeof(objectSize) || objectSize == 0) {
        error = "Cannot obtain the SHA-256 working buffer size.";
        return false;
    }
    std::vector<UCHAR> object(objectSize);
    struct Hash {
        BCRYPT_HASH_HANDLE value = nullptr;
        ~Hash() { if (value) BCryptDestroyHash(value); }
    } hash;
    if (BCryptCreateHash(algorithm.value, &hash.value, object.data(), objectSize, nullptr, 0, 0) < 0) {
        error = "Cannot initialize SHA-256 verification.";
        return false;
    }
    std::array<UCHAR, 64 * 1024> buffer;
    uint64_t total = 0;
    for (;;) {
        if (Cancelled(stop, error)) return false;
        DWORD read = 0;
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
            error = WindowsError("Cannot read staged file for verification");
            return false;
        }
        if (!read) break;
        if (read > size - total) {
            error = "Staged file grew during verification.";
            return false;
        }
        total += read;
        if (BCryptHashData(hash.value, buffer.data(), read, 0) < 0) {
            error = "Cannot hash staged file.";
            return false;
        }
    }
    std::array<UCHAR, 32> actual{};
    if (BCryptFinishHash(hash.value, actual.data(), static_cast<ULONG>(actual.size()), 0) < 0) {
        error = "Cannot finish SHA-256 verification.";
        return false;
    }
    if (total != size || actual != expected) {
        error = "Downloaded file SHA-256 or size does not match the release asset.";
        return false;
    }
    return true;
}

struct LocalMemory {
    HLOCAL value = nullptr;
    ~LocalMemory() { if (value) LocalFree(value); }
};

bool CreateStage(StageGuard& stage, std::string& error) {
    FileHandle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.value)) {
        error = WindowsError("Cannot obtain the current user's identity");
        return false;
    }
    DWORD required = 0;
    GetTokenInformation(token.value, TokenUser, nullptr, 0, &required);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || required == 0) {
        error = WindowsError("Cannot obtain the current user's SID size");
        return false;
    }
    std::vector<BYTE> identity(required);
    if (!GetTokenInformation(token.value, TokenUser, identity.data(), required, &required)) {
        error = WindowsError("Cannot obtain the current user's SID");
        return false;
    }
    LocalMemory sid;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(identity.data())->User.Sid, reinterpret_cast<LPWSTR*>(&sid.value))) {
        error = WindowsError("Cannot encode the current user's SID");
        return false;
    }
    const std::wstring dacl = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;" + std::wstring(static_cast<LPCWSTR>(sid.value)) + L")";
    LocalMemory descriptor;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(dacl.c_str(), SDDL_REVISION_1,
        reinterpret_cast<PSECURITY_DESCRIPTOR*>(&descriptor.value), nullptr)) {
        error = WindowsError("Cannot create a private staging security descriptor");
        return false;
    }
    SECURITY_ATTRIBUTES security{sizeof(security), descriptor.value, FALSE};
    const DWORD length = GetTempPathW(0, nullptr);
    if (length == 0 || length > 32767) {
        error = WindowsError("Cannot locate the temporary directory");
        return false;
    }
    std::vector<wchar_t> temporary(static_cast<size_t>(length) + 1);
    const DWORD copied = GetTempPathW(static_cast<DWORD>(temporary.size()), temporary.data());
    if (copied == 0 || copied >= temporary.size()) {
        error = WindowsError("Cannot read the temporary directory");
        return false;
    }
    constexpr wchar_t hex[] = L"0123456789abcdef";
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
        std::array<UCHAR, 16> guid{};
        if (BCryptGenRandom(nullptr, guid.data(), static_cast<ULONG>(guid.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
            error = "Cannot generate a private staging directory identifier.";
            return false;
        }
        guid[6] = static_cast<UCHAR>((guid[6] & 0x0f) | 0x40);
        guid[8] = static_cast<UCHAR>((guid[8] & 0x3f) | 0x80);
        std::wstring name = L"VoopooFirmwareStudio-update-";
        for (size_t i = 0; i < guid.size(); ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) name += L'-';
            name += hex[guid[i] >> 4];
            name += hex[guid[i] & 15];
        }
        const auto directory = std::filesystem::path(temporary.data()) / name;
        auto file = directory / L"staged.exe";
        if (CreateDirectoryW(directory.c_str(), &security)) {
            stage.file = std::move(file);
            return true;
        }
        const DWORD code = GetLastError();
        if (code != ERROR_ALREADY_EXISTS) {
            error = WindowsError("Cannot create a private staging directory", code);
            return false;
        }
    }
    error = "Cannot create a unique staging directory.";
    return false;
}

bool QueryHeader(HINTERNET request, const wchar_t* name, std::wstring& value, std::string& error) {
    value.clear();
    DWORD bytes = 0;
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, name, nullptr, &bytes, WINHTTP_NO_HEADER_INDEX)) {
        const DWORD code = GetLastError();
        if (code == ERROR_WINHTTP_HEADER_NOT_FOUND) return true;
        if (code != ERROR_INSUFFICIENT_BUFFER) {
            error = WindowsError("Cannot read HTTP response headers", code);
            return false;
        }
    }
    if (bytes > 32 * 1024 || bytes % sizeof(wchar_t) != 0) {
        error = "HTTP response header is too large or malformed.";
        return false;
    }
    std::vector<wchar_t> buffer(static_cast<size_t>(bytes) / sizeof(wchar_t) + 1, L'\0');
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, name, buffer.data(), &bytes, WINHTTP_NO_HEADER_INDEX)) {
        error = WindowsError("Cannot read HTTP response header");
        return false;
    }
    value.assign(buffer.data(), static_cast<size_t>(bytes) / sizeof(wchar_t));
    return true;
}

bool Decimal(std::wstring_view text, uint64_t& value) {
    if (text.empty()) return false;
    value = 0;
    for (const wchar_t ch : text) {
        if (ch < L'0' || ch > L'9' || value > ((std::numeric_limits<uint64_t>::max)() - (ch - L'0')) / 10) return false;
        value = value * 10 + (ch - L'0');
    }
    return true;
}

struct RequestUrl {
    std::wstring host;
    std::wstring object;
};

bool CrackUrl(const std::wstring& url, bool api, std::wstring_view original, RequestUrl& result, std::string& error) {
    for (const wchar_t ch : url) {
        if (ch <= L' ' || ch >= 127 || ch == L'#' || ch == L'\\') {
            error = "Update URL contains forbidden characters.";
            return false;
        }
    }
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    parts.dwUserNameLength = parts.dwPasswordLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts) ||
        parts.nScheme != INTERNET_SCHEME_HTTPS || parts.nPort != INTERNET_DEFAULT_HTTPS_PORT ||
        parts.dwUserNameLength != 0 || parts.dwPasswordLength != 0) {
        error = "Update URL must be HTTPS without credentials and use port 443.";
        return false;
    }
    result.host.assign(parts.lpszHostName, parts.dwHostNameLength);
    const std::wstring_view path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (api) {
        if (url != LatestUrl) {
            error = "GitHub API redirects are not accepted.";
            return false;
        }
    } else if (url != original &&
        !(result.host == L"release-assets.githubusercontent.com" && path.starts_with(L"/github-production-release-asset/")) &&
        !(result.host == L"objects.githubusercontent.com" && path.starts_with(L"/github-production-release-asset-2e65be/"))) {
        error = "Asset redirect is not an official GitHub release asset HTTPS endpoint.";
        return false;
    }
    result.object.assign(path);
    if (parts.dwExtraInfoLength) result.object.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    return true;
}

bool NetworkReady(std::stop_token stop, Clock::time_point deadline, std::string& error) {
    if (Cancelled(stop, error)) return false;
    if (Clock::now() <= deadline) return true;
    error = "Update request exceeded its total time limit.";
    return false;
}
// The worker waits on asynchronous WinHTTP operations rather than closing a
// synchronous handle from another thread (which WinHTTP explicitly forbids).
// Its callback reference also keeps a cancelled read's buffer alive until the
// final HANDLE_CLOSING notification; no detached application thread is needed.
struct AsyncState {
    std::atomic<unsigned> references{1};
    std::atomic<DWORD> completed{0};
    std::atomic<DWORD> error{0};
    std::atomic<DWORD> read{0};
    std::array<UCHAR, 64 * 1024> buffer;
    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    AsyncState() {
        if (!event) throw std::runtime_error(WindowsError("Cannot create the HTTPS completion event"));
    }
    ~AsyncState() { CloseHandle(event); }
    void Release() { if (references.fetch_sub(1) == 1) delete this; }
};

struct AsyncRequest {
    HINTERNET value;
    AsyncState* state = nullptr;
    explicit AsyncRequest(HINTERNET handle) : value(handle) {}
    AsyncRequest(const AsyncRequest&) = delete;
    AsyncRequest& operator=(const AsyncRequest&) = delete;
    ~AsyncRequest() {
        if (value) WinHttpCloseHandle(value);
        if (state) state->Release();
    }

    static void CALLBACK Callback(HINTERNET, DWORD_PTR context, DWORD status, void* information, DWORD length) {
        auto* state = reinterpret_cast<AsyncState*>(context);
        if (!state) return;
        state->references.fetch_add(1);
        struct CallbackReference {
            AsyncState* state;
            ~CallbackReference() { state->Release(); }
        } reference{state};
        if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
            state->Release();
            return;
        }
        if (status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
            state->error = static_cast<WINHTTP_ASYNC_RESULT*>(information)->dwError;
        } else if (status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE) {
            state->read = length;
        } else if (status != WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE &&
            status != WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE) return;
        state->completed = status;
        SetEvent(state->event);
    }

    bool Initialize(std::string& error) {
        state = new AsyncState;
        if (WinHttpSetStatusCallback(value, Callback, WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES, 0) == WINHTTP_INVALID_STATUS_CALLBACK) {
            error = WindowsError("Cannot register HTTPS completion callbacks");
            return false;
        }
        DWORD_PTR context = reinterpret_cast<DWORD_PTR>(state);
        state->references.fetch_add(1);
        if (!WinHttpSetOption(value, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof(context))) {
            state->Release();
            error = WindowsError("Cannot register the HTTPS request context");
            return false;
        }
        return true;
    }

    void Begin() {
        ResetEvent(state->event);
        state->completed = 0;
        state->error = 0;
        state->read = 0;
    }

    bool Wait(DWORD expected, std::stop_token stop, Clock::time_point totalDeadline, std::string& error) {
        const auto deadline = (std::min)(totalDeadline, Clock::now() + std::chrono::milliseconds(NetworkTimeoutMs));
        std::stop_callback wake(stop, [event = state->event] { SetEvent(event); });
        for (;;) {
            if (Cancelled(stop, error)) return false;
            const DWORD completed = state->completed.load();
            if (completed) {
                if (completed == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
                    error = WindowsError("HTTPS update operation failed", state->error.load());
                    return false;
                }
                if (completed == expected) return true;
                error = "Unexpected HTTPS update completion notification.";
                return false;
            }
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
            if (remaining <= 0) {
                error = "HTTPS update operation timed out after at most 5 seconds.";
                return false;
            }
            const DWORD waited = WaitForSingleObject(state->event, static_cast<DWORD>(remaining));
            if (waited == WAIT_OBJECT_0) ResetEvent(state->event);
            if (waited == WAIT_FAILED) {
                error = WindowsError("Cannot wait for the HTTPS update response");
                return false;
            }
        }
    }
};


template<class Sink>
bool Fetch(const std::wstring& original, bool api, uint64_t limit, bool exactSize,
    std::stop_token stop, Sink&& sink, std::string& error) {
    const auto deadline = Clock::now() + (api ? std::chrono::seconds(30) : std::chrono::seconds(300));
    if (!NetworkReady(stop, deadline, error)) return false;
    InternetHandle session(WinHttpOpen(L"VoopooFirmwareStudio/" STUDIO_VERSION, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC));
    if (!session.value) {
        error = WindowsError("Cannot initialize HTTPS");
        return false;
    }
    DWORD retries = 0;
    if (!WinHttpSetTimeouts(session.value, NetworkTimeoutMs, NetworkTimeoutMs, NetworkTimeoutMs, NetworkTimeoutMs) ||
        !WinHttpSetOption(session.value, WINHTTP_OPTION_CONNECT_RETRIES, &retries, sizeof(retries))) {
        error = WindowsError("Cannot configure bounded HTTPS timeouts");
        return false;
    }
    std::wstring url = original;
    for (unsigned redirects = 0; redirects <= 5; ++redirects) {
        if (!NetworkReady(stop, deadline, error)) return false;
        RequestUrl parsed;
        if (!CrackUrl(url, api, original, parsed, error)) return false;
        InternetHandle connection(WinHttpConnect(session.value, parsed.host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
        if (!connection.value) {
            error = WindowsError("Cannot connect to the update server");
            return false;
        }
        AsyncRequest request(WinHttpOpenRequest(connection.value, L"GET", parsed.object.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
        if (!request.value) {
            error = WindowsError("Cannot create the update request");
            return false;
        }
        if (!request.Initialize(error)) return false;
        DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        DWORD disabled = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
        DWORD logonPolicy = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
        DWORD responseTimeout = NetworkTimeoutMs;
        if (!WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy)) ||
            !WinHttpSetOption(request.value, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)) ||
            !WinHttpSetOption(request.value, WINHTTP_OPTION_AUTOLOGON_POLICY, &logonPolicy, sizeof(logonPolicy)) ||
            !WinHttpSetOption(request.value, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &responseTimeout, sizeof(responseTimeout))) {
            error = WindowsError("Cannot configure secure update request options");
            return false;
        }
        const wchar_t* headers = api ? L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n" : L"Accept: application/octet-stream\r\n";
        if (!NetworkReady(stop, deadline, error)) return false;
        request.Begin();
        if (!WinHttpSendRequest(request.value, headers, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0,
            reinterpret_cast<DWORD_PTR>(request.state))) {
            error = WindowsError("Cannot send the HTTPS update request");
            return false;
        }
        if (!request.Wait(WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, stop, deadline, error)) return false;
        if (!NetworkReady(stop, deadline, error)) return false;
        request.Begin();
        if (!WinHttpReceiveResponse(request.value, nullptr)) {
            error = WindowsError("Cannot receive the HTTPS update response");
            return false;
        }
        if (!request.Wait(WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, stop, deadline, error)) return false;
        if (!NetworkReady(stop, deadline, error)) return false;
        DWORD status = 0, statusSize = sizeof(status);
        if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
            error = WindowsError("Cannot read the update HTTP status");
            return false;
        }
        if (!api && (status == 301 || status == 302 || status == 303 || status == 307 || status == 308)) {
            if (redirects == 5) {
                error = "Too many GitHub asset redirects.";
                return false;
            }
            if (!QueryHeader(request.value, L"Location", url, error)) return false;
            if (url.empty()) {
                error = "GitHub asset redirect has no Location header.";
                return false;
            }
            continue;
        }
        if (status != 200) {
            error = std::string(api ? "GitHub releases API" : "GitHub asset download") + " returned HTTP " + std::to_string(status) + ".";
            if (status == 403 || status == 429) {
                std::wstring remaining, reset, retry;
                std::string headerError;
                QueryHeader(request.value, L"X-RateLimit-Remaining", remaining, headerError);
                QueryHeader(request.value, L"X-RateLimit-Reset", reset, headerError);
                QueryHeader(request.value, L"Retry-After", retry, headerError);
                uint64_t number = 0;
                if (status == 429 || remaining == L"0") error += " GitHub rate limit exceeded.";
                else error += " GitHub denied the request (possibly a rate limit).";
                if (Decimal(reset, number)) error += " Rate limit reset (Unix seconds): " + std::to_string(number) + ".";
                if (Decimal(retry, number)) error += " Retry-After: " + std::to_string(number) + " seconds.";
            }
            return false;
        }
        std::wstring contentLength;
        if (!QueryHeader(request.value, L"Content-Length", contentLength, error)) return false;
        if (!contentLength.empty()) {
            uint64_t advertised = 0;
            if (!Decimal(contentLength, advertised) || advertised > limit || (exactSize && advertised != limit)) {
                error = "HTTP Content-Length does not match the permitted update size.";
                return false;
            }
        }
        uint64_t total = 0;
        for (;;) {
            if (!NetworkReady(stop, deadline, error)) return false;
            const DWORD requested = static_cast<DWORD>((std::min)(static_cast<uint64_t>(request.state->buffer.size()), limit - total + 1));
            request.Begin();
            if (!WinHttpReadData(request.value, request.state->buffer.data(), requested, nullptr)) {
                error = WindowsError("Cannot read the update response body");
                return false;
            }
            if (!request.Wait(WINHTTP_CALLBACK_STATUS_READ_COMPLETE, stop, deadline, error)) return false;
            if (!NetworkReady(stop, deadline, error)) return false;
            const DWORD read = request.state->read.load();
            if (!read) break;
            if (read > limit - total) {
                error = "HTTP update response exceeds the permitted size.";
                return false;
            }
            if (!sink(request.state->buffer.data(), read)) return false;
            total += read;
        }
        if (exactSize && total != limit) {
            error = "Downloaded response is shorter than the release asset size.";
            return false;
        }
        return true;
    }
    error = "Too many GitHub asset redirects.";
    return false;
}

bool DownloadStaged(const Release& release, std::filesystem::path& stagedFile, std::stop_token stop, std::string& error) {
    if (!ValidateRelease(release, error) || Cancelled(stop, error)) return false;
    StageGuard stage;
    if (!CreateStage(stage, error)) return false;
    FileHandle file;
    file.value = CreateFileW(stage.file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file.value == INVALID_HANDLE_VALUE) {
        error = WindowsError("Cannot create the staged update executable");
        return false;
    }
    auto write = [&](const UCHAR* data, DWORD bytes) {
        DWORD offset = 0;
        while (offset < bytes) {
            if (Cancelled(stop, error)) return false;
            DWORD written = 0;
            if (!WriteFile(file.value, data + offset, bytes - offset, &written, nullptr)) {
                error = WindowsError("Cannot write the staged update executable");
                return false;
            }
            if (!written) {
                error = "Writing the staged update executable made no progress.";
                return false;
            }
            offset += written;
        }
        return true;
    };
    if (!Fetch(Wide(release.assetUrl), false, release.size, true, stop, write, error)) return false;
    if (Cancelled(stop, error)) return false;
    if (!FlushFileBuffers(file.value)) {
        error = WindowsError("Cannot flush the staged update executable");
        return false;
    }
    if (!VerifyHandle(file.value, release.sha256, release.size, stop, error) || Cancelled(stop, error)) return false;
    if (!file.Close()) {
        error = WindowsError("Cannot close the staged update executable");
        return false;
    }
    stagedFile = stage.file;
    stage.owned = false;
    return true;
}

struct WorkResult {
    Status status;
    bool ownsStage = false;
    WorkResult() = default;
    WorkResult(const WorkResult&) = delete;
    WorkResult& operator=(const WorkResult&) = delete;
    WorkResult(WorkResult&& other) noexcept : status(std::move(other.status)), ownsStage(std::exchange(other.ownsStage, false)) {}
    WorkResult& operator=(WorkResult&& other) noexcept {
        if (this != &other) {
            if (ownsStage) RemoveStage(status.stagedFile);
            status = std::move(other.status);
            ownsStage = std::exchange(other.ownsStage, false);
        }
        return *this;
    }
    ~WorkResult() { if (ownsStage) RemoveStage(status.stagedFile); }
};

WorkResult CheckRelease(std::stop_token stop) {
    WorkResult result;
    result.status.phase = Phase::Failed;
    VersionParts current;
    if (!ParseVersion(Version, current)) {
        result.status.error = "The application's compiled version is not a stable numeric semantic version.";
        return result;
    }
    std::string json;
    auto append = [&](const UCHAR* bytes, DWORD size) {
        json.append(reinterpret_cast<const char*>(bytes), size);
        return true;
    };
    if (!Fetch(std::wstring(LatestUrl), true, MaxJsonSize, false, stop, append, result.status.error) ||
        !ParseRelease(json, result.status.release, result.status.error) || Cancelled(stop, result.status.error)) return result;
    result.status.phase = IsNewerVersion(result.status.release.tag, Version) ? Phase::Available : Phase::Current;
    return result;
}
} // namespace

bool IsNewerVersion(std::string_view candidate, std::string_view current) {
    VersionParts a, b;
    if (!ParseVersion(candidate, a) || !ParseVersion(current, b)) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].size() != b[i].size()) return a[i].size() > b[i].size();
        const int order = a[i].compare(b[i]);
        if (order) return order > 0;
    }
    return false;
}

bool ParseRelease(std::string_view json, Release& release, std::string& error) {
    error.clear();
    try {
        if (json.empty() || json.size() > MaxJsonSize) throw std::runtime_error("Release JSON is empty or exceeds 4 MiB.");
        Apartment apartment;
        // Static JsonObject::Parse caches its activation factory past our
        // apartment's lifetime. Acquire an uncached, scoped factory instead;
        // it is released before RoUninitialize, also on parse exceptions.
        winrt::hresult_error activationError;
        const auto factory = winrt::try_get_activation_factory<IJsonObjectStatics>(
            L"Windows.Data.Json.JsonObject", activationError);
        if (!factory) throw activationError;
        const auto root = factory.Parse(winrt::hstring(Wide(json)));
        if (Required(root, L"draft", JsonValueType::Boolean).GetBoolean() ||
            Required(root, L"prerelease", JsonValueType::Boolean).GetBoolean())
            throw std::runtime_error("Draft and prerelease updates are not supported.");
        Release parsed;
        parsed.tag = StringField(root, L"tag_name");
        VersionParts version;
        if (!ParseVersion(parsed.tag, version)) throw std::runtime_error("Release tag is not a stable numeric semantic version.");
        parsed.notes = StringField(root, L"body");
        parsed.url = StringField(root, L"html_url");
        const auto assets = Required(root, L"assets", JsonValueType::Array).GetArray();
        bool found = false;
        const auto expectedName = AssetName(parsed.tag);
        for (const auto& value : assets) {
            if (value.ValueType() != JsonValueType::Object) throw std::runtime_error("Release asset entries must be JSON objects.");
            const auto asset = value.GetObject();
            if (StringField(asset, L"name") != expectedName) continue;
            if (found) throw std::runtime_error("Release contains duplicate matching executable assets.");
            found = true;
            parsed.assetUrl = StringField(asset, L"browser_download_url");
            const auto digest = StringField(asset, L"digest");
            std::array<UCHAR, 32> bytes;
            if (!digest.starts_with("sha256:") || !ParseDigest(std::string_view(digest).substr(7), bytes))
                throw std::runtime_error("Release asset requires a sha256: digest with exactly 64 hexadecimal digits.");
            parsed.sha256 = digest.substr(7);
            for (char& ch : parsed.sha256) if (ch >= 'A' && ch <= 'F') ch = static_cast<char>(ch - 'A' + 'a');
            const double size = Required(asset, L"size", JsonValueType::Number).GetNumber();
            if (!std::isfinite(size) || size < 1 || size > static_cast<double>(MaxAssetSize) || std::floor(size) != size)
                throw std::runtime_error("Release asset size must be an integer between 1 byte and 256 MiB.");
            parsed.size = static_cast<uint64_t>(size);
        }
        if (!found) throw std::runtime_error("Release does not contain the exact Windows Release executable asset.");
        if (!ValidateRelease(parsed, error)) return false;
        release = std::move(parsed);
        return true;
    } catch (const winrt::hresult_error& exception) {
        error = "Cannot parse release JSON with Windows.Data.Json (HRESULT " + std::to_string(exception.code().value) + ").";
    } catch (const std::exception& exception) {
        error = exception.what();
    } catch (...) {
        error = "Cannot parse release JSON.";
    }
    return false;
}

bool VerifyFile(const std::filesystem::path& path, std::string_view sha256, uint64_t size, std::string& error) {
    error.clear();
    try {
        FileHandle file;
        file.value = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file.value == INVALID_HANDLE_VALUE) {
            error = WindowsError("Cannot open the staged update for verification");
            return false;
        }
        return VerifyHandle(file.value, sha256, size, {}, error);
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool DownloadRelease(const Release& release, std::filesystem::path& stagedFile, std::string& error) {
    error.clear();
    try {
        return DownloadStaged(release, stagedFile, {}, error);
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

struct Client::Impl {
    Status status;
    bool ownsStage = false;
    std::future<WorkResult> pending;
    std::jthread worker;

    ~Impl() {
        worker.request_stop();
        if (worker.joinable()) worker.join();
        // A completed but unpolled result owns its own payload until destroyed.
        if (ownsStage) RemoveStage(status.stagedFile);
    }

    bool Busy() const { return status.phase == Phase::Checking || status.phase == Phase::Downloading; }

    void ClearStage() {
        if (ownsStage) RemoveStage(status.stagedFile);
        ownsStage = false;
        status.stagedFile.clear();
    }

    template<class Operation>
    bool Start(Phase phase, Operation operation) {
        try {
            std::promise<WorkResult> promise;
            auto future = promise.get_future();
            std::jthread next([promise = std::move(promise), operation = std::move(operation)](std::stop_token stop) mutable {
                WorkResult result;
                try {
                    result = operation(stop);
                } catch (const std::exception& exception) {
                    result.status.phase = Phase::Failed;
                    result.status.error = exception.what();
                } catch (...) {
                    result.status.phase = Phase::Failed;
                    result.status.error = "The background update operation failed.";
                }
                promise.set_value(std::move(result));
            });
            worker = std::move(next);
            pending = std::move(future);
            ClearStage();
            status.phase = phase;
            status.error.clear();
            return true;
        } catch (const std::exception& exception) {
            status.phase = Phase::Failed;
            status.error = exception.what();
            return false;
        }
    }
};

Client::Client() : impl_(std::make_unique<Impl>()) {}
Client::~Client() = default;

bool Client::Check() {
    if (impl_->Busy()) return false;
    return impl_->Start(Phase::Checking, [](std::stop_token stop) { return CheckRelease(stop); });
}

bool Client::Download() {
    if (impl_->Busy() || impl_->status.phase != Phase::Available || !IsNewerVersion(impl_->status.release.tag, Version)) return false;
    return impl_->Start(Phase::Downloading, [release = impl_->status.release](std::stop_token stop) mutable {
        WorkResult result;
        result.status.release = std::move(release);
        result.status.phase = Phase::Failed;
        if (DownloadStaged(result.status.release, result.status.stagedFile, stop, result.status.error)) {
            result.ownsStage = true;
            result.status.phase = Phase::Ready;
        }
        return result;
    });
}

void Client::Poll() {
    if (!impl_->pending.valid() || impl_->pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    auto result = impl_->pending.get();
    if (impl_->worker.joinable()) impl_->worker.join();
    impl_->ClearStage();
    impl_->status = std::move(result.status);
    impl_->ownsStage = std::exchange(result.ownsStage, false);
}

const Status& Client::Get() const { return impl_->status; }

void Client::ReleaseStage() {
    // StartInstall has already verified and copied the payload into its own
    // private helper workspace before it permits this handoff.
    impl_->ClearStage();
}
} // namespace AppUpdate
