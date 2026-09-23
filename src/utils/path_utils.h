#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include <windows.h>
#include <shlobj.h>

namespace PathUtils {

inline std::string FormatTimestampNow() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &tt);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    return std::string(buf);
}

inline std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

inline std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

inline std::filesystem::path ToFsPath(const std::string& utf8Path) {
    return std::filesystem::path(Utf8ToWide(utf8Path));
}

inline std::ifstream OpenReadStream(const std::string& utf8Path, std::ios_base::openmode mode = std::ios::in | std::ios::binary) {
    return std::ifstream(ToFsPath(utf8Path), mode);
}

inline std::ofstream OpenWriteStream(const std::string& utf8Path, std::ios_base::openmode mode = std::ios::out | std::ios::binary) {
    return std::ofstream(ToFsPath(utf8Path), mode);
}

inline FILE* FopenUtf8(const std::string& utf8Path, const wchar_t* mode) {
    return _wfopen(Utf8ToWide(utf8Path).c_str(), mode);
}

inline bool PickFileDialog(HWND hwnd, const wchar_t* filter, std::string& outPath) {
    OPENFILENAMEW ofn = { sizeof(ofn) };
    wchar_t szFile[MAX_PATH] = L"";
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        outPath = WideToUtf8(szFile);
        return true;
    }
    return false;
}

inline bool SaveFileDialog(HWND hwnd, const wchar_t* filter, const std::string& defaultName, std::string& outPath) {
    OPENFILENAMEW ofn = { sizeof(ofn) };
    wchar_t szFile[MAX_PATH] = L"";
    std::wstring wDef = Utf8ToWide(defaultName);
    wcsncpy_s(szFile, wDef.c_str(), MAX_PATH - 1);

    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&ofn)) {
        outPath = WideToUtf8(szFile);
        return true;
    }
    return false;
}

inline bool PickFolderDialog(HWND hwnd, std::string& outPath) {
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hwnd;
    bi.lpszTitle = L"Select Directory";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            outPath = WideToUtf8(path);
            CoTaskMemFree(pidl);
            return true;
        }
        CoTaskMemFree(pidl);
    }
    return false;
}

} // namespace PathUtils
