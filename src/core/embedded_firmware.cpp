#include "embedded_firmware.h"
#include <windows.h>
bool LoadEmbeddedFirmware(FirmwarePackage& package, std::string& error) {
    auto module = GetModuleHandleW(nullptr);
    auto resource = FindResourceW(module, MAKEINTRESOURCEW(101), MAKEINTRESOURCEW(10));
    auto handle = resource ? LoadResource(module, resource) : nullptr;
    auto bytes = handle ? static_cast<const uint8_t*>(LockResource(handle)) : nullptr;
    auto size = resource ? SizeofResource(module, resource) : 0;
    if (!bytes || !size) { error = "Embedded firmware resource missing"; return false; }
    return FirmwarePackage::Parse(std::vector<uint8_t>(bytes, bytes + size), package, error);
}
