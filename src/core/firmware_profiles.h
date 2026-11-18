#pragma once
#include "firmware_catalog.h"

struct FirmwareProfile {
    FirmwareModel model;
    FirmwareEdition edition;
    const char* filename;
    int resourceId;
};

inline const std::vector<FirmwareProfile>& FirmwareProfiles() {
    static const std::vector<FirmwareProfile> profiles = {
        {FirmwareModel::DragS, FirmwareEdition::Score, "A050 score version.zwdf", 101},
        {FirmwareModel::DragS, FirmwareEdition::Factory, "A050 Factory version.zwdf", 104},
        {FirmwareModel::DragX, FirmwareEdition::Score, "E050 score version.zwdf", 102},
        {FirmwareModel::DragX, FirmwareEdition::Factory, "DRAG X Factory version.zwdf", 105},
        {FirmwareModel::Drag2, FirmwareEdition::Standard, "DRAG2_V1.0.0.1.xwdf", 103},
        {FirmwareModel::DragSPro, FirmwareEdition::Factory, "DRAG S PRO -FC.swdf", 106},
        {FirmwareModel::DragSPro, FirmwareEdition::Tc, "DRAG S PRO -TC.zwdf", 107},
        {FirmwareModel::DragXPro, FirmwareEdition::Tc, "DRAG X PRO-TC.zwdf", 108},
        {FirmwareModel::DragXPlus, FirmwareEdition::Factory, "DRAG X PLUS Factory Version.zwdf", 109},
        {FirmwareModel::DragXPlus, FirmwareEdition::Tc, "TC Version.zwdf", 110},
        {FirmwareModel::DragXPlusPro, FirmwareEdition::Tc, "DRAG X PLUS-TC.zwdf", 111},
        {FirmwareModel::Drag3, FirmwareEdition::Factory, "DRAG 3 Factory ver.zwdf", 112},
        {FirmwareModel::Drag3, FirmwareEdition::Tc, "Drag 3 (X051)-TC.zwdf", 113},
        {FirmwareModel::DragMax, FirmwareEdition::Standard, "DRAG MAX.swdf", 114},
        {FirmwareModel::Drag1, FirmwareEdition::Standard, "DRAG_V1.0.0.1.xwdf", 115},
        {FirmwareModel::DragMini, FirmwareEdition::V1, "DRAG Mini_V1.0.0.1.xwdf", 116},
        {FirmwareModel::DragMini, FirmwareEdition::V2, "DRAG Mini_V1.0.0.2.xwdf", 117},
        {FirmwareModel::Drag2Refresh, FirmwareEdition::Standard, "Drag 2 PnP Refresh Edition.swdf", 118},
        {FirmwareModel::Drag2Refresh, FirmwareEdition::Fit3, "DRAG 2-FIT3.swdf", 119},
        {FirmwareModel::DragMiniRefresh, FirmwareEdition::Standard, "Drag mini PnP Refresh Edition.swdf", 120},
        {FirmwareModel::DragMiniRefresh, FirmwareEdition::Fit3, "DRAG MINI-FIT3.swdf", 121},
    };
    return profiles;
}

inline const FirmwareProfile* FindFirmwareProfile(FirmwareModel model, FirmwareEdition edition) {
    for (const auto& profile : FirmwareProfiles())
        if (profile.model == model && profile.edition == edition) return &profile;
    return nullptr;
}
