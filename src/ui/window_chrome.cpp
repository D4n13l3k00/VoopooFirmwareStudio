#include "window_chrome.h"
#include "app_update.h"
#include "imgui.h"
#include "texture_cache.h"
#include <dwmapi.h>
#include <windowsx.h>
#include <algorithm>

namespace WindowChrome {
namespace {
int height = 32, buttonWidth = 46, resizeHeight = 5, clientWidth = 0;
int hovered = HTNOWHERE, pressed = HTNOWHERE;
bool maximized = false, active = true;
Dx11Texture titleIcon;
void Metrics(UINT dpi) {
    height = MulDiv(32, dpi, 96);
    buttonWidth = MulDiv(46, dpi, 96);
    resizeHeight = MulDiv(5, dpi, 96);
}
bool Control(int hit) { return hit == HTMINBUTTON || hit == HTMAXBUTTON || hit == HTCLOSE; }
int ButtonAt(POINT point) {
    if (point.y < 0 || point.y >= height || point.x >= clientWidth) return HTNOWHERE;
    if (point.x >= clientWidth - buttonWidth) return HTCLOSE;
    if (point.x >= clientWidth - buttonWidth * 2) return HTMAXBUTTON;
    if (point.x >= clientWidth - buttonWidth * 3) return HTMINBUTTON;
    return HTNOWHERE;
}
void Release(HWND hwnd, POINT point) {
    const int button = pressed;
    pressed = HTNOWHERE;
    if (GetCapture() == hwnd) ReleaseCapture();
    if (ButtonAt(point) != button) return;
    const WPARAM command = button == HTCLOSE ? SC_CLOSE : button == HTMINBUTTON ? SC_MINIMIZE :
        maximized ? SC_RESTORE : SC_MAXIMIZE;
    PostMessageW(hwnd, WM_SYSCOMMAND, command, 0);
}
}
void Initialize(HWND hwnd) {
    Metrics((std::max)(96u, GetDpiForWindow(hwnd)));
    RECT client{};
    GetClientRect(hwnd, &client);
    clientWidth = client.right;
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    // Windows 11 appearance hints; Windows 10 retains its native square frame.
    const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    const COLORREF border = DWMWA_COLOR_NONE;
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
bool InitializeGraphics(ID3D11Device* device, ID3D11DeviceContext* context) {
    constexpr int size = 64;
    HICON icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
        IMAGE_ICON, size, size, 0));
    if (!icon) return false;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool uploaded = false;
    if (dc && bitmap && bits) {
        auto* pixels = static_cast<uint32_t*>(bits);
        std::fill_n(pixels, size * size, 0u);
        HGDIOBJ previous = SelectObject(dc, bitmap);
        if (DrawIconEx(dc, 0, 0, icon, size, size, 0, nullptr, DI_NORMAL)) {
            GdiFlush();
            // GDI writes premultiplied BGRA; ImGui samples straight RGBA.
            for (int i = 0; i < size * size; ++i) {
                const uint32_t pixel = pixels[i], alpha = pixel >> 24;
                if (!alpha) { pixels[i] = 0; continue; }
                const auto channel = [alpha](uint32_t value) {
                    return (std::min)(255u, (value * 255 + alpha / 2) / alpha);
                };
                pixels[i] = (alpha << 24) | (channel(pixel & 255) << 16) |
                    (channel((pixel >> 8) & 255) << 8) | channel((pixel >> 16) & 255);
            }
            uploaded = titleIcon.Update(device, context, pixels, size, size);
        }
        SelectObject(dc, previous);
    }
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
    DestroyIcon(icon);
    return uploaded;
}
void ReleaseGraphics() { titleIcon.Release(); }
float Height() { return static_cast<float>(height); }
bool HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result) {
    result = 0;
    switch (message) {
    case WM_NCCALCSIZE: {
        // Keep native side/bottom borders and shadows; reclaim only the top.
        auto* client = reinterpret_cast<RECT*>(lParam);
        const LONG top = client->top;
        DefWindowProcW(hwnd, message, wParam, lParam);
        client->top = top;
        if (IsZoomed(hwnd)) {
            const UINT dpi = (std::max)(96u, GetDpiForWindow(hwnd));
            client->top += GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) +
                GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        }
        return true;
    }
    case WM_NCHITTEST: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &point);
        const int button = ButtonAt(point);
        if (Control(button)) { result = button; return true; }
        result = DefWindowProcW(hwnd, message, wParam, lParam);
        if (result != HTCLIENT) return true;
        if (!maximized && point.y >= 0 && point.y < resizeHeight) result = HTTOP;
        else if (point.y >= 0 && point.y < height) result = HTCAPTION;
        return true;
    }
    case WM_NCMOUSEMOVE: {
        hovered = Control(static_cast<int>(wParam)) ? static_cast<int>(wParam) : HTNOWHERE;
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE | TME_NONCLIENT, hwnd, 0};
        TrackMouseEvent(&track);
        // The OS still sees HTMAXBUTTON for Windows 11 Snap Layouts.
        return false;
    }
    case WM_NCMOUSELEAVE:
        hovered = HTNOWHERE;
        return false;
    case WM_NCLBUTTONDOWN:
        if (!Control(static_cast<int>(wParam))) return false;
        pressed = hovered = static_cast<int>(wParam);
        SetCapture(hwnd);
        return true;
    case WM_MOUSEMOVE:
        if (pressed == HTNOWHERE) return false;
        hovered = ButtonAt({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return true;
    case WM_LBUTTONUP:
        if (pressed == HTNOWHERE) return false;
        Release(hwnd, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return true;
    case WM_NCLBUTTONUP:
        if (pressed != HTNOWHERE) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &point);
            Release(hwnd, point);
            return true;
        }
        return false;
    case WM_CAPTURECHANGED:
        pressed = HTNOWHERE;
        return false;
    case WM_NCLBUTTONDBLCLK:
        if (wParam != HTCAPTION) return false;
        PostMessageW(hwnd, WM_SYSCOMMAND, maximized ? SC_RESTORE : SC_MAXIMIZE, 0);
        return true;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            clientWidth = LOWORD(lParam);
            maximized = wParam == SIZE_MAXIMIZED;
        }
        return false;
    case WM_ACTIVATE:
        active = LOWORD(wParam) != WA_INACTIVE;
        return false;
    case WM_DPICHANGED: {
        Metrics(HIWORD(wParam));
        const auto* rect = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd, nullptr, rect->left, rect->top, rect->right - rect->left,
            rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return true;
    }
    }
    return false;
}
void Render(HWND) {
    auto* viewport = ImGui::GetMainViewport();
    auto* draw = ImGui::GetBackgroundDrawList(viewport);
    const ImVec2 origin = viewport->Pos;
    const ImU32 text = ImGui::GetColorU32(active ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    const float scale = height / 32.0f;
    const char* title = "Voopoo Firmware Studio v" STUDIO_VERSION;
    draw->PushClipRect(origin, ImVec2(origin.x + clientWidth - buttonWidth * 3,
        origin.y + height), true);
    float titleX = origin.x + 12 * scale;
    if (titleIcon.IsValid()) {
        const float size = 16 * scale, top = origin.y + (height - size) * 0.5f;
        draw->AddImage((ImTextureID)titleIcon.GetSRV(), ImVec2(titleX, top), ImVec2(titleX + size, top + size));
        titleX += size + 8 * scale;
    }
    draw->AddText(ImVec2(titleX, origin.y + (height - ImGui::GetFontSize()) * 0.5f), text, title);
    draw->PopClipRect();
    const int controls[]{HTMINBUTTON, HTMAXBUTTON, HTCLOSE};
    for (int i = 0; i < 3; ++i) {
        const float left = origin.x + clientWidth - buttonWidth * (3 - i);
        if (hovered == controls[i]) {
            const ImU32 background = controls[i] == HTCLOSE ? IM_COL32(205, 38, 48, 255) :
                ImGui::GetColorU32(pressed == controls[i] ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered);
            draw->AddRectFilled(ImVec2(left, origin.y), ImVec2(left + buttonWidth, origin.y + height), background);
        }
        const ImVec2 center(left + buttonWidth * 0.5f, origin.y + height * 0.5f);
        const float half = 5 * scale, stroke = (std::max)(1.0f, scale);
        const ImU32 icon = hovered == HTCLOSE && controls[i] == HTCLOSE ? IM_COL32_WHITE : text;
        if (controls[i] == HTMINBUTTON) {
            draw->AddLine(ImVec2(center.x - half, center.y), ImVec2(center.x + half, center.y), icon, stroke);
        } else if (controls[i] == HTMAXBUTTON) {
            if (maximized) {
                draw->AddLine(ImVec2(center.x - half + 2 * scale, center.y - half),
                    ImVec2(center.x + half, center.y - half), icon, stroke);
                draw->AddLine(ImVec2(center.x + half, center.y - half),
                    ImVec2(center.x + half, center.y + half - 2 * scale), icon, stroke);
                draw->AddRect(ImVec2(center.x - half, center.y - half + 2 * scale),
                    ImVec2(center.x + half - 2 * scale, center.y + half), icon, 0, stroke);
            } else {
                draw->AddRect(ImVec2(center.x - half, center.y - half),
                    ImVec2(center.x + half, center.y + half), icon, 0, stroke);
            }
        } else {
            draw->AddLine(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), icon, stroke);
            draw->AddLine(ImVec2(center.x - half, center.y + half), ImVec2(center.x + half, center.y - half), icon, stroke);
        }
    }
}
}
