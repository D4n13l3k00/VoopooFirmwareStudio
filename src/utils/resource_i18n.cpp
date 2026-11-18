#include "resource_i18n.h"
#include "i18n.h"
#include "path_utils.h"

#include <array>
#include <map>
#include <span>
#include <string>
#include <algorithm>

namespace {
struct Translation {
    std::string_view key;
    const char* text[3];
};
constexpr Translation slavic[] = {
#include "resource_i18n_ru_uk_be.inc"
};
constexpr Translation western[] = {
#include "resource_i18n_pl_de_ro.inc"
};
constexpr Translation romance[] = {
#include "resource_i18n_es_fr_uz.inc"
};
constexpr Translation turkic[] = {
#include "resource_i18n_kk_az_ky.inc"
};
constexpr Translation caucasus[] = {
#include "resource_i18n_tg_hy_ka.inc"
};
struct Dictionary {
    std::span<const Translation> rows;
    size_t column;
};
Dictionary DictionaryFor(i18n::Language language) {
    using enum i18n::Language;
    switch (language) {
    case Russian: return {slavic, 0};
    case Ukrainian: return {slavic, 1};
    case Belarusian: return {slavic, 2};
    case Polish: return {western, 0};
    case German: return {western, 1};
    case Romanian: return {western, 2};
    case Spanish: return {romance, 0};
    case French: return {romance, 1};
    case Uzbek: return {romance, 2};
    case Kazakh: return {turkic, 0};
    case Azerbaijani: return {turkic, 1};
    case Kyrgyz: return {turkic, 2};
    case Tajik: return {caucasus, 0};
    case Armenian: return {caucasus, 1};
    case Georgian: return {caucasus, 2};
    default: return {};
    }
}

std::string Translate(std::string_view source, i18n::Language language) {
    if (language == i18n::Language::English) return std::string(source);
    std::string key;
    key.reserve(source.size());
    std::array<std::string_view, 10> arguments;
    size_t count = 0;
    for (size_t i = 0; i < source.size();) {
        if (source[i] < '0' || source[i] > '9') {
            key += source[i++];
            continue;
        }
        const size_t begin = i;
        while (i < source.size() && source[i] >= '0' && source[i] <= '9') ++i;
        if (count == arguments.size()) return std::string(source);
        arguments[count] = source.substr(begin, i - begin);
        key += '{';
        key += static_cast<char>('0' + count++);
        key += '}';
    }
    const auto dictionary = DictionaryFor(language);
    const auto found = std::lower_bound(dictionary.rows.begin(), dictionary.rows.end(), key,
        [](const Translation& row, std::string_view value) { return row.key < value; });
    if (found == dictionary.rows.end() || found->key != key) return std::string(source);
    const std::string_view pattern = found->text[dictionary.column];
    std::string translated;
    translated.reserve(pattern.size());
    for (size_t i = 0; i < pattern.size();) {
        if (i + 2 < pattern.size() && pattern[i] == '{' && pattern[i + 2] == '}' &&
            pattern[i + 1] >= '0' && pattern[i + 1] < '0' + count) {
            translated += arguments[pattern[i + 1] - '0'];
            i += 3;
        } else translated += pattern[i++];
    }
    return translated;
}

struct DisplayText {
    std::string utf8;
    std::wstring wide;
};
DisplayText& CachedText(std::string_view source, i18n::Language language) {
    static std::array<std::map<std::string, DisplayText, std::less<>>,
        static_cast<size_t>(i18n::Language::Count)> cache;
    auto& entries = cache[static_cast<size_t>(language)];
    const auto found = entries.find(source);
    if (found != entries.end()) return found->second;
    DisplayText text;
    text.utf8 = Translate(source, language);
    return entries.emplace(source, std::move(text)).first->second;
}
bool Contains(DisplayText& display, std::wstring_view query) {
    if (display.wide.empty()) display.wide = PathUtils::Utf8ToWide(display.utf8);
    const std::wstring_view text = display.wide;
    if (query.empty()) return true;
    if (text.empty()) return false;
    return FindNLSStringEx(LOCALE_NAME_INVARIANT, FIND_FROMSTART | NORM_IGNORECASE,
        text.data(), static_cast<int>(text.size()), query.data(), static_cast<int>(query.size()),
        nullptr, nullptr, nullptr, 0) >= 0;
}
}

namespace i18n {
const char* ResourceText(std::string_view source) {
    return CachedText(source, GetCurrentLanguage()).utf8.c_str();
}
bool ResourceTextMatches(std::string_view source, std::wstring_view query) {
    if (query.empty()) return true;
    const Language current = GetCurrentLanguage();
    if (Contains(CachedText(source, current), query)) return true;
    for (const auto& language : GetSupportedLanguages()) {
        if (language.lang != current && Contains(CachedText(source, language.lang), query))
            return true;
    }
    return false;
}
}
