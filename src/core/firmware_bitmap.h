#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <windows.h>
#include "firmware_color.h"

namespace FirmwareBitmap {

bool DecodeMask(const uint8_t* raw, size_t raw_len, uint32_t width, uint32_t height, std::vector<uint8_t>& out_gray);

bool EncodeMaskPreserving(
    const uint8_t* gray_pixels,
    uint32_t width,
    uint32_t height,
    const uint8_t* original_raw,
    size_t raw_len,
    std::vector<uint8_t>& out_raw
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
