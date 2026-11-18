#pragma once
#include "firmware_preview.h"
#include "firmware_emulator_screens.h"
#include "texture_cache.h"
#include "ui_firmware_device.h"

namespace UiFirmwareEmulator {
struct State {
    FirmwarePreview::Cache preview;
    FirmwareEmulator::DeviceState device;
    Dx11Texture beforeTexture;
    Dx11Texture afterTexture;
    ID3D11Device* textureDevice = nullptr;
    ID3D11DeviceContext* textureContext = nullptr;
    uint64_t beforeGeneration = 0;
    uint64_t afterGeneration = 0;
    bool beforeTextureError = false;
    bool afterTextureError = false;
    bool showBefore = false;
    bool sideBySide = true;
    int zoom = StudioPreferences::Get().emulatorZoom;
    bool dirty = true;
    uint64_t documentIdentity = 0;
    bool activeTab = false;
    std::shared_ptr<const FirmwarePreview::Cache::Publication> presentation;
    uint64_t presentationGeneration = 0;
    uint16_t presentationHardware = 0;
    uint32_t presentationScreen = UINT32_MAX;
    UiFirmwareDevice::State live;
    bool wholeDevice = false;
};
const char* ScreenTitle(const FirmwareEmulator::ScreenInfo& screen);
bool DrawDeviceControls(FirmwareEmulator::DeviceState& device, const StudioState& studio);
void RenderTab(State& state, StudioState& studio, ID3D11Device* device, ID3D11DeviceContext* context);
}
