#pragma once
#include "app_state.h"
#include "embedded_firmware.h"
#include "firmware_emulator.h"
#include <ostream>

inline bool RunFlashUsageRegressions(std::string& error, std::ostream* log = nullptr) {
    size_t records = 0;
    for (const auto edition : {FirmwareEdition::Score, FirmwareEdition::Factory}) {
        FirmwarePackage package;
        StudioState state;
        if (!LoadEmbeddedFirmware(package, error, FirmwareModel::DragS, edition) ||
            !state.LoadPackage(std::move(package), "flash.zwdf", error)) return false;
        FirmwareLogo::Patches empty{}, screens{};
        for (size_t i = 0; i < screens.size(); ++i) {
            screens[i].enabled = true;
            screens[i].pixels.assign(size_t(FirmwareLogo::Width) * FirmwareLogo::Height * 2, uint8_t(i * 37));
        }
        std::vector<FirmwareLogo::FlashUsage> baseline, added, removed, unchanged;
        if (!FirmwareLogo::MeasureFlash(state.firmware, empty, baseline, error) ||
            !FirmwareLogo::MeasureFlashChanges(state.firmware, empty, screens, added, error) ||
            !FirmwareLogo::MeasureFlashChanges(state.firmware, screens, empty, removed, error) ||
            !FirmwareLogo::MeasureFlashChanges(state.firmware, screens, screens, unchanged, error)) return false;
        std::vector<uint8_t> bytes;
        BuildReport report;
        if (!FirmwareLogo::Build(state.firmware, screens, {}, {}, bytes, report, error)) return false;
        FirmwarePackage exported;
        if (!FirmwarePackage::Parse(bytes, exported, error)) return false;
        for (size_t r = 0; r < added.size(); ++r) {
            int64_t addSum = 0, removeSum = 0;
            for (size_t target = 0; target < screens.size(); ++target) {
                addSum += added[r].screenDelta[target];
                removeSum += removed[r].screenDelta[target];
                if (unchanged[r].screenDelta[target] != 0) {
                    error = "Unchanged screen reported nonzero Flash delta"; return false;
                }
            }
            const int64_t actual = int64_t(exported.records[r].image.size()) - int64_t(baseline[r].Used());
            if (added[r].Used() != exported.records[r].image.size() || addSum != actual ||
                addSum <= 0 || removeSum != -actual || removed[r].Used() != baseline[r].Used()) {
                error = "Signed screen contributions disagree with exported Flash sizes"; return false;
            }
            ++records;
        }
        // Different screens share most rows, including raw (>256 colors) and
        // indexed art whose per-screen palettes are not identical.
        for (bool indexed : {false, true}) {
            FirmwareLogo::Patches shared{};
            for (size_t target = 0; target < shared.size(); ++target) {
                auto& screen = shared[target];
                screen.enabled = true;
                screen.pixels.resize(80 * 160 * 2);
                for (int y = 0; y < 160; ++y)
                    for (int x = 0; x < 80; ++x) {
                        uint16_t color = indexed ? uint16_t(0x8001 | ((x + y) & 7) << 5) :
                            uint16_t(x * 619 + y * 233);
                        if (y == int(target) * 3) color ^= uint16_t(indexed ? (uint64_t{1} << (target + 1)) : (target + 1) * 73);
                        const size_t at = size_t(y * 80 + x) * 2;
                        screen.pixels[at] = uint8_t(color);
                        screen.pixels[at + 1] = uint8_t(color >> 8);
                    }
            }
            std::vector<FirmwareLogo::FlashUsage> pooled;
            if (!FirmwareLogo::MeasureFlash(state.firmware, shared, pooled, error) ||
                !FirmwareLogo::Build(state.firmware, shared, {}, {}, bytes, report, error) ||
                !FirmwarePackage::Parse(bytes, exported, error)) return false;
            auto canonical = exported;
            FirmwareLogo::Patches decoded;
            if (!FirmwareLogo::Canonicalize(canonical, decoded, error)) return false;
            if (decoded != shared) { error = "Shared rows changed screen pixels"; return false; }
            const auto& image = exported.records.front().image;
            const size_t header = state.firmware.records.front().image.size();
            auto word = [&](size_t at) {
                return uint32_t(image[at]) | uint32_t(image[at + 1]) << 8 |
                    uint32_t(image[at + 2]) << 16 | uint32_t(image[at + 3]) << 24;
            };
            const size_t overhead = 112 + word(header + 32) + 4 * 44;
            size_t independent = overhead;
            for (size_t target = 0; target < shared.size(); ++target) {
                FirmwareLogo::Patches single{};
                single[target] = shared[target];
                std::vector<FirmwareLogo::FlashUsage> usage;
                if (!FirmwareLogo::MeasureFlash(state.firmware, single, usage, error)) return false;
                independent += usage.front().patchBytes - overhead;
            }
            if (pooled.front().patchBytes >= independent) {
                error = "Shared rows failed to reduce independent screen Flash"; return false;
            }
            state.screenPatches = shared;
            FirmwareEmulator::Renderer renderer;
            for (size_t target = 0; target < shared.size(); ++target) {
                FirmwareEmulator::DeviceState device;
                device.hardwareCode = pooled.front().hardwareCode;
                device.screen = target == 0 ? 1 : target == 1 ? 3 : 15;
                device.logoPhase = 0;
                FirmwareEmulator::Frame frame;
                if (!renderer.Render(state, device, frame)) { error = frame.error; return false; }
                if (frame.rgba.size() != 80 * 160 || !frame.spiBytes || !frame.pixelWrites) {
                    error = "Shared-row native LCD render missing"; return false;
                }
                for (size_t pixel = 0; pixel < frame.rgba.size(); ++pixel) {
                    const auto& source = shared[target].pixels;
                    const uint16_t color = uint16_t(source[pixel * 2]) | uint16_t(source[pixel * 2 + 1]) << 8;
                    const uint32_t r = color >> 11, g = (color >> 5) & 63, b = color & 31;
                    const uint32_t expected = (r << 3 | r >> 2) | (g << 2 | g >> 4) << 8 |
                        (b << 3 | b >> 2) << 16 | 0xff000000u;
                    if (frame.rgba[pixel] != expected) {
                        error = "Shared-row native decoding changed a pixel"; return false;
                    }
                }
            }
            if (log) *log << "SHARED ROWS " << (indexed ? "indexed" : "raw") <<
                " independent=" << independent << " pooled=" << pooled.front().patchBytes << std::endl;
            if (!indexed) {
                auto reduced = shared;
                for (auto& screen : reduced) {
                    std::vector<uint32_t> pixels;
                    if (!FirmwareBitmap::DecodeRgb565(screen.pixels.data(), screen.pixels.size(), 80, 160, pixels))
                        return false;
                    FirmwareBitmap::ImageAdjustments adjustments;
                    adjustments.paletteColors = 16;
                    FirmwareBitmap::ApplyAdjustments(pixels, 80, 160, adjustments);
                    if (!FirmwareBitmap::EncodeRgb565(pixels.data(), 80, 160, screen.pixels)) return false;
                }
                std::vector<FirmwareLogo::FlashUsage> reducedUsage;
                if (!FirmwareLogo::MeasureFlash(state.firmware, reduced, reducedUsage, error)) return false;
                if (reducedUsage.front().patchBytes >= pooled.front().patchBytes) {
                    error = "Palette reduction failed to reduce photograph Flash"; return false;
                }
                if (log) *log << "PALETTE 16 before=" << pooled.front().patchBytes <<
                    " after=" << reducedUsage.front().patchBytes << std::endl;
            }
        }
    }
    if (records != 16) { error = "Flash delta coverage missed DRAG S records"; return false; }
    if (log) *log << "FLASH DELTA REGRESSIONS OK records=" << records << std::endl;
    return true;
}
