#include "ui_tabs.h"
#include "ui_firmware_emulator.h"
#include "app_state.h"
#include "ui_resource_studio.h"
#include "imgui.h"
#include "ui_controls.h"
#include <string_view>

namespace UiFirmwareEmulator {
namespace {
void ReleaseTextures(State& state) {
    state.beforeTexture.Release();
    state.afterTexture.Release();
    state.beforeGeneration = state.afterGeneration = 0;
}

const FirmwareEmulator::Frame& PresentedFrame(const State& state, bool before) {
    static const FirmwareEmulator::Frame empty;
    if (!state.presentation) return empty;
    const auto& frame = before ? state.presentation->before : state.presentation->after;
    return frame ? *frame : empty;
}

void UpdateTextures(State& state, ID3D11Device* device, ID3D11DeviceContext* context) {
    if (state.textureDevice != device || state.textureContext != context) {
        ReleaseTextures(state);
        state.textureDevice = device;
        state.textureContext = context;
    }
    bool usable = device && context && SUCCEEDED(device->GetDeviceRemovedReason());
    if (usable) {
        ID3D11Device* contextDevice = nullptr;
        context->GetDevice(&contextDevice);
        usable = contextDevice == device;
        if (contextDevice) contextDevice->Release();
    }
    if (!usable) {
        ReleaseTextures(state);
        state.beforeTextureError = state.afterTextureError = true;
        return;
    }
    auto upload = [&](const FirmwareEmulator::Frame& frame, uint64_t generation,
        Dx11Texture& texture, uint64_t& uploaded, bool& error) {
        if (uploaded == generation) return;
        error = false;
        if (!frame.error.empty() || frame.rgba.empty()) {
            texture.Release();
            uploaded = generation;
            return;
        }
        if (!texture.Update(device, context, frame.rgba.data(), frame.width, frame.height)) {
            texture.Release();
            error = true;
        }
        uploaded = generation;
    };
    upload(PresentedFrame(state, true), state.presentationGeneration, state.beforeTexture,
        state.beforeGeneration, state.beforeTextureError);
    upload(PresentedFrame(state, false), state.presentationGeneration, state.afterTexture,
        state.afterGeneration, state.afterTextureError);
    if (FAILED(device->GetDeviceRemovedReason())) {
        ReleaseTextures(state);
        state.beforeTextureError = state.afterTextureError = true;
    }
}

void DrawDisplay(const State& state, bool before, const StudioState& studio) {
    const auto& frame = PresentedFrame(state, before);
    const auto& texture = before ? state.beforeTexture : state.afterTexture;
    const auto* source = before ? (state.presentation ? state.presentation->original.get() : nullptr) : &studio;
    ImGui::SeparatorText(Tr(before ? i18n::Str::EmulatorBefore : i18n::Str::EmulatorAfter));
    if (!frame.error.empty()) {
        if (before) ImGui::TextWrapped(Tr(i18n::Str::EmulatorOriginalError), frame.error.c_str());
        else ImGui::TextWrapped("%s", frame.error.c_str());
        return;
    }
    if (before ? state.beforeTextureError : state.afterTextureError) {
        ImGui::TextWrapped("%s", Tr(i18n::Str::CompareTextureError));
        return;
    }
    if (!texture.IsValid()) return;
    const ImVec2 size(static_cast<float>(frame.width * state.zoom), static_cast<float>(frame.height * state.zoom));
    const float inset = (ImGui::GetContentRegionAvail().x - size.x) * 0.5f;
    if (inset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + inset);
    ImGui::Image((ImTextureID)texture.GetSRV(), size);
    ImGui::TextDisabled("%d x %d / %s / HW %04X", frame.width, frame.height,
        frame.monochrome ? "1-bit" : "RGB565", frame.hardwareCode);
    ImGui::TextDisabled("Bytes: %llu / pixels: %llu", frame.spiBytes, frame.pixelWrites);
    if (frame.emulatedUid) ImGui::TextWrapped("%s", Tr(i18n::Str::EmulatorUidNote));
    ImGui::SeparatorText(Tr(i18n::Str::EmulatorResources));
    if (frame.resources.empty()) ImGui::TextDisabled("%s", Tr(i18n::Str::EmulatorNoResources));
    for (size_t index : frame.resources) {
        if (!source || index >= source->catalog.size()) continue;
        const auto& resource = source->catalog[index];
        ImGui::Text("%s (%u x %u) — %s", i18n::ResourceText(resource.title),
            resource.width, resource.height, resource.id.c_str());
    }
}
}

const char* ScreenTitle(const FirmwareEmulator::ScreenInfo& screen) {
    if (!screen.id) return Tr(i18n::Str::EmulatorMainScreen);
    const std::string_view title(screen.title);
    if (title == "Version (emulated loader data)") return Tr(i18n::Str::EmulatorLoaderVersion);
    if (title == "CHIP-ID (emulated loader data)") return Tr(i18n::Str::EmulatorLoaderId);
    return i18n::ResourceText(screen.title);
}

bool DrawDeviceControls(FirmwareEmulator::DeviceState& device, const StudioState& studio) {
    bool dirty = false;
    const auto screens = FirmwareEmulator::AvailableScreens(studio, device.hardwareCode);
    const auto selectedScreen = std::find_if(screens.begin(), screens.end(),
        [&](const auto& screen) { return screen.id == device.screen; });
    if (selectedScreen != screens.end())
        dirty |= FirmwareEmulator::ApplyScreenMode(device, *selectedScreen);
    if (!Ui::BeginControlTable("ScreenParameters")) return dirty;
    const auto checkbox = [&](const char* label, bool& value) {
        ImGui::PushID(label);
        Ui::ControlRow(label);
        dirty |= ImGui::Checkbox("##Value", &value);
        ImGui::PopID();
    };
    if (FirmwareLogo::Supported(studio.firmware)) {
        const bool animatedScreen = device.screen == 1 || device.screen == 3 || device.screen == 15;
        const auto target = device.screen == 3 ? FirmwareLogo::Target::Shutdown :
            device.screen == 15 ? FirmwareLogo::Target::DeviceInfo : FirmwareLogo::Target::Boot;
        const auto& patch = studio.screenPatches[static_cast<size_t>(target)];
        const int frameCount = int(FirmwareLogo::FrameCount(patch));
        if (animatedScreen && patch.enabled && frameCount > 1) {
            Ui::ControlRow(Tr(i18n::Str::PatchAnimationPhase));
            dirty |= ImGui::SliderInt("##AnimationPhase", &device.logoPhase, -1,
                frameCount - 1, "%d", ImGuiSliderFlags_AlwaysClamp);
        } else if (device.logoPhase != -1) {
            device.logoPhase = -1;
            dirty = true;
        }
    }
    const bool refresh = studio.model == FirmwareModel::Drag2Refresh || studio.model == FirmwareModel::DragMiniRefresh;
    const bool mini = studio.model == FirmwareModel::DragMini || studio.model == FirmwareModel::DragMiniRefresh;
    const bool legacy = studio.model == FirmwareModel::Drag1 || studio.model == FirmwareModel::Drag2 || mini || refresh;
    const bool largePanel = studio.model == FirmwareModel::Drag3 || studio.model == FirmwareModel::DragMax;
    const bool plus = studio.model == FirmwareModel::DragXPlus || studio.model == FirmwareModel::DragXPlusPro;
    const bool sx = studio.model == FirmwareModel::DragS || studio.model == FirmwareModel::DragX;
    const int maxWatts = largePanel && studio.edition == FirmwareEdition::Tc && device.temperatureControl ? 80 :
        studio.model == FirmwareModel::DragS ? 60 :
        (studio.model == FirmwareModel::DragX || studio.model == FirmwareModel::DragSPro) ? 80 :
        studio.model == FirmwareModel::Drag1 ? 157 : mini ? 117 :
        (legacy || largePanel) ? 177 : 100;
    const auto clamp = [&](int& value, int low, int high) {
        const int next = std::clamp(value, low, high);
        dirty |= next != value;
        value = next;
    };
    clamp(device.watts, 5, maxWatts);
    if (!largePanel && !plus && device.landscape) { device.landscape = false; dirty = true; }
    auto slider = [&](i18n::Str key, int& value, int low, int high) {
        clamp(value, low, high);
        ImGui::PushID(int(key));
        Ui::ControlRow(Tr(key));
        dirty |= ImGui::SliderInt("##Value", &value, low, high, "%d", ImGuiSliderFlags_AlwaysClamp);
        ImGui::PopID();
    };
    slider(i18n::Str::EmulatorWatts, device.watts, 5, maxWatts);
    if (!legacy) slider(i18n::Str::EmulatorPuffs, device.puffs, 0, 99999);
    slider(i18n::Str::EmulatorBattery, device.battery, 0, 5);
    slider(i18n::Str::EmulatorResistance, device.resistanceMilliOhms, 0, 9990);
    if (studio.model != FirmwareModel::Drag1)
        slider(i18n::Str::EmulatorDuration, device.timeCentiseconds, 0, 999);
    if (!legacy || refresh) checkbox("RBA", device.rba);
    checkbox(Tr(i18n::Str::EmulatorLocked), device.locked);
    if (studio.model == FirmwareModel::DragS) checkbox("P&A", device.activation);
    if (!legacy) checkbox(Tr(i18n::Str::EmulatorCharging), device.charging);
    const bool hasTemperature = legacy || studio.edition == FirmwareEdition::Tc;
    if (!hasTemperature && device.temperatureControl) { device.temperatureControl = false; dirty = true; }
    if (hasTemperature) {
        ImGui::BeginDisabled(selectedScreen != screens.end() && selectedScreen->temperatureMode >= 0);
        checkbox("TC", device.temperatureControl);
        ImGui::EndDisabled();
        if (device.temperatureControl || (selectedScreen != screens.end() && selectedScreen->temperatureMaterial)) {
            checkbox("°F", device.fahrenheit);
            slider(i18n::Str::LayoutTemperature, device.temperature,
                device.fahrenheit ? 200 : 100, device.fahrenheit ? 600 : 315);
        }
    }
    if (largePanel || plus) {
        if (studio.edition == FirmwareEdition::Tc && device.temperatureControl && device.landscape) {
            device.landscape = false;
            dirty = true;
        }
        ImGui::BeginDisabled((studio.edition == FirmwareEdition::Tc && device.temperatureControl) ||
            (selectedScreen != screens.end() && selectedScreen->orientation >= 0));
        checkbox(Tr(i18n::Str::EmulatorLandscape), device.landscape);
        ImGui::EndDisabled();
    }
    if (selectedScreen != screens.end()) {
        if (selectedScreen->menuItems) {
            int row = std::clamp(device.menuSelection, 0, static_cast<int>(selectedScreen->menuItems) - 1) + 1;
            slider(i18n::Str::EmulatorMenuItem, row, 1, static_cast<int>(selectedScreen->menuItems));
            dirty |= device.menuSelection != row - 1;
            device.menuSelection = row - 1;
        }
        if (selectedScreen->temperatureMaterial) {
            clamp(device.temperatureMaterial, 0, 2);
            Ui::ControlRow(Tr(i18n::Str::EmulatorMaterial));
            dirty |= ImGui::Combo("##Material", &device.temperatureMaterial, "Ni\0Ti\0SS\0");
        }
        if (selectedScreen->menuConfirmation)
            checkbox(Tr(i18n::Str::EmulatorMenuConfirmed), device.menuConfirmed);
    }
    if (studio.edition == FirmwareEdition::Score || refresh)
        slider(i18n::Str::EmulatorScore, device.score, 0, 99999);
    if (studio.edition == FirmwareEdition::Score)
        slider(i18n::Str::EmulatorScoreDisplay, device.scoreDisplay, 0, 9999);
    if (sx && device.screen == 18) {
        ImGui::PushID("NativeHistory");
        int day = std::clamp(device.historySelection, 0, 13) + 1;
        slider(i18n::Str::EmulatorHistoryDay, day, 1, 14);
        dirty |= device.historySelection != day - 1;
        device.historySelection = day - 1;
        slider(i18n::Str::EmulatorPuffs, device.puffHistory[day - 1], 0, 65535);
        ImGui::PopID();
    }
    if (sx && device.screen == 19) {
        slider(i18n::Str::EmulatorClockHour, device.clockHour, 0, 23);
        slider(i18n::Str::EmulatorClockMinute, device.clockMinute, 0, 59);
        clamp(device.menuSelection, 0, 1);
        const char* fields[] = {Tr(i18n::Str::EmulatorClockHour), Tr(i18n::Str::EmulatorClockMinute)};
        Ui::ControlRow(Tr(i18n::Str::EmulatorClockField));
        dirty |= ImGui::Combo("##ClockField", &device.menuSelection, fields, 2);
    }
    ImGui::EndTable();
    if (legacy && !refresh && (device.screen == 91 || device.screen == 92)) {
        ImGui::TextUnformatted(Tr(device.screen == 91 ?
            i18n::Str::EmulatorLoaderVersion : i18n::Str::EmulatorLoaderId));
        ImGui::SetNextItemWidth(-1);
        if (device.screen == 91) {
            const uint8_t low = 0, high = 9;
            dirty |= ImGui::DragScalarN("##VirtualLoaderVersion", ImGuiDataType_U8,
                device.loaderVersion.data(), 4, 1.0f, &low, &high, "%u", ImGuiSliderFlags_AlwaysClamp);
        } else {
            dirty |= ImGui::InputScalarN("##VirtualLoaderId", ImGuiDataType_U8, device.loaderId.data(),
                8, nullptr, nullptr, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
        }
    }
    return dirty;
}

void RenderTab(State& state, StudioState& studio, ID3D11Device* device, ID3D11DeviceContext* context) {
    state.activeTab = false;
    char title[128];
    snprintf(title, sizeof(title), "%s###TabEmulator", Tr(i18n::Str::TabFirmwareEmulator));
    if (!Ui::BeginTabItem(title)) {
        UiFirmwareDevice::PauseAndRelease(state.live);
        return;
    }
    state.activeTab = true;
    UiResourceStudio::CommitPatchDrafts(studio, true);
    if (state.documentIdentity != studio.DocumentIdentity()) {
        UiFirmwareDevice::PauseAndRelease(state.live);
        state.documentIdentity = studio.DocumentIdentity();
        state.presentation.reset();
        ReleaseTextures(state);
        state.device = FirmwareEmulator::DeviceState{};
        state.dirty = true;
    }
    if (ImGui::RadioButton(Tr(i18n::Str::EmulatorModeScreens), !state.wholeDevice)) state.wholeDevice = false;
    ImGui::SameLine();
    if (ImGui::RadioButton(Tr(i18n::Str::EmulatorModeDevice), state.wholeDevice)) state.wholeDevice = true;
    if (state.wholeDevice) {
        UiFirmwareDevice::Render(state.live, studio, device, context);
        ImGui::EndTabItem();
        return;
    }
    UiFirmwareDevice::PauseAndRelease(state.live);
    if (!FirmwareEmulator::Supports(studio)) {
        ReleaseTextures(state);
        ImGui::TextWrapped("%s", Tr(i18n::Str::EmulatorSupported));
        ImGui::EndTabItem();
        return;
    }
    ImGui::TextWrapped("%s", Tr(i18n::Str::EmulatorNativeNote));
    ImGui::BeginChild("DeviceControls", ImVec2(330, 0), true);
    ImGui::Text("%s / %s", FirmwareModelName(studio.model), FirmwareEditionName(studio.edition));
    const auto containsHardware = [&](uint16_t code) {
        return std::any_of(studio.firmware.records.begin(), studio.firmware.records.end(), [&](const auto& record) {
            return ((record.device_code[0] << 8) | record.device_code[1]) == code;
        });
    };
    if (state.device.hardwareCode && !containsHardware(state.device.hardwareCode)) {
        state.device.hardwareCode = 0;
        state.dirty = true;
    }
    char hardwareTitle[16];
    snprintf(hardwareTitle, sizeof(hardwareTitle), "%04X",
        state.device.hardwareCode ? state.device.hardwareCode : FirmwareCanonicalCode(studio.model));
    if (Ui::BeginControlTable("ScreenSelection")) {
    Ui::ControlRow(Tr(i18n::Str::LayoutRevision));
    if (ImGui::BeginCombo("##ScreenHardware", hardwareTitle)) {
        for (const auto& record : studio.firmware.records) {
            const auto code = static_cast<uint16_t>((record.device_code[0] << 8) | record.device_code[1]);
            char label[16];
            snprintf(label, sizeof(label), "%04X", code);
            if (ImGui::Selectable(label, code == (state.device.hardwareCode ?
                state.device.hardwareCode : FirmwareCanonicalCode(studio.model)))) {
                state.device.hardwareCode = code;
                state.dirty = true;
            }
        }
        ImGui::EndCombo();
    }
    const auto screens = FirmwareEmulator::AvailableScreens(studio, state.device.hardwareCode);
    if (std::none_of(screens.begin(), screens.end(),
        [&](const auto& screen) { return screen.id == state.device.screen; })) {
        state.device.screen = 0;
        state.dirty = true;
    }
    const auto selectedScreen = std::find_if(screens.begin(), screens.end(),
        [&](const auto& screen) { return screen.id == state.device.screen; });
    const char* selectedTitle = selectedScreen == screens.end() ?
        Tr(i18n::Str::EmulatorMainScreen) : ScreenTitle(*selectedScreen);
    Ui::ControlRow(Tr(i18n::Str::EmulatorScreen));
    if (ImGui::BeginCombo("##NativeScreen", selectedTitle)) {
        for (const auto& screen : screens) {
            ImGui::PushID(static_cast<int>(screen.id));
            if (ImGui::Selectable(ScreenTitle(screen), screen.id == state.device.screen)) {
                state.device.screen = screen.id;
                state.dirty = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Tr(i18n::Str::EmulatorScreen));
    ImGui::EndTable();
    }
    state.dirty |= DrawDeviceControls(state.device, studio);
    ImGui::Separator();
    if (Ui::BeginControlTable("ScreenPreviewParameters")) {
        Ui::ControlRow(Tr(i18n::Str::LabelZoom));
        Ui::ZoomSlider("##Zoom", state.zoom);
        ImGui::EndTable();
    }
    if (ImGui::Button(Tr(i18n::Str::EmulatorRedraw))) state.dirty = true;
    ImGui::EndChild();
    ImGui::SameLine();
    state.preview.Request(studio, state.device, state.dirty);
    state.preview.Poll();
    if (state.presentationHardware != state.device.hardwareCode || state.presentationScreen != state.device.screen) {
        state.presentation.reset();
        state.presentationHardware = state.device.hardwareCode;
        state.presentationScreen = state.device.screen;
        ReleaseTextures(state);
    }
    const auto publication = state.preview.Published();
    if (publication != state.presentation && (publication || !state.preview.Busy())) {
        state.presentation = publication;
        ++state.presentationGeneration;
    }
    state.dirty = false;
    UpdateTextures(state, device, context);
    ImGui::BeginChild("NativeDisplay", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::BeginDisabled(state.sideBySide);
    if (ImGui::RadioButton(Tr(i18n::Str::EmulatorBefore), state.showBefore)) state.showBefore = true;
    ImGui::SameLine();
    if (ImGui::RadioButton(Tr(i18n::Str::EmulatorAfter), !state.showBefore)) state.showBefore = false;
    ImGui::EndDisabled();
    ImGui::Checkbox(Tr(i18n::Str::EmulatorSideBySide), &state.sideBySide);
    if (state.preview.Busy()) { ImGui::SameLine(); ImGui::TextDisabled("ARM..."); }
    if (state.preview.Before().emulatedLoaderMetadata || state.preview.After().emulatedLoaderMetadata)
        ImGui::TextWrapped("%s", Tr(i18n::Str::EmulatorLoaderNote));
    if (state.sideBySide) {
        if (ImGui::BeginTable("NativeBeforeAfter", 2, ImGuiTableFlags_SizingStretchSame,
            ImVec2(0, ImGui::GetContentRegionAvail().y))) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::BeginChild("BeforeDisplay", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            DrawDisplay(state, true, studio);
            ImGui::EndChild();
            ImGui::TableNextColumn();
            ImGui::BeginChild("AfterDisplay", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            DrawDisplay(state, false, studio);
            ImGui::EndChild();
            ImGui::EndTable();
        }
    } else {
        DrawDisplay(state, state.showBefore, studio);
    }
    ImGui::EndChild();
    ImGui::EndTabItem();
}
}
