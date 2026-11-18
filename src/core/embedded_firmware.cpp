#include "embedded_firmware.h"
#include "firmware_profiles.h"
#include <windows.h>
bool LoadEmbeddedFirmware(FirmwarePackage& package, std::string& error, FirmwareModel model, FirmwareEdition edition) {
    auto module = GetModuleHandleW(nullptr);
    if (model == FirmwareModel::Drag2) edition = FirmwareEdition::Standard;
    const auto* profile = FindFirmwareProfile(model, edition);
    if (!profile) { error = "Unknown embedded firmware profile"; return false; }
    auto resource = FindResourceW(module, MAKEINTRESOURCEW(profile->resourceId), MAKEINTRESOURCEW(10));
    auto handle = resource ? LoadResource(module, resource) : nullptr;
    auto bytes = handle ? static_cast<const uint8_t*>(LockResource(handle)) : nullptr;
    auto size = resource ? SizeofResource(module, resource) : 0;
    if (!bytes || !size) { error = "Embedded firmware resource missing"; return false; }
    FirmwarePackage loaded;
    if (!FirmwarePackage::Parse(std::vector<uint8_t>(bytes, bytes + size), loaded, error)) return false;
    FirmwareModel detectedModel;
    FirmwareEdition detectedEdition;
    if (!DetectFirmwareProfile(loaded, detectedModel, detectedEdition) ||
        detectedModel != model || detectedEdition != edition) {
        error = "Embedded firmware does not match the requested model and edition";
        return false;
    }
    package = std::move(loaded);
    return true;
}
