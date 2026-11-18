#include "firmware_bitmap.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#include "path_utils.h"

#include <cmath>
#include <algorithm>
#include <array>
#include <map>
#include <wincodec.h>
#include <wrl/client.h>
#pragma comment(lib, "windowscodecs.lib")

namespace FirmwareBitmap {

void ApplyAdjustments(std::vector<uint32_t>& pixels, int width, int height,
                      const ImageAdjustments& adjustments) {
    if (adjustments == ImageAdjustments{} || !adjustments.Valid() ||
        width <= 0 || height <= 0 || pixels.size() != size_t(width) * height) return;
    std::array<std::array<uint8_t, 256>, 3> tone{};
    const float contrast = std::exp2(adjustments.contrast / 50.0f);
    const float points[] = {0, adjustments.curve[0], adjustments.curve[1], adjustments.curve[2], 1};
    constexpr std::array<float, 3> identity{0.25f, 0.5f, 0.75f};
    std::array<std::array<float, 5>, 3> channelPoints{};
    std::array<bool, 3> channelIdentity{};
    for (size_t channel = 0; channel < tone.size(); ++channel) {
        const auto& curve = adjustments.channelCurves[channel];
        channelPoints[channel] = {0, curve[0], curve[1], curve[2], 1};
        channelIdentity[channel] = curve == identity;
    }
    for (size_t i = 0; i < tone[0].size(); ++i) {
        float value = (std::clamp)((float(i) / 255 - 0.5f) * contrast + 0.5f +
            adjustments.brightness / 100, 0.0f, 1.0f);
        value = std::pow(value, 1 / adjustments.gamma);
        const int segment = (std::min)(int(value * 4), 3);
        const float fraction = value * 4 - segment;
        value = points[segment] + (points[segment + 1] - points[segment]) * fraction;
        const int channelSegment = (std::min)(int(value * 4), 3);
        const float channelFraction = value * 4 - channelSegment;
        for (size_t channel = 0; channel < tone.size(); ++channel) {
            const auto& points = channelPoints[channel];
            // Identity channels retain the legacy master's exact rounding.
            const float output = channelIdentity[channel] ? value :
                points[channelSegment] + (points[channelSegment + 1] - points[channelSegment]) * channelFraction;
            tone[channel][i] = uint8_t(std::lround(output * 255));
        }
    }
    const float saturation = 1 + adjustments.saturation / 100;
    for (auto& pixel : pixels) {
        const float red = tone[0][pixel & 255], green = tone[1][(pixel >> 8) & 255], blue = tone[2][(pixel >> 16) & 255];
        const float luminance = red * 0.2126f + green * 0.7152f + blue * 0.0722f;
        auto channel = [&](float value) {
            return uint32_t(std::lround((std::clamp)(luminance + (value - luminance) * saturation, 0.0f, 255.0f)));
        };
        pixel = (pixel & 0xff000000u) | channel(red) | channel(green) << 8 | channel(blue) << 16;
    }
    if (adjustments.sharpness != 0) {
    const auto source = pixels;
    const float strength = adjustments.sharpness / 100;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const size_t at = size_t(y) * width + x;
            const size_t left = size_t(y) * width + (std::max)(x - 1, 0);
            const size_t right = size_t(y) * width + (std::min)(x + 1, width - 1);
            const size_t above = size_t((std::max)(y - 1, 0)) * width + x;
            const size_t below = size_t((std::min)(y + 1, height - 1)) * width + x;
            uint32_t result = source[at] & 0xff000000u;
            for (unsigned shift : {0u, 8u, 16u}) {
                const float center = float((source[at] >> shift) & 255);
                const float average = float(((source[left] >> shift) & 255) + ((source[right] >> shift) & 255) +
                    ((source[above] >> shift) & 255) + ((source[below] >> shift) & 255)) / 4;
                const uint32_t value = uint32_t(std::lround((std::clamp)(center + (center - average) * strength, 0.0f, 255.0f)));
                result |= value << shift;
            }
            pixels[at] = result;
        }
    }
    if (!adjustments.paletteColors) return;
    struct Color { uint32_t rgb, count; };
    std::map<uint32_t, uint32_t> histogram;
    for (const auto pixel : pixels) ++histogram[pixel & 0xffffff];
    if (histogram.size() <= adjustments.paletteColors) return;
    std::vector<Color> colors;
    colors.reserve(histogram.size());
    for (const auto& [rgb, count] : histogram) colors.push_back({rgb, count});
    struct Box { size_t begin, end; };
    std::vector<Box> boxes{{0, colors.size()}};
    while (boxes.size() < adjustments.paletteColors) {
        size_t best = boxes.size();
        unsigned axis = 0;
        uint64_t score = 0;
        for (size_t box = 0; box < boxes.size(); ++box) {
            const auto [begin, end] = boxes[box];
            if (end - begin < 2) continue;
            uint32_t total = 0;
            unsigned minimum[3] = {255, 255, 255}, maximum[3]{};
            for (size_t i = begin; i < end; ++i) {
                total += colors[i].count;
                for (unsigned channel = 0; channel < 3; ++channel) {
                    const unsigned value = (colors[i].rgb >> (channel * 8)) & 255;
                    minimum[channel] = (std::min)(minimum[channel], value);
                    maximum[channel] = (std::max)(maximum[channel], value);
                }
            }
            for (unsigned channel = 0; channel < 3; ++channel) {
                const uint64_t candidate = uint64_t(maximum[channel] - minimum[channel]) * total;
                if (candidate > score) { score = candidate; best = box; axis = channel * 8; }
            }
        }
        if (best == boxes.size()) break;
        const auto [begin, end] = boxes[best];
        std::sort(colors.begin() + begin, colors.begin() + end, [axis](const Color& a, const Color& b) {
            const auto left = (a.rgb >> axis) & 255, right = (b.rgb >> axis) & 255;
            return left != right ? left < right : a.rgb < b.rgb;
        });
        uint32_t total = 0, accumulated = 0;
        for (size_t i = begin; i < end; ++i) total += colors[i].count;
        size_t middle = begin;
        do { accumulated += colors[middle++].count; } while (middle < end - 1 && accumulated < total / 2);
        boxes[best].end = middle;
        boxes.push_back({middle, end});
    }
    std::vector<uint32_t> palette;
    for (const auto [begin, end] : boxes) {
        uint64_t sums[3]{}, total = 0;
        for (size_t i = begin; i < end; ++i) {
            total += colors[i].count;
            for (unsigned c = 0; c < 3; ++c) sums[c] += uint64_t((colors[i].rgb >> (c * 8)) & 255) * colors[i].count;
        }
        palette.push_back(uint32_t(sums[0] / total) | uint32_t(sums[1] / total) << 8 | uint32_t(sums[2] / total) << 16);
    }
    for (auto& [rgb, replacement] : histogram) {
        uint32_t distance = UINT32_MAX;
        for (const auto candidate : palette) {
            uint32_t current = 0;
            for (unsigned shift : {0u, 8u, 16u}) {
                const int difference = int((rgb >> shift) & 255) - int((candidate >> shift) & 255);
                current += difference * difference;
            }
            if (current < distance) { distance = current; replacement = candidate; }
        }
    }
    for (auto& pixel : pixels) pixel = (pixel & 0xff000000u) | histogram.find(pixel & 0xffffff)->second;
}

bool LoadImageRgba(const std::string& path, std::vector<uint8_t>& rgba,
                   int& width, int& height, std::string& error) {
    constexpr uint64_t MaximumPixels = 32 * 1024 * 1024;
    const auto valid = [](uint64_t w, uint64_t h) {
        return w && h && w <= 32768 && h <= 32768 && w * h <= MaximumPixels;
    };
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    struct ComScope { bool owned; ~ComScope() { if (owned) CoUninitialize(); } } com{SUCCEEDED(initialized)};
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    std::vector<uint8_t> decoded;
    int w = 0, h = 0;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
        SUCCEEDED(factory->CreateDecoderFromFilename(PathUtils::Utf8ToWide(path).c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnDemand, &decoder)) &&
        SUCCEEDED(decoder->GetFrame(0, &frame))) {
        UINT fw = 0, fh = 0;
        if (SUCCEEDED(frame->GetSize(&fw, &fh)) && valid(fw, fh)) {
            ComPtr<IWICBitmapSource> source = frame;
            ComPtr<IWICBitmapFlipRotator> rotated;
            ComPtr<IWICMetadataQueryReader> metadata;
            PROPVARIANT orientation{};
            if (SUCCEEDED(frame->GetMetadataQueryReader(&metadata))) {
                HRESULT hr = metadata->GetMetadataByName(L"/app1/ifd/{ushort=274}", &orientation);
                if (FAILED(hr)) hr = metadata->GetMetadataByName(L"/ifd/{ushort=274}", &orientation);
                if (SUCCEEDED(hr) && orientation.vt == VT_UI2 && orientation.uiVal >= 2 && orientation.uiVal <= 8) {
                    constexpr WICBitmapTransformOptions transforms[] = {
                        WICBitmapTransformRotate0, WICBitmapTransformFlipHorizontal,
                        WICBitmapTransformRotate180, WICBitmapTransformFlipVertical,
                        WICBitmapTransformOptions(WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal),
                        WICBitmapTransformRotate90,
                        WICBitmapTransformOptions(WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal),
                        WICBitmapTransformRotate270};
                    if (SUCCEEDED(factory->CreateBitmapFlipRotator(&rotated)) &&
                        SUCCEEDED(rotated->Initialize(frame.Get(), transforms[orientation.uiVal - 1])))
                        source = rotated;
                }
                PropVariantClear(&orientation);
            }
            if (SUCCEEDED(source->GetSize(&fw, &fh)) &&
                SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
                SUCCEEDED(converter->Initialize(source.Get(), GUID_WICPixelFormat32bppRGBA,
                    WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) {
                decoded.resize(size_t(fw) * fh * 4);
                if (SUCCEEDED(converter->CopyPixels(nullptr, fw * 4, static_cast<UINT>(decoded.size()), decoded.data()))) {
                    w = int(fw); h = int(fh);
                }
            }
        }
    }
    if (!w) {
        FILE* file = PathUtils::FopenUtf8(path, L"rb");
        int channels = 0;
        if (!file) { error = "Cannot open image"; return false; }
        const bool dimensions = stbi_info_from_file(file, &w, &h, &channels) && valid(w, h);
        rewind(file);
        stbi_uc* pixels = dimensions ? stbi_load_from_file(file, &w, &h, &channels, 4) : nullptr;
        fclose(file);
        if (!pixels) {
            error = dimensions ? "Cannot decode image" : "Unsupported image or image exceeds 32 megapixels / 32768 pixels per side";
            return false;
        }
        decoded.assign(pixels, pixels + size_t(w) * h * 4);
        stbi_image_free(pixels);
    }
    rgba = std::move(decoded); width = w; height = h; error.clear();
    return true;
}

namespace {
struct SampleWeight { int index; float weight; };
using Weights = std::vector<std::vector<SampleWeight>>;
double Kernel(double x, Resampling filter) {
    x = std::abs(x);
    if (filter == Resampling::Bilinear) return (std::max)(0.0, 1.0 - x);
    if (filter == Resampling::Bicubic)
        return x < 1 ? (1.5 * x - 2.5) * x * x + 1 :
            x < 2 ? ((-0.5 * x + 2.5) * x - 4) * x + 2 : 0;
    if (x >= 3) return 0;
    if (x < 1e-9) return 1;
    constexpr double Pi = 3.14159265358979323846;
    return std::sin(Pi * x) * std::sin(Pi * x / 3) / (Pi * Pi * x * x / 3);
}
Weights MakeWeights(int source, int destination, int firstPixel, int pixelCount, Resampling filter) {
    Weights result(pixelCount);
    const double reduction = (std::min)(1.0, double(destination) / source);
    const double radius = (filter == Resampling::Bilinear ? 1 : filter == Resampling::Bicubic ? 2 : 3) / reduction;
    for (int i = 0; i < pixelCount; ++i) {
        const double center = (i + firstPixel + 0.5) * source / destination - 0.5;
        auto& weights = result[i];
        if (filter == Resampling::Nearest) {
            weights.push_back({(std::clamp)(int(std::floor(center + 0.5)), 0, source - 1), 1});
            continue;
        }
        const int first = int(std::ceil(center - radius)), last = int(std::floor(center + radius));
        weights.reserve(last - first + 1);
        double total = 0;
        for (int j = first; j <= last; ++j) {
            const float weight = float(Kernel((j - center) * reduction, filter));
            if (!weight) continue;
            const int index = (std::clamp)(j, 0, source - 1);
            if (!weights.empty() && weights.back().index == index) weights.back().weight += weight;
            else weights.push_back({index, weight});
            total += weight;
        }
        for (auto& weight : weights) weight.weight = float(weight.weight / total);
    }
    return result;
}
}

void ResizeRgbaOnBlack(const uint8_t* source, int width, int height,
                       int outputWidth, int outputHeight, Resampling filter,
                       std::vector<uint32_t>& output, int firstX, int firstY, int regionWidth, int regionHeight) {
    if (!regionWidth) regionWidth = outputWidth;
    if (!regionHeight) regionHeight = outputHeight;
    if (!source || width <= 0 || height <= 0 || outputWidth <= 0 || outputHeight <= 0 ||
        firstX < 0 || firstY < 0 || regionWidth <= 0 || regionHeight <= 0 ||
        regionWidth > outputWidth - firstX || regionHeight > outputHeight - firstY) { output.clear(); return; }
    const auto wx = MakeWeights(width, outputWidth, firstX, regionWidth, filter);
    const auto wy = MakeWeights(height, outputHeight, firstY, regionHeight, filter);
    // Choose the smaller intermediate, including extremely tall/wide inputs.
    const bool horizontal = int64_t(regionWidth) * height <= int64_t(width) * regionHeight;
    const int intermediateWidth = horizontal ? regionWidth : width;
    const int intermediateHeight = horizontal ? height : regionHeight;
    std::vector<std::array<float, 3>> intermediate(size_t(intermediateWidth) * intermediateHeight);
    for (int y = 0; y < intermediateHeight; ++y) {
        for (int x = 0; x < intermediateWidth; ++x) {
            auto& pixel = intermediate[size_t(y) * intermediateWidth + x];
            const auto& weights = horizontal ? wx[x] : wy[y];
            for (const auto& weight : weights) {
                const auto* sample = source + (horizontal ? size_t(y) * width + weight.index : size_t(weight.index) * width + x) * 4;
                const float alphaWeight = weight.weight * (sample[3] / 255.0f);
                for (unsigned c = 0; c < 3; ++c) pixel[c] += sample[c] * alphaWeight;
            }
        }
    }
    output.resize(size_t(regionWidth) * regionHeight);
    for (int y = 0; y < regionHeight; ++y) {
        for (int x = 0; x < regionWidth; ++x) {
            float rgb[3]{};
            const auto& weights = horizontal ? wy[y] : wx[x];
            for (const auto& weight : weights) {
                const auto& sample = intermediate[horizontal ? size_t(weight.index) * intermediateWidth + x :
                    size_t(y) * intermediateWidth + weight.index];
                for (unsigned c = 0; c < 3; ++c) rgb[c] += sample[c] * weight.weight;
            }
            uint32_t pixel = 0xff000000;
            for (unsigned c = 0; c < 3; ++c) pixel |= uint32_t((std::clamp)(int(std::lround(rgb[c])), 0, 255)) << (c * 8);
            output[size_t(y) * regionWidth + x] = pixel;
        }
    }
}

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

bool DecodeMask(const uint8_t* raw, size_t raw_len, uint32_t width, uint32_t height,
                std::vector<uint8_t>& out_gray, AssetFormat layout) {
    if (layout == AssetFormat::Mask1BitPages) {
        if (raw_len != static_cast<size_t>(width) * ((height + 7) / 8)) return false;
        out_gray.assign(static_cast<size_t>(width) * height, 0);
        for (uint32_t y = 0; y < height; ++y)
            for (uint32_t x = 0; x < width; ++x)
                out_gray[static_cast<size_t>(y) * width + x] =
                    (raw[static_cast<size_t>(y / 8) * width + x] & (1u << (y & 7))) ? 255 : 0;
        return true;
    }
    if (layout != AssetFormat::Mask1Bit) return false;
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
    std::vector<uint8_t>& out_raw,
    AssetFormat layout
) {
    if (layout == AssetFormat::Mask1BitPages) {
        if (raw_len != static_cast<size_t>(width) * ((height + 7) / 8)) return false;
        out_raw.assign(original_raw, original_raw + raw_len);
        for (uint32_t y = 0; y < height; ++y) {
            const auto mask = static_cast<uint8_t>(1u << (y & 7));
            for (uint32_t x = 0; x < width; ++x) {
                auto& byte = out_raw[static_cast<size_t>(y / 8) * width + x];
                if (gray_pixels[static_cast<size_t>(y) * width + x] >= 128) byte |= mask;
                else byte &= static_cast<uint8_t>(~mask);
            }
        }
        return true;
    }
    if (layout != AssetFormat::Mask1Bit) return false;
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
