#pragma once

#include <string_view>

namespace i18n {
// Display-only translation; canonical titles and resource IDs remain unchanged.
// Returned pointers remain valid across language changes.
const char* ResourceText(std::string_view source);
bool ResourceTextMatches(std::string_view source, std::wstring_view query);
}
