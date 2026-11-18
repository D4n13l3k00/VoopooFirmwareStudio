#include "ui_pixel_editor.h"
#include "../imgui/imgui.h"
#include <queue>
#include <algorithm>

namespace UiPixelEditor {

static void FloodFillMask(std::vector<uint8_t>& gray, int width, int height, int startX, int startY, uint8_t targetVal, uint8_t fillVal) {
    if (targetVal == fillVal) return;
    if (startX < 0 || startX >= width || startY < 0 || startY >= height) return;
    if (gray[startY * width + startX] != targetVal) return;

    std::queue<std::pair<int, int>> q;
    q.push({ startX, startY });
    gray[startY * width + startX] = fillVal;

    const int dx[4] = { 0, 0, -1, 1 };
    const int dy[4] = { -1, 1, 0, 0 };

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                size_t idx = ny * width + nx;
                if (gray[idx] == targetVal) {
                    gray[idx] = fillVal;
                    q.push({ nx, ny });
                }
            }
        }
    }
}

static void FloodFillRgb(std::vector<uint32_t>& rgba, int width, int height, int startX, int startY, uint32_t targetVal, uint32_t fillVal) {
    if (targetVal == fillVal) return;
    if (startX < 0 || startX >= width || startY < 0 || startY >= height) return;
    if (rgba[startY * width + startX] != targetVal) return;

    std::queue<std::pair<int, int>> q;
    q.push({ startX, startY });
    rgba[startY * width + startX] = fillVal;

    const int dx[4] = { 0, 0, -1, 1 };
    const int dy[4] = { -1, 1, 0, 0 };

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                size_t idx = ny * width + nx;
                if (rgba[idx] == targetVal) {
                    rgba[idx] = fillVal;
                    q.push({ nx, ny });
                }
            }
        }
    }
}

void RenderToolbar(StudioState& studio, size_t catIdx) {
    const auto& spec = studio.catalog[catIdx];
    const auto& style = ImGui::GetStyle();
    const float right = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() -
        style.WindowPadding.x - style.ScrollbarSize - ImGui::GetScrollX();
    const auto nextItem = [&](float width) {
        if (ImGui::GetItemRectMax().x + style.ItemSpacing.x + width <= right)
            ImGui::SameLine();
    };
    const auto nextButton = [&](i18n::Str label) {
        nextItem(ImGui::CalcTextSize(Tr(label)).x + style.FramePadding.x * 2);
    };

    ImGui::TextDisabled("%s", Tr(i18n::Str::LabelPixelTools));
    nextButton(i18n::Str::ToolPencil);

    // Pencil
    bool isPencil = (studio.activePixelTool == StudioState::PixelTool::Pencil);
    if (isPencil) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.8f, 1.0f));
    if (ImGui::Button(Tr(i18n::Str::ToolPencil))) {
        studio.activePixelTool = StudioState::PixelTool::Pencil;
    }
    if (isPencil) ImGui::PopStyleColor();

    nextButton(i18n::Str::ToolEraser);
    // Eraser
    bool isEraser = (studio.activePixelTool == StudioState::PixelTool::Eraser);
    if (isEraser) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.35f, 0.2f, 1.0f));
    if (ImGui::Button(Tr(i18n::Str::ToolEraser))) {
        studio.activePixelTool = StudioState::PixelTool::Eraser;
    }
    if (isEraser) ImGui::PopStyleColor();

    nextButton(i18n::Str::ToolEyedropper);
    // Eyedropper
    bool isEye = (studio.activePixelTool == StudioState::PixelTool::Eyedropper);
    if (isEye) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.4f, 1.0f));
    if (ImGui::Button(Tr(i18n::Str::ToolEyedropper))) {
        studio.activePixelTool = StudioState::PixelTool::Eyedropper;
    }
    if (isEye) ImGui::PopStyleColor();

    nextButton(i18n::Str::ToolBucketFill);
    // Bucket Fill
    bool isFill = (studio.activePixelTool == StudioState::PixelTool::BucketFill);
    if (isFill) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.3f, 0.7f, 1.0f));
    if (ImGui::Button(Tr(i18n::Str::ToolBucketFill))) {
        studio.activePixelTool = StudioState::PixelTool::BucketFill;
    }
    if (isFill) ImGui::PopStyleColor();

    nextItem(ImGui::CalcTextSize(Tr(i18n::Str::LabelBrushSize)).x + style.ItemSpacing.x + 135);
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(Tr(i18n::Str::LabelBrushSize));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(135);
    ImGui::SliderInt("##BrushSize", &studio.brushSize, 1, 8, "%dpx", ImGuiSliderFlags_AlwaysClamp);
    ImGui::EndGroup();
    nextItem(ImGui::CalcTextSize(Tr(i18n::Str::BtnInvertCanvas)).x +
        ImGui::CalcTextSize(Tr(i18n::Str::BtnClearCanvas)).x + style.FramePadding.x * 4 + style.ItemSpacing.x);

    // Invert Canvas Action
    if (ImGui::Button(Tr(i18n::Str::BtnInvertCanvas))) {
        studio.PushUndo("Invert " + spec.id);
        const uint8_t* raw = studio.GetCurrentRaw(catIdx);
        if (spec.format == AssetFormat::Rgb565) {
            std::vector<uint32_t> rgba;
            FirmwareBitmap::DecodeRgb565(raw, spec.byte_len(), spec.width, spec.height, rgba);
            for (auto& px : rgba) {
                uint8_t r = 255 - (px & 0xFF);
                uint8_t g = 255 - ((px >> 8) & 0xFF);
                uint8_t b = 255 - ((px >> 16) & 0xFF);
                px = 0xFF000000 | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
            }
            std::vector<uint8_t> newRaw;
            if (FirmwareBitmap::EncodeRgb565(rgba.data(), spec.width, spec.height, newRaw)) {
                studio.SetBitmapReplacement(catIdx, std::move(newRaw));
            }
        } else {
            std::vector<uint8_t> gray;
            FirmwareBitmap::DecodeMask(raw, spec.byte_len(), spec.width, spec.height, gray, spec.format);
            for (auto& v : gray) {
                v = (v > 127) ? 0 : 255;
            }
            std::vector<uint8_t> newRaw;
            if (FirmwareBitmap::EncodeMaskPreserving(gray.data(), spec.width, spec.height, studio.originals[catIdx].data(), spec.byte_len(), newRaw, spec.format)) {
                studio.SetBitmapReplacement(catIdx, std::move(newRaw));
            }
        }
        studio.textureNeedsUpdate = true;
    }

    nextButton(i18n::Str::BtnClearCanvas);
    // Clear Canvas Action
    if (ImGui::Button(Tr(i18n::Str::BtnClearCanvas))) {
        studio.PushUndo("Clear " + spec.id);
        if (spec.format == AssetFormat::Rgb565) {
            std::vector<uint32_t> rgba((size_t)spec.width * spec.height, 0xFF000000);
            std::vector<uint8_t> newRaw;
            if (FirmwareBitmap::EncodeRgb565(rgba.data(), spec.width, spec.height, newRaw)) {
                studio.SetBitmapReplacement(catIdx, std::move(newRaw));
            }
        } else {
            std::vector<uint8_t> gray((size_t)spec.width * spec.height, 0);
            std::vector<uint8_t> newRaw;
            if (FirmwareBitmap::EncodeMaskPreserving(gray.data(), spec.width, spec.height, studio.originals[catIdx].data(), spec.byte_len(), newRaw, spec.format)) {
                studio.SetBitmapReplacement(catIdx, std::move(newRaw));
            }
        }
        studio.textureNeedsUpdate = true;
    }
}

void RenderCanvas(StudioState& studio, size_t catIdx, ID3D11Device* device, ID3D11DeviceContext* context) {
    const auto& spec = studio.catalog[catIdx];
    Rgb8 bg = studio.GetBackgroundForAsset(spec).ScreenColor();
    Rgb8 fgCol = (spec.format == AssetFormat::Rgb565 ? studio.GetPaintColor(spec) : studio.GetForegroundForAsset(spec)).ScreenColor();
    float pZoom = (float)studio.previewZoom;
    float canvasW = spec.width * pZoom;
    float canvasH = spec.height * pZoom;

    // Decode current active image to RGBA32
    const uint8_t* raw = studio.GetCurrentRaw(catIdx);
    std::vector<uint32_t> activeRgba;
    if (spec.format == AssetFormat::Rgb565) {
        FirmwareBitmap::DecodeRgb565(raw, spec.byte_len(), spec.width, spec.height, activeRgba);
    } else {
        std::vector<uint8_t> gray;
        FirmwareBitmap::DecodeMask(raw, spec.byte_len(), spec.width, spec.height, gray, spec.format);
        FirmwareBitmap::ColorizeMask(gray.data(), spec.width, spec.height, fgCol, bg, activeRgba);
    }

    // Update GPU Texture
    if (studio.textureNeedsUpdate || !studio.activePreviewTexture.IsValid()) {
        studio.activePreviewTexture.Update(device, context, activeRgba.data(), spec.width, spec.height);
        studio.textureNeedsUpdate = false;
    }

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + canvasW, p0.y + canvasH);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Border and Background
    drawList->AddRect(ImVec2(p0.x - 1, p0.y - 1), ImVec2(p1.x + 1, p1.y + 1), IM_COL32(100, 100, 110, 255));
    ImU32 colBg = IM_COL32(bg.r, bg.g, bg.b, 255);
    drawList->AddRectFilled(p0, p1, colBg);

    // Image is display-only: the button owns mouse capture for both drawing buttons.
    ImGui::PushID(spec.id.c_str());
    ImGui::InvisibleButton("##CanvasInteractive", ImVec2(canvasW, canvasH),
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    ImGui::PopID();
    if (studio.activePreviewTexture.IsValid()) {
        drawList->AddImage((ImTextureID)studio.activePreviewTexture.GetSRV(), p0, p1);
    }

    bool canvasHovered = ImGui::IsItemHovered();
    bool canvasActive = ImGui::IsItemActive();
    bool lmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool rmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);

    ImVec2 mousePos = ImGui::GetIO().MousePos;
    int hoverPx = (int)((mousePos.x - p0.x) / pZoom);
    int hoverPy = (int)((mousePos.y - p0.y) / pZoom);
    bool inBounds = mousePos.x >= p0.x && mousePos.y >= p0.y && mousePos.x < p1.x && mousePos.y < p1.y;

    // Mouse Interaction
    if (canvasActive && (lmbDown || rmbDown)) {
        if (inBounds) {
            // Eyedropper Tool
            if (studio.activePixelTool == StudioState::PixelTool::Eyedropper && lmbDown) {
                uint32_t px = activeRgba[hoverPy * spec.width + hoverPx];
                Rgb8 sampled((uint8_t)(px & 0xFF), (uint8_t)((px >> 8) & 0xFF), (uint8_t)((px >> 16) & 0xFF));
                studio.PushUndo(std::string(Tr(i18n::Str::ToolEyedropper)) + ": " + spec.id);
                studio.SetAssetPaintColor(spec.id, sampled);
                strncpy_s(studio.assetHexEditBuf, sampled.ToHex().c_str(), sizeof(studio.assetHexEditBuf));
                studio.activePixelTool = StudioState::PixelTool::Pencil;
                studio.textureNeedsUpdate = true;
            }
            // Bucket Fill Tool
            else if (studio.activePixelTool == StudioState::PixelTool::BucketFill && lmbDown && !studio.isPixelStrokeActive) {
                studio.PushUndo("Fill " + spec.id);
                studio.isPixelStrokeActive = true;
                if (spec.format == AssetFormat::Rgb565) {
                    std::vector<uint32_t> editRgba;
                    FirmwareBitmap::DecodeRgb565(raw, spec.byte_len(), spec.width, spec.height, editRgba);
                    uint32_t targetCol = editRgba[hoverPy * spec.width + hoverPx];
                    uint32_t fillCol = 0xFF000000 | ((uint32_t)fgCol.b << 16) | ((uint32_t)fgCol.g << 8) | fgCol.r;
                    FloodFillRgb(editRgba, spec.width, spec.height, hoverPx, hoverPy, targetCol, fillCol);
                    std::vector<uint8_t> newRaw;
                    if (FirmwareBitmap::EncodeRgb565(editRgba.data(), spec.width, spec.height, newRaw)) {
                        studio.SetBitmapReplacement(catIdx, std::move(newRaw));
                    }
                } else {
                    std::vector<uint8_t> editGray;
                    FirmwareBitmap::DecodeMask(raw, spec.byte_len(), spec.width, spec.height, editGray, spec.format);
                    uint8_t targetVal = editGray[hoverPy * spec.width + hoverPx];
                    uint8_t fillVal = (targetVal > 127) ? 0 : 255;
                    FloodFillMask(editGray, spec.width, spec.height, hoverPx, hoverPy, targetVal, fillVal);
                    std::vector<uint8_t> newRaw;
                    if (FirmwareBitmap::EncodeMaskPreserving(editGray.data(), spec.width, spec.height, studio.originals[catIdx].data(), spec.byte_len(), newRaw, spec.format)) {
                        studio.SetBitmapReplacement(catIdx, std::move(newRaw));
                    }
                }
                studio.textureNeedsUpdate = true;
            }
            // Pencil & Eraser with Brush Size
            else if (studio.activePixelTool == StudioState::PixelTool::Pencil || studio.activePixelTool == StudioState::PixelTool::Eraser) {
                if (!studio.isPixelStrokeActive) {
                    studio.PushUndo("Pixel stroke " + spec.id);
                    studio.isPixelStrokeActive = true;
                }
                bool drawPixel = (lmbDown && studio.activePixelTool == StudioState::PixelTool::Pencil);
                if (rmbDown) drawPixel = false;

                int bSize = studio.brushSize;
                int half = bSize / 2;
                int startDx = -(bSize - 1) / 2;
                int endDx = bSize / 2;
                int startDy = -(bSize - 1) / 2;
                int endDy = bSize / 2;

                if (spec.format == AssetFormat::Rgb565) {
                    std::vector<uint32_t> editRgba;
                    FirmwareBitmap::DecodeRgb565(raw, spec.byte_len(), spec.width, spec.height, editRgba);
                    uint32_t newCol = drawPixel ? (0xFF000000 | ((uint32_t)fgCol.b << 16) | ((uint32_t)fgCol.g << 8) | fgCol.r) : 0xFF000000;

                    bool changed = false;
                    for (int dy = startDy; dy <= endDy; ++dy) {
                        for (int dx = startDx; dx <= endDx; ++dx) {
                            int px = hoverPx + dx;
                            int py = hoverPy + dy;
                            if (px >= 0 && px < (int)spec.width && py >= 0 && py < (int)spec.height) {
                                size_t idx = (size_t)py * spec.width + px;
                                if (editRgba[idx] != newCol) {
                                    editRgba[idx] = newCol;
                                    changed = true;
                                }
                            }
                        }
                    }
                    if (changed) {
                        std::vector<uint8_t> newRaw;
                        if (FirmwareBitmap::EncodeRgb565(editRgba.data(), spec.width, spec.height, newRaw)) {
                            studio.SetBitmapReplacement(catIdx, std::move(newRaw));
                            studio.textureNeedsUpdate = true;
                        }
                    }
                } else {
                    std::vector<uint8_t> editGray;
                    FirmwareBitmap::DecodeMask(raw, spec.byte_len(), spec.width, spec.height, editGray, spec.format);
                    uint8_t newCol = drawPixel ? 255 : 0;

                    bool changed = false;
                    for (int dy = startDy; dy <= endDy; ++dy) {
                        for (int dx = startDx; dx <= endDx; ++dx) {
                            int px = hoverPx + dx;
                            int py = hoverPy + dy;
                            if (px >= 0 && px < (int)spec.width && py >= 0 && py < (int)spec.height) {
                                size_t idx = (size_t)py * spec.width + px;
                                if (editGray[idx] != newCol) {
                                    editGray[idx] = newCol;
                                    changed = true;
                                }
                            }
                        }
                    }
                    if (changed) {
                        std::vector<uint8_t> newRaw;
                        if (FirmwareBitmap::EncodeMaskPreserving(editGray.data(), spec.width, spec.height, studio.originals[catIdx].data(), spec.byte_len(), newRaw, spec.format)) {
                            studio.SetBitmapReplacement(catIdx, std::move(newRaw));
                            studio.textureNeedsUpdate = true;
                        }
                    }
                }
            }
        }
    } else {
        if (studio.isPixelStrokeActive) studio.EndUndoAction();
        studio.isPixelStrokeActive = false;
    }

    // Overlay: Pixel Grid
    if (studio.previewShowGrid && studio.previewZoom >= 4) {
        ImU32 gridCol = IM_COL32(80, 80, 90, 110);
        for (uint32_t gx = 1; gx < spec.width; ++gx) {
            float x = p0.x + gx * pZoom;
            drawList->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), gridCol);
        }
        for (uint32_t gy = 1; gy < spec.height; ++gy) {
            float y = p0.y + gy * pZoom;
            drawList->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y), gridCol);
        }
    }

    // Overlay: Brush Footprint Hover Cursor
    if (canvasHovered && inBounds) {
        int bSize = studio.brushSize;
        int startDx = -(bSize - 1) / 2;
        int endDx = bSize / 2;
        int startDy = -(bSize - 1) / 2;
        int endDy = bSize / 2;

        float bx0 = p0.x + (hoverPx + startDx) * pZoom;
        float by0 = p0.y + (hoverPy + startDy) * pZoom;
        float bx1 = p0.x + (hoverPx + endDx + 1) * pZoom;
        float by1 = p0.y + (hoverPy + endDy + 1) * pZoom;

        ImU32 cursorCol = (studio.activePixelTool == StudioState::PixelTool::Eraser)
            ? IM_COL32(255, 80, 80, 200)
            : ((studio.activePixelTool == StudioState::PixelTool::Eyedropper)
                ? IM_COL32(80, 255, 120, 200)
                : ((studio.activePixelTool == StudioState::PixelTool::BucketFill)
                    ? IM_COL32(200, 100, 255, 200)
                    : IM_COL32(255, 255, 100, 200)));

        drawList->AddRect(ImVec2(bx0, by0), ImVec2(bx1, by1), cursorCol, 0.0f, 0, 2.0f);
    }
}

} // namespace UiPixelEditor
