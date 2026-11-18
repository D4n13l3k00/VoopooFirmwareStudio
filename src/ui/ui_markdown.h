#pragma once
#include "markdown/imgui_md.h"
#include <string_view>
#include <string>
#include <vector>

namespace UiMarkdown {
void InitializeFonts(const ImWchar* ranges);
ImFont* MonospaceFont();
// Cache parser events when release metadata changes; replay only layout per frame.
class Document final : private imgui_md {
public:
    Document();
    bool SetText(std::string_view);
    void Render();
private:
    enum class Kind { Block, Span, Text };
    struct Event {
        Kind kind;
        int type;
        bool enter = false;
        unsigned number = 0;
        char mark = 0;
        size_t offset = 0, size = 0;
    };
    std::vector<Event> events;
    std::string pool;
    std::vector<ImVec2> quotes;
    std::vector<bool> imageLinks;
    bool blockCode = false;
    unsigned strongDepth = 0, emphasisDepth = 0;
    void CollectBlock(MD_BLOCKTYPE, void*, bool);
    void CollectSpan(MD_SPANTYPE, void*, bool);
    void CollectText(MD_TEXTTYPE, const char*, MD_SIZE);
    void AppendEntity(std::string_view);
    void AppendAttribute(const MD_ATTRIBUTE&);
    ImFont* get_font() const override;
    void open_url() const override;
    void BLOCK_H(const MD_BLOCK_H_DETAIL*, bool) override;
    void BLOCK_QUOTE(bool) override;
    void BLOCK_CODE(const MD_BLOCK_CODE_DETAIL*, bool) override;
    void SPAN_CODE(bool) override;
    void SPAN_STRONG(bool) override;
    void SPAN_EM(bool) override;
    void SPAN_IMG(const MD_SPAN_IMG_DETAIL*, bool) override;
};
}
