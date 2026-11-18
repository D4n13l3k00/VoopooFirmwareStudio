#pragma once
#include "firmware_logo_patch.h"
#include <algorithm>
#include <cstdlib>

namespace FirmwareLogo {
// Mirrors the integer RGB565 transforms in firmware_logo_boot.S; no emulation.
inline void PreviewFrame(const Settings& settings, uint32_t phase, std::vector<uint32_t>& rgba) {
    rgba.assign(size_t(Width) * Height, 0xff000000u);
    if (!settings.enabled || settings.pixels.size() != size_t(Width) * Height * 2) return;
    const uint32_t entrance = settings.animation == Animation::None ? 1 : PhaseCount;
    const bool exiting = phase >= entrance && settings.exitAnimation != Animation::None;
    const auto effect = exiting ? settings.exitAnimation : settings.animation;
    const int visibility = exiting ? 15 - int((std::min)(phase - entrance, 15u)) :
        effect == Animation::None ? 16 : int((std::min)(phase, 15u)) + 1;
    for (int y = 0; y < int(Height); ++y)
        for (int x = 0; x < int(Width); ++x) {
            int sx = x, sy = y, alpha = 16;
            bool visible = visibility > 0 || effect == Animation::None;
            const int dx = std::abs(x * 2 - 79), dy = std::abs(y * 2 - 159);
            const auto id = static_cast<uint32_t>(effect);
            if (id >= 17 && id < static_cast<uint32_t>(Animation::Count)) {
                if (id >= 29 && id <= 32) {
                    sx += ((id & 1) ? -1 : 1) * (16 - visibility) * 5;
                    sy += (id < 31 ? -1 : 1) * (16 - visibility) * 10;
                } else {
                    constexpr unsigned modes[] = {1,3,5,6,7,9,10,11,12,13,14,15,
                        4,5,6,7,17,18,19,20,21,22,23};
                    const unsigned mode = modes[id - 17], direction = mode & 7;
                    const int mx = direction == 3 || direction == 5 || direction == 7 ? 79 - x : x;
                    const int my = direction == 1 || direction == 6 || direction == 7 ? 159 - y : y;
                    int gradient = direction < 2 ? my : direction < 4 ? mx * 2 : mx * 2 + my;
                    if (mode >= 8 && direction >= 4) gradient /= 2;
                    if (mode < 8) visible &= gradient < visibility * (direction < 4 ? 10 : 20);
                    else if (mode < 16) alpha = (std::clamp)(visibility * 11 - gradient, 0, 16);
                    else visible &= gradient % 20 < visibility * 5 / 4;
                }
            } else
            switch (effect) {
            case Animation::None: break;
            case Animation::WipeDown: visible &= y < visibility * 10; break;
            case Animation::WipeRight: visible &= x < visibility * 5; break;
            case Animation::Fade: alpha = visibility; break;
            case Animation::Iris: visible &= dx * dx + dy * dy <= visibility * visibility * 124; break;
            case Animation::Diamond: visible &= dx + dy < visibility * 15; break;
            case Animation::CenterHorizontal: visible &= dx < visibility * 5; break;
            case Animation::CenterVertical: visible &= dy < visibility * 10; break;
            case Animation::Blinds: visible &= y % 20 < visibility * 5 / 4; break;
            case Animation::Dissolve: visible &= ((x * 13 + y * 7) & 15) < visibility; break;
            case Animation::SoftWipe: alpha = (std::clamp)(visibility * 11 - y, 0, 16); break;
            case Animation::SlideRight: sx -= (16 - visibility) * 5; break;
            case Animation::SlideLeft: sx += (16 - visibility) * 5; break;
            case Animation::SlideUp: sy += (16 - visibility) * 10; break;
            case Animation::SlideDown: sy -= (16 - visibility) * 10; break;
            case Animation::Checker: visible &= ((x / 10 + y / 20) & 1) * 8 + x % 10 * 8 / 10 < visibility; break;
            case Animation::Diagonal: visible &= x * 2 + y < visibility * 20; break;
            default: break;
            }
            if (!visible || sx < 0 || sx >= int(Width) || sy < 0 || sy >= int(Height)) continue;
            const size_t at = (size_t(sy) * Width + sx) * 2;
            uint16_t color = uint16_t(settings.pixels[at]) | uint16_t(settings.pixels[at + 1]) << 8;
            if (alpha != 16) color = uint16_t(((color >> 11) * alpha >> 4) << 11 |
                ((((color >> 5) & 63) * alpha >> 4) << 5) | ((color & 31) * alpha >> 4));
            const uint32_t r = color >> 11, g = (color >> 5) & 63, b = color & 31;
            rgba[size_t(y) * Width + x] = (r << 3 | r >> 2) | (g << 2 | g >> 4) << 8 |
                (b << 3 | b >> 2) << 16 | 0xff000000u;
        }
}
} // namespace FirmwareLogo
