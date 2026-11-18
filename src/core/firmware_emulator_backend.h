#pragma once
#include "firmware_emulator.h"

namespace FirmwareEmulator {
class Backend {
public:
    virtual ~Backend() = default;
    virtual bool Render(const StudioState&, const DeviceState&, Frame&) = 0;
};
std::unique_ptr<Backend> CreateLegacyBackend();
std::unique_ptr<Backend> CreateDrag3Backend();
std::unique_ptr<Backend> CreateProPlusBackend();
}
