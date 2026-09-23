#include "font_renderer.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"

#include "../third_party/stb_image_write.h"
#include "path_utils.h"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <windows.h>

namespace fs = std::filesystem;

struct RawBitmap {
    int width = 0;
    int height = 0;
    int xoff = 0;
    int yoff = 0;
    std::vector<uint8_t> data;
};

FontRenderer::FontRenderer() {
    m_fontInfo = malloc(sizeof(stbtt_fontinfo));
}

FontRenderer::~FontRenderer() {
    if (m_fontInfo) {
        free(m_fontInfo);
        m_fontInfo = nullptr;
    }
}

bool FontRenderer::LoadFontFile(const std::string& path) {
    auto file = PathUtils::OpenReadStream(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        m_loaded = false;
        return false;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    m_fontBuffer.resize(size);
    if (!file.read((char*)m_fontBuffer.data(), size)) {
        m_loaded = false;
        return false;
    }

    stbtt_fontinfo* info = (stbtt_fontinfo*)m_fontInfo;
    if (!stbtt_InitFont(info, m_fontBuffer.data(), stbtt_GetFontOffsetForIndex(m_fontBuffer.data(), 0))) {
        m_loaded = false;
        return false;
    }

    m_fontPath = path;
    m_loaded = true;
    return true;
}

static RawBitmap RasterizeChar(stbtt_fontinfo* info, char ch, float font_size, int oversample, uint8_t threshold) {
    RawBitmap res;
    int factor = (oversample < 1) ? 1 : oversample;
    float scale = stbtt_ScaleForPixelHeight(info, font_size * factor);

    int w = 0, h = 0, xoff = 0, yoff = 0;
    unsigned char* hr = stbtt_GetCodepointBitmap(info, 0, scale, (int)ch, &w, &h, &xoff, &yoff);
    if (!hr || w == 0 || h == 0) {
        if (hr) stbtt_FreeBitmap(hr, nullptr);
        return res;
    }

    if (factor == 1) {
        res.width = w;
        res.height = h;
        res.xoff = xoff;
        res.yoff = yoff;
        res.data.resize(w * h);
        for (int i = 0; i < w * h; ++i) {
            res.data[i] = (hr[i] >= threshold) ? 255 : 0;
        }
        stbtt_FreeBitmap(hr, nullptr);
        return res;
    }

    int target_w = (w + factor - 1) / factor;
    int target_h = (h + factor - 1) / factor;
    res.width = target_w;
    res.height = target_h;
    res.xoff = xoff / factor;
    res.yoff = yoff / factor;
    res.data.resize(target_w * target_h, 0);

    for (int ty = 0; ty < target_h; ++ty) {
        for (int tx = 0; tx < target_w; ++tx) {
            uint32_t sum = 0;
            uint32_t count = 0;
            int sy_start = ty * factor;
            int sy_end = (std::min)((ty + 1) * factor, h);
            int sx_start = tx * factor;
            int sx_end = (std::min)((tx + 1) * factor, w);

            for (int sy = sy_start; sy < sy_end; ++sy) {
                for (int sx = sx_start; sx < sx_end; ++sx) {
                    sum += hr[sy * w + sx];
                    count++;
                }
            }
            uint8_t avg = (count > 0) ? (uint8_t)(sum / (factor * factor)) : 0;
            res.data[ty * target_w + tx] = (avg >= threshold) ? 255 : 0;
        }
    }

    stbtt_FreeBitmap(hr, nullptr);
    return res;
}

static bool GetActiveBBox(const RawBitmap& raw, int& min_x, int& min_y, int& max_x, int& max_y) {
    if (raw.width == 0 || raw.height == 0) return false;
    min_x = raw.width;
    max_x = -1;
    min_y = raw.height;
    max_y = -1;

    for (int y = 0; y < raw.height; ++y) {
        for (int x = 0; x < raw.width; ++x) {
            if (raw.data[y * raw.width + x] > 0) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
    }
    return (max_x >= 0);
}

float FontRenderer::FindOptimalFontSize(const std::vector<char>& chars, int target_w, int target_h, int pad_x, int pad_y, int oversample, uint8_t threshold, bool stretch) {
    stbtt_fontinfo* info = (stbtt_fontinfo*)m_fontInfo;
    int max_w = (std::max)(1, target_w - pad_x * 2);
    int max_h = (std::max)(1, target_h - pad_y * 2);

    float low = 1.0f;
    float high = (float)(target_h * 4);
    float best = 1.0f;

    for (int iter = 0; iter < 32; ++iter) {
        float mid = (low + high) * 0.5f;
        bool fits = true;

        for (char ch : chars) {
            RawBitmap raw = RasterizeChar(info, ch, mid, oversample, threshold);
            int min_x, min_y, max_x, max_y;
            if (GetActiveBBox(raw, min_x, min_y, max_x, max_y)) {
                int act_w = max_x - min_x + 1;
                int act_h = max_y - min_y + 1;

                if (stretch) {
                    if (act_h > max_h) { fits = false; break; }
                } else {
                    if (act_w > max_w || act_h > max_h) { fits = false; break; }
                }
            }
        }

        if (fits) {
            best = mid;
            low = mid;
        } else {
            high = mid;
        }
    }
    return best;
}

int FontRenderer::ComputeCommonBaseline(const std::vector<char>& chars, float font_size, const RenderOptions& opts) {
    stbtt_fontinfo* info = (stbtt_fontinfo*)m_fontInfo;
    int max_top = -999999;
    int min_bottom = 999999;

    for (char ch : chars) {
        RawBitmap raw = RasterizeChar(info, ch, font_size, opts.oversample, opts.threshold);
        int min_x, min_y, max_x, max_y;
        if (GetActiveBBox(raw, min_x, min_y, max_x, max_y)) {
            int top_rel = raw.yoff + min_y;
            int bot_rel = raw.yoff + max_y + 1;
            if (top_rel > max_top) max_top = top_rel;
            if (bot_rel < min_bottom) min_bottom = bot_rel;
        }
    }

    if (max_top == -999999 || min_bottom == 999999) {
        return opts.target_height / 2;
    }

    int span_h = max_top - min_bottom;
    return opts.target_height / 2;
}

std::pair<float, std::vector<RenderedGlyph>> FontRenderer::RenderPreset(const Preset& preset, const RenderOptions& opts) {
    std::vector<RenderedGlyph> result;
    if (!m_loaded) return { 0.0f, result };

    stbtt_fontinfo* info = (stbtt_fontinfo*)m_fontInfo;
    std::vector<char> chars;
    for (const auto& g : preset.glyphs) chars.push_back(g.ch);

    float font_size = FindOptimalFontSize(chars, opts.target_width, opts.target_height, opts.padding_x, opts.padding_y, opts.oversample, opts.threshold, opts.stretch);

    // Compute baseline stats if needed
    int common_baseline_y = opts.target_height / 2;
    int max_act_h = 0;
    for (char ch : chars) {
        RawBitmap raw = RasterizeChar(info, ch, font_size, opts.oversample, opts.threshold);
        int min_x, min_y, max_x, max_y;
        if (GetActiveBBox(raw, min_x, min_y, max_x, max_y)) {
            int act_h = max_y - min_y + 1;
            if (act_h > max_act_h) max_act_h = act_h;
        }
    }

    for (const auto& gdef : preset.glyphs) {
        RenderedGlyph rg;
        rg.ch = gdef.ch;
        rg.filename = gdef.filename;
        rg.width = opts.target_width;
        rg.height = opts.target_height;
        rg.pixels.assign(opts.target_width * opts.target_height, 0);

        RawBitmap raw = RasterizeChar(info, gdef.ch, font_size, opts.oversample, opts.threshold);
        int min_x, min_y, max_x, max_y;
        if (GetActiveBBox(raw, min_x, min_y, max_x, max_y)) {
            int act_w = max_x - min_x + 1;
            int act_h = max_y - min_y + 1;
            rg.active_width = act_w;
            rg.active_height = act_h;

            if (opts.stretch && act_w > 0 && act_h > 0) {
                int draw_w = (std::max)(1, opts.target_width - opts.padding_x * 2);
                int draw_h = (std::max)(1, opts.target_height - opts.padding_y * 2);

                for (int dy = 0; dy < draw_h; ++dy) {
                    for (int dx = 0; dx < draw_w; ++dx) {
                        int sx = min_x + (dx * act_w) / draw_w;
                        int sy = min_y + (dy * act_h) / draw_h;
                        if (sx < raw.width && sy < raw.height && raw.data[sy * raw.width + sx] > 0) {
                            int out_x = opts.padding_x + dx;
                            int out_y = opts.padding_y + dy;
                            if (out_x >= 0 && out_x < opts.target_width && out_y >= 0 && out_y < opts.target_height) {
                                rg.pixels[out_y * opts.target_width + out_x] = 255;
                            }
                        }
                    }
                }
            } else {
                int offset_x = (opts.target_width - act_w) / 2 - min_x;
                int offset_y = 0;
                if (opts.align_center) {
                    offset_y = (opts.target_height - act_h) / 2 - min_y;
                } else {
                    // Place so bottom is aligned comfortably
                    int top_pad = (opts.target_height - max_act_h) / 2;
                    offset_y = top_pad + (max_act_h - act_h) - min_y;
                }

                for (int gy = min_y; gy <= max_y; ++gy) {
                    for (int gx = min_x; gx <= max_x; ++gx) {
                        if (raw.data[gy * raw.width + gx] > 0) {
                            int out_x = gx + offset_x;
                            int out_y = gy + offset_y;
                            if (out_x >= 0 && out_x < opts.target_width && out_y >= 0 && out_y < opts.target_height) {
                                rg.pixels[out_y * opts.target_width + out_x] = 255;
                            }
                        }
                    }
                }
            }
        }

        result.push_back(std::move(rg));
    }

    return { font_size, result };
}

bool FontRenderer::SaveGlyphs(const std::vector<RenderedGlyph>& glyphs, const std::string& out_dir, std::string& err_msg) {
    try {
        fs::create_directories(PathUtils::ToFsPath(out_dir));
    } catch (const std::exception& e) {
        err_msg = e.what();
        return false;
    }

    for (const auto& g : glyphs) {
        fs::path p = PathUtils::ToFsPath(out_dir) / PathUtils::Utf8ToWide(g.filename);
        FILE* fp = _wfopen(p.c_str(), L"wb");
        if (!fp) {
            err_msg = "Failed to open PNG file for writing: " + g.filename;
            return false;
        }
        int res = stbi_write_png_to_func([](void* context, void* data, int size) {
            fwrite(data, 1, size, (FILE*)context);
        }, fp, g.width, g.height, 1, g.pixels.data(), g.width);
        fclose(fp);
        if (!res) {
            err_msg = "Failed to write PNG file: " + g.filename;
            return false;
        }
    }
    return true;
}

std::vector<SystemFont> FontRenderer::ListPopularWindowsFonts() {
    std::vector<SystemFont> result;
    const std::pair<const char*, const char*> popular[] = {
        { "Impact", "impact.ttf" },
        { "Consolas Bold", "consolab.ttf" },
        { "Consolas Regular", "consola.ttf" },
        { "Arial Bold", "arialbd.ttf" },
        { "Arial Regular", "arial.ttf" },
        { "Segoe UI Bold", "segoeuib.ttf" },
        { "Segoe UI Regular", "segoeui.ttf" },
        { "Bahnschrift (DIN)", "bahnschrift.ttf" },
        { "Verdana Bold", "verdanab.ttf" },
        { "Trebuchet MS Bold", "trebucbd.ttf" },
    };

    char winDir[MAX_PATH];
    if (GetWindowsDirectoryA(winDir, MAX_PATH)) {
        fs::path fontsDir = fs::path(winDir) / "Fonts";
        for (const auto& item : popular) {
            fs::path p = fontsDir / item.second;
            if (fs::exists(p)) {
                result.push_back({ item.first, p.string() });
            }
        }
    }
    return result;
}
