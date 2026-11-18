#include "ui_startup.h"
#include "app_state.h"
#include "firmware_profiles.h"
#include "ui_resource_studio.h"
#include "ui_modal_transition.h"
#include "../../imgui/imgui.h"

#include <wincodec.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <cstdio>
#include <utility>
#include <vector>
#include <cstring>
#include <cfloat>
#include <string_view>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace UiStartup {
namespace {

using Microsoft::WRL::ComPtr;



struct DeviceImage {
    FirmwareModel model;
    int resourceId;
    Dx11Texture texture;
    bool attempted = false;
};

const char* EditionLabel(FirmwareEdition edition) {
    if (edition == FirmwareEdition::Factory) return Tr(i18n::Str::EditionFactory);
    if (edition == FirmwareEdition::Standard) return Tr(i18n::Str::EditionStandard);
    return FirmwareEditionName(edition);
}

// Official product images, embedded with the matching resource IDs.
std::array<DeviceImage, 13> images{{
    {FirmwareModel::DragS, 201},
    {FirmwareModel::DragX, 202},
    {FirmwareModel::Drag2, 203},
    {FirmwareModel::DragSPro, 204},
    {FirmwareModel::DragXPro, 205},
    {FirmwareModel::DragXPlus, 206},
    {FirmwareModel::DragXPlusPro, 207},
    {FirmwareModel::Drag3, 208},
    {FirmwareModel::DragMax, 209},
    {FirmwareModel::Drag1, 210},
    {FirmwareModel::DragMini, 211},
    {FirmwareModel::Drag2Refresh, 212},
    {FirmwareModel::DragMiniRefresh, 213},
}};

struct DeviceChoice {
    FirmwareModel model;
    std::vector<const FirmwareProfile*> editions;
    size_t selectedEdition = 0;
};

std::vector<DeviceChoice> choices;
size_t selectedDevice = 0;
ID3D11Device* imageDevice = nullptr;
bool registryLoaded = false;
Ui::ModalTransition transition;

DeviceImage* ImageFor(FirmwareModel model) {
    const auto it = std::find_if(images.begin(), images.end(),
        [model](const DeviceImage& image) { return image.model == model; });
    return it == images.end() ? nullptr : &*it;
}

bool LoadImage(DeviceImage& image, IWICImagingFactory* factory,
               ID3D11Device* device, ID3D11DeviceContext* context) {
    const auto module = GetModuleHandleW(nullptr);
    const auto resource = FindResourceW(module, MAKEINTRESOURCEW(image.resourceId), RT_RCDATA);
    if (!resource) return false;
    const DWORD length = SizeofResource(module, resource);
    const auto loaded = LoadResource(module, resource);
    auto* bytes = static_cast<BYTE*>(LockResource(loaded));
    if (!bytes || length == 0) return false;

    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromMemory(bytes, length)) ||
        FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr,
            WICDecodeMetadataCacheOnLoad, &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame)) ||
        FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
        return false;

    UINT width = 0, height = 0;
    if (FAILED(converter->GetSize(&width, &height)) || width == 0 || height == 0 ||
        width > 1024 || height > 1024)
        return false;
    std::vector<uint32_t> pixels(static_cast<size_t>(width) * height);
    if (FAILED(converter->CopyPixels(nullptr, width * 4,
            static_cast<UINT>(pixels.size() * sizeof(uint32_t)),
            reinterpret_cast<BYTE*>(pixels.data()))))
        return false;
    return image.texture.Update(device, context, pixels.data(), width, height);
}

void EnsureImages(ID3D11Device* device, ID3D11DeviceContext* context) {
    if (!device || !context) return;
    if (imageDevice != device) {
        for (auto& image : images) {
            image.texture.Release();
            image.attempted = false;
        }
        imageDevice = device;
    }
    const bool pending = std::any_of(choices.begin(), choices.end(), [](const DeviceChoice& choice) {
        const auto* image = ImageFor(choice.model);
        return image && !image->attempted;
    });
    if (!pending) return;

    // Respect an existing apartment, including one established by a file dialog.
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ComPtr<IWICImagingFactory> factory;
    const HRESULT created = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    for (const auto& choice : choices) {
        auto* image = ImageFor(choice.model);
        if (!image || image->attempted) continue;
        image->attempted = true;
        if (SUCCEEDED(created)) LoadImage(*image, factory.Get(), device, context);
    }
    factory.Reset();
    if (SUCCEEDED(initialized)) CoUninitialize();
}

void EnsureRegistry() {
    if (registryLoaded) return;
    const auto& profiles = FirmwareProfiles();
    choices.reserve(profiles.size());
    for (const auto& profile : profiles) {
        auto it = std::find_if(choices.begin(), choices.end(),
            [&profile](const DeviceChoice& choice) { return choice.model == profile.model; });
        if (it == choices.end()) {
            choices.push_back({profile.model, {}, 0});
            it = choices.end() - 1;
        }
        it->editions.push_back(&profile);
    }
    registryLoaded = true;
}

void DrawSupportTooltip(const FirmwareProfile& profile) {
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) return;
    using enum i18n::Str;
    const bool legacy = profile.model == FirmwareModel::Drag1 || profile.model == FirmwareModel::Drag2 ||
        profile.model == FirmwareModel::DragMini;
    const bool refresh = profile.model == FirmwareModel::Drag2Refresh ||
        profile.model == FirmwareModel::DragMiniRefresh;
    const bool customPatches = profile.model == FirmwareModel::DragS &&
        (profile.edition == FirmwareEdition::Score || profile.edition == FirmwareEdition::Factory);
    const float width = (std::min)(ImGui::GetFontSize() * 32.0f,
        ImGui::GetMainViewport()->WorkSize.x - 32.0f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0), ImVec2(width, FLT_MAX));
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 4));
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(FirmwareModelName(profile.model));
    ImGui::TextDisabled("%s", EditionLabel(profile.edition));
    ImGui::Separator();
    const auto row = [](i18n::Str feature, i18n::Str status) {
        std::string_view label(Tr(feature));
        while (!label.empty() && label.front() == ' ') label.remove_prefix(1);
        while (!label.empty() && label.back() == ' ') label.remove_suffix(1);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::TableNextColumn();
        const char* text = Tr(status);
        const float inset = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text).x;
        if (inset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + inset);
        ImGui::TextUnformatted(text);
    };
    const auto note = [](i18n::Str text) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", Tr(text));
        ImGui::PopStyleColor();
    };
    if (ImGui::BeginTable("SupportFeatures", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Feature", ImGuiTableColumnFlags_WidthStretch, 1.1f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        row(TabResourceStudio, SupportFull);
        row(SupportPalette, legacy || refresh ? SupportNone : SupportFull);
        row(TabFirmwareEmulator, SupportFull);
        ImGui::EndTable();
    }
    ImGui::SeparatorText(Tr(TabFirmwarePatches));
    if (ImGui::BeginTable("PatchSupport", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Feature", ImGuiTableColumnFlags_WidthStretch, 1.1f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        row(PatchCustomScreens, customPatches ? SupportFull : SupportNone);
        row(TabFirmwareLayout, SupportFull);
        row(PatchDeviceMenu, customPatches ? SupportFull : SupportNone);
        ImGui::EndTable();
    }
    ImGui::Separator();
    if (legacy || refresh) note(SupportMonochrome);
    if (legacy) note(EmulatorLoaderNote);
    ImGui::EndTooltip();
    ImGui::PopStyleVar(2);
}

void DrawCard(DeviceChoice& choice, size_t index, const ImVec2& size, float imageHeight) {
    ImGui::PushID(static_cast<int>(choice.model));
    const bool selected = index == selectedDevice;
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(
        selected ? ImGuiCol_ButtonActive : ImGuiCol_Button));
    const auto origin = ImGui::GetCursorScreenPos();
    if (ImGui::Button("##Device", size)) selectedDevice = index;
    DrawSupportTooltip(*choice.editions[choice.selectedEdition]);
    ImGui::PopStyleColor();

    auto* draw = ImGui::GetWindowDrawList();
    if (index == selectedDevice) {
        draw->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y),
            ImGui::GetColorU32(ImGuiCol_CheckMark), ImGui::GetStyle().FrameRounding, 0, 1.5f);
    }
    const char* name = FirmwareModelName(choice.model);
    const float wrapWidth = size.x - 24.0f;
    const auto textSize = ImGui::CalcTextSize(name, nullptr, false, wrapWidth);
    const float contentHeight = imageHeight + 9.0f + textSize.y;
    const float imageTop = origin.y + (size.y - contentHeight) * 0.5f;
    const auto* image = ImageFor(choice.model);
    if (image && image->texture.IsValid()) {
        const float scale = (std::min)(imageHeight / image->texture.GetHeight(),
            (size.x - 24.0f) / image->texture.GetWidth());
        const ImVec2 imageSize(image->texture.GetWidth() * scale,
            image->texture.GetHeight() * scale);
        const ImVec2 imagePos(origin.x + (size.x - imageSize.x) * 0.5f,
            imageTop + (imageHeight - imageSize.y) * 0.5f);
        draw->AddImage((ImTextureID)(intptr_t)image->texture.GetSRV(), imagePos,
            ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y),
            ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(IM_COL32_WHITE));
    } else {
        // All product downloads are bundled. Report a missing runtime resource
        // honestly rather than substituting a photograph of another model.
        const auto* unavailable = Tr(i18n::Str::StartupImageUnavailable);
        const auto textSize = ImGui::CalcTextSize(unavailable);
        draw->AddText(ImVec2(origin.x + (size.x - textSize.x) * 0.5f,
            imageTop + imageHeight * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), unavailable);
    }
    const char* end = name + std::strlen(name);
    float textY = imageTop + imageHeight + 9.0f;
    while (name < end) {
        const char* lineEnd = ImGui::GetFont()->CalcWordWrapPosition(
            ImGui::GetFontSize(), name, end, wrapWidth);
        if (lineEnd == name) ++lineEnd;
        const char* textEnd = lineEnd;
        while (textEnd > name && textEnd[-1] == ' ') --textEnd;
        const float lineWidth = ImGui::CalcTextSize(name, textEnd).x;
        draw->AddText(ImVec2(origin.x + (size.x - lineWidth) * 0.5f, textY),
            ImGui::GetColorU32(ImGuiCol_Text), name, textEnd);
        textY += ImGui::GetTextLineHeight();
        name = lineEnd;
        while (name < end && (*name == ' ' || *name == '\n')) ++name;
    }
    ImGui::PopID();
}

void OpenBuiltin(StudioState& studio, HWND hwnd, const FirmwareProfile& profile) {
    UiResourceStudio::OpenBuiltin(studio, hwnd, profile.model, profile.edition);
}

} // namespace

void Render(StudioState& studio, HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context) {
    const double now = ImGui::GetTime();
    char title[160];
    snprintf(title, sizeof(title), "%s###StartupChooser", Tr(i18n::Str::StartupChooseFirmware));
    // A file-operation modal may have replaced the chooser while it was not
    // dispatched. Never reopen that stale popup after successful adoption.
    if (!studio.showStartupChooser && !ImGui::IsPopupOpen(title)) {
        transition = {};
        return;
    }
    // Keep an already-closing chooser alive through builtin adoption's completion
    // frame, but do not compete with another file operation's modal.
    if (studio.showStartupChooser && UiResourceStudio::IsFileActionActive() &&
        !UiResourceStudio::IsLoadingBuiltin()) return;
    transition.Update(studio.showStartupChooser, now);
    if (transition.FullyHidden()) {
        if (ImGui::IsPopupOpen(title)) {
            ImGui::ClosePopupToLevel(ImGui::GetCurrentContext()->BeginPopupStack.Size, true);
            ImGui::GetCurrentContext()->DimBgRatio = 0;
        }
        return;
    }
    EnsureRegistry();
    EnsureImages(device, context);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * transition.Opacity());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2((std::min)(1040.0f, viewport->WorkSize.x - 64.0f),
        (std::min)(720.0f, viewport->WorkSize.y - 64.0f)), ImGuiCond_Always);
    if (studio.showStartupChooser && !ImGui::IsPopupOpen(title)) ImGui::OpenPopup(title);
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        (transition.IsClosing() ? ImGuiWindowFlags_NoInputs : 0))) {
        ImGui::PopStyleVar(4);
        return;
    }
    ImGui::GetCurrentContext()->DimBgRatio = transition.Opacity();
    const bool blocked = transition.IsClosing() || UiResourceStudio::IsFileActionActive();
    if (Ui::ModalCloseButton(blocked)) {
        studio.showStartupChooser = false;
        transition.Update(false, now);
    }
    ImGui::BeginDisabled(transition.IsClosing() || UiResourceStudio::IsFileActionActive());

    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float width = ImGui::GetContentRegionAvail().x;
    const float left = ImGui::GetCursorPosX();
    const char* openFile = Tr(i18n::Str::StartupOpenFile);
    const float fileWidth = ImGui::CalcTextSize(openFile).x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SetCursorPosX(left + width - fileWidth);
    if (ImGui::Button(openFile, ImVec2(fileWidth, 0)))
        UiResourceStudio::OpenFirmware(studio, hwnd);
    ImGui::SetItemDefaultFocus();
    if (studio.statusIsErr && !studio.statusMsg.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.42f, 1.0f));
        ImGui::TextWrapped("%s", studio.statusMsg.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();

    if (choices.empty()) {
        ImGui::TextWrapped("%s", Tr(i18n::Str::StartupNoProfiles));
        ImGui::EndDisabled();
        ImGui::EndPopup();
        ImGui::PopStyleVar(4);
        return;
    }

    auto& current = choices[selectedDevice];
    const float line = ImGui::GetTextLineHeightWithSpacing();
    const float frame = ImGui::GetFrameHeightWithSpacing();
    const char* openBuiltin = Tr(i18n::Str::StartupOpenBuiltin);
    const float openWidth = ImGui::CalcTextSize(openBuiltin).x + ImGui::GetStyle().FramePadding.x * 2;
    const bool wideFooter = width > openWidth + 260.0f;
    const float editionWidth = wideFooter ? width - openWidth - gap : width;
    float chipX = 0.0f;
    int chipRows = 1;
    for (const auto* profile : current.editions) {
        const float chipWidth = ImGui::CalcTextSize(EditionLabel(profile->edition)).x +
            ImGui::GetStyle().FramePadding.x * 2;
        if (chipX > 0 && chipX + chipWidth > editionWidth) { ++chipRows; chipX = 0; }
        chipX += chipWidth + gap;
    }
    const float footerHeight = line + frame * chipRows + (wideFooter ? 12.0f : frame + 12.0f);
    const float gridHeight = (std::max)(100.0f, ImGui::GetContentRegionAvail().y - footerHeight);
    const float scale = ImGui::GetFontSize() / 16.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
    if (ImGui::BeginChild("DeviceGrid", ImVec2(0, gridHeight), ImGuiChildFlags_None,
            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        const float gridWidth = ImGui::GetContentRegionAvail().x;
        const int columns = (std::max)(1, static_cast<int>((gridWidth + gap) / (172.0f * scale + gap)));
        const float cardWidth = (gridWidth - gap * (columns - 1)) / columns;
        const float imageHeight = 130.0f * scale;
        const ImVec2 cardSize(cardWidth, imageHeight + 33.0f + line * 2.0f);
        for (size_t i = 0; i < choices.size(); ++i) {
            if (i % columns != 0) ImGui::SameLine();
            DrawCard(choices[i], i, cardSize, imageHeight);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Re-read selection after the grid: clicking a card updates its edition
    // controls immediately, without loading firmware or entering the editor.
    auto& selected = choices[selectedDevice];
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted(FirmwareModelName(selected.model));
    float used = 0.0f;
    for (size_t i = 0; i < selected.editions.size(); ++i) {
        const auto* profile = selected.editions[i];
        const char* label = EditionLabel(profile->edition);
        const float chipWidth = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2;
        if (used > 0.0f && used + chipWidth <= editionWidth) ImGui::SameLine();
        else used = 0.0f;
        const bool active = selected.selectedEdition == i;
        ImGui::PushID(static_cast<int>(i));
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(
            active ? ImGuiCol_ButtonActive : ImGuiCol_Button));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(
            active ? ImGuiCol_CheckMark : ImGuiCol_Border));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, active ? 1.0f : 0.0f);
        if (ImGui::Button(label, ImVec2(chipWidth, 0))) selected.selectedEdition = i;
        DrawSupportTooltip(*profile);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::PopID();
        used += chipWidth + gap;
    }
    if (wideFooter) ImGui::SameLine(left + width - openWidth);
    const bool open = ImGui::Button(openBuiltin, ImVec2(wideFooter ? openWidth : -1.0f, 0));
    if (open) OpenBuiltin(studio, hwnd, *selected.editions[selected.selectedEdition]);

    transition.Update(studio.showStartupChooser, now);
    ImGui::EndDisabled();
    ImGui::EndPopup();
    ImGui::PopStyleVar(4);
}

void Release() {
    for (auto& image : images) {
        image.texture.Release();
        image.attempted = false;
    }
    choices.clear();
    selectedDevice = 0;
    imageDevice = nullptr;
    registryLoaded = false;
    transition = {};
}

} // namespace UiStartup
