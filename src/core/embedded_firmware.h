#pragma once
#include "zwdf_package.h"
#include "firmware_catalog.h"
bool LoadEmbeddedFirmware(FirmwarePackage& package, std::string& error,
    FirmwareModel model = FirmwareModel::DragS, FirmwareEdition edition = FirmwareEdition::Score);
