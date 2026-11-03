#pragma once
#include "ui_pixel_editor.h"
#include "ui_resource_studio.h"
#include "imgui.h"

// Exercise real ImGui mouse events and GPU textures, without opening USB.
inline bool RunEditorRegressions(ID3D11Device* device, ID3D11DeviceContext* gpu, std::string& error) {
    StudioState state;
    state.catalog = {{"font_test", "", "", 0, 8, 8}, {"status_00", "", "", 0, 16, 4}};
    state.originals = {std::vector<uint8_t>(8, 0), std::vector<uint8_t>(8, 0)};
    state.referenceBitmaps = {std::vector<uint8_t>(8, 0xff), std::vector<uint8_t>(8, 0)};
    state.previewZoom = 4;
    auto check = [&](bool ok, const char* message) { if (!ok) { error += message; error += "; "; } };
    error.clear();
    for (const auto& theme : GetPaletteThemes()) {
        check(theme.background == Rgb8(0, 0, 0) && theme.digits_stats == Rgb8(255, 255, 255),
              "dark theme uses non-white statistics digits");
    }

    StudioState colors;
    colors.palette = GetDefaultPalette();
    for (auto& entry : colors.palette) entry.current = {12, 34, 56};
    const std::pair<const char*, Rgb8> fixed[] = {
        {"battery_frame", {255,255,255}}, {"battery_bolt", {255,255,255}},
        {"rank_label_01", {255,255,255}}, {"logo_drag_s", {220,199,0}},
        {"banner_activated", {243,219,1}}, {"icon_puff_cleared", {255,220,0}},
        {"icon_unlock_large", {90,225,1}}, {"icon_lock_large", {235,0,1}},
        {"icon_max_power", {234,99,0}}, {"icon_time_out", {234,99,0}},
        {"icon_low_power", {235,0,1}}, {"icon_check_battery", {235,0,1}},
        {"icon_check_atomizer_tank", {235,0,1}}, {"icon_check_atomizer_base", {0,223,255}}
    };
    for (const auto& item : fixed) {
        AssetSpec spec; spec.id = item.first; spec.ordinal = 1;
        check(colors.GetForegroundForAsset(spec) == item.second, "theme changed fixed foreground");
    }
    for (const auto& spec : GetAllAssets()) {
        if (spec.id.rfind("state_", 0) == 0)
            check(colors.GetForegroundForAsset(spec) == Rgb8(12,34,56), "state ignores highlight palette");
        if (spec.id.rfind("meter_", 0) == 0) {
            colors.previewRank = 3;
            check(colors.GetForegroundForAsset(spec) == Rgb8(12,34,56), "meter uses frame ordinal as rank");
            colors.previewRank = 0;
            check(colors.GetForegroundForAsset(spec) == Rgb8(255,255,255), "meter rank selection ignored");
        }
    }

    colors.palette = GetDefaultPalette();
    colors.palette[0].current = {255, 0, 0};
    colors.palette[1].current = {0, 255, 255};
    AssetSpec puffDigit; puffDigit.id = "font_11x20_00";
    colors.previewCyanMode = false;
    check(colors.GetBackgroundForAsset(puffDigit) == Rgb8(255,0,0), "puff digit background lost red mode");
    colors.previewCyanMode = true;
    check(colors.GetBackgroundForAsset(puffDigit) == Rgb8(0,255,255), "puff digit background lost cyan mode");
    colors.assetPaintColors[puffDigit.id] = {12,34,56};
    check(colors.GetForegroundForAsset(puffDigit) == Rgb8(255,255,255), "brush color tinted firmware mask");

    UiResourceStudio::UpdateReferenceTexture(state, 0, device, gpu);
    UiResourceStudio::UpdateReferenceTexture(state, 1, device, gpu);
    check(state.referencePreviewTexture.GetWidth() == 16 && state.referencePreviewTexture.GetHeight() == 4,
          "reference kept previous asset dimensions");
    // Same-size changes must also update pixel data (e.g. changing glyphs/colors).
    state.referenceBitmaps[1].assign(8, 0xff);
    state.palette = GetDefaultPalette();
    state.palette[0].current = Rgb8(255, 0, 0);
    UiResourceStudio::UpdateReferenceTexture(state, 1, device, gpu);
    ID3D11Resource* resource = nullptr;
    state.referencePreviewTexture.GetSRV()->GetResource(&resource);
    ID3D11Texture2D* texture = nullptr;
    resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture));
    resource->Release();
    D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* staging = nullptr;
    if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &staging))) {
        gpu->CopyResource(staging, texture);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(gpu->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
            check(*static_cast<uint32_t*>(mapped.pData) == 0xff0000ff, "reference retained previous pixels/color");
            gpu->Unmap(staging, 0);
        } else check(false, "GPU readback failed");
        staging->Release();
    } else check(false, "staging texture failed");
    texture->Release();

    auto* previous = ImGui::GetCurrentContext();
    auto* test = ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DisplaySize = ImVec2(400, 300); io.DeltaTime = 1.0f / 60;
    unsigned char* atlas; int w, h;
    io.Fonts->GetTexDataAsRGBA32(&atlas, &w, &h);
    io.Fonts->SetTexID((ImTextureID)1);
    auto frame = [&](bool left, bool right, float x = 46, float y = 66) {
        io.AddMousePosEvent(x, y); io.AddMouseButtonEvent(0, left); io.AddMouseButtonEvent(1, right);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImVec2(400, 300));
        ImGui::Begin("Editor regression", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
        ImGui::SetCursorScreenPos(ImVec2(40, 60));
        UiPixelEditor::RenderCanvas(state, 0, device, gpu);
        ImGui::End(); ImGui::Render();
    };
    auto pixel = [&] {
        std::vector<uint8_t> gray;
        FirmwareBitmap::DecodeMask(state.GetCurrentRaw(0), 8, 8, 8, gray);
        return gray[9];
    };
    frame(false, false); frame(false, false);
    frame(true, false); frame(true, false); frame(false, false);
    check(pixel() == 255, "pencil left click did not draw");
    state.activePixelTool = StudioState::PixelTool::Eraser;
    frame(true, false); frame(false, false);
    check(state.replacements.count("font_test") == 0, "manual restoration retained modified marker");
    state.Undo();
    check(pixel() == 255, "undo manual restoration failed");
    state.Redo();
    check(pixel() == 0 && state.replacements.count("font_test") == 0, "redo manual restoration retained modification");

    state.replacements["font_test"].assign(8, 0xff); state.textureNeedsUpdate = true;
    state.activePixelTool = StudioState::PixelTool::Eraser;
    frame(true, false); frame(false, false);
    check(pixel() == 0, "eraser left click did not erase");
    state.replacements["font_test"].assign(8, 0); state.textureNeedsUpdate = true;
    state.activePixelTool = StudioState::PixelTool::BucketFill;
    frame(true, false); frame(false, false);
    check(pixel() == 255, "bucket fill left click did not fill");
    state.replacements["font_test"].assign(8, 0xff); state.textureNeedsUpdate = true;
    state.activePixelTool = StudioState::PixelTool::Pencil;
    frame(false, true); frame(false, false);
    check(pixel() == 0, "right click did not erase");
    check(state.originals[0] == std::vector<uint8_t>(8, 0), "editing changed original bitmap");
    state.replacements["font_test"].assign(8, 0); state.textureNeedsUpdate = true;
    frame(true, false); frame(true, false, 50, 66); frame(false, false);
    std::vector<uint8_t> stroke;
    FirmwareBitmap::DecodeMask(state.GetCurrentRaw(0), 8, 8, 8, stroke);
    check(stroke[9] == 255 && stroke[10] == 255, "held pencil did not draw while moving");
    // A drag that starts outside the canvas must not erase when it enters.
    frame(false, true, 20, 66); frame(false, true); frame(false, false);
    check(pixel() == 255, "outside click leaked into canvas");
    state.activePixelTool = StudioState::PixelTool::Eyedropper;
    frame(true, false); frame(false, false);
    check(state.activePixelTool == StudioState::PixelTool::Pencil &&
          state.assetPaintColors.count("font_test") != 0, "eyedropper did not sample");
    state.catalog[0].id = "rgb_test"; state.catalog[0].format = AssetFormat::Rgb565;
    state.originals[0].assign(128, 0); state.textureNeedsUpdate = true;
    state.assetPaintColors["rgb_test"] = Rgb8(255, 0, 0);
    frame(true, false); frame(false, false);
    std::vector<uint32_t> rgb;
    FirmwareBitmap::DecodeRgb565(state.GetCurrentRaw(0), 128, 8, 8, rgb);
    check(rgb[9] == 0xff0000ff, "RGB565 pencil did not write selected color");

    // Selecting a different RGB brush must not recolor already stored pixels.
    state.assetPaintColors["rgb_test"] = Rgb8(0, 255, 0);
    state.textureNeedsUpdate = true;
    frame(false, false);
    ID3D11Resource* activeResource = nullptr;
    state.activePreviewTexture.GetSRV()->GetResource(&activeResource);
    ID3D11Texture2D* activeTexture = nullptr;
    activeResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&activeTexture));
    activeResource->Release();
    activeTexture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* activeStaging = nullptr;
    if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &activeStaging))) {
        gpu->CopyResource(activeStaging, activeTexture);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(gpu->Map(activeStaging, 0, D3D11_MAP_READ, 0, &mapped))) {
            const auto* row = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(mapped.pData) + mapped.RowPitch);
            check(row[1] == 0xff0000ff, "RGB brush recolored existing preview pixels");
            gpu->Unmap(activeStaging, 0);
        } else check(false, "active GPU readback failed");
        activeStaging->Release();
    } else check(false, "active staging texture failed");
    activeTexture->Release();

    ImGui::DestroyContext(test); ImGui::SetCurrentContext(previous);
    return error.empty();
}
