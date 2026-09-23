#pragma once

#if STUDIO_FLASHER

#include "app_state.h"
#include "voopoo_usb_flasher.h"
#include <windows.h>

namespace UiDeviceFlasher {

void RenderTab(StudioState& studio, voopoo::UsbFlasher& flasher, HWND hwnd, bool& requestOpenFlasherTab, bool smokeTest, int debugFrame);

} // namespace UiDeviceFlasher

#endif // STUDIO_FLASHER

