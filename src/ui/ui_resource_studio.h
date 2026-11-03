#pragma once

#include "app_state.h"
#include <d3d11.h>

namespace UiResourceStudio {

bool UpdateReferenceTexture(StudioState& studio, size_t catIdx, ID3D11Device* device, ID3D11DeviceContext* context);

bool SaveFirmware(StudioState& studio, HWND hwnd, bool showSuccessModal = true);
bool ConfirmUnsavedChanges(StudioState& studio, HWND hwnd);
bool OpenFirmware(StudioState& studio, HWND hwnd, const std::string& path = {});
bool IsFileActionActive();

void RenderTab(
    StudioState& studio,
    HWND hwnd,
    ID3D11Device* device,
    ID3D11DeviceContext* context
);

} // namespace UiResourceStudio

