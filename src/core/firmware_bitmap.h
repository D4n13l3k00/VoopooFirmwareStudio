#pragma once

#include <cstdint>
#include <array>
#include <cmath>
#include <vector>
#include <string>
#include <windows.h>
#include "firmware_color.h"
#include "firmware_catalog.h"

namespace FirmwareBitmap {

// Decodes frame zero using Windows codecs, then stb for PSD/TGA/HDR/PNM.
// Decoder failure leaves the output unchanged.
bool LoadImageRgba(const std::string& path, std::vector<uint8_t>& rgba,
                   int& width, int& height, std::string& error);
enum class Resampling { Nearest, Bilinear, Bicubic, Lanczos };
struct ImageAdjustments {
    float brightness = 0, contrast = 0, gamma = 1, saturation = 0, sharpness = 0;
    std::array<float, 3> curve{0.25f, 0.5f, 0.75f};
    // R, G, B curves run after the RGB master curve.
    std::array<std::array<float, 3>, 3> channelCurves{{
        {0.25f, 0.5f, 0.75f}, {0.25f, 0.5f, 0.75f}, {0.25f, 0.5f, 0.75f}
    }};
    uint32_t paletteColors = 0;
    bool Valid() const {
        if (!std::isfinite(brightness) || brightness < -100 || brightness > 100 ||
            !std::isfinite(contrast) || contrast < -100 || contrast > 100 ||
            !std::isfinite(gamma) || gamma < 0.2f || gamma > 3 ||
            !std::isfinite(saturation) || saturation < -100 || saturation > 100 ||
            !std::isfinite(sharpness) || sharpness < 0 || sharpness > 100 ||
            (paletteColors != 0 && paletteColors != 16 && paletteColors != 64 && paletteColors != 256)) return false;
        for (float point : curve)
            if (!std::isfinite(point) || point < 0 || point > 1) return false;
        for (const auto& channel : channelCurves)
            for (float point : channel)
                if (!std::isfinite(point) || point < 0 || point > 1) return false;
        return true;
    }
    bool operator==(const ImageAdjustments&) const = default;
};
// Operates on the resized visible region; the immutable original is untouched.
// Brightness/contrast/gamma and the RGB master precede R/G/B curves, then
// saturation, sharpening, and palette reduction. Neutral settings are a no-op.
void ApplyAdjustments(std::vector<uint32_t>& pixels, int width, int height,
                      const ImageAdjustments& adjustments);
// Alpha is composited on black before filtering to avoid transparent-edge halos.
void ResizeRgbaOnBlack(const uint8_t* source, int width, int height,
                       int outputWidth, int outputHeight, Resampling filter,
                       std::vector<uint32_t>& output, int firstX = 0, int firstY = 0,
                       int regionWidth = 0, int regionHeight = 0);
bool DecodeMask(const uint8_t* raw, size_t raw_len, uint32_t width, uint32_t height,
                std::vector<uint8_t>& out_gray, AssetFormat layout = AssetFormat::Mask1Bit);

bool EncodeMaskPreserving(
    const uint8_t* gray_pixels,
    uint32_t width,
    uint32_t height,
    const uint8_t* original_raw,
    size_t raw_len,
    std::vector<uint8_t>& out_raw,
    AssetFormat layout = AssetFormat::Mask1Bit
);

void FitGrayscale(
    const uint8_t* src_gray,
    int sw,
    int sh,
    uint32_t target_w,
    uint32_t target_h,
    std::vector<uint8_t>& out_gray
);

bool LoadAndFit(
    const std::string& path,
    uint32_t target_w,
    uint32_t target_h,
    std::vector<uint8_t>& out_gray,
    std::string& out_err
);

bool SavePng(
    const std::string& path,
    const uint8_t* gray_pixels,
    uint32_t width,
    uint32_t height,
    std::string& out_err
);

void ColorizeMask(
    const uint8_t* gray_pixels,
    uint32_t width,
    uint32_t height,
    Rgb8 fg,
    Rgb8 bg,
    std::vector<uint32_t>& out_rgba32
);

bool DecodeRgb565(
    const uint8_t* raw,
    size_t raw_len,
    uint32_t width,
    uint32_t height,
    std::vector<uint32_t>& out_rgba32
);

bool EncodeRgb565(
    const uint32_t* rgba32,
    uint32_t width,
    uint32_t height,
    std::vector<uint8_t>& out_raw
);

void FitRgba32(
    const uint8_t* src_rgba,
    int sw,
    int sh,
    uint32_t target_w,
    uint32_t target_h,
    std::vector<uint32_t>& out_rgba32
);

bool LoadAndFitRgb565(
    const std::string& path,
    uint32_t target_w,
    uint32_t target_h,
    std::vector<uint32_t>& out_rgba32,
    std::string& out_err
);

bool SavePngRgb565(
    const std::string& path,
    const uint32_t* rgba32,
    uint32_t width,
    uint32_t height,
    std::string& out_err
);

bool SavePngRgb565Rotated(
    const std::string& path,
    const uint32_t* rgba32,
    uint32_t width,
    uint32_t height,
    bool counterClockwise,
    std::string& out_err
);

void RecolorRgb565(
    const uint32_t* in_rgba,
    uint32_t width,
    uint32_t height,
    Rgb8 targetColor,
    std::vector<uint32_t>& out_rgba
);

// Windows Clipboard Helpers
bool CopyImageToClipboard(
    HWND hwnd,
    const uint32_t* rgba32,
    uint32_t width,
    uint32_t height
);

bool PasteImageFromClipboard(
    HWND hwnd,
    std::vector<uint32_t>& out_rgba32,
    int& out_w,
    int& out_h
);

} // namespace FirmwareBitmap
