// MD4C v0.6.0 (7fc1815), imgui_md b9f86be; licenses in third_party/markdown.
// Renderer adaptations: current ImGui API, UTF-8 wrapping, cached-event access.
#include "ui_markdown.h"
#include "path_utils.h"
#include <charconv>
#include <filesystem>
#include <limits>
#include <shellapi.h>
extern "C" {
#include "markdown/entity.h"
}

namespace UiMarkdown {
namespace {
struct Fonts { ImFont* regular; ImFont* bold; ImFont* italic; ImFont* boldItalic; ImFont* mono; } fonts{};
void AppendCodepoint(std::string& out, unsigned point) {
    if (!point || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) point = 0xfffd;
    if (point < 0x80) out += static_cast<char>(point);
    else if (point < 0x800) {
        out += static_cast<char>(0xc0 | (point >> 6)); out += static_cast<char>(0x80 | (point & 63));
    } else if (point < 0x10000) {
        out += static_cast<char>(0xe0 | (point >> 12)); out += static_cast<char>(0x80 | ((point >> 6) & 63));
        out += static_cast<char>(0x80 | (point & 63));
    } else {
        out += static_cast<char>(0xf0 | (point >> 18)); out += static_cast<char>(0x80 | ((point >> 12) & 63));
        out += static_cast<char>(0x80 | ((point >> 6) & 63)); out += static_cast<char>(0x80 | (point & 63));
    }
}
}
void InitializeFonts(const ImWchar* ranges) {
    auto* atlas = ImGui::GetIO().Fonts;
    fonts.regular = atlas->Fonts[0];
    wchar_t windows[MAX_PATH]{};
    const UINT length = GetWindowsDirectoryW(windows, MAX_PATH);
    if (!length || length >= MAX_PATH) {
        fonts.bold = fonts.italic = fonts.boldItalic = fonts.mono = fonts.regular;
        return;
    }
    const auto directory = std::filesystem::path(windows) / L"Fonts";
    auto load = [&](const wchar_t* name) {
        const auto path = directory / name;
        if (!std::filesystem::exists(path)) return fonts.regular;
        const auto utf8 = PathUtils::WideToUtf8(path.wstring());
        auto* font = atlas->AddFontFromFileTTF(utf8.c_str(), fonts.regular->LegacySize, nullptr, ranges);
        return font ? font : fonts.regular;
    };
    fonts.bold = load(L"segoeuib.ttf");
    fonts.italic = load(L"segoeuii.ttf");
    fonts.boldItalic = load(L"segoeuiz.ttf");
    fonts.mono = load(L"consola.ttf");
}
ImFont* MonospaceFont() { return fonts.mono ? fonts.mono : ImGui::GetFont(); }
Document::Document() {
    m_md.flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_NOHTML;
    m_md.enter_block = [](MD_BLOCKTYPE type, void* detail, void* self) {
        static_cast<Document*>(self)->CollectBlock(type, detail, true); return 0;
    };
    m_md.leave_block = [](MD_BLOCKTYPE type, void* detail, void* self) {
        static_cast<Document*>(self)->CollectBlock(type, detail, false); return 0;
    };
    m_md.enter_span = [](MD_SPANTYPE type, void* detail, void* self) {
        static_cast<Document*>(self)->CollectSpan(type, detail, true); return 0;
    };
    m_md.leave_span = [](MD_SPANTYPE type, void* detail, void* self) {
        static_cast<Document*>(self)->CollectSpan(type, detail, false); return 0;
    };
    m_md.text = [](MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* self) {
        static_cast<Document*>(self)->CollectText(type, text, size); return 0;
    };
}
bool Document::SetText(std::string_view source) {
    events.clear(); pool.clear();
    if (source.size() > (std::numeric_limits<MD_SIZE>::max)()) return false;
    pool.reserve(source.size());
    if (source.empty()) return true;
    const bool ok = md_parse(source.data(), static_cast<MD_SIZE>(source.size()), &m_md, this) == 0;
    if (!ok) { events.clear(); pool.clear(); }
    return ok;
}
void Document::CollectBlock(MD_BLOCKTYPE type, void* detail, bool enter) {
    Event event{Kind::Block, type, enter};
    if (type == MD_BLOCK_H) event.number = static_cast<MD_BLOCK_H_DETAIL*>(detail)->level;
    else if (type == MD_BLOCK_UL) event.mark = static_cast<MD_BLOCK_UL_DETAIL*>(detail)->mark;
    else if (type == MD_BLOCK_OL) {
        const auto* list = static_cast<MD_BLOCK_OL_DETAIL*>(detail);
        event.number = list->start; event.mark = list->mark_delimiter;
    } else if (type == MD_BLOCK_TD || type == MD_BLOCK_TH)
        event.number = static_cast<MD_BLOCK_TD_DETAIL*>(detail)->align;
    events.push_back(event);
}
void Document::CollectSpan(MD_SPANTYPE type, void* detail, bool enter) {
    Event event{Kind::Span, type, enter};
    event.offset = pool.size();
    if (enter && type == MD_SPAN_A) AppendAttribute(static_cast<MD_SPAN_A_DETAIL*>(detail)->href);
    else if (enter && type == MD_SPAN_IMG) AppendAttribute(static_cast<MD_SPAN_IMG_DETAIL*>(detail)->src);
    event.size = pool.size() - event.offset;
    events.push_back(event);
}
void Document::AppendEntity(std::string_view value) {
    if (const auto* entity = entity_lookup(value.data(), value.size())) {
        AppendCodepoint(pool, entity->codepoints[0]);
        if (entity->codepoints[1]) AppendCodepoint(pool, entity->codepoints[1]);
        return;
    }
    if (value.size() >= 4 && value.starts_with("&#") && value.back() == ';') {
        value.remove_prefix(2); value.remove_suffix(1);
        const bool hex = value.starts_with('x') || value.starts_with('X');
        if (hex) value.remove_prefix(1);
        unsigned point = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), point, hex ? 16 : 10);
        AppendCodepoint(pool, parsed.ec == std::errc{} ? point : 0xfffd);
    } else pool.append(value);
}
void Document::AppendAttribute(const MD_ATTRIBUTE& value) {
    if (!value.size) return;
    if (!value.substr_types || !value.substr_offsets) { pool.append(value.text, value.size); return; }
    for (size_t i = 0; value.substr_offsets[i] < value.size; ++i) {
        const auto start = value.substr_offsets[i], end = value.substr_offsets[i + 1];
        if (value.substr_types[i] == MD_TEXT_ENTITY) AppendEntity({value.text + start, end - start});
        else if (value.substr_types[i] == MD_TEXT_NULLCHAR) AppendCodepoint(pool, 0xfffd);
        else pool.append(value.text + start, end - start);
    }
}
void Document::CollectText(MD_TEXTTYPE type, const char* text, MD_SIZE size) {
    Event event{Kind::Text, type};
    event.offset = pool.size();
    if (type == MD_TEXT_ENTITY) { AppendEntity({text, size}); event.type = MD_TEXT_NORMAL; }
    else if (type == MD_TEXT_NULLCHAR) { AppendCodepoint(pool, 0xfffd); event.type = MD_TEXT_NORMAL; }
    else if (size) pool.append(text, size);
    event.size = pool.size() - event.offset;
    events.push_back(event);
}
void Document::Render() {
    quotes.clear(); imageLinks.clear();
    blockCode = false; strongDepth = emphasisDepth = 0;
    for (const auto& event : events) {
        if (event.kind == Kind::Text) {
            const char* begin = pool.data() + event.offset;
            const char* end = begin + event.size;
            if (blockCode && event.type == MD_TEXT_CODE) {
                if (end > begin && end[-1] == '\n') --end;
                ImGui::TextUnformatted(begin, end);
            } else text(static_cast<MD_TEXTTYPE>(event.type), begin, end);
        } else if (event.kind == Kind::Block) {
            MD_BLOCK_H_DETAIL heading{event.number};
            MD_BLOCK_UL_DETAIL unordered{0, event.mark};
            MD_BLOCK_OL_DETAIL ordered{event.number, 0, event.mark};
            MD_BLOCK_TD_DETAIL cell{static_cast<MD_ALIGN>(event.number)};
            void* detail = nullptr;
            if (event.type == MD_BLOCK_H) detail = &heading;
            else if (event.type == MD_BLOCK_UL) detail = &unordered;
            else if (event.type == MD_BLOCK_OL) detail = &ordered;
            else if (event.type == MD_BLOCK_TD || event.type == MD_BLOCK_TH) detail = &cell;
            block(static_cast<MD_BLOCKTYPE>(event.type), detail, event.enter);
        } else {
            MD_ATTRIBUTE attribute{};
            attribute.text = pool.data() + event.offset; attribute.size = static_cast<MD_SIZE>(event.size);
            MD_SPAN_A_DETAIL link{}; link.href = attribute;
            MD_SPAN_IMG_DETAIL image{}; image.src = attribute;
            void* detail = event.type == MD_SPAN_A ? static_cast<void*>(&link) :
                event.type == MD_SPAN_IMG ? static_cast<void*>(&image) : nullptr;
            span(static_cast<MD_SPANTYPE>(event.type), detail, event.enter);
        }
    }
    ImGui::NewLine();
}
ImFont* Document::get_font() const {
    if (m_is_code) return fonts.mono;
    if (m_is_strong || m_hlevel || m_is_table_header) return m_is_em ? fonts.boldItalic : fonts.bold;
    return m_is_em ? fonts.italic : fonts.regular;
}
void Document::BLOCK_H(const MD_BLOCK_H_DETAIL* detail, bool enter) {
    if (enter) {
        const float size = ImGui::GetStyle().FontSizeBase;
        imgui_md::BLOCK_H(detail, true);
        const float scale = detail->level == 1 ? 1.6f : detail->level == 2 ? 1.35f : 1.15f;
        ImGui::PushFont(get_font(), size * scale);
    } else { ImGui::PopFont(); imgui_md::BLOCK_H(detail, false); }
}
void Document::BLOCK_QUOTE(bool enter) {
    if (enter) { ImGui::NewLine(); quotes.push_back(ImGui::GetCursorScreenPos()); ImGui::Indent(12); }
    else {
        ImGui::Unindent(12);
        const ImVec2 start = quotes.back(); quotes.pop_back();
        ImGui::GetWindowDrawList()->AddLine(start, ImVec2(start.x, ImGui::GetCursorScreenPos().y),
            ImGui::GetColorU32(ImGuiCol_TextDisabled), 2);
        ImGui::NewLine();
    }
}
void Document::BLOCK_CODE(const MD_BLOCK_CODE_DETAIL*, bool enter) {
    m_is_code = blockCode = enter;
    if (enter) {
        ImGui::NewLine(); set_font(true);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0));
    } else { ImGui::PopStyleVar(); set_font(false); ImGui::NewLine(); }
}
void Document::SPAN_CODE(bool enter) { m_is_code = enter; set_font(enter); }
void Document::SPAN_STRONG(bool enter) {
    if (enter) ++strongDepth; else --strongDepth;
    imgui_md::SPAN_STRONG(enter); m_is_strong = strongDepth != 0;
}
void Document::SPAN_EM(bool enter) {
    if (enter) ++emphasisDepth; else --emphasisDepth;
    imgui_md::SPAN_EM(enter); m_is_em = emphasisDepth != 0;
}
void Document::SPAN_IMG(const MD_SPAN_IMG_DETAIL* detail, bool enter) {
    // No network image loading. Render alt text as a link, preserving an outer link.
    if (enter) {
        const bool ownLink = m_href.empty(); imageLinks.push_back(ownLink);
        if (ownLink) { MD_SPAN_A_DETAIL link{}; link.href = detail->src; SPAN_A(&link, true); }
    } else {
        if (imageLinks.back()) { MD_SPAN_A_DETAIL link{}; SPAN_A(&link, false); }
        imageLinks.pop_back();
    }
}
void Document::open_url() const {
    if (!m_href.starts_with("https://") && !m_href.starts_with("http://")) return;
    for (const unsigned char ch : m_href) if (ch <= ' ' || ch == '\\' || ch == 127) return;
    const auto url = PathUtils::Utf8ToWide(m_href);
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
}
