#pragma once
#include "firmware_menu_patch.h"
#include "texture_cache.h"
#include "studio_preferences.h"

struct StudioState;
namespace UiFirmwarePatches { struct State; }

namespace UiFirmwareMenu {
struct Preview {
    uint64_t documentIdentity = 0;
    uint16_t hardwareCode = 0;
    int zoom = StudioPreferences::Get().menuZoom;
    unsigned keyboardOwner = 0, mouseOwner = 0;
    bool keyboardHolding = false;
    unsigned keyboardPressed = 0;
    int lastInputFrame = -2;
    FirmwareMenu::PreviewState state, renderedState;
    FirmwareMenu::Settings observed, renderedSettings;
    FirmwareMenu::PreviewValues values, renderedValues;
    std::array<uint32_t, 80 * 160> pixels{};
    Dx11Texture texture;
    ID3D11Device* device = nullptr;
    bool initialized = false, rendered = false, textureError = false;
    bool hasAchievements = false, achievementsSupported = false;
    unsigned puffResult = 0; // Numeric/rank sample: 0 no result, 1 changed, 2 unchanged.
    unsigned heldKey = 0; // 1 Plus/up, 2 Minus/down, 3 Fire.
    double holdStarted = 0, holdLast = 0;
    bool holdCanceled = false;
};
void RenderTab(UiFirmwarePatches::State& patches, StudioState& studio,
    ID3D11Device* device, ID3D11DeviceContext* context);
}
