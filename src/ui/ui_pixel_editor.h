#pragma once

#include "app_state.h"
#include <d3d11.h>

namespace UiPixelEditor {

void RenderToolbar(StudioState& studio, size_t catIdx);
void RenderCanvas(StudioState& studio, size_t catIdx, ID3D11Device* device, ID3D11DeviceContext* context);

} // namespace UiPixelEditor

