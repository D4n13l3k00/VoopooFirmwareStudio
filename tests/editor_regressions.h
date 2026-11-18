#pragma once
#include "ui_tabs.h"
#include "ui_pixel_editor.h"
#include "ui_resource_studio.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "firmware_logo_preview.h"
#include "ui_firmware_patches.h"
#include "ui_firmware_menu.h"
#include "ui_firmware_layout.h"
#include "embedded_firmware.h"
#include "ui_capture.h"
#include "ui_firmware_risk.h"
#include "ui_collapsible_section.h"
#include "ui_firmware_emulator.h"
#include <chrono>
#include <thread>

// Exercise real ImGui mouse events and GPU textures, without opening USB.
inline bool RunEditorRegressions(ID3D11Device* device, ID3D11DeviceContext* gpu, std::string& error) {
    StudioState state;
    state.catalog = {{"font_test", "", "", 0, 8, 8}, {"status_00", "", "", 0, 16, 4}};
    state.originals = {std::vector<uint8_t>(8, 0), std::vector<uint8_t>(8, 0)};
    state.referenceBitmaps = {std::vector<uint8_t>(8, 0xff), std::vector<uint8_t>(8, 0)};
    state.previewZoom = 4;
    auto check = [&](bool ok, const char* message) { if (!ok) { error += message; error += "; "; } };
    error.clear();
    {
        const auto originalLanguage = i18n::GetCurrentLanguage();
        StudioState search;
        search.catalog = {
            {"unit_w", "Unit W", "Units", 0, 8, 8},
            {"font_6x10_07", "Digit 7", "Font 6x10", 0, 6, 10}
        };
        for (const auto& language : i18n::GetSupportedLanguages()) {
            i18n::SetLanguage(language.lang);
            const auto title = PathUtils::Utf8ToWide(i18n::ResourceText("Unit W"));
            const int size = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE,
                title.data(), static_cast<int>(title.size()), nullptr, 0, nullptr, nullptr, 0);
            std::wstring upper(size, L'\0');
            LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE,
                title.data(), static_cast<int>(title.size()), upper.data(), size, nullptr, nullptr, 0);
            strncpy_s(search.filterBuf, PathUtils::WideToUtf8(upper).c_str(), sizeof(search.filterBuf));
            for (const auto& interfaceLanguage : i18n::GetSupportedLanguages()) {
                i18n::SetLanguage(interfaceLanguage.lang);
                search.UpdateVisible();
                check(search.visibleAssetIndices == std::vector<size_t>{0},
                    "localized uppercase search depends on interface language");
            }
            strcpy_s(search.filterBuf, "FONT_6X10_07");
            search.UpdateVisible();
            check(search.visibleAssetIndices == std::vector<size_t>{1},
                "resource ID search changed with interface language");
        }
        i18n::SetLanguage(i18n::Language::Russian);
        const std::string russianTitle = i18n::ResourceText("Unit W");
        strncpy_s(search.filterBuf, russianTitle.c_str(), sizeof(search.filterBuf));
        search.UpdateVisible();
        i18n::SetLanguage(i18n::Language::English);
        search.SynchronizeEditorState();
        check(search.visibleAssetIndices == std::vector<size_t>{0},
            "language change lost cross-language search results");
        check(search.catalog[0].title == "Unit W" && search.catalog[0].id == "unit_w",
            "localization changed canonical resource identity");
        i18n::SetLanguage(originalLanguage);
    }
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

// Render the production section helper through WARP and inspect actual seam pixels.
inline bool RunCollapsibleSectionUiRegressions(ID3D11Device* device, ID3D11DeviceContext* gpu,
    std::string& error) {
    auto& io = ImGui::GetIO();
    const auto oldConfig = io.ConfigFlags;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    struct RestoreConfig {
        ImGuiIO& io;
        ImGuiConfigFlags flags;
        ~RestoreConfig() { io.ConfigFlags = flags; }
    } restore{io, oldConfig};
    ImGuiWindow *parent = nullptr, *body = nullptr, *nested = nullptr, *second = nullptr;
    ImRect header, bodyRect;
    ImGuiID headerId = 0, secondId = 0;
    bool open = false, secondOpen = false, balanced = true;
    auto frame = [&](ImVec2 mouse = ImVec2(-100, -100), bool pressed = false) {
        io.AddMousePosEvent(mouse.x, mouse.y); io.AddMouseButtonEvent(0, pressed);
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.10f, .12f, .14f, 1));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.40f, .50f, .60f, 1));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(.28f, .32f, .38f, 1));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(.28f, .32f, .38f, 1));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(.28f, .32f, .38f, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 13));
        ImGui::SetNextWindowPos(ImVec2(20, 20)); ImGui::SetNextWindowSize(ImVec2(460, 620));
        ImGui::Begin("Joined sections regression", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        parent = ImGui::GetCurrentWindow();
        const int colors = GImGui->ColorStack.Size, styles = GImGui->StyleVarStack.Size;
        const int windows = GImGui->CurrentWindowStack.Size, ids = parent->IDStack.Size;
        const auto start = ImGui::GetCursorScreenPos();
        const float extend = float(int(parent->WindowPadding.x * .5f));
        header = ImRect(ImVec2(start.x - extend, start.y),
            ImVec2(parent->WorkRect.Max.x + extend, start.y + ImGui::GetFrameHeight()));
        ImGui::PushID("FirstBody"); headerId = ImGui::GetID("##SectionHeader"); ImGui::PopID();
        open = secondOpen = false;
        if (Ui::CollapsibleSection section{"FirstBody", "Repeated title",
            ImGuiTreeNodeFlags_DefaultOpen}; section) {
            open = true; body = ImGui::GetCurrentWindow();
            bodyRect = ImRect(body->Pos, ImVec2(body->Pos.x + body->Size.x, body->Pos.y + body->Size.y));
            ImGui::Button("Body action");
            if (Ui::CollapsibleSection inner{"NestedBody", "Repeated title",
                ImGuiTreeNodeFlags_DefaultOpen}; inner) {
                nested = ImGui::GetCurrentWindow();
                ImGui::TextUnformatted("Nested content");
            }
        }
        ImGui::PushID("SecondBody"); secondId = ImGui::GetID("##SectionHeader"); ImGui::PopID();
        if (Ui::CollapsibleSection section{"SecondBody", "Repeated title",
            ImGuiTreeNodeFlags_DefaultOpen}; section) {
            secondOpen = true; second = ImGui::GetCurrentWindow();
            ImGui::TextUnformatted("Independent sibling");
        }
        const auto earlyReturn = [&] {
            Ui::CollapsibleSection section{"EarlyReturnBody", "Early return", ImGuiTreeNodeFlags_DefaultOpen};
            if (!section) return;
            ImGui::TextUnformatted("Scope closes on return");
            return;
        };
        earlyReturn();
        balanced &= ImGui::GetCurrentWindow() == parent && colors == GImGui->ColorStack.Size &&
            styles == GImGui->StyleVarStack.Size && windows == GImGui->CurrentWindowStack.Size &&
            ids == parent->IDStack.Size;
        ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor(5); ImGui::Render();
    };
    auto pixels = [&](std::vector<uint32_t>& rgba) {
        using Microsoft::WRL::ComPtr;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = UINT(io.DisplaySize.x); desc.Height = UINT(io.DisplaySize.y);
        desc.MipLevels = desc.ArraySize = 1; desc.SampleDesc.Count = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> target, staging;
        ComPtr<ID3D11RenderTargetView> view;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &target)) ||
            FAILED(device->CreateRenderTargetView(target.Get(), nullptr, &view))) return false;
        auto* renderTarget = view.Get();
        gpu->OMSetRenderTargets(1, &renderTarget, nullptr);
        const float clear[]{0, 0, 0, 1}; gpu->ClearRenderTargetView(renderTarget, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) {
            gpu->OMSetRenderTargets(0, nullptr, nullptr); return false;
        }
        gpu->CopyResource(staging.Get(), target.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(gpu->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
            gpu->OMSetRenderTargets(0, nullptr, nullptr); return false;
        }
        rgba.resize(size_t(desc.Width) * desc.Height);
        for (UINT y = 0; y < desc.Height; ++y)
            std::memcpy(rgba.data() + size_t(y) * desc.Width,
                static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch, size_t(desc.Width) * 4);
        gpu->Unmap(staging.Get(), 0); gpu->OMSetRenderTargets(0, nullptr, nullptr);
        return true;
    };
    auto matches = [&](const std::vector<uint32_t>& rgba, float x, float y, ImVec4 expected) {
        const auto actual = rgba[size_t(int(y)) * size_t(io.DisplaySize.x) + int(x)];
        const auto packed = ImGui::ColorConvertFloat4ToU32(expected);
        for (int shift : {0, 8, 16})
            if (std::abs(int((actual >> shift) & 255) - int((packed >> shift) & 255)) > 1) return false;
        return true;
    };
    for (int i = 0; i < 4; ++i) frame();
    if (!balanced || !open || !secondOpen || !body || !nested || !second ||
        body->ParentWindow != parent || nested->ParentWindow != body || second->ParentWindow != parent ||
        headerId == secondId || body->ChildId == headerId ||
        std::abs(bodyRect.Min.y - header.Max.y) > .01f ||
        bodyRect.Min.x < header.Min.x || bodyRect.Max.x > header.Max.x ||
        std::abs((bodyRect.Min.x - header.Min.x) - (header.Max.x - bodyRect.Max.x)) > .01f) {
        error = "Joined sections lost seam geometry, nesting, unique IDs or early-return scope balance"; return false;
    }
    std::vector<uint32_t> rgba;
    if (!pixels(rgba)) { error = "Joined section WARP readback failed"; return false; }
    const float sampleX = header.Max.x - 24;
    if (!matches(rgba, sampleX, header.Max.y - 3, ImVec4(.28f, .32f, .38f, 1)) ||
        !matches(rgba, sampleX, header.Max.y + 1, ImVec4(.40f * .86f, .50f * .86f, .60f * .86f, 1)) ||
        !matches(rgba, sampleX, header.Max.y + 4, ImVec4(.40f * .86f, .50f * .86f, .60f * .86f, 1))) {
        error = "Expanded section has a visible seam or changed header/darkened-body colors"; return false;
    }
    const auto click = ImVec2(header.Min.x + 15, (header.Min.y + header.Max.y) * .5f);
    frame(click); frame(click, true); frame(click); frame();
    if (open || !secondOpen || body->LastFrameActive == GImGui->FrameCount || !balanced) {
        error = "Collapsing one section retained its body or changed an identically titled sibling"; return false;
    }
    if (!pixels(rgba) || !matches(rgba, sampleX, header.Max.y + 3, ImVec4(.10f, .12f, .14f, 1))) {
        error = "Collapsed section retained background below its header"; return false;
    }
    ImGui::FocusWindow(parent); ImGui::SetFocusID(headerId, parent); ImGui::SetNavCursorVisible(true);
    io.AddKeyEvent(ImGuiKey_RightArrow, true); frame();
    io.AddKeyEvent(ImGuiKey_RightArrow, false); frame(); frame();
    if (!open || !secondOpen || !balanced) {
        error = "Joined section could not expand using native keyboard navigation"; return false;
    }
    io.AddKeyEvent(ImGuiKey_LeftArrow, true); frame();
    io.AddKeyEvent(ImGuiKey_LeftArrow, false); frame(); frame();
    if (open || !secondOpen || !balanced) {
        error = "Joined section could not collapse using native keyboard navigation"; return false;
    }
    const auto savedPreferences = StudioPreferences::Get();
    struct RestorePreferences {
        StudioPreferences::Preferences saved;
        ~RestorePreferences() { StudioPreferences::Get() = saved; }
    } restorePreferences{savedPreferences};
    auto& defaults = StudioPreferences::Get().sectionOpen;
    for (size_t i = 0; i < defaults.size(); ++i) defaults[i] = i % 2 == 0;
    std::array<bool, size_t(StudioPreferences::Section::Count)> actual{};
    bool translated = false;
    const auto preferenceFrame = [&](const char* view, bool toggle) {
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(460, 800));
        ImGui::Begin(view, nullptr, ImGuiWindowFlags_NoTitleBar);
        for (size_t i = 0; i < actual.size(); ++i) {
            const char* id = StudioPreferences::SectionIds[i];
            ImGui::PushID(id);
            const ImGuiID header = ImGui::GetID("##SectionHeader");
            ImGui::PopID();
            auto* storage = ImGui::GetStateStorage();
            if (toggle) storage->SetInt(header, !actual[i]);
            {
                Ui::CollapsibleSection section{id, translated ? "Translated section" : "Preference section",
                    ImGuiTreeNodeFlags_DefaultOpen};
                if (section) ImGui::TextUnformatted("Body");
            }
            actual[i] = storage->GetInt(header, -1) == 1;
        }
        ImGui::End(); ImGui::Render();
    };
    preferenceFrame("Initial preference sections", false);
    if (actual != defaults) { error = "Not all nine sections used configured first-encounter defaults"; return false; }
    preferenceFrame("Initial preference sections", true);
    const auto toggled = actual;
    translated = true;
    defaults.fill(true);
    preferenceFrame("Initial preference sections", false);
    if (actual != toggled) { error = "Changing section defaults overwrote current view toggles"; return false; }
    preferenceFrame("Fresh preference sections", false);
    if (actual != defaults) { error = "Fresh section view did not use changed defaults"; return false; }
    return true;
}

// Enter the real emulator tab without saving, then exercise its native preview,
// first device load, and explicit Apply against independently retained VM state.
inline bool RunPatchEmulatorBoundaryUiRegressions(ID3D11Device* device,
    ID3D11DeviceContext* gpu, std::string& error) {
    const auto directory = std::filesystem::temp_directory_path() /
        ("vfs-patch-emulator-" + std::to_string(GetCurrentProcessId()) + "-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct RemoveDirectory {
        std::filesystem::path path;
        ~RemoveDirectory() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
    } removeDirectory{directory};
    FirmwarePackage package;
    StudioState document;
    if (!LoadEmbeddedFirmware(package, error) ||
        !document.LoadPackage(std::move(package), "patch-emulator-ui.zwdf", error)) return false;
    document.MarkSaved();
    const auto original = document.screenPatches;
    const auto originalSources = document.screenPatchSources;
    const auto originalRevision = document.ResourceRevision();
    UiFirmwarePatches::State editor;
    UiFirmwarePatches::SynchronizeDocument(editor, document);
    UiFirmwarePatches::SelectTarget(editor, FirmwareLogo::Target::DeviceInfo);
    UiResourceStudio::BindPatchEditor(&editor);
    struct Unbind { ~Unbind() { UiResourceStudio::BindPatchEditor(nullptr); } } unbind;
    UiFirmwareEmulator::State emulator{.live = {FirmwareDevice::Worker(directory)}};
    emulator.documentIdentity = document.DocumentIdentity();
    emulator.device.screen = 15;
    emulator.device.logoPhase = int(FirmwareLogo::PhaseCount) - 1;
    ImGuiTabBar* tabs = nullptr;
    auto frame = [&] {
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Patch to emulator boundary", nullptr, ImGuiWindowFlags_NoResize);
        if (Ui::BeginTabBar("PatchEmulatorSections")) {
            tabs = GImGui->CurrentTabBar;
            UiFirmwarePatches::RenderTab(editor, document, device, gpu);
            UiFirmwareEmulator::RenderTab(emulator, document, device, gpu);
            ImGui::EndTabBar();
        }
        ImGui::End(); ImGui::Render();
    };
    auto selectTab = [&](int order) {
        auto* tab = tabs ? ImGui::TabBarFindTabByOrder(tabs, order) : nullptr;
        if (!tab) { error = "Patch/emulator integration tab is missing"; return false; }
        ImGui::TabBarQueueFocus(tabs, tab);
        frame(); frame();
        return true;
    };
    auto wait = [&](const char* operation, auto ready) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        do {
            frame();
            if (!emulator.live.worker.Current().error.empty()) {
                error = std::string(operation) + ": " + emulator.live.worker.Current().error;
                return false;
            }
            if (ready()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        error = std::string(operation) + ": timed out";
        return false;
    };
    frame(); frame();
    std::vector<uint32_t> photo(size_t(FirmwareLogo::Width) * FirmwareLogo::Height);
    for (size_t i = 0; i < photo.size(); ++i)
        photo[i] = ((i / FirmwareLogo::Width / 8 + i % FirmwareLogo::Width / 8) & 1) ?
            0xff00ff00u : 0xffff0000u;
    const auto photoPath = PathUtils::WideToUtf8((directory / "photo.png").wstring());
    if (!FirmwareBitmap::SavePngRgb565(photoPath, photo.data(), FirmwareLogo::Width, FirmwareLogo::Height, error) ||
        !UiFirmwarePatches::LoadImage(editor, photoPath, error)) return false;
    const auto imported = editor.draft;
    editor.draft.animation = FirmwareLogo::Animation::SlideDownRight;
    editor.draft.durationMs = 1800;
    editor.draft.holdMs = 730;
    editor.draft.exitAnimation = FirmwareLogo::Animation::SoftWipeUpLeft;
    editor.draft.exitDurationMs = 1200;
    const auto expected = editor.draft;
    UiFirmwarePatches::RecordDraftChange(editor);
    frame();
    if (emulator.activeTab || document.ResourceRevision() != originalRevision ||
        document.screenPatches != original || document.screenPatchSources != originalSources ||
        document.HasUnsavedChanges() || !UiResourceStudio::HasUnsavedChanges(document) ||
        !UiFirmwarePatches::UndoDraft(editor) || editor.draft != imported) {
        error = "Inactive emulator committed edits or broke local photo/animation undo"; return false;
    }
    editor.draft = expected;
    UiFirmwarePatches::RecordDraftChange(editor);
    const auto historySize = document.undoStack.size();
    const auto draftHistorySize = editor.undo.size();
    if (!selectTab(1)) return false;
    constexpr size_t info = size_t(FirmwareLogo::Target::DeviceInfo);
    if (!emulator.activeTab || document.screenPatches[info] != expected ||
        !document.screenPatchSources[info].rgba || document.screenPatchSources[info].rgba != editor.sourceImage ||
        document.undoStack.size() != historySize + 1 || !document.HasUnsavedChanges() ||
        UiFirmwarePatches::HasDraftChanges(editor, document) || editor.undo.size() != draftHistorySize) {
        error = "Entering emulator did not atomically publish the CHIP ID photo and complete animation"; return false;
    }
    const auto snapshot = document.CreateEmulationSnapshot();
    std::vector<uint8_t> bytes;
    FirmwarePackage built;
    FirmwareLogo::Patches decoded;
    if (snapshot->screenPatches[info] != expected ||
        !snapshot->BuildZwdfBytes(bytes, error) || !FirmwarePackage::Parse(bytes, built, error) ||
        !FirmwareLogo::Canonicalize(built, decoded, error)) return false;
    if (decoded[info] != expected || decoded[0] != original[0] || decoded[1] != original[1]) {
        error = "Actual emulation snapshot lost photo/animation settings or changed another screen"; return false;
    }
    if (!wait("CHIP ID native preview", [&] {
        return !emulator.preview.Busy() && emulator.preview.Published();
    })) return false;
    if (!emulator.preview.After().error.empty() || emulator.preview.After().rgba != editor.canvas) {
        error = "Emulator native preview did not render the unsaved CHIP ID photo: " + emulator.preview.After().error;
        return false;
    }
    for (const int phase : {16, 31}) {
        const auto generation = emulator.preview.AfterGeneration();
        emulator.device.logoPhase = phase;
        emulator.dirty = true;
        frame();
        if (!wait("CHIP ID native exit phase", [&] {
            return !emulator.preview.Busy() && emulator.preview.AfterGeneration() > generation;
        })) return false;
        std::vector<uint32_t> expectedFrame;
        FirmwareLogo::PreviewFrame(expected, uint32_t(phase), expectedFrame);
        if (emulator.device.logoPhase != phase || !emulator.preview.After().error.empty() ||
            emulator.preview.After().rgba != expectedFrame) {
            error = "CHIP ID native UI reset/skipped the entrance-to-exit boundary or final phase: " +
                emulator.preview.After().error;
            return false;
        }
    }
    emulator.wholeDevice = true;
    frame();
    if (!wait("first device load", [&] {
        return emulator.live.worker.Current().loaded;
    })) return false;
    const auto firstRevision = document.ResourceRevision();
    if (emulator.live.loadedRevision != firstRevision ||
        emulator.live.worker.Current().revision != firstRevision) {
        error = "First device load used the document from before draft publication"; return false;
    }
    UiFirmwareDevice::PauseAndRelease(emulator.live);
    if (!wait("pause initial snapshot", [&] { return !emulator.live.worker.Current().running; })) return false;
    const auto paused = emulator.live.worker.Current().state;
    if (!selectTab(0)) return false;
    if (!UiFirmwarePatches::UndoDraft(editor) || editor.draft != imported ||
        !UiFirmwarePatches::UndoDraft(editor) || editor.draft != original[info] ||
        editor.sourceImage || document.screenPatches[info] != expected) {
        error = "Returning from emulator lost target-local photo/animation undo"; return false;
    }
    if (!selectTab(1)) return false;
    if (document.screenPatches != original || document.screenPatchSources != originalSources ||
        emulator.live.loadedRevision != firstRevision || emulator.live.worker.Current().revision != firstRevision) {
        error = "Publishing local photo undo retained its source or replaced the running snapshot"; return false;
    }
    if (!selectTab(0) || !UiFirmwarePatches::LoadImage(editor, photoPath, error)) return false;
    editor.adjustments.brightness = -40;
    UiFirmwarePatches::ComposeLogo(editor);
    editor.draft.animation = FirmwareLogo::Animation::DiagonalUpLeft;
    editor.draft.durationMs = 400;
    editor.draft.holdMs = 2400;
    editor.draft.exitAnimation = FirmwareLogo::Animation::BlindsUpLeft;
    editor.draft.exitDurationMs = 900;
    UiFirmwarePatches::RecordDraftChange(editor);
    const auto changed = editor.draft;
    if (!selectTab(1)) return false;
    if (document.screenPatches[info] != changed || document.ResourceRevision() == firstRevision ||
        emulator.live.loadedRevision != firstRevision || emulator.live.worker.Current().revision != firstRevision ||
        emulator.live.worker.Current().state.virtualMicroseconds != paused.virtualMicroseconds ||
        emulator.live.worker.Current().state.instructions != paused.instructions || emulator.live.wantsRunning) {
        error = "Publishing a second CHIP ID draft silently replaced or resumed the existing VM"; return false;
    }
    ImGuiWindow* controls = nullptr;
    for (auto* window : GImGui->Windows)
        if (window->LastFrameActive == GImGui->FrameCount && std::strstr(window->Name, "LiveDeviceControls"))
            controls = window;
    if (!controls) { error = "Explicit Apply controls are missing"; return false; }
    ImGui::ActivateItemByID(controls->GetID(Tr(i18n::Str::DeviceApply)));
    frame();
    if (emulator.live.loadedRevision != document.ResourceRevision()) {
        error = "Explicit VM Apply did not consume the published CHIP ID draft"; return false;
    }
    if (!wait("explicit Apply snapshot", [&] {
        const auto& view = emulator.live.worker.Current();
        return view.loaded && view.revision == document.ResourceRevision();
    })) return false;
    if (!document.Undo()) { error = "Published draft lost document undo"; return false; }
    UiFirmwarePatches::SynchronizeDocument(editor, document);
    if (document.screenPatches != original || editor.draft != original[info] ||
        document.screenPatchSources != originalSources) {
        error = "Document undo did not restore the prior CHIP ID photo and complete animation"; return false;
    }
    editor.draft = changed;
    UiFirmwarePatches::RecordDraftChange(editor);
    StudioState replacement;
    if (!replacement.LoadPackage(document.firmware, "replacement-ui.zwdf", error)) return false;
    const auto replacementRevision = replacement.ResourceRevision();
    UiResourceStudio::CommitPatchDrafts(replacement, true);
    if (replacement.screenPatches != replacement.originalScreenPatches ||
        replacement.screenPatchSources != replacement.originalScreenPatchSources ||
        replacement.ResourceRevision() != replacementRevision || !replacement.undoStack.empty()) {
        error = "Stale bound drafts were published into a replacement document"; return false;
    }
    // Exercise the actual phase slider, including exit-only animations, for all
    // three targets. A right-edge mouse click must reach the complete sequence.
    UiFirmwareDevice::PauseAndRelease(emulator.live);
    FirmwareEmulator::DeviceState phaseDevice;
    ImRect phaseBounds;
    auto phaseFrame = [&](ImVec2 mouse, bool pressed) {
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent(mouse.x, mouse.y); io.AddMouseButtonEvent(0, pressed);
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Complete native phase controls", nullptr, ImGuiWindowFlags_NoResize);
        auto* window = ImGui::GetCurrentWindow();
        const auto tableId = ImGui::GetID("ScreenParameters");
        ImGui::SetNavWindow(window);
        ImGui::SetNavID(ImHashStr("##AnimationPhase", 0, tableId), ImGuiNavLayer_Main,
            window->NavRootFocusScopeId, ImRect());
        UiFirmwareEmulator::DrawDeviceControls(phaseDevice, replacement);
        phaseBounds = ImGui::WindowRectRelToAbs(window, window->NavRectRel[ImGuiNavLayer_Main]);
        ImGui::End(); ImGui::Render();
    };
    constexpr uint32_t screens[] = {1, 3, 15};
    for (size_t target = 0; target < FirmwareLogo::TargetCount; ++target) {
        for (const bool exitOnly : {false, true}) {
            auto settings = expected;
            if (exitOnly) settings.animation = FirmwareLogo::Animation::None;
            replacement.SetScreenPatch(static_cast<FirmwareLogo::Target>(target), settings);
            phaseDevice.screen = screens[target];
            phaseDevice.logoPhase = -1;
            for (int warm = 0; warm < 3; ++warm) phaseFrame(ImVec2(-100, -100), false);
            const ImVec2 end(phaseBounds.Max.x - 1, phaseBounds.GetCenter().y);
            phaseFrame(end, false); phaseFrame(end, true); phaseFrame(end, false);
            if (phaseDevice.logoPhase != (exitOnly ? 16 : 31) ||
                replacement.screenPatches[target] != settings) {
                error = "Native phase slider timing: screen=" + std::to_string(phaseDevice.screen) +
                    " exitOnly=" + std::to_string(exitOnly) + " phase=" + std::to_string(phaseDevice.logoPhase) +
                    " bounds=" + std::to_string(phaseBounds.Min.x) + "," + std::to_string(phaseBounds.Min.y) +
                    ".." + std::to_string(phaseBounds.Max.x) + "," + std::to_string(phaseBounds.Max.y);
                return false;
            }
        }
    }
    ImGui::GetIO().AddMousePosEvent(-100, -100);
    return true;
}

inline bool RunDraftPatchSetUiRegressions(ID3D11Device* device, ID3D11DeviceContext* gpu, std::string& error) {
    FirmwarePackage package;
    StudioState document;
    if (!LoadEmbeddedFirmware(package, error) ||
        !document.LoadPackage(std::move(package), "draft-set-ui.zwdf", error)) return false;
    FirmwareLogo::Settings large;
    large.enabled = true;
    large.pixels.resize(size_t(FirmwareLogo::Width) * FirmwareLogo::Height * 2);
    uint32_t random = 0x12345678;
    auto randomize = [&](FirmwareLogo::Settings& settings) {
        for (auto& byte : settings.pixels) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            byte = uint8_t(random);
        }
    };
    randomize(large);
    document.SetScreenPatch(FirmwareLogo::Target::Boot, large);
    UiFirmwarePatches::State editor;
    UiFirmwarePatches::SynchronizeDocument(editor, document);
    auto draft = [&](FirmwareLogo::Target target, const FirmwareLogo::Settings& settings) {
        UiFirmwarePatches::SelectTarget(editor, target);
        editor.draft = settings;
        FirmwareBitmap::DecodeRgb565(settings.pixels.data(), settings.pixels.size(),
            FirmwareLogo::Width, FirmwareLogo::Height, editor.canvas);
        UiFirmwarePatches::RecordDraftChange(editor);
        ++editor.draftRevision;
    };
    auto compactImage = large;
    std::fill(compactImage.pixels.begin(), compactImage.pixels.end(), 0);
    draft(FirmwareLogo::Target::Boot, compactImage);
    draft(FirmwareLogo::Target::DeviceInfo, compactImage);
    auto expected = document.screenPatches;
    expected[0] = expected[2] = compactImage;
    const auto resolved = UiFirmwarePatches::ResolveDraftPatches(editor, document);
    if (resolved != expected) { error = "Inactive smaller boot draft was omitted from the preview patch set"; return false; }
    FirmwareLogo::EditableSource externalSource;
    externalSource.fitMode = 2;
    document.SetScreenPatchSource(FirmwareLogo::Target::Boot, externalSource);
    if (UiFirmwarePatches::ResolveDraftPatches(editor, document)[0] != large) {
        error = "Draft resolver ignored an external source metadata conflict"; return false;
    }
    document.SetScreenPatchSource(FirmwareLogo::Target::Boot, {});
    auto externalSettings = compactImage;
    externalSettings.durationMs = 900;
    document.SetScreenPatch(FirmwareLogo::Target::Boot, externalSettings);
    if (UiFirmwarePatches::ResolveDraftPatches(editor, document)[0] != externalSettings) {
        error = "Draft resolver ignored an external applied settings conflict"; return false;
    }
    document.SetScreenPatch(FirmwareLogo::Target::Boot, large);
    StudioState other;
    if (UiFirmwarePatches::ResolveDraftPatches(editor, other) != other.screenPatches) {
        error = "Draft resolver leaked a previous document's screens"; return false;
    }
    auto frame = [&] {
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Coherent draft patch set", nullptr, ImGuiWindowFlags_NoResize);
        if (Ui::BeginTabBar("Sections")) {
            UiFirmwarePatches::RenderTab(editor, document, device, gpu);
            ImGui::EndTabBar();
        }
        ImGui::End(); ImGui::Render();
    };
    frame(); frame();
    if (editor.previewPatches != expected) {
        error = "Actual native preview retained a large committed inactive screen"; return false;
    }
    std::vector<FirmwareLogo::FlashUsage> usage;
    if (!FirmwareLogo::MeasureFlashChanges(document.firmware, document.originalScreenPatches,
        resolved, usage, error)) return false;
    std::vector<uint8_t> bytes;
    FirmwarePackage exported;
    if (!document.CreateEmulationSnapshot(&editor.previewPatches)->BuildZwdfBytes(bytes, error) ||
        !FirmwarePackage::Parse(bytes, exported, error)) return false;
    if (usage.size() != exported.records.size()) { error = "Draft flash accounting lost hardware records"; return false; }
    for (size_t i = 0; i < usage.size(); ++i) {
        if (usage[i].Used() != exported.records[i].image.size() || usage[i].Used() > usage[i].capacityBytes) {
            error = "Preview build and flash accounting disagree for the same smaller draft screens"; return false;
        }
    }
    // The reverse mismatch must not hide genuine overflow in either active or
    // inactive targets. Measure the exact preview set, then require real rejection.
    for (size_t i = 0; i < FirmwareLogo::TargetCount; ++i) {
        randomize(large);
        draft(static_cast<FirmwareLogo::Target>(i), large);
    }
    for (const auto active : {FirmwareLogo::Target::Boot, FirmwareLogo::Target::DeviceInfo}) {
        UiFirmwarePatches::SelectTarget(editor, active);
        frame(); frame();
        const auto overflowing = UiFirmwarePatches::ResolveDraftPatches(editor, document);
        if (editor.previewPatches != overflowing ||
            !FirmwareLogo::MeasureFlashChanges(document.firmware, document.originalScreenPatches,
                overflowing, usage, error)) return false;
        const bool overflow = std::any_of(usage.begin(), usage.end(),
            [](const auto& entry) { return entry.Used() > entry.capacityBytes; });
        std::string rejected;
        if (!overflow || document.CreateEmulationSnapshot(&editor.previewPatches)->BuildZwdfBytes(bytes, rejected) ||
            rejected.empty()) {
            error = "Resolving complete drafts concealed genuine Flash overflow"; return false;
        }
    }
    return true;
}

inline bool RunTabPaddingUiRegressions(ID3D11Device* device, ID3D11DeviceContext* gpu,
    const char* capturePath, std::string& error) {
    struct RestoreLanguage {
        i18n::Language saved = i18n::GetCurrentLanguage();
        ~RestoreLanguage() { i18n::SetLanguage(saved); }
    } restore;
    const i18n::Str titles[]{i18n::Str::TabResourceStudio, i18n::Str::TabFontGenerator,
        i18n::Str::TabFirmwareEmulator, i18n::Str::TabFirmwarePatches,
        i18n::Str::TabFirmwareComparison, i18n::Str::TabFirmwareInfo};
    for (const auto& language : i18n::GetSupportedLanguages()) {
        i18n::SetLanguage(language.lang);
        for (int frame = 0; frame < 3; ++frame) {
            ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImVec2(1280, 160));
            ImGui::Begin("Tab padding", nullptr, ImGuiWindowFlags_NoResize);
            const auto originalPadding = ImGui::GetStyle().FramePadding;
            bool equal = true;
            if (Ui::BeginTabBar("MainTabs")) {
                auto* bar = GImGui->CurrentTabBar;
                float firstInset = -1;
                for (size_t i = 0; i < std::size(titles); ++i) {
                    const char* text = Tr(titles[i]);
                    const char* end = text + std::strlen(text);
                    while (text < end && *text == ' ') ++text;
                    while (end > text && end[-1] == ' ') --end;
                    char label[256];
                    snprintf(label, sizeof(label), "%s###PaddingTab%zu", Tr(titles[i]), i);
                    const bool open = Ui::BeginTabItem(label);
                    const float inset = bar->Tabs[bar->LastTabItemIdx].ContentWidth - ImGui::CalcTextSize(text, end).x;
                    if (i == 0) firstInset = inset;
                    equal = equal && std::abs(inset - firstInset) < .01f &&
                        inset >= bar->FramePadding.x * 2 &&
                        ImGui::GetStyle().FramePadding.x == originalPadding.x &&
                        ImGui::GetStyle().FramePadding.y == originalPadding.y;
                    if (open) { ImGui::Button("Content control"); ImGui::EndTabItem(); }
                }
                ImGui::EndTabBar();
            }
            ImGui::End(); ImGui::Render();
            if (!equal) { error = "Tab title insets differ by label/language or leak into content"; return false; }
            if (capturePath && *capturePath && frame == 2 && language.lang == i18n::Language::Russian &&
                !CaptureUiDrawData(device, gpu, (std::string(capturePath) + ".tabs.png").c_str(), error)) return false;
        }
    }
    return true;
}

template<class RenderFrame>
bool CheckFlashPanelResize(RenderFrame&& render, const char* rootName,
    const char* previewName, std::string& error) {
    auto& io = ImGui::GetIO();
    const ImVec2 originalSize = io.DisplaySize;
    struct RestoreSize { ImVec2 size; ~RestoreSize() { ImGui::GetIO().DisplaySize = size; } } restore{originalSize};
    render(); render(); render();
    auto* root = ImGui::FindWindowByName(rootName);
    ImGuiWindow *preview = nullptr, *memory = nullptr;
    for (auto* child : ImGui::GetCurrentContext()->Windows) {
        for (auto* ancestor = child->ParentWindow; ancestor; ancestor = ancestor->ParentWindow)
            if (ancestor == root && std::strstr(child->Name, previewName)) preview = child;
    }
    if (preview)
        for (auto* child : ImGui::GetCurrentContext()->Windows)
            if (child->ParentWindow == preview->ParentWindow && std::strstr(child->Name, "PatchMemory")) memory = child;
    if (!root || !preview || !memory) { error = "Flash split panels were not rendered"; return false; }
    const auto bottomGap = [&] {
        return memory->ParentWindow->Pos.y + memory->ParentWindow->Size.y - memory->Pos.y - memory->Size.y;
    };
    const float gap = bottomGap();
    const auto resize = [&](float height, float expectedMemory) {
        io.DisplaySize.y = height;
        render(); render();
        return std::abs(memory->Size.y - expectedMemory) <= 1 &&
            std::abs(bottomGap() - gap) <= 1;
    };
    const float initialHeight = memory->Size.y;
    if (!resize(originalSize.y + 240, initialHeight) ||
        !resize(originalSize.y - 180, initialHeight) ||
        !resize(originalSize.y, initialHeight)) {
        error = std::string(previewName) + ": resizing the window changed the bottom Flash panel height";
        return false;
    }
    const ImVec2 border(preview->Pos.x + preview->Size.x * .5f,
        preview->Pos.y + preview->Size.y - 1);
    io.AddMousePosEvent(border.x, border.y); render();
    io.AddMouseButtonEvent(0, true); render();
    io.AddMousePosEvent(border.x, border.y - 48); render(); render();
    io.AddMouseButtonEvent(0, false); render();
    io.AddMousePosEvent(-100, -100); render();
    const float chosenHeight = memory->Size.y;
    if (chosenHeight < initialHeight + 20 ||
        !resize(originalSize.y + 200, chosenHeight) ||
        !resize(originalSize.y - 160, chosenHeight)) {
        error = std::string(previewName) + ": dragged Flash height was not retained across window resizing";
        return false;
    }
    io.DisplaySize.y = 260; render(); render();
    if (!resize(originalSize.y, chosenHeight)) {
        error = std::string(previewName) + ": a cramped window overwrote the preferred Flash height";
        return false;
    }
    return true;
}

// Reopening/restoring source metadata must feed the actual patch editor, not
// silently reconstruct its editable image from the cropped firmware frame.
inline bool RunEditableSourceUiRegressions(std::string& error, std::ostream* log) {
    // Clipboard ownership is per window station. Keep Explorer/history and the
    // user's clipboard out of this synthetic UI round-trip matrix.
    struct PrivateDesktop {
        HWINSTA previousStation = GetProcessWindowStation(), station = nullptr;
        HDESK previousDesktop = GetThreadDesktop(GetCurrentThreadId()), desktop = nullptr;
        bool Open() {
            station = CreateWindowStationW(nullptr, 0, WINSTA_ALL_ACCESS, nullptr);
            if (!station || !SetProcessWindowStation(station)) return false;
            desktop = CreateDesktopW(L"ClipboardRegression", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
            return desktop && SetThreadDesktop(desktop);
        }
        ~PrivateDesktop() {
            if (desktop) SetThreadDesktop(previousDesktop);
            if (station) {
                SetProcessWindowStation(previousStation);
                if (desktop) CloseDesktop(desktop);
                CloseWindowStation(station);
            }
        }
    } privateDesktop;
    if (!privateDesktop.Open()) {
        error = "Cannot isolate UI clipboard: Win32 " + std::to_string(GetLastError());
        return false;
    }
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* gpu = nullptr;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &device, nullptr, &gpu))) {
        error = "Editable-source UI test could not create software D3D device";
        return false;
    }
    auto* previous = ImGui::GetCurrentContext();
    auto* context = ImGui::CreateContext();
    char capturePath[MAX_PATH]{};
    const bool capture = GetEnvironmentVariableA("VFS_UI_CAPTURE", capturePath, MAX_PATH) > 0;
    struct Cleanup {
        ImGuiContext *context, *previous;
        ID3D11Device* device;
        ID3D11DeviceContext* gpu;
        bool renderer = false;
        bool previousConsent = UiFirmwareRisk::Accepted();
        bool previousAdjustmentsOpen = StudioPreferences::Get().sectionOpen[size_t(StudioPreferences::Section::PatchAdjustments)];
        ~Cleanup() {
            if (renderer) ImGui_ImplDX11_Shutdown();
            UiFirmwareRisk::State().accepted = previousConsent;
            StudioPreferences::Get().sectionOpen[size_t(StudioPreferences::Section::PatchAdjustments)] = previousAdjustmentsOpen;
            ImGui::DestroyContext(context); ImGui::SetCurrentContext(previous);
            gpu->Release(); device->Release();
        }
    } cleanup{context, previous, device, gpu};
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.DisplaySize = ImVec2(1280, 860); io.DeltaTime = 1.0f / 60;
    if (capture)
        io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 16, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    if (!ImGui_ImplDX11_Init(device, gpu)) { error = "UI WARP renderer initialization failed"; return false; }
    cleanup.renderer = true;
    UiFirmwareRisk::State().accepted = true;
    StudioPreferences::Get().sectionOpen[size_t(StudioPreferences::Section::PatchAdjustments)] = true;
    io.ConfigInputTrickleEventQueue = false;
    if (!RunTabPaddingUiRegressions(device, gpu, capture ? capturePath : nullptr, error)) return false;
    if (!RunCollapsibleSectionUiRegressions(device, gpu, error)) return false;
    FirmwarePackage package;
    StudioState document;
    if (!LoadEmbeddedFirmware(package, error) ||
        !document.LoadPackage(std::move(package), "editable-ui.zwdf", error)) return false;
    {
        const auto savedPreferences = StudioPreferences::Get();
        struct RestorePreferences {
            StudioPreferences::Preferences saved;
            ~RestorePreferences() { StudioPreferences::Get() = saved; }
        } restorePreferences{savedPreferences};
        auto& preferences = StudioPreferences::Get();
        preferences.defaultBootStages = 3;
        document.EnableProjectDocument();
        const auto revision = document.ResourceRevision();
        const auto applied = document.screenPatches;
        for (int target = 0; target < int(FirmwareLogo::TargetCount); ++target) {
            preferences.defaultPatchTarget = target;
            UiFirmwarePatches::State fresh;
            UiFirmwarePatches::SynchronizeDocument(fresh, document);
            if (fresh.target != static_cast<FirmwareLogo::Target>(target)) {
                error = "Fresh custom screen did not select preferred target"; return false;
            }
            for (size_t i = 0; i < FirmwareLogo::TargetCount; ++i) {
                UiFirmwarePatches::SelectTarget(fresh, static_cast<FirmwareLogo::Target>(i));
                const auto stages = i == 0 ? FirmwareLogo::BootStages::Both : FirmwareLogo::BootStages::Second;
                if (fresh.draft.bootStages != stages || fresh.observed != applied[i]) {
                    error = "Preferred target initialized the wrong draft slot or leaked startup stages"; return false;
                }
            }
            preferences.defaultBootStages = 1;
            UiFirmwarePatches::SynchronizeDocument(fresh, document);
            UiFirmwarePatches::SelectTarget(fresh, FirmwareLogo::Target::Boot);
            if (fresh.draft.bootStages != FirmwareLogo::BootStages::Both ||
                UiFirmwarePatches::HasDraftChanges(fresh, document)) {
                error = "Changed defaults reset an existing draft or made an untouched draft dirty"; return false;
            }
            UiFirmwarePatches::CommitDrafts(fresh, document);
            if (document.screenPatches != applied || document.HasUnsavedChanges() ||
                document.ResourceRevision() != revision || !document.undoStack.empty()) {
                error = "Opening or saving untouched preferred startup stages dirtied the document"; return false;
            }
            fresh.draft.enabled = true;
            UiFirmwarePatches::RecordDraftChange(fresh);
            if (!UiFirmwarePatches::HasDraftChanges(fresh, document) ||
                !UiFirmwarePatches::UndoDraft(fresh) || UiFirmwarePatches::HasDraftChanges(fresh, document)) {
                error = "Preferred startup baseline masked real edits or broke clean draft undo"; return false;
            }
            preferences.defaultBootStages = 3;
        }
        StudioState reopened;
        reopened.previewZoom = 11;
        // Exercise the same adoption boundary used by decoded project loading.
        StudioState project;
        if (!project.LoadPackage(document.firmware, "preference-project.zwdf", error)) return false;
        project.screenPatches[0].bootStages = FirmwareLogo::BootStages::First;
        reopened.AdoptProjectDocument(project);
        UiFirmwarePatches::State loaded;
        UiFirmwarePatches::SynchronizeDocument(loaded, reopened);
        UiFirmwarePatches::SelectTarget(loaded, FirmwareLogo::Target::Boot);
        if (loaded.draft.bootStages != FirmwareLogo::BootStages::First ||
            UiFirmwarePatches::HasDraftChanges(loaded, reopened) || reopened.HasUnsavedChanges() ||
            reopened.previewZoom != 11) {
            error = "Preferences overwrote reopened project settings, local zoom or clean state"; return false;
        }
        FirmwareLogo::Patches nativeSettings;
        // Even a shutdown-only native extension must not turn an existing
        // stock boot target into a new preferred-stage draft.
        nativeSettings[1].enabled = true;
        nativeSettings[1].pixels.assign(size_t(FirmwareLogo::Width) * FirmwareLogo::Height * 2, 0);
        std::vector<uint8_t> encoded;
        BuildReport nativeReport;
        FirmwarePackage encodedPackage;
        StudioState imported;
        if (!FirmwareLogo::Build(document.firmware, nativeSettings, {}, {}, encoded, nativeReport, error) ||
            !FirmwarePackage::Parse(encoded, encodedPackage, error) ||
            !imported.LoadPackage(std::move(encodedPackage), "preferred-native.zwdf", error)) return false;
        UiFirmwarePatches::State native;
        UiFirmwarePatches::SynchronizeDocument(native, imported);
        UiFirmwarePatches::SelectTarget(native, FirmwareLogo::Target::Boot);
        if (native.draft != nativeSettings[0] || UiFirmwarePatches::HasDraftChanges(native, imported)) {
            error = "Startup defaults overwrote decoded native custom screen settings"; return false;
        }
    }
    {
        StudioState menuDocument;
        if (!menuDocument.LoadPackage(document.firmware, "menu-ui.zwdf", error)) return false;
        menuDocument.EnableProjectDocument();
        UiFirmwarePatches::State memory;
        auto menuFrame = [&] {
            ImGui_ImplDX11_NewFrame();
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("Device menu UI", nullptr, ImGuiWindowFlags_NoResize);
            if (Ui::BeginTabBar("MenuSections")) {
                UiFirmwareMenu::RenderTab(memory, menuDocument, device, gpu);
                ImGui::EndTabBar();
            }
            ImGui::End(); ImGui::Render();
        };
        menuFrame(); menuFrame();
        if (!CheckFlashPanelResize(menuFrame, "Device menu UI", "MenuPreview", error)) return false;
        auto* window = ImGui::FindWindowByName("Device menu UI");
        ImGuiWindow* controlsWindow = nullptr;
        for (auto* child : ImGui::GetCurrentContext()->Windows)
            if (child->ParentWindow == window && std::strstr(child->Name, "MenuControls")) controlsWindow = child;
        if (!controlsWindow) { error = "Additional menu controls pane did not render"; return false; }
        const auto controlsId = controlsWindow->ID;
        const auto sectionId = [&](const char* name) -> ImGuiID {
            for (const auto* child : ImGui::GetCurrentContext()->Windows) {
                if (!std::strstr(child->Name, name)) continue;
                for (auto* ancestor = child->ParentWindow; ancestor; ancestor = ancestor->ParentWindow)
                    if (ancestor == window) return child->ID;
            }
            return 0;
        };
        const auto directionId = sectionId("MenuDirection"), voltageId = sectionId("MenuVoltage");
        const auto rowsId = ImHashStr("MenuRows", 0, sectionId("MenuItems"));
        if (!directionId || !voltageId || !sectionId("MenuItems")) {
            error = "Additional menu settings sections did not render"; return false;
        }
        ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::PatchMenuEnabled), 0, controlsId));
        menuFrame();
        if (!menuDocument.menuPatch.enabled || !menuDocument.HasUnsavedChanges()) {
            error = "Enabling the native menu through its UI did not dirty the project"; return false;
        }
        ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::PatchMenuBattery), 0, voltageId));
        menuFrame();
        if (!menuDocument.menuPatch.batteryVoltage || !menuDocument.menuPatch.reverseNavigation) {
            error = "Native menu UI changed its default physical navigation"; return false;
        }
        ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::PatchMenuPlusDown), 0, directionId));
        menuFrame();
        if (menuDocument.menuPatch.reverseNavigation) {
            error = "Native menu UI did not apply Plus-down physical navigation"; return false;
        }
        ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::PatchMenuPlusUp), 0, directionId));
        menuFrame();
        if (!menuDocument.menuPatch.reverseNavigation || !menuDocument.Undo() ||
            menuDocument.menuPatch.reverseNavigation || !menuDocument.Undo() ||
            !menuDocument.menuPatch.reverseNavigation || !menuDocument.menuPatch.batteryVoltage ||
            !menuDocument.Undo() || !menuDocument.menuPatch.enabled || menuDocument.menuPatch.batteryVoltage ||
            !menuDocument.Undo() || menuDocument.menuPatch.enabled || menuDocument.HasUnsavedChanges() ||
            !menuDocument.Redo() || !menuDocument.Redo() || !menuDocument.Redo() ||
            !menuDocument.menuPatch.batteryVoltage || menuDocument.menuPatch.reverseNavigation) {
            error = "Native menu UI voltage/navigation did not participate in clean project undo/redo"; return false;
        }
        ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::PatchMenuEnabled), 0, controlsId));
        menuFrame();
        ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::PatchMenuPlusUp), 0, directionId));
        menuFrame();
        if (menuDocument.menuPatch.enabled || menuDocument.menuPatch.reverseNavigation ||
            !menuDocument.menuPatch.batteryVoltage || !menuDocument.Undo() ||
            !menuDocument.menuPatch.enabled || menuDocument.menuPatch.reverseNavigation) {
            error = "Disabled native menu UI changed stored preferences"; return false;
        }
        const auto itemControl = [&](FirmwareMenu::Item item, const char* label) {
            const int id = static_cast<int>(item);
            return ImHashStr(label, 0, ImHashData(&id, sizeof(id), rowsId));
        };
        const auto beforeItems = menuDocument.menuPatch;
        if (beforeItems.enabledItems & FirmwareMenu::PuffEditorMask) {
            error = "Puff editor was enabled by default"; return false;
        }
        ImGui::ActivateItemByID(itemControl(FirmwareMenu::Item::PuffEditor, Tr(i18n::Str::PatchMenuPuffEditor)));
        menuFrame();
        if (menuDocument.menuPatch.enabledItems != (beforeItems.enabledItems | FirmwareMenu::PuffEditorMask) ||
            !menuDocument.Undo() || menuDocument.menuPatch != beforeItems ||
            !menuDocument.Redo() || !(menuDocument.menuPatch.enabledItems & FirmwareMenu::PuffEditorMask) ||
            !menuDocument.Undo() || menuDocument.menuPatch != beforeItems) {
            error = "Optional puff editor checkbox did not preserve atomic undo/redo"; return false;
        }
        ImGui::ActivateItemByID(itemControl(FirmwareMenu::Item::Voltage, Tr(i18n::Str::PatchMenuVoltage)));
        menuFrame();
        for (unsigned step = 0; step + 1 < FirmwareMenu::ItemCount; ++step) {
            ImGui::ActivateItemByID(itemControl(FirmwareMenu::Item::Exit, "##MoveUp"));
            menuFrame();
        }
        for (unsigned step = 0; step < 3; ++step) {
            ImGui::ActivateItemByID(itemControl(FirmwareMenu::Item::PuffEditor, "##MoveUp"));
            menuFrame();
        }
        auto selectedItems = beforeItems;
        selectedItems.enabledItems = FirmwareMenu::InformationMask;
        selectedItems.order = {FirmwareMenu::Item::PuffEditor, FirmwareMenu::Item::Exit,
            FirmwareMenu::Item::Information, FirmwareMenu::Item::Voltage,
            FirmwareMenu::Item::ScoreEditor, FirmwareMenu::Item::RankSelector};
        bool itemHistoryValid = menuDocument.menuPatch == selectedItems;
        for (unsigned step = 0; step < FirmwareMenu::ItemCount + 3; ++step) itemHistoryValid = menuDocument.Undo() && itemHistoryValid;
        itemHistoryValid = itemHistoryValid && menuDocument.menuPatch == beforeItems;
        for (unsigned step = 0; step < FirmwareMenu::ItemCount + 3; ++step) itemHistoryValid = menuDocument.Redo() && itemHistoryValid;
        if (!itemHistoryValid || menuDocument.menuPatch != selectedItems) {
            error = "Native menu item toggles/order did not participate in project undo/redo"; return false;
        }
        ImGui::ActivateItemByID(itemControl(FirmwareMenu::Item::Information, Tr(i18n::Str::PatchMenuInformation)));
        menuFrame();
        ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::PatchMenuEnabled), 0, controlsId));
        menuFrame();
        ImGui::ActivateItemByID(itemControl(FirmwareMenu::Item::Exit, Tr(i18n::Str::PatchMenuExit)));
        menuFrame();
        if (menuDocument.menuPatch.enabled || menuDocument.menuPatch.enabledItems != 0 ||
            menuDocument.menuPatch.order != selectedItems.order) {
            error = "Native menu allowed an enabled exit-only menu or disabled mandatory Exit"; return false;
        }
        ImGui::ActivateItemByID(itemControl(FirmwareMenu::Item::Information, Tr(i18n::Str::PatchMenuInformation)));
        menuFrame();
        if (menuDocument.menuPatch.enabled || menuDocument.menuPatch.enabledItems != FirmwareMenu::InformationMask) {
            error = "Disabled native menu item controls could not restore a functional item"; return false;
        }
        ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::PatchMenuEnabled), 0, controlsId));
        menuFrame();
        if (menuDocument.menuPatch != selectedItems || !menuDocument.Undo() || !menuDocument.Undo() ||
            !menuDocument.Undo() || menuDocument.menuPatch != selectedItems) {
            error = "Native menu empty-item recovery did not preserve settings or atomic undo"; return false;
        }
        const auto fieldControl = [&](i18n::Str label) {
            return itemControl(FirmwareMenu::Item::Information, Tr(label));
        };
        const auto fieldHistory = menuDocument.undoStack.size();
        ImGui::ActivateItemByID(fieldControl(i18n::Str::PatchMenuInfoBattery));
        menuFrame();
        if (menuDocument.menuPatch.infoFields != (FirmwareMenu::InfoResistanceMask | FirmwareMenu::InfoDurationMask) ||
            menuDocument.undoStack.size() != fieldHistory + 1 || !menuDocument.Undo() ||
            menuDocument.menuPatch != selectedItems || !menuDocument.Redo()) {
            error = "INFO field checkbox did not create one atomic undo/redo action"; return false;
        }
        ImGui::ActivateItemByID(fieldControl(i18n::Str::PatchMenuInfoResistance));
        menuFrame();
        ImGui::ActivateItemByID(fieldControl(i18n::Str::PatchMenuInfoDuration));
        menuFrame();
        const auto emptyInfo = menuDocument.menuPatch;
        if (emptyInfo.infoFields || emptyInfo.enabledItems || emptyInfo.enabled ||
            !menuDocument.Undo() || menuDocument.menuPatch.infoFields != FirmwareMenu::InfoDurationMask ||
            !menuDocument.menuPatch.enabled || !menuDocument.Redo() || menuDocument.menuPatch != emptyInfo) {
            error = "Removing the last INFO field did not atomically disable the empty module/menu"; return false;
        }
        ImGui::ActivateItemByID(itemControl(FirmwareMenu::Item::Information, Tr(i18n::Str::PatchMenuInformation)));
        menuFrame();
        if (menuDocument.menuPatch.enabled || menuDocument.menuPatch.enabledItems != FirmwareMenu::InformationMask ||
            menuDocument.menuPatch.infoFields != FirmwareMenu::AllInfoFields ||
            !menuDocument.Undo() || !menuDocument.Undo() || !menuDocument.Undo() || !menuDocument.Undo() ||
            menuDocument.menuPatch != selectedItems) {
            error = "Explicit INFO recovery did not restore fields or preserve atomic history"; return false;
        }
        ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::PatchMenuEnabled), 0, controlsId));
        menuFrame();
        ImGui::ActivateItemByID(fieldControl(i18n::Str::PatchMenuInfoBattery));
        menuFrame();
        if (menuDocument.menuPatch.enabled || menuDocument.menuPatch.enabledItems != selectedItems.enabledItems ||
            menuDocument.menuPatch.infoFields == FirmwareMenu::AllInfoFields ||
            !menuDocument.Undo() || !menuDocument.Undo() || menuDocument.menuPatch != selectedItems) {
            error = "Master-off INFO preferences changed module flags or could not be edited"; return false;
        }

        ImGuiWindow* itemsWindow = nullptr;
        for (auto* child : ImGui::GetCurrentContext()->Windows)
            if (child->ID == sectionId("MenuItems")) itemsWindow = child;
        if (!itemsWindow) { error = "Menu drag handle window was not found"; return false; }
        const auto handlePoint = [&](FirmwareMenu::Item item) {
            ImGui::SetNavWindow(itemsWindow);
            ImGui::SetNavID(itemControl(item, "###DragHandle"), ImGuiNavLayer_Main,
                itemsWindow->NavRootFocusScopeId, ImRect());
            menuFrame();
            return ImGui::WindowRectRelToAbs(itemsWindow,
                itemsWindow->NavRectRel[ImGuiNavLayer_Main]).GetCenter();
        };
        const auto sourceHandle = handlePoint(FirmwareMenu::Item::Exit);
        const auto targetHandle = handlePoint(FirmwareMenu::Item::Voltage);
        const auto dragHistory = menuDocument.undoStack.size();
        io.AddMousePosEvent(sourceHandle.x, sourceHandle.y); menuFrame();
        io.AddMouseButtonEvent(0, true); menuFrame();
        io.AddMousePosEvent(sourceHandle.x + io.MouseDragThreshold + 2, sourceHandle.y); menuFrame();
        io.AddMousePosEvent(targetHandle.x, targetHandle.y); menuFrame(); menuFrame();
        io.AddMouseButtonEvent(0, false); menuFrame();
        io.AddMousePosEvent(-100, -100); menuFrame();
        auto draggedItems = selectedItems;
        draggedItems.order = {FirmwareMenu::Item::PuffEditor, FirmwareMenu::Item::Information,
            FirmwareMenu::Item::Voltage, FirmwareMenu::Item::Exit,
            FirmwareMenu::Item::ScoreEditor, FirmwareMenu::Item::RankSelector};
        if (menuDocument.menuPatch != draggedItems || menuDocument.undoStack.size() != dragHistory + 1 ||
            !menuDocument.Undo() || menuDocument.menuPatch != selectedItems ||
            !menuDocument.Redo() || menuDocument.menuPatch != draggedItems ||
            !menuDocument.Undo() || menuDocument.menuPatch != selectedItems) {
            error = "Real mouse drag did not insert the stable menu item with one atomic undo/redo"; return false;
        }

        menuDocument.MarkSaved();
        menuFrame();
        const auto previewId = sectionId("MenuPreview");
        const auto previewHistory = menuDocument.undoStack.size();
        const auto previewRevision = menuDocument.ResourceRevision();
        const auto directionLabel = [&](bool up) {
            return up == menuDocument.menuPatch.reverseNavigation ? "+##PreviewPlus" : "-##PreviewMinus";
        };
        ImGui::ActivateItemByID(ImHashStr(directionLabel(false), 0, previewId));
        menuFrame();
        ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::PatchMenuFire), 0, previewId));
        menuFrame(); menuFrame();
        if (!memory.menuPreview.state.information || memory.menuPreview.textureError ||
            !memory.menuPreview.texture.IsValid() || menuDocument.HasUnsavedChanges() ||
            menuDocument.undoStack.size() != previewHistory || menuDocument.ResourceRevision() != previewRevision) {
            error = "INFO preview failed or preview-only interaction dirtied project/history"; return false;
        }
        if (capture && !CaptureUiDrawData(device, gpu,
            (std::string(capturePath) + ".menu-info.png").c_str(), error)) return false;
        ImGui::ActivateItemByID(fieldControl(i18n::Str::PatchMenuInfoResistance));
        menuFrame(); menuFrame();
        if (!memory.menuPreview.state.information ||
            menuDocument.menuPatch.infoFields != (FirmwareMenu::InfoBatteryMask | FirmwareMenu::InfoDurationMask)) {
            error = "Partial INFO preview lost its page or selected fields"; return false;
        }
        if (capture && !CaptureUiDrawData(device, gpu,
            (std::string(capturePath) + ".menu-info-partial.png").c_str(), error)) return false;
        if (!menuDocument.Undo()) { error = "Partial INFO capture could not restore full fields"; return false; }
        ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::PatchMenuBack), 0, previewId));
        menuFrame();
        if (memory.menuPreview.state.information || menuDocument.HasUnsavedChanges()) {
            error = "Returning from INFO preview changed the project"; return false;
        }
        const auto memoryDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (memory.memoryJob.valid() && std::chrono::steady_clock::now() < memoryDeadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            menuFrame();
        }
        menuFrame(); menuFrame();
        menuFrame();
        if (capture && !CaptureUiDrawData(device, gpu,
            (std::string(capturePath) + ".menu.png").c_str(), error)) return false;
        const auto wideDisplaySize = io.DisplaySize;
        io.DisplaySize.x = 560;
        menuFrame(); menuFrame(); menuFrame();
        if (capture && !CaptureUiDrawData(device, gpu,
            (std::string(capturePath) + ".menu-narrow.png").c_str(), error)) return false;
        const auto scrollToPreview = [&] {
            for (auto* child : ImGui::GetCurrentContext()->Windows) {
                if (child->ID != sectionId("MenuPreview")) continue;
                ImGui::SetScrollY(child, 0);
                break;
            }
            menuFrame(); menuFrame(); menuFrame();
        };
        scrollToPreview();
        ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::PatchMenuFire), 0, previewId));
        menuFrame(); menuFrame();
        if (!memory.menuPreview.state.information || memory.menuPreview.textureError) {
            error = "Narrow workspace could not open the full INFO preview"; return false;
        }
        if (capture && !CaptureUiDrawData(device, gpu,
            (std::string(capturePath) + ".menu-info-narrow.png").c_str(), error)) return false;
        ImGui::SetScrollY(controlsWindow, 0);
        menuFrame(); menuFrame();
        ImGui::ActivateItemByID(fieldControl(i18n::Str::PatchMenuInfoResistance));
        menuFrame(); menuFrame();
        scrollToPreview();
        if (menuDocument.menuPatch.infoFields != (FirmwareMenu::InfoBatteryMask | FirmwareMenu::InfoDurationMask)) {
            error = "Narrow workspace lost partial INFO preferences"; return false;
        }
        if (capture && !CaptureUiDrawData(device, gpu,
            (std::string(capturePath) + ".menu-info-partial-narrow.png").c_str(), error)) return false;
        if (!menuDocument.Undo()) { error = "Narrow INFO capture could not restore field settings"; return false; }
        io.DisplaySize = wideDisplaySize;
        menuFrame(); menuFrame();
        ImGui::ActivateItemByID(ImHashStr(Tr(i18n::Str::PatchMenuBack), 0, previewId));
        menuFrame();
        ImGui::SetScrollY(controlsWindow, 0);
        menuFrame(); menuFrame();
        ImGui::ActivateItemByID(itemControl(FirmwareMenu::Item::PuffEditor, Tr(i18n::Str::PatchMenuPuffEditor)));
        menuFrame();
        if (!(menuDocument.menuPatch.enabledItems & FirmwareMenu::PuffEditorMask)) {
            error = "Puff preview module could not be enabled"; return false;
        }
        menuDocument.MarkSaved();
        const auto puffHistory = menuDocument.undoStack.size();
        const auto puffRevision = menuDocument.ResourceRevision();
        const auto puffSettings = menuDocument.menuPatch;
        auto& preview = memory.menuPreview;
        const auto previewButton = [&](const char* label) {
            ImGui::ActivateItemByID(ImHashStr(label, 0, previewId));
            menuFrame();
        };
        const auto pressPreviewMouse = [&](const char* label) {
            ImGuiWindow* pane = nullptr;
            for (auto* child : ImGui::GetCurrentContext()->Windows)
                if (child->ID == previewId) pane = child;
            if (!pane) return false;
            // Playback buttons deliberately reject nav focus. Locate the
            // rendered rectangle before sending a real mouse press.
            ImGui::DebugLocateItem(ImHashStr(label, 0, previewId));
            menuFrame();
            if (ImGui::GetCurrentContext()->DebugLocateId) return false;
            const auto* outline = ImGui::GetForegroundDrawList(pane->Viewport);
            const int vertices = (outline->Flags & ImDrawListFlags_AntiAliasedLines) ? 8 : 4;
            if (outline->VtxBuffer.Size < vertices) return false;
            ImRect bounds(ImVec2(FLT_MAX, FLT_MAX), ImVec2(-FLT_MAX, -FLT_MAX));
            for (int i = 0; i < vertices; ++i) bounds.Add(outline->VtxBuffer[i].pos);
            ImGui::GetCurrentContext()->DebugLocateFrames = 0;
            const auto center = bounds.GetCenter();
            io.AddMousePosEvent(center.x, center.y); menuFrame();
            io.AddMouseButtonEvent(0, true); menuFrame();
            return preview.heldKey != 0;
        };
        const auto heldPreviewFrame = [&] {
            const float delta = io.DeltaTime;
            io.DeltaTime = 0.05f;
            menuFrame();
            io.DeltaTime = delta;
        };
        const auto releasePreviewMouse = [&] {
            io.AddMouseButtonEvent(0, false); menuFrame();
            io.AddMousePosEvent(-100, -100); menuFrame();
        };
        const auto firePuff = [&] { previewButton(Tr(i18n::Str::PatchMenuFire)); };
        const auto cleanPreview = [&] {
            return !menuDocument.HasUnsavedChanges() && menuDocument.undoStack.size() == puffHistory &&
                menuDocument.ResourceRevision() == puffRevision && menuDocument.menuPatch == puffSettings;
        };
        const auto exactPreview = [&] {
            std::array<uint32_t, 80 * 160> expected;
            return FirmwareMenu::RenderPreview(menuDocument.menuPatch, preview.state, preview.values, expected) &&
                expected == preview.pixels && !preview.textureError && preview.texture.IsValid();
        };
        if (preview.state.selection != 1 || preview.state.information) {
            error = "INFO preview did not return to its list selection"; return false;
        }
        previewButton(directionLabel(true));
        const uint32_t sample = preview.values.puffs;
        const uint32_t limit = FirmwareMenu::PuffLimit(menuDocument.firmware.records.front());
        firePuff();
        if (!preview.state.editingPuffs || preview.state.puffDigit != 0 ||
            preview.state.puffValue != sample || preview.state.originalPuffs != sample ||
            sample != (std::min)(12345u, limit) || preview.values.maximumPuffs != limit ||
            preview.renderedState.puffDigit != 0 || !exactPreview() || !cleanPreview()) {
            error = "Puff preview did not load the current bounded sample with its first digit highlighted immediately"; return false;
        }
        auto secondDigit = preview.state;
        secondDigit.puffDigit = 1;
        std::array<uint32_t, 80 * 160> secondPixels;
        if (!FirmwareMenu::RenderPreview(menuDocument.menuPatch, secondDigit, preview.values, secondPixels) ||
            secondPixels == preview.pixels) {
            error = "Puff digit highlight is absent from the native pixel oracle"; return false;
        }
        const auto stablePixels = preview.pixels;
        const auto stableView = preview.texture.GetSRV();
        menuFrame(); menuFrame();
        if (preview.pixels != stablePixels || preview.texture.GetSRV() != stableView ||
            preview.renderedState.puffDigit != 0 || !exactPreview()) {
            error = "Unchanged puff preview did not retain its two-frame texture cache"; return false;
        }
        if (capture && !CaptureUiDrawData(device, gpu,
            (std::string(capturePath) + ".menu-puff-editor.png").c_str(), error)) return false;
        io.DisplaySize.x = 560;
        menuFrame(); menuFrame(); menuFrame();
        if (!preview.state.editingPuffs || preview.state.puffDigit != 0 || !exactPreview()) {
            error = "Narrow workspace lost the puff editor highlight"; return false;
        }
        if (capture && !CaptureUiDrawData(device, gpu,
            (std::string(capturePath) + ".menu-puff-editor-narrow.png").c_str(), error)) return false;
        io.DisplaySize = wideDisplaySize;
        menuFrame(); menuFrame();
        const auto expectedDigit = [&](uint32_t value, unsigned digit, bool increase) {
            constexpr uint32_t places[]{10000, 1000, 100, 10, 1};
            const uint32_t place = places[digit], current = value / place % 10, base = value - current * place;
            for (unsigned step = 1; step <= 10; ++step) {
                const uint32_t next = increase ? (current + step) % 10 : (current + 10 - step) % 10;
                if (base + next * place <= limit) return base + next * place;
            }
            return value;
        };
        // The leading slot may be fixed at zero on lower-bound revisions.
        for (unsigned digit = 0; digit < FirmwareMenu::PuffDigitCount; ++digit) {
            const auto original = preview.state.puffValue;
            for (unsigned step = 0; step < 10; ++step) {
                const auto next = expectedDigit(preview.state.puffValue, digit, true);
                previewButton("+##PreviewPlus");
                if (preview.state.puffValue != next || next > limit || !exactPreview()) {
                    error = "Puff Plus carried a digit, exceeded the revision bound or left stale pixels"; return false;
                }
            }
            // Complete the cycle through all valid digits, not all ten candidates.
            for (unsigned step = 0; step < 9 && preview.state.puffValue != original; ++step) {
                const auto next = expectedDigit(preview.state.puffValue, digit, true);
                previewButton("+##PreviewPlus");
                if (preview.state.puffValue != next) {
                    error = "Puff Plus did not wrap to the original digit"; return false;
                }
            }
            if (preview.state.puffValue != original) {
                error = "Puff digit did not complete its bounded cycle"; return false;
            }
            const auto previous = expectedDigit(original, digit, false);
            previewButton("-##PreviewMinus");
            if (preview.state.puffValue != previous || !exactPreview()) {
                error = "Puff Minus did not decrement/wrap the selected digit"; return false;
            }
            previewButton("+##PreviewPlus");
            if (preview.state.puffValue != original || !cleanPreview()) {
                error = "Puff digit reversal changed other digits or dirtied the project"; return false;
            }
            firePuff();
            if ((digit + 1 < FirmwareMenu::PuffDigitCount && (!preview.state.editingPuffs || preview.state.puffDigit != digit + 1)) ||
                (digit + 1 == FirmwareMenu::PuffDigitCount && (preview.state.editingPuffs || preview.puffResult != 2 || preview.values.puffs != sample))) {
                error = "Five Fire presses did not finish the unchanged preview without saving"; return false;
            }
        }
        firePuff();
        for (unsigned digit = 0; digit + 1 < FirmwareMenu::PuffDigitCount; ++digit) firePuff();
        const auto changedSample = expectedDigit(sample, FirmwareMenu::PuffDigitCount - 1, true);
        previewButton("+##PreviewPlus");
        firePuff();
        if (preview.state.editingPuffs || preview.values.puffs != changedSample ||
            preview.puffResult != 1 || !exactPreview() || !cleanPreview()) {
            error = "Fifth Fire did not update only the changed preview sample and return to the menu"; return false;
        }
        firePuff();
        if (preview.state.puffValue != changedSample || preview.state.originalPuffs != changedSample ||
            preview.state.puffDigit != 0 || !exactPreview()) {
            error = "Reopening the puff preview reset to a fixed sample instead of the current count"; return false;
        }
        for (unsigned digit = 0; digit < FirmwareMenu::PuffDigitCount; ++digit) firePuff();
        if (preview.puffResult != 2 || preview.values.puffs != changedSample || !cleanPreview()) {
            error = "Unchanged current puff sample triggered a project edit"; return false;
        }
        // Actual mouse holds, not synthesized keyboard activations.
        firePuff();
        for (unsigned digit = 0; digit < 4; ++digit) firePuff();
        if (!pressPreviewMouse("+##PreviewPlus")) {
            error = "Puff preview Plus did not capture a physical mouse hold"; return false;
        }
        unsigned heldChanges = 1;
        auto heldValue = preview.state.puffValue;
        for (unsigned frame = 0; frame < 30; ++frame) {
            heldPreviewFrame();
            if (preview.state.puffValue != heldValue) ++heldChanges;
            heldValue = preview.state.puffValue;
            if (preview.state.puffDigit != 4 || heldValue / 10 != changedSample / 10 ||
                heldValue > limit || preview.values.puffs != changedSample || !exactPreview() || !cleanPreview()) {
                error = "Mouse-held puff repeat carried, changed the sample/project or lost native pixels"; return false;
            }
        }
        if (heldChanges < 10 || heldChanges > 23) {
            error = "Mouse-held puff repeat did not use delayed accelerating intervals"; return false;
        }
        releasePreviewMouse();
        if (preview.state.puffValue != heldValue || preview.heldKey) {
            error = "Mouse release advanced the preview digit twice"; return false;
        }
        if (!pressPreviewMouse(Tr(i18n::Str::PatchMenuFire))) {
            error = "Puff preview Fire did not capture a physical mouse hold"; return false;
        }
        for (unsigned frame = 0; frame < 9; ++frame) heldPreviewFrame();
        if (!preview.state.editingPuffs || preview.state.puffDigit != 4 ||
            preview.values.puffs != changedSample || !cleanPreview()) {
            error = "Held Fire pre-confirmed the last puff preview digit"; return false;
        }
        for (unsigned frame = 0; frame < 7; ++frame) heldPreviewFrame();
        if (preview.state.editingPuffs || preview.puffResult || preview.values.puffs != changedSample ||
            !exactPreview() || !cleanPreview()) {
            error = "Long mouse Fire did not discard the puff preview without saving"; return false;
        }
        if (capture && !CaptureUiDrawData(device, gpu,
            (std::string(capturePath) + ".menu-hold-cancel.png").c_str(), error)) return false;
        releasePreviewMouse();
        if (preview.state.editingPuffs || preview.heldKey || !cleanPreview()) {
            error = "Canceled mouse Fire re-opened the puff preview on release"; return false;
        }
        ImGui::SetScrollY(controlsWindow, 0);
        menuFrame(); menuFrame();
        ImGui::ActivateItemByID(itemControl(FirmwareMenu::Item::ScoreEditor, Tr(i18n::Str::PatchMenuScoreEditor)));
        menuFrame();
        ImGui::ActivateItemByID(itemControl(FirmwareMenu::Item::RankSelector, Tr(i18n::Str::PatchMenuRankSelector)));
        menuFrame();
        if ((menuDocument.menuPatch.enabledItems & (FirmwareMenu::ScoreEditorMask | FirmwareMenu::RankSelectorMask)) !=
            (FirmwareMenu::ScoreEditorMask | FirmwareMenu::RankSelectorMask)) {
            error = "Achievement modules could not be enabled independently"; return false;
        }
        menuDocument.MarkSaved();
        const auto achievementHistory = menuDocument.undoStack.size();
        const auto achievementRevision = menuDocument.ResourceRevision();
        const auto cleanAchievementPreview = [&] {
            return !menuDocument.HasUnsavedChanges() && menuDocument.undoStack.size() == achievementHistory &&
                menuDocument.ResourceRevision() == achievementRevision && exactPreview();
        };
        scrollToPreview();
        for (unsigned step = 0; step < 3; ++step) previewButton(directionLabel(false));
        firePuff();
        if (!preview.state.editingScore || preview.state.editingPuffs || preview.state.puffDigit ||
            preview.state.puffValue != preview.values.scorePoints || !cleanAchievementPreview()) {
            error = "Points preview did not open the current score with the first digit highlighted"; return false;
        }
        if (capture && !CaptureUiDrawData(device, gpu,
            (std::string(capturePath) + ".menu-points.png").c_str(), error)) return false;
        const auto oldPoints = preview.values.scorePoints;
        previewButton("-##PreviewMinus");
        for (unsigned digit = 0; digit < FirmwareMenu::PuffDigitCount; ++digit) firePuff();
        if (preview.state.editingScore || preview.values.scorePoints != oldPoints - 10000 ||
            preview.puffResult != 1 || !cleanAchievementPreview()) {
            error = "Points preview did not commit its fifth digit independently of project edits"; return false;
        }
        firePuff();
        if (preview.state.puffValue != preview.values.scorePoints || preview.state.puffDigit) {
            error = "Points preview reopened a fixed sample rather than the edited total"; return false;
        }
        for (unsigned digit = 0; digit < FirmwareMenu::PuffDigitCount; ++digit) firePuff();
        if (preview.puffResult != 2 || !cleanAchievementPreview()) {
            error = "Unchanged points preview changed the project or reported a save"; return false;
        }
        firePuff();
        const auto cancelPoints = preview.values.scorePoints;
        for (unsigned digit = 0; digit < 4; ++digit) firePuff();
        previewButton("+##PreviewPlus");
        if (!pressPreviewMouse(Tr(i18n::Str::PatchMenuFire))) {
            error = "Points preview Fire did not capture a mouse hold"; return false;
        }
        for (unsigned frame = 0; frame < 9; ++frame) heldPreviewFrame();
        if (!preview.state.editingScore || preview.state.puffDigit != 4 || preview.values.scorePoints != cancelPoints) {
            error = "Held Fire committed the final points preview digit before cancellation"; return false;
        }
        for (unsigned frame = 0; frame < 7; ++frame) heldPreviewFrame();
        releasePreviewMouse();
        if (preview.state.editingScore || preview.puffResult || preview.values.scorePoints != cancelPoints ||
            !cleanAchievementPreview()) {
            error = "Long mouse Fire failed to discard points without changing project/history"; return false;
        }
        previewButton(directionLabel(false));
        for (unsigned rank = 0; rank < FirmwareMenu::RankCount; ++rank) {
            firePuff();
            unsigned currentRank = 0;
            while (currentRank + 1 < FirmwareMenu::RankCount &&
                preview.values.scorePoints >= FirmwareMenu::RankPoints[currentRank + 1]) ++currentRank;
            if (!preview.state.selectingRank || preview.state.rankSelection != currentRank ||
                !cleanAchievementPreview()) {
                error = "Rank preview did not derive the initial rank from current points"; return false;
            }
            for (unsigned step = 0; step < FirmwareMenu::RankCount &&
                preview.state.rankSelection != rank; ++step) previewButton(directionLabel(false));
            if (preview.state.rankSelection != rank) {
                error = "Rank preview navigation did not wrap through all eight choices"; return false;
            }
            if (capture && rank == 3 && !CaptureUiDrawData(device, gpu,
                (std::string(capturePath) + ".menu-ranks.png").c_str(), error)) return false;
            firePuff();
            if (preview.state.selectingRank || preview.values.scorePoints != FirmwareMenu::RankPoints[rank] ||
                !cleanAchievementPreview()) {
                error = "Rank selection did not update only preview points to the selected minimum"; return false;
            }
            for (unsigned step = 0; step < 4; ++step) previewButton(directionLabel(false));
        }
        const auto pointsBeforeBack = preview.values.scorePoints;
        firePuff();
        previewButton(directionLabel(false));
        if (preview.state.rankSelection != FirmwareMenu::RankCount || !cleanAchievementPreview()) {
            error = "Rank preview did not expose its non-destructive BACK row"; return false;
        }
        firePuff();
        if (preview.state.selectingRank || preview.values.scorePoints != pointsBeforeBack ||
            preview.puffResult || !cleanAchievementPreview()) {
            error = "Leaving the rank picker through BACK changed the preview total or project"; return false;
        }
        firePuff();
        previewButton(directionLabel(true));
        const auto cancelRankPoints = preview.values.scorePoints;
        if (!pressPreviewMouse(directionLabel(false))) {
            error = "Rank preview did not capture a navigation mouse hold"; return false;
        }
        const auto initialHeldRank = preview.state.rankSelection;
        unsigned rankChanges = 0;
        auto lastHeldRank = initialHeldRank;
        for (unsigned frame = 0; frame < 30; ++frame) {
            heldPreviewFrame();
            if (lastHeldRank != preview.state.rankSelection) ++rankChanges;
            lastHeldRank = preview.state.rankSelection;
            if (lastHeldRank >= FirmwareMenu::RankRowCount || preview.values.scorePoints != cancelRankPoints ||
                !cleanAchievementPreview()) {
                error = "Mouse-held rank navigation changed points/project or escaped its rows"; return false;
            }
        }
        releasePreviewMouse();
        if (rankChanges < 9 || rankChanges > 22 || preview.state.rankSelection != lastHeldRank ||
            !pressPreviewMouse(Tr(i18n::Str::PatchMenuFire))) {
            error = "Rank preview did not accelerate safely or advanced on release"; return false;
        }
        for (unsigned frame = 0; frame < 9; ++frame) heldPreviewFrame();
        if (!preview.state.selectingRank || preview.values.scorePoints != cancelRankPoints) {
            error = "Held Fire pre-confirmed a rank preview selection"; return false;
        }
        for (unsigned frame = 0; frame < 7; ++frame) heldPreviewFrame();
        releasePreviewMouse();
        if (preview.state.selectingRank || preview.puffResult || preview.values.scorePoints != cancelRankPoints ||
            !cleanAchievementPreview()) {
            error = "Long mouse Fire changed rank points or re-entered on release"; return false;
        }
    }
    {
        auto layout = std::make_unique<UiFirmwareLayout::State>();
        auto memory = std::make_unique<UiFirmwarePatches::State>();
        auto layoutFrame = [&] {
            ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("Layout flash UI", nullptr, ImGuiWindowFlags_NoResize);
            if (Ui::BeginTabBar("LayoutSections")) {
                UiFirmwareLayout::RenderTab(*layout, document, device, gpu, *memory);
                ImGui::EndTabBar();
            }
            ImGui::End(); ImGui::Render();
        };
        if (!CheckFlashPanelResize(layoutFrame, "Layout flash UI", "LayoutPreview", error)) return false;
    }
    FirmwareLogo::EditableSource source;
    source.width = 173; source.height = 217;
    auto gradient = std::make_shared<std::vector<uint8_t>>(size_t(173 * 217 * 4));
    for (int y = 0; y < 217; ++y) for (int x = 0; x < 173; ++x) {
        auto* pixel = gradient->data() + size_t(y * 173 + x) * 4;
        pixel[0] = uint8_t(x * 255 / 172); pixel[1] = uint8_t(y * 255 / 216);
        pixel[2] = uint8_t((x + y) & 255); pixel[3] = 127;
    }
    source.rgba = std::move(gradient);
    source.scalePercent = 153.5f; source.fitMode = 1;
    source.x = -17; source.y = -29; source.alignX = source.alignY = -1;
    source.resampling = FirmwareBitmap::Resampling::Bicubic;
    source.adjustments.gamma = 1.2f;
    source.adjustments.curve = {0.2f, 0.55f, 0.8f};
    source.adjustments.paletteColors = 64;
    for (size_t i = 0; i < FirmwareLogo::TargetCount; ++i)
        document.SetScreenPatchSource(static_cast<FirmwareLogo::Target>(i), source);
    UiFirmwarePatches::State editor;
    auto frame = [&] {
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Editable source UI", nullptr, ImGuiWindowFlags_NoResize);
        if (Ui::BeginTabBar("Sections")) {
            UiFirmwarePatches::RenderTab(editor, document, device, gpu);
            ImGui::EndTabBar();
        }
        ImGui::End(); ImGui::Render();
    };
    auto matches = [&](const FirmwareLogo::EditableSource& expected) {
        return editor.sourceWidth == expected.width && editor.sourceHeight == expected.height &&
            expected.rgba && editor.source == *expected.rgba && editor.x == expected.x &&
            editor.y == expected.y && editor.scalePercent == expected.scalePercent &&
            editor.alignX == expected.alignX && editor.alignY == expected.alignY &&
            editor.fitMode == expected.fitMode && editor.resampling == expected.resampling &&
            editor.adjustments == expected.adjustments;
    };
    for (size_t i = 0; i < FirmwareLogo::TargetCount; ++i) {
        if (i) UiFirmwarePatches::SelectTarget(editor, static_cast<FirmwareLogo::Target>(i));
        frame(); frame();
        if (!matches(source)) { error = "Patch editor lost full original or placement on target selection"; return false; }
    }
    if (!CheckFlashPanelResize(frame, "Editable source UI", "PatchPreview", error)) return false;
    auto moved = source; moved.x -= 7;
    document.PushUndo("Source placement only");
    document.SetScreenPatchSource(FirmwareLogo::Target::DeviceInfo, moved);
    document.EndUndoAction();
    frame(); frame();
    if (!matches(moved) || !document.Undo()) { error = "Patch editor did not synchronize metadata-only change"; return false; }
    frame(); frame();
    if (!matches(source) || !document.Redo()) { error = "Patch editor did not restore full source on undo"; return false; }
    frame(); frame();
    if (!matches(moved)) { error = "Patch editor did not restore placement on redo"; return false; }
    const auto appliedSettings = document.screenPatches;
    const auto appliedSources = document.screenPatchSources;
    const auto historySize = document.undoStack.size();
    const auto sourceImage = editor.sourceImage;
    const auto originalDraft = editor.draft;
    editor.x -= 5;
    UiFirmwarePatches::ComposeLogo(editor, 42);
    editor.x -= 5;
    UiFirmwarePatches::ComposeLogo(editor, 42);
    if (editor.undo.size() != 1 || editor.undo.back().source.rgba != sourceImage ||
        !UiFirmwarePatches::UndoDraft(editor) || !matches(moved) || editor.draft != originalDraft ||
        editor.sourceImage != sourceImage || !editor.undo.empty()) {
        error = "Draft placement drag did not undo atomically with shared immutable source"; return false;
    }
    editor.draft.holdMs = 750;
    UiFirmwarePatches::RecordDraftChange(editor);
    UiFirmwarePatches::SelectTarget(editor, FirmwareLogo::Target::Boot);
    editor.draft.durationMs = 900;
    UiFirmwarePatches::RecordDraftChange(editor);
    if (!UiFirmwarePatches::UndoDraft(editor) || editor.draft.durationMs != appliedSettings[0].durationMs) {
        error = "Draft history crossed custom-screen targets"; return false;
    }
    UiFirmwarePatches::SelectTarget(editor, FirmwareLogo::Target::DeviceInfo);
    if (!UiFirmwarePatches::UndoDraft(editor) || editor.draft.holdMs != originalDraft.holdMs ||
        document.screenPatches != appliedSettings || document.screenPatchSources != appliedSources ||
        document.undoStack.size() != historySize) {
        error = "Draft setting undo mutated applied studio history"; return false;
    }
    // Adjustments invalidate the resized cache without replacing the original image.
    UiFirmwarePatches::ComposeLogo(editor);
    const auto unadjustedPixels = editor.draft.pixels;
    UiFirmwarePatches::UndoDraft(editor);
    editor.adjustments.brightness = 70;
    editor.adjustments.paletteColors = 16;
    UiFirmwarePatches::ComposeLogo(editor, 81);
    if (editor.draft.pixels == unadjustedPixels || editor.sourceImage != sourceImage ||
        editor.undo.back().source.rgba != sourceImage || !UiFirmwarePatches::UndoDraft(editor) ||
        !matches(moved) || editor.draft != originalDraft) {
        error = "Image corrections did not invalidate composition or undo without changing the original"; return false;
    }
    // Drive the channel buttons and their actual numeric slider inputs. The
    // histogram is cached when composition changes, never rebuilt by UI frames.
    frame();
    ImGuiWindow* settingsWindow = nullptr;
    for (auto* window : GImGui->Windows) {
        if (window->LastFrameActive != GImGui->FrameCount) continue;
        if (std::strstr(window->Name, "AdjustmentsBody")) settingsWindow = window;
    }
    ImGuiWindow* settingsScroll = settingsWindow ? settingsWindow->ParentWindow : nullptr;
    if (!settingsWindow || !settingsScroll) { error = "Curve controls child is missing"; return false; }
    bool selectedRed = false;
    const float scrollStep = (std::max)(1.0f, settingsScroll->Size.y * .3f);
    for (float scroll = 0; scroll <= settingsScroll->ScrollMax.y + scrollStep; scroll += scrollStep) {
        ImGui::SetScrollY(settingsScroll, scroll); frame();
        ImGui::ActivateItemByID(settingsWindow->GetID("R")); frame();
        if (editor.curveChannel == 1) { selectedRed = true; break; }
    }
    if (!selectedRed) { error = "Could not reach the real curve channel controls"; return false; }
    const auto histogramGeneration = editor.histogramGeneration;
    frame(); frame();
    if (editor.histogramGeneration != histogramGeneration) {
        error = "Unchanged UI frames rescanned the image histogram"; return false;
    }
    const auto beforeCurves = editor.adjustments;
    auto curveEdit = [&](const char* channel, int index, const char* text, float value) {
        ImGui::ActivateItemByID(settingsWindow->GetID(channel)); frame();
        if (editor.curveChannel != index) { error = "Curve channel button did not select its controls"; return false; }
        auto expected = editor.adjustments;
        if (index) expected.channelCurves[index - 1][1] = value;
        else expected.curve[1] = value;
        const ImGuiID midpoint = settingsWindow->GetID(int(i18n::Str::PatchMidtones));
        ImGui::ActivateItemByID(ImHashStr("##Adjustment", 0, midpoint));
        GImGui->NavNextActivateFlags = ImGuiActivateFlags_PreferInput;
        frame();
        io.AddInputCharactersUTF8(text); frame();
        io.AddKeyEvent(ImGuiKey_Enter, true); frame();
        io.AddKeyEvent(ImGuiKey_Enter, false); frame();
        if (editor.adjustments != expected || editor.sourceImage != sourceImage) {
            error = "Curve slider changed the wrong channel or modified the original image"; return false;
        }
        return true;
    };
    if (!curveEdit("R", 1, "0.9", .9f) || !curveEdit("G", 2, "0.1", .1f) ||
        !curveEdit("B", 3, "0.4", .4f) || !curveEdit("RGB", 0, "0.8", .8f)) return false;
    if (capture) {
        const float savedScroll = settingsScroll->Scroll.y;
        ImGui::SetScrollY(settingsScroll, (std::min)(520.0f, settingsScroll->ScrollMax.y));
        frame(); frame();
        if (!CaptureUiDrawData(device, gpu, (std::string(capturePath) + ".curves.png").c_str(), error)) return false;
        ImGui::SetScrollY(settingsScroll, savedScroll); frame();
    }
    std::array<std::array<uint32_t, 256>, 4> expectedHistogram{};
    for (uint32_t pixel : editor.canvas) {
        const uint32_t red = pixel & 255, green = (pixel >> 8) & 255, blue = (pixel >> 16) & 255;
        ++expectedHistogram[0][(77 * red + 150 * green + 29 * blue + 128) >> 8];
        ++expectedHistogram[1][red]; ++expectedHistogram[2][green]; ++expectedHistogram[3][blue];
    }
    for (size_t channel = 0; channel < expectedHistogram.size(); ++channel) {
        uint32_t pixels = 0;
        for (uint32_t count : editor.histogram[channel]) pixels += count;
        if (pixels != FirmwareLogo::Width * FirmwareLogo::Height ||
            editor.histogramMaximum[channel] != *std::max_element(expectedHistogram[channel].begin(), expectedHistogram[channel].end())) {
            error = "Histogram lost composed pixels or its rendered normalization"; return false;
        }
    }
    if (editor.histogram != expectedHistogram || editor.histogramGeneration <= histogramGeneration) {
        error = "Curve histogram does not describe the composed RGB565 image"; return false;
    }
    const auto editedGeneration = editor.histogramGeneration;
    frame(); frame();
    if (editor.histogramGeneration != editedGeneration) {
        error = "Rendering channel curves recomputed an unchanged histogram"; return false;
    }
    ImGui::ActivateItemByID(settingsWindow->GetID(Tr(i18n::Str::PatchResetAdjustments))); frame();
    if (editor.adjustments != FirmwareBitmap::ImageAdjustments{} || editor.sourceImage != sourceImage) {
        error = "Reset adjustments did not reset RGB and every channel curve"; return false;
    }
    for (int i = 0; i < 5; ++i)
        if (!UiFirmwarePatches::UndoDraft(editor)) { error = "Curve edit lost draft undo"; return false; }
    if (editor.adjustments != beforeCurves || !matches(moved) || editor.draft != originalDraft ||
        document.screenPatches != appliedSettings || document.screenPatchSources != appliedSources) {
        error = "Curve reset/undo changed applied settings or lost original source metadata"; return false;
    }
    // Operate the real family/direction combos and verify independent entrance
    // and exit IDs, including every diagonal and the highest persisted value.
    ImGui::SetScrollY(settingsScroll, settingsScroll->ScrollMax.y);
    frame(); frame();
    ImGuiWindow* animationWindow = nullptr;
    for (auto* window : GImGui->Windows)
        if (window->LastFrameActive == GImGui->FrameCount && std::strstr(window->Name, "AnimationBody"))
            animationWindow = window;
    if (!animationWindow) { error = "Animation controls child is missing"; return false; }
    auto chooseAnimation = [&](i18n::Str phase, const char* combo, i18n::Str label, int row, int rows) {
        ImGui::SetScrollY(settingsScroll, settingsScroll->ScrollMax.y);
        frame(); frame();
        const ImGuiID phaseId = animationWindow->GetID(int(phase));
        ImGui::ActivateItemByID(ImHashStr(combo, 0, phaseId)); frame();
        if (GImGui->OpenPopupStack.empty()) { error = "Animation selector did not open"; return false; }
        auto* popup = GImGui->OpenPopupStack.back().Window;
        if (!popup) { error = "Animation selector popup is missing"; return false; }
        ImGui::SetScrollY(popup, popup->ScrollMax.y * row / (rows - 1));
        frame();
        ImGui::ActivateItemByID(popup->GetID(Tr(label))); frame(); frame();
        if (!GImGui->OpenPopupStack.empty()) { error = "Animation selector did not accept its choice"; return false; }
        return true;
    };
    using Animation = FirmwareLogo::Animation;
    const struct {
        i18n::Str label;
        int row;
        Animation variants[8];
    } directionalFamilies[]{
        {i18n::Str::PatchWipe, 1, {Animation::WipeDown, Animation::WipeUp, Animation::WipeRight,
            Animation::WipeLeft, Animation::Diagonal, Animation::DiagonalDownLeft,
            Animation::DiagonalUpRight, Animation::DiagonalUpLeft}},
        {i18n::Str::PatchSoftWipe, 9, {Animation::SoftWipe, Animation::SoftWipeUp, Animation::SoftWipeRight,
            Animation::SoftWipeLeft, Animation::SoftWipeDownRight, Animation::SoftWipeDownLeft,
            Animation::SoftWipeUpRight, Animation::SoftWipeUpLeft}},
        {i18n::Str::PatchSlide, 10, {Animation::SlideDown, Animation::SlideUp, Animation::SlideRight,
            Animation::SlideLeft, Animation::SlideDownRight, Animation::SlideDownLeft,
            Animation::SlideUpRight, Animation::SlideUpLeft}},
        {i18n::Str::PatchBlinds, 7, {Animation::Blinds, Animation::BlindsUp, Animation::BlindsRight,
            Animation::BlindsLeft, Animation::BlindsDownRight, Animation::BlindsDownLeft,
            Animation::BlindsUpRight, Animation::BlindsUpLeft}}
    };
    const i18n::Str directionLabels[]{i18n::Str::PatchDirectionDown, i18n::Str::PatchDirectionUp,
        i18n::Str::PatchDirectionRight, i18n::Str::PatchDirectionLeft, i18n::Str::PatchDirectionDownRight,
        i18n::Str::PatchDirectionDownLeft, i18n::Str::PatchDirectionUpRight, i18n::Str::PatchDirectionUpLeft};
    const auto directionUndoSize = editor.undo.size();
    for (const auto& family : directionalFamilies) {
        if (!chooseAnimation(i18n::Str::PatchAnimation, "##Effect", family.label, family.row, 12) ||
            !chooseAnimation(i18n::Str::PatchExitAnimation, "##Effect", family.label, family.row, 12)) return false;
        for (int d = 0; d < 8; ++d) {
            const auto choiceUndoSize = editor.undo.size();
            auto expected = editor.draft;
            expected.animation = family.variants[d];
            if (!chooseAnimation(i18n::Str::PatchAnimation, "##Direction", directionLabels[d], d, 8)) return false;
            if (editor.draft != expected) {
                error = "Entrance direction changed another setting or used the wrong animation ID"; return false;
            }
            expected.exitAnimation = family.variants[d ^ 1];
            if (!chooseAnimation(i18n::Str::PatchExitAnimation, "##Direction", directionLabels[d ^ 1], d ^ 1, 8)) return false;
            if (editor.draft != expected || document.screenPatches != appliedSettings ||
                document.screenPatchSources != appliedSources || !matches(moved)) {
                error = "Exit direction changed the entrance, duration, source, or applied document"; return false;
            }
            if (family.label == i18n::Str::PatchSoftWipe && d == 6) {
                editor.draft.enabled = true;
                UiFirmwarePatches::ComposeLogo(editor);
                frame();
                editor.playback.Seek(editor.draft, editor.draft.durationMs / 2000.0);
                frame();
                std::vector<uint32_t> expectedPixels;
                FirmwareLogo::PreviewFrame(editor.draft,
                    uint32_t(editor.playback.Phase(editor.draft)), expectedPixels);
                if (editor.animationFrame != expectedPixels ||
                    !UiFirmwarePatches::HasDraftChanges(editor, document)) {
                    error = "Selected soft-wipe direction did not update direct pixels and draft dirty state"; return false;
                }
                if (capture && !CaptureUiDrawData(device, gpu,
                    (std::string(capturePath) + ".directions.png").c_str(), error)) return false;
            }
            while (editor.undo.size() > choiceUndoSize)
                if (!UiFirmwarePatches::UndoDraft(editor)) {
                    error = "Entrance/exit direction pair lost draft undo"; return false;
                }
            frame();
        }
    }
    while (editor.undo.size() > directionUndoSize)
        if (!UiFirmwarePatches::UndoDraft(editor)) { error = "Direction selection lost draft undo"; return false; }
    if (editor.draft != originalDraft || !matches(moved)) {
        error = "Direction selection undo did not restore the complete draft"; return false;
    }
    frame();
    auto press = [&](ImGuiKey key) {
        io.AddKeyEvent(key, true); frame();
        io.AddKeyEvent(key, false); frame();
    };
    for (int i = 0; i < 3; ++i) {
        press(static_cast<ImGuiKey>(ImGuiKey_1 + i));
        if (editor.target != static_cast<FirmwareLogo::Target>(i)) {
            error = "Number shortcut did not select the custom-screen target"; return false;
        }
    }
    // Every modal choice is driven by actual key events, never by calling paste directly.
    UiFirmwarePatches::State transfer;
    transfer.draft = editor.draft;
    transfer.draft.enabled = !editor.draft.enabled;
    transfer.draft.animation = FirmwareLogo::Animation::SoftWipeUpLeft;
    transfer.draft.exitAnimation = FirmwareLogo::Animation::BlindsUpLeft;
    transfer.draft.holdMs = 1230;
    transfer.draft.durationMs = 1200;
    transfer.draft.exitDurationMs = 1800;
    transfer.sourceWidth = transfer.sourceHeight = 2;
    transfer.source.assign(16, 255);
    transfer.sourceImage = std::make_shared<const std::vector<uint8_t>>(transfer.source);
    transfer.fitMode = 2; transfer.scalePercent = 87;
    transfer.x = 7; transfer.y = 9; transfer.alignX = transfer.alignY = -1;
    transfer.resampling = FirmwareBitmap::Resampling::Nearest;
    transfer.adjustments.brightness = 25;
    transfer.adjustments.paletteColors = 16;
    transfer.adjustments.channelCurves[0] = {.1f, .7f, .95f};
    transfer.adjustments.channelCurves[1] = {.3f, .4f, .6f};
    UiFirmwarePatches::ComposeLogo(transfer);
    struct ClipboardOwner {
        HWND window = CreateWindowExW(0, L"STATIC", L"Patch transfer regression", WS_POPUP,
            0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        ~ClipboardOwner() { if (window) DestroyWindow(window); }
    } clipboardOwner;
    if (!clipboardOwner.window || !UiFirmwarePatches::CopyToClipboard(transfer, clipboardOwner.window)) {
        error = "Cannot seed patch transfer regression clipboard"; return false;
    }
    const auto beforePaste = editor.draft;
    auto modalAlpha = [](const ImGuiWindow* modal) {
        unsigned alpha = 0;
        if (modal)
            for (const auto& vertex : modal->DrawList->VtxBuffer)
                alpha = (std::max)(alpha, (vertex.col >> IM_COL32_A_SHIFT) & 255u);
        return alpha;
    };
    auto openPaste = [&] {
        editor.requestPaste = true;
        frame(); frame();
        auto* modal = ImGui::GetTopMostPopupModal();
        const float opening = editor.pasteTransition.Opacity();
        if (!modal || !editor.pasteOpen || opening <= 0 || opening >= 1 ||
            modalAlpha(modal) == 0 || modalAlpha(modal) >= 255 ||
            std::abs(GImGui->DimBgRatio - opening) > .001f) {
            error = "Paste chooser or dim background did not fade in"; return false;
        }
        for (int i = 0; i < 10; ++i) frame();
        if (editor.pasteTransition.Opacity() != 1 || modalAlpha(modal) != 255) {
            error = "Paste chooser did not finish its 150ms entrance"; return false;
        }
        return true;
    };
    auto finishPasteClose = [&](double closedAt, FirmwareLogo::Target target,
        uint32_t mask, uint64_t revision, size_t undoSize) {
        auto* modal = ImGui::GetTopMostPopupModal();
        if (!modal || editor.pasteOpen || !editor.pasteTransition.IsClosing()) {
            error = "Paste chooser closed immediately instead of retaining its content"; return false;
        }
        const auto modalId = modal->ID;
        const auto modalSize = modal->Size;
        // Closing consumes shortcuts, cannot reopen, and cannot apply a second time.
        press(ImGuiKey_1);
        const float fading = editor.pasteTransition.Opacity();
        modal = ImGui::GetTopMostPopupModal();
        if (!modal || fading <= 0 || fading >= 1 ||
            modalAlpha(modal) == 0 || modalAlpha(modal) >= 255 ||
            std::abs(GImGui->DimBgRatio - fading) > .001f) {
            error = "Paste chooser or dim background did not fade out"; return false;
        }
        press(ImGuiKey_Enter);
        press(ImGuiKey_Escape);
        editor.requestPaste = true; frame();
        while (ImGui::GetTime() + io.DeltaTime < closedAt + .18) {
            modal = ImGui::GetTopMostPopupModal();
            if (!modal || modal->ID != modalId || modal->Size.x != modalSize.x ||
                modal->Size.y != modalSize.y || editor.pasteOpen ||
                editor.target != target || uint32_t(editor.clipboardPart) != mask ||
                editor.draftRevision != revision || editor.undo.size() != undoSize) {
                error = "Closing paste chooser lost content, leaked shortcuts or applied more than once"; return false;
            }
            frame();
        }
        if (!ImGui::GetTopMostPopupModal()) {
            error = "Paste chooser disappeared before the 180ms dismissal completed"; return false;
        }
        frame();
        if (ImGui::GetTopMostPopupModal() || !editor.pasteTransition.FullyHidden() ||
            editor.target != target || uint32_t(editor.clipboardPart) != mask ||
            editor.draftRevision != revision || editor.undo.size() != undoSize) {
            error = "Paste chooser did not close cleanly after its dismissal"; return false;
        }
        return true;
    };
    const uint32_t masks[] = {15, 1, 2, 4, 8};
    for (const auto mask : masks) {
        if (!openPaste()) return false;
        auto* modal = ImGui::GetTopMostPopupModal();
        const auto center = ImGui::GetMainViewport()->GetCenter();
        if (!modal || !(modal->Flags & ImGuiWindowFlags_Modal) ||
            modal->Size.x < ImGui::GetFontSize() * 30 ||
            std::abs(modal->Pos.x + modal->Size.x * 0.5f - center.x) > 2 ||
            std::abs(modal->Pos.y + modal->Size.y * 0.5f - center.y) > 2) {
            error = "Patch paste chooser is not a large centered modal"; return false;
        }
        const auto target = editor.target;
        if (editor.clipboardPart != UiFirmwarePatches::ClipboardPart::All) {
            error = "Paste modal did not default to all groups selected"; return false;
        }
        for (int bit = 0; bit < 4; ++bit)
            if (!(mask & (1u << bit))) press(static_cast<ImGuiKey>(ImGuiKey_1 + bit));
        if (!ImGui::GetTopMostPopupModal() || editor.draft != beforePaste || !matches(moved)) {
            error = "Toggling paste checkboxes applied data before confirmation"; return false;
        }
        double closedAt;
        if (mask == 15) {
            const ImVec2 click(modal->Pos.x + modal->Size.x * .5f,
                modal->Pos.y + modal->Size.y - modal->WindowPadding.y -
                ImGui::GetFrameHeight() * 1.7f - ImGui::GetStyle().ItemSpacing.y * 2);
            io.AddMousePosEvent(click.x, click.y); frame();
            io.AddMouseButtonEvent(0, true); frame();
            io.AddMouseButtonEvent(0, false); frame();
            closedAt = ImGui::GetTime();
            io.AddMousePosEvent(-100, -100);
        } else {
            press(ImGuiKey_Enter);
            closedAt = ImGui::GetTime() - io.DeltaTime;
        }
        if (!finishPasteClose(closedAt, target, mask, editor.draftRevision, editor.undo.size())) return false;
        const bool image = (mask & 1) != 0, placement = (mask & 2) != 0;
        const bool adjustments = (mask & 4) != 0, animation = (mask & 8) != 0;
        if (editor.target != target || uint32_t(editor.clipboardPart) != mask ||
            ImGui::GetTopMostPopupModal() || !editor.error.empty() ||
            (image && (editor.source != transfer.source || editor.sourceWidth != 2 || editor.sourceHeight != 2)) ||
            (!image && (editor.sourceImage != sourceImage || editor.source != *moved.rgba)) ||
            editor.alignX != (placement ? transfer.alignX : moved.alignX) ||
            editor.alignY != (placement ? transfer.alignY : moved.alignY) ||
            editor.x != (placement ? transfer.x : moved.x) || editor.y != (placement ? transfer.y : moved.y) ||
            editor.fitMode != (placement ? transfer.fitMode : moved.fitMode) ||
            editor.scalePercent != (placement ? transfer.scalePercent : moved.scalePercent) ||
            editor.resampling != (placement ? transfer.resampling : moved.resampling) ||
            editor.adjustments != (adjustments ? transfer.adjustments : moved.adjustments) ||
            editor.draft.holdMs != (animation && target != FirmwareLogo::Target::DeviceInfo ?
                transfer.draft.holdMs : beforePaste.holdMs) ||
            editor.draft.animation != (animation ? transfer.draft.animation : beforePaste.animation) ||
            editor.draft.exitAnimation != (animation ? transfer.draft.exitAnimation : beforePaste.exitAnimation) ||
            editor.draft.enabled != (animation ? transfer.draft.enabled : beforePaste.enabled) ||
            (mask == 8 && editor.draft.pixels != beforePaste.pixels)) {
            error = "Granular paste changed targets or overwrote an unchecked group"; return false;
        }
        UiFirmwarePatches::UndoDraft(editor);
        if (editor.draft != beforePaste || !matches(moved)) {
            error = "Clipboard draft undo did not restore original settings and placement"; return false;
        }
    }
    // Animation children are operated through real mouse events. A mixed
    // master selects all on activation; a checked master clears every child.
    auto clickAnimation = [&](int child) {
        auto* modal = ImGui::GetTopMostPopupModal();
        if (!modal) { error = "Animation paste chooser is missing"; return false; }
        const auto& style = ImGui::GetStyle();
        const float row = ImGui::GetFrameHeight() + style.ItemSpacing.y;
        const float first = modal->DC.CursorStartPos.y + ImGui::GetFontSize() + style.ItemSpacing.y * 2 + 1;
        const ImVec2 point(modal->DC.CursorStartPos.x + ImGui::GetFrameHeight() * (child < 0 ? .5f : 1.5f),
            first + row * (child < 0 ? 3 : 4 + child) + ImGui::GetFrameHeight() * .5f);
        io.AddMousePosEvent(point.x, point.y); frame();
        io.AddMouseButtonEvent(0, true); frame();
        io.AddMouseButtonEvent(0, false); frame();
        io.AddMousePosEvent(-100, -100);
        return true;
    };
    for (uint32_t animationMask = 1; animationMask < 7; ++animationMask) {
        if (!openPaste()) return false;
        press(ImGuiKey_1); press(ImGuiKey_2); press(ImGuiKey_3);
        for (int child = 0; child < 3; ++child)
            if (!(animationMask & (1u << child)) && !clickAnimation(child)) return false;
        if (uint32_t(editor.clipboardAnimation) != animationMask ||
            editor.clipboardPart != UiFirmwarePatches::ClipboardPart::Animation || editor.draft != beforePaste) {
            error = "Nested animation selection applied edits early or failed to expose a mixed master"; return false;
        }
        if (animationMask == 1) {
            press(ImGuiKey_4);
            if (editor.clipboardAnimation != UiFirmwarePatches::AnimationPart::All) {
                error = "Animation master hotkey did not select all from mixed"; return false;
            }
            press(ImGuiKey_4);
            if (editor.clipboardAnimation != UiFirmwarePatches::AnimationPart::None ||
                uint32_t(editor.clipboardPart) != 0 || !clickAnimation(0) || !clickAnimation(-1) ||
                editor.clipboardAnimation != UiFirmwarePatches::AnimationPart::All || !clickAnimation(-1) ||
                editor.clipboardAnimation != UiFirmwarePatches::AnimationPart::None || !clickAnimation(0)) {
                error = "Animation master/child mouse controls did not retain all/none/mixed semantics"; return false;
            }
        }
        if (capture && animationMask == 1 &&
            !CaptureUiDrawData(device, gpu, (std::string(capturePath) + ".paste.png").c_str(), error)) return false;
        press(ImGuiKey_Enter);
        const double closedAt = ImGui::GetTime() - io.DeltaTime;
        if (!finishPasteClose(closedAt, editor.target, 8, editor.draftRevision, editor.undo.size())) return false;
        auto expected = beforePaste;
        expected.enabled = transfer.draft.enabled;
        if (animationMask & 1) expected.animation = transfer.draft.animation;
        if (animationMask & 2) expected.exitAnimation = transfer.draft.exitAnimation;
        if (animationMask & 4) {
            expected.durationMs = transfer.draft.durationMs;
            expected.exitDurationMs = transfer.draft.exitDurationMs;
        }
        if (editor.draft != expected || !matches(moved) ||
            uint32_t(editor.clipboardAnimation) != animationMask ||
            !UiFirmwarePatches::UndoDraft(editor) || editor.draft != beforePaste || !matches(moved)) {
            error = "Partial animation paste changed an unselected field, CHIP ID hold, source, or draft undo"; return false;
        }
    }
    for (const auto target : {FirmwareLogo::Target::Boot, FirmwareLogo::Target::Shutdown, FirmwareLogo::Target::DeviceInfo}) {
        UiFirmwarePatches::State durations;
        durations.target = target;
        durations.draft.holdMs = 4321;
        durations.draft.durationMs = 400; durations.draft.exitDurationMs = 900;
        durations.clipboardPart = UiFirmwarePatches::ClipboardPart::Animation;
        durations.clipboardAnimation = UiFirmwarePatches::AnimationPart::Durations;
        auto expected = durations.draft;
        expected.enabled = transfer.draft.enabled;
        expected.durationMs = transfer.draft.durationMs;
        expected.exitDurationMs = transfer.draft.exitDurationMs;
        if (target != FirmwareLogo::Target::DeviceInfo) expected.holdMs = transfer.draft.holdMs;
        if (!UiFirmwarePatches::PasteFromClipboard(durations, clipboardOwner.window) || durations.draft != expected) {
            error = "Durations-only paste lost target-specific hold behavior or copied an effect"; return false;
        }
    }
    // Clipboard accepts every concrete animation ID, but never the sentinel.
    for (uint32_t id = 0; id < uint32_t(FirmwareLogo::Animation::Count); ++id) {
        transfer.draft.animation = static_cast<Animation>(id);
        transfer.draft.exitAnimation = static_cast<Animation>(uint32_t(Animation::Count) - 1 - id);
        UiFirmwarePatches::State pasted;
        pasted.clipboardPart = UiFirmwarePatches::ClipboardPart::Animation;
        const bool copied = UiFirmwarePatches::CopyToClipboard(transfer, clipboardOwner.window);
        const bool accepted = copied && UiFirmwarePatches::PasteFromClipboard(pasted, clipboardOwner.window);
        const DWORD clipboardError = GetLastError();
        if (!accepted || pasted.draft.animation != transfer.draft.animation ||
            pasted.draft.exitAnimation != transfer.draft.exitAnimation) {
            DWORD clipboardProcess = 0;
            if (const HWND lockedBy = GetOpenClipboardWindow()) GetWindowThreadProcessId(lockedBy, &clipboardProcess);
            error = "Directional clipboard ID " + std::to_string(id) +
                " copied=" + std::to_string(copied) + " accepted=" + std::to_string(accepted) +
                " entrance=" + std::to_string(uint32_t(pasted.draft.animation)) +
                " exit=" + std::to_string(uint32_t(pasted.draft.exitAnimation)) +
                "; win32=" + std::to_string(clipboardError) +
                "; clipboardProcess=" + std::to_string(clipboardProcess) +
                "; copy error=" + transfer.error + "; paste error=" + pasted.error; return false;
        }
    }
    for (const bool invalidExit : {false, true}) {
        transfer.draft.animation = invalidExit ? Animation::SoftWipeUpLeft : Animation::Count;
        transfer.draft.exitAnimation = invalidExit ? Animation::Count : Animation::BlindsUpLeft;
        UiFirmwarePatches::State pasted;
        pasted.clipboardPart = UiFirmwarePatches::ClipboardPart::Animation;
        const auto unchanged = pasted.draft;
        if (!UiFirmwarePatches::CopyToClipboard(transfer, clipboardOwner.window) ||
            UiFirmwarePatches::PasteFromClipboard(pasted, clipboardOwner.window) || pasted.draft != unchanged) {
            error = "Clipboard accepted Animation::Count or modified the draft on rejection"; return false;
        }
    }
    transfer.draft.animation = Animation::SoftWipeUpLeft;
    transfer.draft.exitAnimation = Animation::BlindsUpLeft;
    if (!UiFirmwarePatches::CopyToClipboard(transfer, clipboardOwner.window)) {
        error = "Cannot restore the valid patch transfer clipboard"; return false;
    }
    // Direct playback advances and seeks without waiting for native phase preparation.
    editor.draft.enabled = true;
    editor.draft.animation = FirmwareLogo::Animation::SlideUpRight;
    editor.draft.exitAnimation = FirmwareLogo::Animation::SoftWipeDownLeft;
    editor.draft.holdMs = 500;
    UiFirmwarePatches::ComposeLogo(editor);
    frame();
    const auto validationGeneration = editor.preview.AfterGeneration();
    editor.playback.Play(editor.draft);
    frame();
    std::vector<uint32_t> directFrame;
    FirmwareLogo::PreviewFrame(editor.draft, uint32_t(editor.playback.Phase(editor.draft)), directFrame);
    if (editor.playback.position <= 0 || !editor.playback.showAnimation ||
        editor.uploadedPhase != editor.playback.Phase(editor.draft) || editor.animationFrame != directFrame) {
        error = "Animation playback waited for native preparation or did not render its current phase"; return false;
    }
    editor.playback.Seek(editor.draft, UiFirmwarePatches::Playback::Duration(editor.draft, editor.target) * 0.9);
    frame();
    FirmwareLogo::PreviewFrame(editor.draft, uint32_t(editor.playback.Phase(editor.draft)), directFrame);
    if (editor.animationFrame != directFrame || editor.preview.AfterGeneration() > validationGeneration + 1) {
        error = "Seeking animation queued native phases or displayed an obsolete direct frame"; return false;
    }
    const double entrance = editor.draft.durationMs / 1000.0;
    editor.playback.Seek(editor.draft, entrance + .001);
    frame();
    if (editor.playback.Phase(editor.draft) != 16 || editor.draft.holdMs != 500 ||
        std::abs(UiFirmwarePatches::Playback::Duration(editor.draft, editor.target) -
            (editor.draft.durationMs + editor.draft.exitDurationMs) / 1000.0) > .0001) {
        error = "CHIP ID fast preview applied an artificial hold or changed legacy stored timing"; return false;
    }
    for (const auto target : {FirmwareLogo::Target::Boot, FirmwareLogo::Target::Shutdown}) {
        if (std::abs(UiFirmwarePatches::Playback::Duration(editor.draft, target) -
            (editor.draft.durationMs + editor.draft.holdMs + editor.draft.exitDurationMs) / 1000.0) > .0001) {
            error = "Hiding CHIP ID hold removed startup/shutdown hold timing"; return false;
        }
    }
    if (!UiFirmwarePatches::UndoDraft(editor) || editor.draft != beforePaste || !matches(moved)) {
        error = "Animated draft undo failed to restore the original composition"; return false;
    }
    frame();
    for (int closeAction = 0; closeAction < 3; ++closeAction) {
        if (!openPaste()) return false;
        const auto target = editor.target;
        const auto revision = editor.draftRevision;
        const auto undoSize = editor.undo.size();
        // With no selected groups, Enter must not apply or dismiss the chooser.
        for (int bit = 0; bit < 4; ++bit) press(static_cast<ImGuiKey>(ImGuiKey_1 + bit));
        press(ImGuiKey_Enter);
        if (!ImGui::GetTopMostPopupModal() || !editor.pasteOpen ||
            uint32_t(editor.clipboardPart) != 0 || editor.draftRevision != revision ||
            editor.undo.size() != undoSize || editor.draft != beforePaste || !matches(moved)) {
            error = "Empty paste selection accepted data or dismissed the chooser"; return false;
        }
        double closedAt;
        if (closeAction == 0) {
            press(ImGuiKey_Escape);
            closedAt = ImGui::GetTime() - io.DeltaTime;
        } else {
            auto* modal = ImGui::GetTopMostPopupModal();
            const auto padding = ImGui::GetStyle().FramePadding;
            const ImVec2 click = closeAction == 1 ?
                ImVec2(modal->Pos.x + modal->Size.x * .5f,
                    modal->Pos.y + modal->Size.y - modal->WindowPadding.y - ImGui::GetFrameHeight() * .5f) :
                ImVec2(modal->Pos.x + modal->Size.x - padding.x - ImGui::GetFontSize() * .5f,
                    modal->Pos.y + padding.y + ImGui::GetFontSize() * .5f);
            io.AddMousePosEvent(click.x, click.y); frame();
            io.AddMouseButtonEvent(0, true); frame();
            io.AddMouseButtonEvent(0, false); frame();
            closedAt = ImGui::GetTime();
            io.AddMousePosEvent(-100, -100);
        }
        if (!finishPasteClose(closedAt, target, 0, revision, undoSize)) return false;
        if (editor.draft != beforePaste || !matches(moved)) {
            error = "Escape, Cancel or titlebar close changed paste content"; return false;
        }
    }
    // A pending flash calculation must keep the prior visible table intact.
    std::promise<UiFirmwarePatches::State::MemoryResult> pendingMemory;
    editor.memoryJob = pendingMemory.get_future();
    editor.memory.usage = {{{0x3103, 100, 10, 200}, 0}, {{0x3102, 100, 10, 200}, 0}};
    editor.memoryDocument = document.DocumentIdentity();
    editor.memorySource = document.ResourceRevision();
    editor.memoryDraft = editor.draftRevision;
    ++editor.draftRevision;
    frame();
    bool visibleTable = false;
    for (int i = 0; i < GImGui->Tables.GetMapSize(); ++i) {
        const auto* table = GImGui->Tables.TryGetMapData(i);
        if (table && table->ColumnsCount == 9 && table->LastFrameActive == GImGui->FrameCount)
            visibleTable = true;
    }
    if (!visibleTable || editor.memory.usage.size() != 2) {
        error = "Flash table disappeared during draft memory recomputation"; return false;
    }
    UiFirmwarePatches::State::MemoryResult memoryResult;
    memoryResult.usage = {{{1, 100, 10, 200}, 0}, {{2, 190, 20, 200}, 0},
                          {{3, 180, 10, 200}, 0}, {{4, 170, 20, 200}, 0}};
    pendingMemory.set_value(std::move(memoryResult));
    editor.memoryDraft = editor.draftRevision;
    frame();
    if (editor.memory.usage.size() != 4 || editor.memory.usage[0].hardwareCode != 2 ||
        editor.memory.usage[1].hardwareCode != 3 || editor.memory.usage[2].hardwareCode != 4 ||
        editor.memory.usage[3].hardwareCode != 1) {
        error = "Flash revision rows are not stably sorted by signed remaining capacity"; return false;
    }
    // New documents discard old rows even after RenderTab adopted their identity.
    editor.memoryDocument = 0;
    frame();
    if (!editor.memory.usage.empty()) {
        error = "New-document memory request retained the previous document table"; return false;
    }
    if (capture && !CaptureUiDrawData(device, gpu, capturePath, error)) return false;
    if (!RunPatchEmulatorBoundaryUiRegressions(device, gpu, error)) return false;
    if (!RunDraftPatchSetUiRegressions(device, gpu, error)) return false;
    if (log) *log << "EDITABLE SOURCE UI REGRESSIONS OK (joined/collapsed section WARP pixels, keyboard/mouse/nesting/scope balance, source/placement, target-local draft undo, shared images, number shortcuts, modal paste, retained/sorted flash rows, unsaved CHIP ID photo/animation emulator boundary and explicit VM Apply)\n";
    return true;
}
