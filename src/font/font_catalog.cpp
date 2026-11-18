#include "font_renderer.h"
#include "path_utils.h"
#include <dwrite.h>
#include <wrl/client.h>
#include <algorithm>
#include <map>
#include <tuple>
#include <filesystem>

using Microsoft::WRL::ComPtr;
namespace {
std::wstring FontName(IDWriteLocalizedStrings* names) {
    UINT32 index = 0;
    BOOL found = FALSE;
    wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};
    if (GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH)) names->FindLocaleName(locale, &index, &found);
    if (!found) names->FindLocaleName(L"en-us", &index, &found);
    if (!found) index = 0;
    UINT32 length = 0;
    if (FAILED(names->GetStringLength(index, &length))) return {};
    std::wstring result(length + 1, L'\0');
    if (FAILED(names->GetString(index, result.data(), length + 1))) return {};
    result.resize(length);
    return result;
}
struct FaceKeyLess {
    bool operator()(const std::pair<std::wstring, UINT32>& a, const std::pair<std::wstring, UINT32>& b) const {
        const int order = CompareStringOrdinal(a.first.c_str(), static_cast<int>(a.first.size()),
            b.first.c_str(), static_cast<int>(b.first.size()), TRUE);
        return order == CSTR_LESS_THAN || (order == CSTR_EQUAL && a.second < b.second);
    }
};
bool DiscoveryError(HRESULT result, std::string& error) {
    if (SUCCEEDED(result)) return false;
    char message[96];
    snprintf(message, sizeof(message), "Cannot enumerate installed fonts (HRESULT 0x%08lX).", static_cast<unsigned long>(result));
    error = message;
    return true;
}
}
std::vector<SystemFont> FontRenderer::ListInstalledFonts(std::string& error) {
    error.clear();
    ComPtr<IDWriteFactory> factory;
    if (DiscoveryError(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(factory.GetAddressOf())), error)) return {};
    ComPtr<IDWriteFontCollection> collection;
    if (DiscoveryError(factory->GetSystemFontCollection(&collection, TRUE), error)) return {};
    wchar_t windows[MAX_PATH]{};
    const UINT length = GetWindowsDirectoryW(windows, MAX_PATH);
    if (!length || length >= MAX_PATH) { error = "Cannot locate the Windows fonts directory."; return {}; }
    const std::wstring systemPrefix = std::wstring(windows, length) + L"\\Fonts\\";
    std::map<std::pair<std::wstring, UINT32>, SystemFont, FaceKeyLess> unique;
    const UINT32 familyCount = collection->GetFontFamilyCount();
    for (UINT32 familyIndex = 0; familyIndex < familyCount; ++familyIndex) {
        ComPtr<IDWriteFontFamily> family;
        if (FAILED(collection->GetFontFamily(familyIndex, &family))) continue;
        const UINT32 fontCount = family->GetFontCount();
        for (UINT32 fontIndex = 0; fontIndex < fontCount; ++fontIndex) {
            ComPtr<IDWriteFont> font;
            if (FAILED(family->GetFont(fontIndex, &font)) || font->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE) continue;
            ComPtr<IDWriteFontFace> face;
            if (FAILED(font->CreateFontFace(&face))) continue;
            UINT32 count = 0;
            face->GetFiles(&count, nullptr);
            if (count != 1) continue; // Only local, independently loadable font files.
            ComPtr<IDWriteFontFile> file;
            if (FAILED(face->GetFiles(&count, file.GetAddressOf()))) continue;
            ComPtr<IDWriteFontFileLoader> loader;
            ComPtr<IDWriteLocalFontFileLoader> local;
            if (FAILED(file->GetLoader(&loader)) || FAILED(loader.As(&local))) continue;
            const void* key = nullptr;
            UINT32 keySize = 0, pathLength = 0;
            if (FAILED(file->GetReferenceKey(&key, &keySize)) ||
                FAILED(local->GetFilePathLengthFromKey(key, keySize, &pathLength))) continue;
            std::wstring path(pathLength + 1, L'\0');
            if (FAILED(local->GetFilePathFromKey(key, keySize, path.data(), pathLength + 1))) continue;
            path.resize(pathLength);
            const auto extension = std::filesystem::path(path).extension().wstring();
            if (_wcsicmp(extension.c_str(), L".ttf") && _wcsicmp(extension.c_str(), L".otf") &&
                _wcsicmp(extension.c_str(), L".ttc") && _wcsicmp(extension.c_str(), L".otc")) continue;
            const UINT32 faceIndex = face->GetIndex();
            if (unique.find({path, faceIndex}) != unique.end()) continue;
            ComPtr<IDWriteLocalizedStrings> names;
            BOOL exists = FALSE;
            font->GetInformationalStrings(DWRITE_INFORMATIONAL_STRING_FULL_NAME, &names, &exists);
            if (!exists && FAILED(family->GetFamilyNames(&names))) continue;
            if (!names) continue;
            auto name = FontName(names.Get());
            if (name.empty()) continue;
            const bool user = path.size() < systemPrefix.size() ||
                CompareStringOrdinal(path.data(), static_cast<int>(systemPrefix.size()), systemPrefix.c_str(),
                    static_cast<int>(systemPrefix.size()), TRUE) != CSTR_EQUAL;
            SystemFont item{PathUtils::WideToUtf8(name), PathUtils::WideToUtf8(path), user, static_cast<int>(faceIndex)};
            unique.emplace(std::make_pair(std::move(path), faceIndex), std::move(item));
        }
    }
    std::vector<SystemFont> result;
    result.reserve(unique.size());
    for (auto& entry : unique) result.push_back(std::move(entry.second));
    std::sort(result.begin(), result.end(), [](const SystemFont& a, const SystemFont& b) {
        return std::tie(a.userInstalled, a.name, a.path, a.faceIndex) < std::tie(b.userInstalled, b.name, b.path, b.faceIndex);
    });
    return result;
}
