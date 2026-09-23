#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "presets.h"

struct RenderOptions {
    int target_width = 19;
    int target_height = 40;
    uint8_t threshold = 110;
    int oversample = 4;
    int padding_x = 0;
    int padding_y = 0;
    bool stretch = false;
    bool align_center = false; // false: baseline, true: center
};

struct RenderedGlyph {
    char ch = 0;
    std::string filename;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels; // values 0 or 255
    int active_width = 0;
    int active_height = 0;
};

struct SystemFont {
    std::string name;
    std::string path;
};

class FontRenderer {
public:
    FontRenderer();
    ~FontRenderer();

    bool LoadFontFile(const std::string& path);
    bool IsLoaded() const { return m_loaded; }
    const std::string& GetLoadedPath() const { return m_fontPath; }

    std::pair<float, std::vector<RenderedGlyph>> RenderPreset(const Preset& preset, const RenderOptions& opts);
    static bool SaveGlyphs(const std::vector<RenderedGlyph>& glyphs, const std::string& out_dir, std::string& err_msg);
    static std::vector<SystemFont> ListPopularWindowsFonts();

private:
    float FindOptimalFontSize(const std::vector<char>& chars, int target_w, int target_h, int pad_x, int pad_y, int oversample, uint8_t threshold, bool stretch);
    int ComputeCommonBaseline(const std::vector<char>& chars, float font_size, const RenderOptions& opts);

    bool m_loaded = false;
    std::string m_fontPath;
    std::vector<uint8_t> m_fontBuffer;
    void* m_fontInfo = nullptr; // stbtt_fontinfo
};
