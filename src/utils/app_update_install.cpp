#include "app_update_install.h"

#include <array>
#include <bcrypt.h>
#include <cstddef>
#include <cwchar>
#include <exception>
#include <limits>
#include <sddl.h>
#include <utility>
#include <vector>

namespace AppUpdate {
namespace {
class Handle {
public:
    explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : value_(value) {}
    ~Handle() { if (valid()) CloseHandle(value_); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    bool valid() const { return value_ != INVALID_HANDLE_VALUE && value_ != nullptr; }
    HANDLE get() const { return value_; }
private:
    HANDLE value_;
};

bool WindowsError(std::string& error, const char* operation, DWORD code = GetLastError()) {
    error = std::string(operation) + " (Windows error " + std::to_string(code) + ").";
    return false;
}

bool RandomId(std::wstring& result, std::string& error) {
    std::array<unsigned char, 16> bytes{};
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
        error = "Cannot generate a private update workspace name.";
        return false;
    }
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);
    constexpr wchar_t hex[] = L"0123456789abcdef";
    result.clear();
    result.reserve(36);
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) result += L'-';
        result += hex[bytes[i] >> 4];
        result += hex[bytes[i] & 15];
    }
    return true;
}

// Never recursively delete a directory, or delete the caller's staged file.
void RemoveWorkspace(const std::filesystem::path& directory) {
    if (directory.empty()) return;
    for (const wchar_t* name : { L"install.ps1", L"manifest.json", L"update.exe" })
        DeleteFileW((directory / name).c_str());
    RemoveDirectoryW(directory.c_str());
}

struct Workspace {
    std::filesystem::path directory;
    bool handedOff = false;
    ~Workspace() { if (!handedOff) RemoveWorkspace(directory); }
};

bool CreateWorkspace(std::filesystem::path& directory, std::string& error) {
    std::vector<wchar_t> temp(32768);
    DWORD length = GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
    if (!length || length >= temp.size()) return WindowsError(error, "Cannot locate the temporary directory");
    std::wstring id;
    if (!RandomId(id, error)) return false;
    auto candidate = std::filesystem::path(temp.data()) / (L"VoopooFirmwareStudio-update-" + id);
    // Windows PowerShell 5.1 uses .NET Framework's conventional path limit.
    if (candidate.native().size() + 32 >= MAX_PATH) {
        error = "The temporary directory path is too long for Windows PowerShell 5.1.";
        return false;
    }
    HANDLE rawToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &rawToken))
        return WindowsError(error, "Cannot read the current user's security token");
    Handle token(rawToken);
    DWORD needed = 0;
    GetTokenInformation(token.get(), TokenUser, nullptr, 0, &needed);
    if (!needed) return WindowsError(error, "Cannot read the current user's identity");
    std::vector<unsigned char> user(needed);
    if (!GetTokenInformation(token.get(), TokenUser, user.data(), needed, &needed))
        return WindowsError(error, "Cannot read the current user's identity");
    LPWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid, &sid))
        return WindowsError(error, "Cannot encode the current user's identity");
    const std::wstring sddl = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;" + std::wstring(sid) + L")";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1,
        &descriptor, nullptr))
        return WindowsError(error, "Cannot secure the update workspace");
    SECURITY_ATTRIBUTES attributes{ sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE };
    const BOOL created = CreateDirectoryW(candidate.c_str(), &attributes);
    const DWORD createError = GetLastError();
    LocalFree(descriptor);
    if (!created) return WindowsError(error, "Cannot create a private update workspace", createError);
    directory = std::move(candidate);
    // A DACL is not protection on FAT/exFAT. Do not put executable helper data there.
    wchar_t volume[MAX_PATH]{};
    DWORD flags = 0;
    if (!GetVolumePathNameW(directory.c_str(), volume, MAX_PATH) ||
        !GetVolumeInformationW(volume, nullptr, 0, nullptr, nullptr, &flags, nullptr, 0))
        return WindowsError(error, "Cannot check update workspace security");
    if (!(flags & FILE_PERSISTENT_ACLS)) {
        error = "The temporary directory must be on a filesystem supporting Windows access controls.";
        return false;
    }
    return true;
}

bool CanonicalFile(const std::filesystem::path& input, bool target,
    std::filesystem::path& result, std::string& error) {
    if (input.empty() || input.native().find(L'\0') != std::wstring::npos) {
        error = "An update file path is empty or contains a null character.";
        return false;
    }
    Handle file(CreateFileW(input.c_str(), target ? FILE_READ_ATTRIBUTES | DELETE : GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (!file.valid()) return WindowsError(error, target ? "Cannot access the application for replacement" : "Cannot open the staged update");
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(file.get(), &info)) return WindowsError(error, "Cannot inspect an update file");
    if ((info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
        (target && ((info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ||
            (info.nFileSizeHigh == 0 && info.nFileSizeLow == 0)))) {
        error = "The update target and payload must be regular files; the target must be nonempty and writable.";
        return false;
    }
    std::vector<wchar_t> path(32768);
    const DWORD length = GetFinalPathNameByHandleW(file.get(), path.data(), static_cast<DWORD>(path.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (!length || length >= path.size()) return WindowsError(error, "Cannot resolve an update file path");
    std::wstring resolved(path.data(), length);
    if (resolved.size() < 7 || resolved.compare(0, 4, L"\\\\?\\") != 0 ||
        resolved[5] != L':' || resolved[6] != L'\\') {
        error = "Updates require files on a local Windows drive, not a network or device path.";
        return false;
    }
    resolved.erase(0, 4);
    if (resolved.size() >= MAX_PATH || resolved.find(L':', 2) != std::wstring::npos) {
        error = "The update file path is too long or identifies an alternate data stream.";
        return false;
    }
    result = std::filesystem::path(resolved);
    if (target && _wcsicmp(result.extension().c_str(), L".exe") != 0) {
        error = "The application update target must be an .exe file.";
        return false;
    }
    return true;
}

bool CheckTargetDirectory(const std::filesystem::path& target, std::string& error) {
    const auto parent = target.parent_path();
    if (parent.native().size() + 64 >= MAX_PATH) {
        error = "The application directory path is too long for safe update backup filenames.";
        return false;
    }
    std::wstring id;
    if (!RandomId(id, error)) return false;
    const auto probe = parent / (L".vfs-update-probe-" + id + L".tmp");
    Handle file(CreateFileW(probe.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr));
    if (!file.valid()) return WindowsError(error, "The application directory is not writable; move the application to a writable directory before updating");
    return true;
}

bool JsonPath(const std::filesystem::path& path, std::string& json, std::string& error) {
    const auto& wide = path.native();
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
        static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (!length) return WindowsError(error, "An update path cannot be encoded as UTF-8");
    std::string utf8(length, '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
        static_cast<int>(wide.size()), utf8.data(), length, nullptr, nullptr))
        return WindowsError(error, "An update path cannot be encoded as UTF-8");
    json = "\"";
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : utf8) {
        if (c == '"' || c == '\\') { json += '\\'; json += static_cast<char>(c); }
        else if (c < 0x20) { json += "\\u00"; json += hex[c >> 4]; json += hex[c & 15]; }
        else json += static_cast<char>(c);
    }
    json += '"';
    return true;
}

bool WriteNewUtf8(const std::filesystem::path& path, std::string_view contents, std::string& error) {
    Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) return WindowsError(error, "Cannot create an update helper file");
    constexpr unsigned char bom[] = { 0xef, 0xbb, 0xbf };
    DWORD written = 0;
    if (!WriteFile(file.get(), bom, sizeof(bom), &written, nullptr) || written != sizeof(bom) ||
        !WriteFile(file.get(), contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr) ||
        written != contents.size() || !FlushFileBuffers(file.get()))
        return WindowsError(error, "Cannot write an update helper file");
    return true;
}

// Fixed code only. Every variable application path comes from data-only JSON.
constexpr std::string_view Helper = R"VFSPS(param([Parameter(Mandatory = $true)][string] $Manifest)
Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetDirectoryName($PSCommandPath)
$download = [IO.Path]::Combine($root, 'update.exe')
$manifestPath = [IO.Path]::Combine($root, 'manifest.json')
$errorPath = [IO.Path]::Combine($root, 'error.txt')
$temporary = $null
$backup = $null
$backupOwned = $false
$target = $null
$folder = $null
$relaunch = $false
$replacementAttempted = $false
$exitCode = 1

function Assert-RegularFile([string] $Path) {
    $attributes = [IO.File]::GetAttributes($Path)
    if (($attributes -band [IO.FileAttributes]::Directory) -ne 0 -or
        ($attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw 'An update file is no longer a regular file.'
    }
}
function Assert-Payload([string] $Path, [string] $Hash, [long] $Size) {
    Assert-RegularFile $Path
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    $sha = $null
    try {
        if ($stream.Length -ne $Size) { throw 'The update payload size does not match the release.' }
        $sha = [Security.Cryptography.SHA256]::Create()
        $actual = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant()
        if (-not [string]::Equals($actual, $Hash, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'The update payload SHA-256 does not match the release.'
        }
    } finally {
        if ($null -ne $sha) { $sha.Dispose() }
        $stream.Dispose()
    }
}
function New-SiblingCopy([string] $Source, [string] $Folder, [string] $Prefix) {
    $path = [IO.Path]::Combine($Folder, $Prefix + [Guid]::NewGuid().ToString('N') + '.tmp')
    $output = $null
    $inputStream = $null
    $finished = $false
    try {
        # CreateNew never overwrites an existing user's file, including on a name collision.
        $output = [IO.File]::Open($path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $inputStream = [IO.File]::Open($Source, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        $inputStream.CopyTo($output)
        $output.Flush($true)
        $finished = $true
    } finally {
        if ($null -ne $inputStream) { $inputStream.Dispose() }
        if ($null -ne $output) {
            $output.Dispose()
            if (-not $finished) { try { [IO.File]::Delete($path) } catch {} }
        }
    }
    return $path
}
function Restore-Original([string] $Backup, [string] $Target, [string] $Folder) {
    # The empty reserved marker is not a backup; never use it for recovery.
    if (-not [IO.File]::Exists($Backup) -or ([IO.FileInfo]::new($Backup)).Length -eq 0) {
        throw 'No original backup was produced; the existing application was not replaced.'
    }
    $restore = New-SiblingCopy $Backup $Folder '.vfs-rollback-'
    try {
        if ([IO.File]::Exists($Target)) {
            [IO.File]::Replace($restore, $Target, $null, $false)
        } else {
            [IO.File]::Move($restore, $Target)
        }
    } finally {
        if ([IO.File]::Exists($restore)) { try { [IO.File]::Delete($restore) } catch {} }
    }
}
function Start-Application([string] $Path, [string] $Folder) {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Path
    $info.WorkingDirectory = $Folder
    $info.UseShellExecute = $false
    $process = [Diagnostics.Process]::Start($info)
    if ($null -eq $process) { throw 'Windows could not launch the application.' }
    $process.Dispose()
}
try {
    if (-not [string]::Equals([IO.Path]::GetFullPath($Manifest), $manifestPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'The helper manifest must be in its private staging directory.'
    }
    Assert-RegularFile $manifestPath
    $data = Microsoft.PowerShell.Utility\ConvertFrom-Json -InputObject ([IO.File]::ReadAllText($manifestPath, [Text.Encoding]::UTF8))
    if ($data.version -ne 1 -or $data.sha256 -cnotmatch '^[0-9a-f]{64}$' -or
        $data.relaunch -isnot [bool] -or $data.parentPid -le 0) {
        throw 'The update manifest is invalid.'
    }
    $size = [long]::Parse([string] $data.size, [Globalization.NumberStyles]::None, [Globalization.CultureInfo]::InvariantCulture)
    $created = [long]::Parse([string] $data.parentCreated, [Globalization.NumberStyles]::None, [Globalization.CultureInfo]::InvariantCulture)
    if ($size -le 0 -or $created -le 0) { throw 'The update manifest contains an invalid size or parent identity.' }
    $target = [IO.Path]::GetFullPath([string] $data.target)
    if (-not [string]::Equals($target, [string] $data.target, [StringComparison]::OrdinalIgnoreCase) -or
        -not [string]::Equals([IO.Path]::GetExtension($target), '.exe', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'The application target path is invalid.'
    }
    $folder = [IO.Path]::GetDirectoryName($target)
    $relaunch = $data.relaunch
    # A creation timestamp prevents PID reuse from making us wait for an unrelated process.
    $parent = $null
    try { $parent = [Diagnostics.Process]::GetProcessById([int] $data.parentPid) }
    catch [ArgumentException] { $parent = $null }
    if ($null -ne $parent) {
        try {
            $sameParent = $false
            try { $sameParent = $parent.StartTime.ToUniversalTime().ToFileTimeUtc() -eq $created }
            catch {
                if (-not $parent.HasExited) { throw }
            }
            if ($sameParent -and -not $parent.WaitForExit(120000)) {
                throw 'The application did not exit within 120 seconds. It was not replaced.'
            }
        } finally { $parent.Dispose() }
    }
    Assert-RegularFile $target
    if (([IO.File]::GetAttributes($target) -band [IO.FileAttributes]::ReadOnly) -ne 0 -or
        ([IO.FileInfo]::new($target)).Length -eq 0) {
        throw 'The application target is empty or read-only.'
    }
    Assert-Payload $download ([string] $data.sha256) $size
    # Always copy to the target volume first; replacement itself is a same-volume operation.
    $temporary = New-SiblingCopy $download $folder '.vfs-update-'
    Assert-Payload $temporary ([string] $data.sha256) $size
    $backup = [IO.Path]::Combine($folder, '.vfs-backup-' + [Guid]::NewGuid().ToString('N') + '.exe')
    $marker = [IO.File]::Open($backup, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $backupOwned = $true
    $marker.Dispose()
    $replacementAttempted = $true
    # Only our own freshly reserved backup placeholder can be overwritten.
    [IO.File]::Replace($temporary, $target, $backup, $false)
    $temporary = $null
    if ($relaunch) { Start-Application $target $folder }
    $exitCode = 0
} catch {
    $failure = $_.Exception.Message
    if ($replacementAttempted -and $backupOwned -and [IO.File]::Exists($backup) -and
        ([IO.FileInfo]::new($backup)).Length -gt 0) {
        try {
            Restore-Original $backup $target $folder
            $failure += "`r`nThe previous application was restored."
            if ($relaunch) {
                try { Start-Application $target $folder }
                catch { $failure += "`r`nThe restored application could not be launched: " + $_.Exception.Message }
            }
        } catch {
            $failure += "`r`nAutomatic recovery failed: " + $_.Exception.Message + "`r`nOriginal backup: " + $backup
        }
    }
    $failure = 'Voopoo Firmware Studio update failed: ' + $failure
    try { [IO.File]::WriteAllText($errorPath, $failure, [Text.UTF8Encoding]::new($true)) } catch {}
    [Console]::Error.WriteLine($failure)
    # Isolated relaunch=false callers receive an exit code and error.txt, never a blocking UI.
    if ($relaunch) {
        try {
            Add-Type -AssemblyName System.Windows.Forms
            [void] [Windows.Forms.MessageBox]::Show($failure + "`r`nDetails: " + $errorPath, 'Voopoo Firmware Studio update')
        } catch {}
    }
} finally {
    if ($null -ne $temporary) { try { [IO.File]::Delete($temporary) } catch {} }
    if ($backupOwned -and [IO.File]::Exists($backup)) {
        # Keep every real original backup for recovery, whether installation succeeded or failed.
        try { if (([IO.FileInfo]::new($backup)).Length -eq 0) { [IO.File]::Delete($backup) } } catch {}
    }
    foreach ($owned in @($download, $manifestPath, $PSCommandPath)) {
        try { [IO.File]::Delete($owned) } catch {}
    }
    # Non-recursive deletion succeeds only when no error log or unexpected files remain.
    try { [IO.Directory]::Delete($root, $false) } catch {}
}
exit $exitCode
)VFSPS";

bool Prepare(const InstallRequest& request, std::filesystem::path& script,
    std::filesystem::path& manifest, std::string& error) {
    if (!request.parentPid || !request.size || request.size > static_cast<uint64_t>((std::numeric_limits<int64_t>::max)()) ||
        request.sha256.size() != 64 || request.sha256.find_first_not_of("0123456789abcdef") != std::string::npos) {
        error = "The update is missing a valid parent process, size, or SHA-256 digest.";
        return false;
    }
    std::filesystem::path target, source;
    if (!CanonicalFile(request.target, true, target, error) ||
        !CanonicalFile(request.stagedFile, false, source, error)) return false;
    if (_wcsicmp(target.c_str(), source.c_str()) == 0) {
        error = "The staged update cannot be the application being replaced.";
        return false;
    }
    if (!CheckTargetDirectory(target, error)) return false;
    Handle parent(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, request.parentPid));
    if (!parent.valid()) return WindowsError(error, "Cannot identify the process the update must wait for");
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(parent.get(), &created, &exited, &kernel, &user))
        return WindowsError(error, "Cannot read the parent process creation time");
    const uint64_t creationTime = (static_cast<uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
    Workspace workspace;
    if (!CreateWorkspace(workspace.directory, error)) return false;
    const auto payload = workspace.directory / L"update.exe";
    if (!CopyFileW(source.c_str(), payload.c_str(), TRUE)) return WindowsError(error, "Cannot create the installer's private update copy");
    if (!SetFileAttributesW(payload.c_str(), FILE_ATTRIBUTE_NORMAL)) return WindowsError(error, "Cannot set the private update copy's attributes");
    if (!VerifyFile(payload, request.sha256, request.size, error)) return false;
    std::string targetJson;
    if (!JsonPath(target, targetJson, error)) return false;
    const std::string contents = "{\r\n  \"version\": 1,\r\n  \"target\": " + targetJson +
        ",\r\n  \"sha256\": \"" + request.sha256 + "\",\r\n  \"size\": \"" + std::to_string(request.size) +
        "\",\r\n  \"parentPid\": " + std::to_string(request.parentPid) + ",\r\n  \"parentCreated\": \"" +
        std::to_string(creationTime) + "\",\r\n  \"relaunch\": " + (request.relaunch ? "true" : "false") + "\r\n}\r\n";
    const auto scriptPath = workspace.directory / L"install.ps1";
    const auto manifestPath = workspace.directory / L"manifest.json";
    if (!WriteNewUtf8(scriptPath, Helper, error) || !WriteNewUtf8(manifestPath, contents, error)) return false;
    script = scriptPath;
    manifest = manifestPath;
    workspace.handedOff = true;
    return true;
}

bool ValidateAmd64Executable(const std::filesystem::path& path, std::string& error) {
    Handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file.valid()) return WindowsError(error, "Cannot inspect the staged Windows executable");
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file.get(), &length)) return WindowsError(error, "Cannot read the staged executable size");
    IMAGE_DOS_HEADER dos{};
    DWORD read = 0;
    bool valid = ReadFile(file.get(), &dos, sizeof(dos), &read, nullptr) && read == sizeof(dos) &&
        dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew >= static_cast<LONG>(sizeof(dos)) &&
        length.QuadPart >= static_cast<int64_t>(sizeof(IMAGE_NT_HEADERS64)) &&
        dos.e_lfanew <= length.QuadPart - static_cast<int64_t>(sizeof(IMAGE_NT_HEADERS64));
    IMAGE_NT_HEADERS64 nt{};
    if (valid) {
        LARGE_INTEGER offset{};
        offset.QuadPart = dos.e_lfanew;
        valid = SetFilePointerEx(file.get(), offset, nullptr, FILE_BEGIN) &&
            ReadFile(file.get(), &nt, sizeof(nt), &read, nullptr) && read == sizeof(nt) &&
            nt.Signature == IMAGE_NT_SIGNATURE && nt.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
            nt.FileHeader.SizeOfOptionalHeader >= sizeof(IMAGE_OPTIONAL_HEADER64) &&
            (nt.FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) &&
            !(nt.FileHeader.Characteristics & IMAGE_FILE_DLL) && nt.FileHeader.NumberOfSections > 0 &&
            nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
            nt.OptionalHeader.AddressOfEntryPoint > 0 &&
            nt.OptionalHeader.AddressOfEntryPoint < nt.OptionalHeader.SizeOfImage &&
            nt.OptionalHeader.SizeOfHeaders <= static_cast<uint64_t>(length.QuadPart);
        const uint64_t sectionsEnd = static_cast<uint64_t>(dos.e_lfanew) + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
            nt.FileHeader.SizeOfOptionalHeader + static_cast<uint64_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        valid = valid && sectionsEnd <= static_cast<uint64_t>(length.QuadPart);
    }
    if (!valid) error = "The staged update is not a valid Windows AMD64 executable.";
    return valid;
}

std::wstring QuoteArgument(std::wstring_view value) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(slashes * (c == L'"' ? 2 : 1), L'\\');
        slashes = 0;
        if (c == L'"') result += L'\\';
        result += c;
    }
    result.append(slashes * 2, L'\\');
    result += L'"';
    return result;
}
} // namespace

bool PrepareInstall(const InstallRequest& request, std::filesystem::path& script,
    std::filesystem::path& manifest, std::string& error) {
    script.clear();
    manifest.clear();
    error.clear();
    try { return Prepare(request, script, manifest, error); }
    catch (const std::exception& exception) {
        error = std::string("Cannot prepare the update installer: ") + exception.what();
        script.clear();
        manifest.clear();
        return false;
    }
}

bool StartInstall(const Status& status, std::string& error) {
    error.clear();
    try {
        if (status.phase != Phase::Ready || status.stagedFile.empty()) {
            error = "The update has not finished downloading and verification.";
            return false;
        }
        if (!VerifyFile(status.stagedFile, status.release.sha256, status.release.size, error) ||
            !ValidateAmd64Executable(status.stagedFile, error)) return false;
        std::vector<wchar_t> module(512);
        DWORD length = 0;
        for (;;) {
            length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
            if (!length) return WindowsError(error, "Cannot locate the running application");
            if (length < module.size()) break;
            if (module.size() >= 32768) { error = "The running application's path is too long."; return false; }
            module.resize(module.size() * 2);
        }
        wchar_t system[MAX_PATH]{};
        const UINT systemLength = GetSystemDirectoryW(system, MAX_PATH);
        if (!systemLength || systemLength >= MAX_PATH) return WindowsError(error, "Cannot locate the Windows system directory");
        const auto powershell = std::filesystem::path(system) / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";
        const DWORD attributes = GetFileAttributesW(powershell.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
            return WindowsError(error, "Windows PowerShell is not available");
        InstallRequest request;
        request.target = std::filesystem::path(std::wstring(module.data(), length));
        request.stagedFile = status.stagedFile;
        request.sha256 = status.release.sha256;
        request.size = status.release.size;
        request.parentPid = GetCurrentProcessId();
        std::filesystem::path script, manifest;
        if (!PrepareInstall(request, script, manifest, error)) return false;
        Workspace workspace{ script.parent_path() };
        std::wstring command = QuoteArgument(powershell.native()) +
            L" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File " + QuoteArgument(script.native()) +
            L" -Manifest " + QuoteArgument(manifest.native());
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(powershell.c_str(), command.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, workspace.directory.c_str(), &startup, &process))
            return WindowsError(error, "Cannot launch the update helper; the application was not closed");
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        workspace.handedOff = true;
        // The caller alone releases its original download, confirms shutdown, and exits.
        return true;
    } catch (const std::exception& exception) {
        error = std::string("Cannot start the update installer: ") + exception.what();
        return false;
    }
}
} // namespace AppUpdate
