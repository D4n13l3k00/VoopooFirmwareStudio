#pragma once

#include <windows.h>
#include <d3d11.h>

struct StudioState;

namespace UiStartup {

// Call each workspace frame, including while the chooser is fading closed.
void Render(StudioState& studio, HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
// Call before destroying the D3D device.
void Release();

} // namespace UiStartup
