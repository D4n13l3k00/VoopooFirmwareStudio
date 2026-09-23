#pragma once

#include "app_state.h"
#include <d3d11.h>

namespace UiResourceStudio {

void RenderTab(
    StudioState& studio,
    HWND hwnd,
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    bool usbConnected,
    bool isFlashing,
    bool& requestOpenFlasherTab
);

} // namespace UiResourceStudio

