#pragma once
#include "firmware_device_worker.h"
#include "texture_cache.h"
#include "studio_preferences.h"

struct StudioState;
namespace UiFirmwareDevice {
struct State {
    FirmwareDevice::Worker worker;
    FirmwareDevice::Inputs inputs;
    FirmwareDevice::Inputs sentInputs;
    Dx11Texture texture;
    ID3D11Device* textureDevice = nullptr;
    uint64_t textureGeneration = UINT64_MAX;
    uint64_t loadedDocument = 0;
    uint64_t loadedRevision = 0;
    bool loadedMenuEnabled = false;
    uint16_t hardwareCode = 0x3103;
    int zoom = StudioPreferences::Get().deviceZoom;
    bool wantsRunning = false;
    uint32_t buttonKeyboardOwner = 0;
    uint32_t buttonMouseOwner = 0;
};
void PauseAndRelease(State& state);
void RenderButtonControls(State& state);
void RenderDiagnostics(const FirmwareDevice::Snapshot& snapshot);
void Render(State& state, const StudioState& studio,
    ID3D11Device* device, ID3D11DeviceContext* context);
}
