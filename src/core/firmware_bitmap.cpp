#include "firmware_bitmap.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#include "path_utils.h"

#include <cmath>
#include <algorithm>

namespace FirmwareBitmap {

static stbi_uc* StbiLoadUtf8(const std::string& path, int* sw, int* sh, int* comp, int req_comp) {
    FILE* f = PathUtils::FopenUtf8(path, L"rb");
    if (!f) return nullptr;
    stbi_uc* res = stbi_load_from_file(f, sw, sh, comp, req_comp);
    fclose(f);
    return res;
}

static int StbiWritePngUtf8(const std::string& path, int w, int h, int comp, const void* data, int stride) {
    FILE* f = PathUtils::FopenUtf8(path, L"wb");
    if (!f) return 0;
    int res = stbi_write_png_to_func([](void* context, void* data, int size) {
        fwrite(data, 1, size, (FILE*)context);
    }, f, w, h, comp, data, stride);
    fclose(f);
    return res;
}

bool DecodeMask(const uint8_t* raw, size_t raw_len, uint32_t width, uint32_t height, std::vector<uint8_t>& out_gray) {
    size_t stride = (width + 7) / 8;
    if (raw_len != stride * height) {
        return false;
    }
    out_gray.assign(width * height, 0);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t byte = raw[y * stride + (x / 8)];
            if ((byte & (1 << (x & 7))) != 0) {
                out_gray[y * width + x] = 255;
            } else {
                out_gray[y * width + x] = 0;
            }
        }
    }
    return true;
}

bool EncodeMaskPreserving(
    const uint8_t* gray_pixels,
    uint32_t width,
    uint32_t height,
    const uint8_t* original_raw,
    size_t raw_len,
    std::vector<uint8_t>& out_raw
) {
    size_t stride = (width + 7) / 8;
    if (raw_len != stride * height) {
        return false;
    }
    out_raw.assign(original_raw, original_raw + raw_len);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t& byte = out_raw[y * stride + (x / 8)];
            uint8_t mask = (uint8_t)(1 << (x & 7));
            if (gray_pixels[y * width + x] >= 128) {
                byte |= mask;
            } else {
                byte &= ~mask;
            }
        }
    }
    return true;
}

void FitGrayscale(
    const uint8_t* src_gray,
    int sw,
    int sh,
    uint32_t target_w,
    uint32_t target_h,
    std::vector<uint8_t>& out_gray
) {
    if (!src_gray || sw <= 0 || sh <= 0 || target_w == 0 || target_h == 0) {
        out_gray.assign(target_w * target_h, 0);
        return;
    }

    float scale = (std::min)((float)target_w / (float)sw, (float)target_h / (float)sh);
    uint32_t nw = (uint32_t)std::clamp(std::round(sw * scale), 1.0f, (float)target_w);
    uint32_t nh = (uint32_t)std::clamp(std::round(sh * scale), 1.0f, (float)target_h);

    uint32_t offset_x = (target_w - nw) / 2;
    uint32_t offset_y = (target_h - nh) / 2;

    out_gray.assign(target_w * target_h, 0);

    for (uint32_t dy = 0; dy < nh; ++dy) {
        int sy = (int)((float)dy / scale);
        if (sy >= sh) sy = sh - 1;
        for (uint32_t dx = 0; dx < nw; ++dx) {
            int sx = (int)((float)dx / scale);
            if (sx >= sw) sx = sw - 1;

            uint8_t val = src_gray[sy * sw + sx];
            uint8_t binary = (val >= 128) ? 255 : 0;
            out_gray[(offset_y + dy) * target_w + (offset_x + dx)] = binary;
        }
    }
}

bool LoadAndFit(
    const std::string& path,
    uint32_t target_w,
    uint32_t target_h,
    std::vector<uint8_t>& out_gray,
    std::string& out_err
) {
    int sw = 0, sh = 0, comp = 0;
    stbi_uc* data = StbiLoadUtf8(path, &sw, &sh, &comp, 1); // Force 1-channel (grayscale)
    if (!data || sw <= 0 || sh <= 0) {
        out_err = "Cannot decode image file or empty: " + path;
        if (data) stbi_image_free(data);
        return false;
    }

    FitGrayscale(data, sw, sh, target_w, target_h, out_gray);
    stbi_image_free(data);
    return true;
}

bool SavePng(
    const std::string& path,
    const uint8_t* gray_pixels,
    uint32_t width,
    uint32_t height,
    std::string& out_err
) {
    int ret = StbiWritePngUtf8(path, (int)width, (int)height, 1, gray_pixels, (int)width);
    if (!ret) {
        out_err = "Failed to write PNG file: " + path;
        return false;
    }
    return true;
}

void ColorizeMask(
    const uint8_t* gray_pixels,
    uint32_t width,
    uint32_t height,
    Rgb8 fg,
    Rgb8 bg,
    std::vector<uint32_t>& out_rgba32
) {
    Rgb8 sfg = fg.ScreenColor();
    Rgb8 sbg = bg.ScreenColor();

    uint32_t fg_rgba = 0xFF000000 | (sfg.b << 16) | (sfg.g << 8) | sfg.r;
    uint32_t bg_rgba = 0xFF000000 | (sbg.b << 16) | (sbg.g << 8) | sbg.r;

    out_rgba32.resize(width * height);
    for (size_t i = 0; i < width * height; ++i) {
        out_rgba32[i] = (gray_pixels[i] > 0) ? fg_rgba : bg_rgba;
    }
}

bool DecodeRgb565(
    const uint8_t* raw,
    size_t raw_len,
    uint32_t width,
    uint32_t height,
    std::vector<uint32_t>& out_rgba32
) {
    if (!raw || raw_len < (size_t)width * height * 2) {
        return false;
    }
    out_rgba32.resize((size_t)width * height);
    for (size_t i = 0; i < (size_t)width * height; ++i) {
        uint16_t c565 = (uint16_t)raw[i * 2] | ((uint16_t)raw[i * 2 + 1] << 8);
        uint8_t r = (uint8_t)((((c565 >> 11) & 0x1F) * 255 + 15) / 31);
        uint8_t g = (uint8_t)((((c565 >> 5) & 0x3F) * 255 + 31) / 63);
        uint8_t b = (uint8_t)(((c565 & 0x1F) * 255 + 15) / 31);
        out_rgba32[i] = 0xFF000000 | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    }
    return true;
}

bool EncodeRgb565(
    const uint32_t* rgba32,
    uint32_t width,
    uint32_t height,
    std::vector<uint8_t>& out_raw
) {
    if (!rgba32) return false;
    out_raw.resize((size_t)width * height * 2);
    for (size_t i = 0; i < (size_t)width * height; ++i) {
        uint32_t px = rgba32[i];
        uint8_t r = (uint8_t)(px & 0xFF);
        uint8_t g = (uint8_t)((px >> 8) & 0xFF);
        uint8_t b = (uint8_t)((px >> 16) & 0xFF);
        uint16_t c565 = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        out_raw[i * 2 + 0] = (uint8_t)(c565 & 0xFF);
        out_raw[i * 2 + 1] = (uint8_t)((c565 >> 8) & 0xFF);
    }
    return true;
}

void FitRgba32(
    const uint8_t* src_rgba,
    int sw,
    int sh,
    uint32_t target_w,
    uint32_t target_h,
    std::vector<uint32_t>& out_rgba32
) {
    if (!src_rgba || sw <= 0 || sh <= 0 || target_w == 0 || target_h == 0) {
        out_rgba32.assign((size_t)target_w * target_h, 0xFF000000);
        return;
    }

    std::vector<uint8_t> local_pixels(src_rgba, src_rgba + (size_t)sw * sh * 4);

    // Auto-detect rotation: if user loads a horizontal image (e.g. 136x22) for a vertical target (e.g. 22x136)
    if (target_w < target_h && sw > sh) {
        std::vector<uint8_t> rotated((size_t)sw * sh * 4);
        int rot_w = sh;
        int rot_h = sw;
        for (int y = 0; y < sh; ++y) {
            for (int x = 0; x < sw; ++x) {
                int src_idx = (y * sw + x) * 4;
                int dst_x = sh - 1 - y;
                int dst_y = x;
                int dst_idx = (dst_y * rot_w + dst_x) * 4;
                for (int c = 0; c < 4; ++c) {
                    rotated[dst_idx + c] = local_pixels[src_idx + c];
                }
            }
        }
        local_pixels = std::move(rotated);
        sw = rot_w;
        sh = rot_h;
    }

    float scale = (std::min)((float)target_w / (float)sw, (float)target_h / (float)sh);
    uint32_t nw = (uint32_t)std::clamp(std::round(sw * scale), 1.0f, (float)target_w);
    uint32_t nh = (uint32_t)std::clamp(std::round(sh * scale), 1.0f, (float)target_h);

    uint32_t offset_x = (target_w - nw) / 2;
    uint32_t offset_y = (target_h - nh) / 2;

    out_rgba32.assign((size_t)target_w * target_h, 0xFF000000);

    for (uint32_t dy = 0; dy < nh; ++dy) {
        int sy = (int)((float)dy / scale);
        if (sy >= sh) sy = sh - 1;
        for (uint32_t dx = 0; dx < nw; ++dx) {
            int sx = (int)((float)dx / scale);
            if (sx >= sw) sx = sw - 1;

            size_t src_idx = ((size_t)sy * sw + sx) * 4;
            uint8_t r = local_pixels[src_idx + 0];
            uint8_t g = local_pixels[src_idx + 1];
            uint8_t b = local_pixels[src_idx + 2];
            uint8_t a = local_pixels[src_idx + 3];

            if (a < 255) {
                r = (uint8_t)(((uint32_t)r * a) / 255);
                g = (uint8_t)(((uint32_t)g * a) / 255);
                b = (uint8_t)(((uint32_t)b * a) / 255);
            }

            out_rgba32[(offset_y + dy) * target_w + (offset_x + dx)] =
                0xFF000000 | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
        }
    }
}

bool LoadAndFitRgb565(
    const std::string& path,
    uint32_t target_w,
    uint32_t target_h,
    std::vector<uint32_t>& out_rgba32,
    std::string& out_err
) {
    int sw = 0, sh = 0, comp = 0;
    stbi_uc* data = StbiLoadUtf8(path, &sw, &sh, &comp, 4); // Force 4-channel (RGBA)
    if (!data || sw <= 0 || sh <= 0) {
        out_err = "Cannot decode image file or empty: " + path;
        if (data) stbi_image_free(data);
        return false;
    }

    FitRgba32(data, sw, sh, target_w, target_h, out_rgba32);
    stbi_image_free(data);
    return true;
}

bool SavePngRgb565(
    const std::string& path,
    const uint32_t* rgba32,
    uint32_t width,
    uint32_t height,
    std::string& out_err
) {
    if (!rgba32) {
        out_err = "Null pixel buffer";
        return false;
    }
    std::vector<uint8_t> pngBytes((size_t)width * height * 4);
    for (size_t i = 0; i < (size_t)width * height; ++i) {
        uint32_t px = rgba32[i];
        pngBytes[i * 4 + 0] = (uint8_t)(px & 0xFF);         // R
        pngBytes[i * 4 + 1] = (uint8_t)((px >> 8) & 0xFF);  // G
        pngBytes[i * 4 + 2] = (uint8_t)((px >> 16) & 0xFF); // B
        pngBytes[i * 4 + 3] = (uint8_t)((px >> 24) & 0xFF); // A
    }
    int ret = StbiWritePngUtf8(path, (int)width, (int)height, 4, pngBytes.data(), (int)width * 4);
    if (!ret) {
        out_err = "Failed to write PNG: " + path;
        return false;
    }
    return true;
}

bool SavePngRgb565Rotated(
    const std::string& path,
    const uint32_t* rgba32,
    uint32_t width,
    uint32_t height,
    bool counterClockwise,
    std::string& out_err
) {
    if (!rgba32) {
        out_err = "Null pixel buffer";
        return false;
    }
    uint32_t new_w = height;
    uint32_t new_h = width;
    std::vector<uint8_t> pngBytes((size_t)new_w * new_h * 4);

    for (uint32_t y = 0; y < new_h; ++y) {
        for (uint32_t x = 0; x < new_w; ++x) {
            uint32_t px = 0;
            if (counterClockwise) {
                px = rgba32[x * width + (width - 1 - y)];
            } else {
                px = rgba32[(height - 1 - x) * width + y];
            }
            size_t dst_idx = ((size_t)y * new_w + x) * 4;
            pngBytes[dst_idx + 0] = (uint8_t)(px & 0xFF);
            pngBytes[dst_idx + 1] = (uint8_t)((px >> 8) & 0xFF);
            pngBytes[dst_idx + 2] = (uint8_t)((px >> 16) & 0xFF);
            pngBytes[dst_idx + 3] = (uint8_t)((px >> 24) & 0xFF);
        }
    }

    int ret = StbiWritePngUtf8(path, (int)new_w, (int)new_h, 4, pngBytes.data(), (int)new_w * 4);
    if (!ret) {
        out_err = "Failed to write PNG: " + path;
        return false;
    }
    return true;
}

void RecolorRgb565(
    const uint32_t* in_rgba,
    uint32_t width,
    uint32_t height,
    Rgb8 targetColor,
    std::vector<uint32_t>& out_rgba
) {
    out_rgba.resize((size_t)width * height);
    for (size_t i = 0; i < (size_t)width * height; ++i) {
        uint32_t px = in_rgba[i];
        uint8_t r = (uint8_t)(px & 0xFF);
        uint8_t g = (uint8_t)((px >> 8) & 0xFF);
        uint8_t b = (uint8_t)((px >> 16) & 0xFF);
        float lum = (r * 0.299f + g * 0.587f + b * 0.114f) / 255.0f;
        uint8_t nr = (uint8_t)std::clamp((float)targetColor.r * lum, 0.0f, 255.0f);
        uint8_t ng = (uint8_t)std::clamp((float)targetColor.g * lum, 0.0f, 255.0f);
        uint8_t nb = (uint8_t)std::clamp((float)targetColor.b * lum, 0.0f, 255.0f);
        out_rgba[i] = 0xFF000000 | ((uint32_t)nb << 16) | ((uint32_t)ng << 8) | nr;
    }
}

bool CopyImageToClipboard(HWND hwnd, const uint32_t* rgba32, uint32_t width, uint32_t height) {
    if (!rgba32 || width == 0 || height == 0) return false;

    size_t rowStride = ((width * 32 + 31) / 32) * 4;
    size_t imageBytes = rowStride * height;
    size_t dibSize = sizeof(BITMAPINFOHEADER) + imageBytes;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, dibSize);
    if (!hMem) return false;

    uint8_t* pDst = (uint8_t*)GlobalLock(hMem);
    if (!pDst) {
        GlobalFree(hMem);
        return false;
    }

    BITMAPINFOHEADER& bih = *(BITMAPINFOHEADER*)pDst;
    memset(&bih, 0, sizeof(BITMAPINFOHEADER));
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = (LONG)width;
    bih.biHeight = (LONG)height; // bottom-up DIB
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = (DWORD)imageBytes;

    uint8_t* pPixels = pDst + sizeof(BITMAPINFOHEADER);
    for (uint32_t y = 0; y < height; ++y) {
        uint32_t srcY = height - 1 - y;
        uint8_t* pRow = pPixels + y * rowStride;
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t px = rgba32[srcY * width + x];
            pRow[x * 4 + 0] = (uint8_t)((px >> 16) & 0xFF); // Blue
            pRow[x * 4 + 1] = (uint8_t)((px >> 8) & 0xFF);  // Green
            pRow[x * 4 + 2] = (uint8_t)(px & 0xFF);         // Red
            pRow[x * 4 + 3] = (uint8_t)((px >> 24) & 0xFF); // Alpha
        }
    }

    GlobalUnlock(hMem);

    if (!OpenClipboard(hwnd)) {
        GlobalFree(hMem);
        return false;
    }
    EmptyClipboard();
    SetClipboardData(CF_DIB, hMem);
    CloseClipboard();
    return true;
}

bool PasteImageFromClipboard(HWND hwnd, std::vector<uint32_t>& out_rgba32, int& out_w, int& out_h) {
    if (!IsClipboardFormatAvailable(CF_DIB)) return false;
    if (!OpenClipboard(hwnd)) return false;

    HANDLE hData = GetClipboardData(CF_DIB);
    if (!hData) {
        CloseClipboard();
        return false;
    }

    const uint8_t* pSrc = (const uint8_t*)GlobalLock(hData);
    if (!pSrc) {
        CloseClipboard();
        return false;
    }

    const BITMAPINFOHEADER* bih = (const BITMAPINFOHEADER*)pSrc;
    if (bih->biSize < sizeof(BITMAPINFOHEADER)) {
        GlobalUnlock(hData);
        CloseClipboard();
        return false;
    }

    int w = bih->biWidth;
    int h = bih->biHeight;
    bool bottomUp = (h > 0);
    if (h < 0) h = -h;

    if (w <= 0 || h <= 0) {
        GlobalUnlock(hData);
        CloseClipboard();
        return false;
    }

    int bpp = bih->biBitCount;
    const uint8_t* pPixels = pSrc + bih->biSize;
    if (bih->biClrUsed > 0) {
        pPixels += bih->biClrUsed * sizeof(RGBQUAD);
    } else if (bpp <= 8) {
        pPixels += ((size_t)1 << bpp) * sizeof(RGBQUAD);
    } else if (bih->biCompression == BI_BITFIELDS) {
        pPixels += 12; // 3 DWORD masks
    }

    out_w = w;
    out_h = h;
    out_rgba32.resize((size_t)w * h);

    if (bpp == 32) {
        size_t stride = ((w * 32 + 31) / 32) * 4;
        for (int y = 0; y < h; ++y) {
            int srcY = bottomUp ? (h - 1 - y) : y;
            const uint8_t* row = pPixels + srcY * stride;
            for (int x = 0; x < w; ++x) {
                uint8_t b = row[x * 4 + 0];
                uint8_t g = row[x * 4 + 1];
                uint8_t r = row[x * 4 + 2];
                uint8_t a = row[x * 4 + 3];
                if (a == 0 && (r != 0 || g != 0 || b != 0)) a = 255;
                out_rgba32[y * w + x] = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
            }
        }
    } else if (bpp == 24) {
        size_t stride = ((w * 24 + 31) / 32) * 4;
        for (int y = 0; y < h; ++y) {
            int srcY = bottomUp ? (h - 1 - y) : y;
            const uint8_t* row = pPixels + srcY * stride;
            for (int x = 0; x < w; ++x) {
                uint8_t b = row[x * 3 + 0];
                uint8_t g = row[x * 3 + 1];
                uint8_t r = row[x * 3 + 2];
                out_rgba32[y * w + x] = 0xFF000000 | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
            }
        }
    } else {
        const RGBQUAD* pal = (const RGBQUAD*)(pSrc + bih->biSize);
        size_t stride = ((w * bpp + 31) / 32) * 4;
        for (int y = 0; y < h; ++y) {
            int srcY = bottomUp ? (h - 1 - y) : y;
            const uint8_t* row = pPixels + srcY * stride;
            for (int x = 0; x < w; ++x) {
                uint8_t idx = (bpp == 8) ? row[x] : 0;
                RGBQUAD q = pal[idx];
                out_rgba32[y * w + x] = 0xFF000000 | ((uint32_t)q.rgbBlue << 16) | ((uint32_t)q.rgbGreen << 8) | q.rgbRed;
            }
        }
    }

    GlobalUnlock(hData);
    CloseClipboard();
    return true;
}

} // namespace FirmwareBitmap
