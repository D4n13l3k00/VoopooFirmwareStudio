#include "firmware_layout_legacy_private.h"
#include "firmware_catalog.h"

namespace FirmwareLayout {
bool DiscoverLegacy(const FirmwarePackage& package, std::vector<Element>& elements, std::string& error) {
    FirmwareModel model;
    FirmwareEdition edition;
    if (!DetectFirmwareProfile(package, model, edition) ||
        (model != FirmwareModel::Drag1 && model != FirmwareModel::DragMini &&
            model != FirmwareModel::DragMiniRefresh)) {
        elements.clear();
        error = "Unsupported legacy OLED native layout profile";
        return false;
    }
    return LegacyLayout::DiscoverRelease(package, elements, error);
}
}
