#pragma once
#include "firmware_catalog.h"
#include "zwdf_package.h"

// DRAG 2 draws into a 512-byte, 128x32 one-bit framebuffer at 0x200008a0.
// The page writer at 0x08008918 sends four 128-byte monochrome pages; there
// are no RGB565 palette operands. Color controls must not invent patch sites.
inline bool DetectDrag2FirmwareColors(const FirmwarePackage& fw,
                                      std::vector<PaletteState>& palette,
                                      Rgb8& background) {
    FirmwareModel model;
    if (!DetectFirmwareModel(fw, model) || model != FirmwareModel::Drag2) return false;
    palette.clear();
    background = {0, 0, 0};
    return true;
}

inline bool BuildDrag2ColorPatches(const FirmwarePackage& fw,
                                  const std::vector<PaletteState>& palette,
                                  Rgb8 background,
                                  std::vector<CodePatch>& out,
                                  std::string& error) {
    FirmwareModel model;
    if (!DetectFirmwareModel(fw, model) || model != FirmwareModel::Drag2) {
        error = "Unsupported DRAG 2 monochrome firmware layout";
        return false;
    }
    if (!palette.empty() || background != Rgb8(0, 0, 0)) {
        error = "DRAG 2 has a monochrome display, not an editable color palette";
        return false;
    }
    out.clear();
    return true;
}
