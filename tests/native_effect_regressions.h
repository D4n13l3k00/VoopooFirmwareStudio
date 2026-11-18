#pragma once
#include "app_state.h"
#include "firmware_emulator.h"
#include "firmware_project.h"
#include "firmware_logo_preview.h"
#include <algorithm>
#include <filesystem>
#include <ostream>
#include <sstream>
#include <psapi.h>

// Expected pixels are independent of the firmware executor. Renderer executes
// the exported Thumb decoder, original LCD driver and real SPI framebuffer.
inline bool RunNativeEffectShard(std::string& error, std::ostream* proof,
    FirmwareLogo::Animation onlyEffect, int onlyEdition) {
    using FirmwareLogo::Animation;
    error.clear();
    auto fail = [&](const std::string& message) { error = message; return false; };
    std::error_code ec;
    auto root = std::filesystem::temp_directory_path(ec);
    if (ec) return fail("Native effects temporary directory unavailable");
    root /= L"Voopoo_native_effects_" + std::to_wstring(GetCurrentProcessId()) +
        L"_" + std::to_wstring(GetTickCount64());
    if (!std::filesystem::create_directory(root, ec) || ec)
        return fail("Native effects temporary directory creation failed");
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    } cleanup{root};
    constexpr Animation effects[] = {Animation::SoftWipe, Animation::SlideRight,
        Animation::Checker, Animation::Diagonal, Animation::SlideLeft, Animation::SlideUp, Animation::SlideDown};
    static_assert(static_cast<uint32_t>(Animation::Dissolve) == 9 &&
        static_cast<uint32_t>(Animation::SoftWipe) == 10 &&
        static_cast<uint32_t>(Animation::SlideRight) == 11 &&
        static_cast<uint32_t>(Animation::Diagonal) == 13 &&
        static_cast<uint32_t>(Animation::SlideLeft) == 14 &&
        static_cast<uint32_t>(Animation::SlideUp) == 15 &&
        static_cast<uint32_t>(Animation::SlideDown) == 16);
    auto rgba = [](uint16_t color) {
        const uint32_t r = color >> 11, g = (color >> 5) & 63, b = color & 31;
        return (r << 3 | r >> 2) | (g << 2 | g >> 4) << 8 |
            (b << 3 | b >> 2) << 16 | 0xff000000u;
    };
    auto colorAt = [](Animation effect, int x, int y) -> uint16_t {
        // Exercise indexed, raw and run-length source rows, not only flat art.
        if (effect == Animation::SoftWipe) {
            constexpr uint16_t colors[] = {0xffff, 0xf81f, 0x07ff, 0xffe0};
            return colors[(x + y) & 3];
        }
        if (effect == Animation::SlideLeft || effect == Animation::SlideUp || effect == Animation::SlideDown) {
            if (y < 40) {
                constexpr uint16_t colors[] = {0xffff, 0xf81f, 0x07ff, 0xffe0};
                return colors[(x + y) & 3];
            }
            if (y < 80) return x < 40 ? 0xffff : 0xf81f;
            return uint16_t(((x + y) % 31 + 1) << 11 | ((x * 3 + y) & 63) << 5 | ((x + y * 7) & 31));
        }
        if (effect == Animation::SlideRight)
            return uint16_t(((x + y) % 31 + 1) << 11 | ((x * 3 + y) & 63) << 5 | ((x + y * 7) & 31));
        if (effect == Animation::Checker) return x < 40 ? 0xffff : 0xf81f;
        return uint16_t(0x8001 | ((x + y * 13) & 255) << 3);
    };
    auto expected = [&](Animation effect, int x, int y, int visibility) {
        if (!visibility) return rgba(0);
        uint16_t color = colorAt(effect, x, y);
        switch (effect) {
        case Animation::SoftWipe: {
            const int alpha = (std::clamp)(11 * visibility - y, 0, 16);
            color = uint16_t((((color >> 11) * alpha) >> 4) << 11 |
                (((((color >> 5) & 63) * alpha) >> 4) << 5) |
                (((color & 31) * alpha) >> 4));
            break;
        }
        case Animation::SlideRight: {
            const int sourceX = x - (16 - visibility) * 5;
            color = sourceX < 0 ? 0 : colorAt(effect, sourceX, y);
            break;
        }
        case Animation::SlideLeft: {
            const int sourceX = x + (16 - visibility) * 5;
            color = sourceX >= 80 ? 0 : colorAt(effect, sourceX, y);
            break;
        }
        case Animation::SlideUp:
        case Animation::SlideDown: {
            const int shift = (16 - visibility) * 10;
            const int sourceY = effect == Animation::SlideUp ? y + shift : y - shift;
            color = sourceY < 0 || sourceY >= 160 ? 0 : colorAt(effect, x, sourceY);
            break;
        }
        case Animation::Checker:
            if (((x / 10 + y / 20) & 1) * 8 + (x % 10) * 8 / 10 >= visibility) color = 0;
            break;
        case Animation::Diagonal:
            if (x * 2 + y >= visibility * 20) color = 0;
            break;
        default: break;
        }
        return rgba(color);
    };
    FirmwareEmulator::Renderer renderer;
    size_t records = 0, frames = 0, projects = 0;
    for (const auto edition : {FirmwareEdition::Score, FirmwareEdition::Factory}) {
        if (onlyEdition >= 0 && int(edition) != onlyEdition) continue;
        FirmwarePackage package;
        StudioState studio;
        if (!LoadEmbeddedFirmware(package, error, FirmwareModel::DragS, edition) ||
            !studio.LoadPackage(std::move(package), "native-effects.zwdf", error)) return false;
        records += studio.firmware.records.size();
        FirmwareLogo::EditableSource original;
        original.width = 93; original.height = 181;
        original.rgba = std::make_shared<const std::vector<uint8_t>>(size_t(93 * 181 * 4), 0x7d);
        original.fitMode = 2; original.scalePercent = 117.5f;
        original.x = -9; original.y = -21; original.alignX = original.alignY = -1;
        original.adjustments.brightness = 12.5f;
        original.adjustments.contrast = -8;
        original.adjustments.gamma = 1.2f;
        original.adjustments.saturation = 14;
        original.adjustments.sharpness = 25;
        original.adjustments.curve = {0.2f, 0.55f, 0.8f};
        original.adjustments.paletteColors = 64;
        for (size_t target = 0; target < FirmwareLogo::TargetCount; ++target)
            studio.SetScreenPatchSource(static_cast<FirmwareLogo::Target>(target), original);
        for (Animation effect : effects) {
            if (onlyEffect != Animation::None && effect != onlyEffect) continue;
            FirmwareLogo::Settings settings;
            settings.enabled = true;
            settings.animation = settings.exitAnimation = effect;
            settings.durationMs = settings.exitDurationMs = 200;
            settings.holdMs = 25;
            settings.pixels.resize(size_t(FirmwareLogo::Width) * FirmwareLogo::Height * 2);
            for (int y = 0; y < int(FirmwareLogo::Height); ++y)
                for (int x = 0; x < int(FirmwareLogo::Width); ++x) {
                    const uint16_t color = colorAt(effect, x, y);
                    const size_t at = (size_t(y) * FirmwareLogo::Width + x) * 2;
                    settings.pixels[at] = uint8_t(color);
                    settings.pixels[at + 1] = uint8_t(color >> 8);
                }
            for (size_t target = 0; target < FirmwareLogo::TargetCount; ++target)
                studio.SetScreenPatch(static_cast<FirmwareLogo::Target>(target), settings);
            if (studio.screenPatches[0] != settings || FirmwareLogo::FrameCount(settings) != 32)
                return fail("New native effect rejected by document validation");
            std::vector<uint8_t> bytes;
            std::vector<FirmwareLogo::FlashUsage> usage;
            FirmwarePackage exported;
            if (!studio.BuildToBuffer(bytes, error) || !FirmwarePackage::Parse(bytes, exported, error) ||
                !FirmwareLogo::MeasureFlash(studio.firmware, studio.screenPatches, usage, error)) return false;
            if (usage.size() != exported.records.size()) return fail("Native effect Flash inventory mismatch");
            for (size_t r = 0; r < usage.size(); ++r) {
                if (usage[r].Used() != exported.records[r].image.size() ||
                    usage[r].Used() > usage[r].capacityBytes ||
                    exported.records[r].linked_base + exported.records[r].image.size() >= 0x0801f000)
                    return fail("Expanded native executor escaped Flash accounting/protected boundary");
                for (size_t target = 0; target < FirmwareLogo::TargetCount; ++target)
                    if (!FirmwareLogo::FrameBoundary(exported.records[r], static_cast<FirmwareLogo::Target>(target)))
                        return fail("Expanded native executor failed strict descriptor validation");
            }
            auto canonical = exported;
            FirmwareLogo::Patches decoded;
            if (!FirmwareLogo::Canonicalize(canonical, decoded, error)) return false;
            if (decoded != studio.screenPatches || !FirmwareLogo::Supported(canonical))
                return fail("New effect export/import lost animation IDs or original hooks");
            // Even a repaired checksum must not authorize different guest code.
            auto damaged = exported;
            auto& damagedImage = damaged.records.front().image;
            const size_t header = usage.front().firmwareBytes;
            auto set32 = [&](size_t at, uint32_t value) {
                for (unsigned i = 0; i < 4; ++i) damagedImage[header + at + i] = uint8_t(value >> (8 * i));
            };
            if (damagedImage[header + 8] != 1 || damagedImage[header + 9] != 0 ||
                damagedImage[header + 10] != 0 || damagedImage[header + 11] != 0)
                return fail("Native screen export must use public envelope version 1");
            std::string rejected;
            set32(8, 2);
            if (FirmwareLogo::Canonicalize(damaged, decoded, rejected) || rejected.empty())
                return fail("Unsupported native screen envelope version was accepted");
            set32(8, 1);
            damagedImage[header + 112 + 64] ^= 1;
            set32(40, FirmwarePackage::Crc16Ccitt(damagedImage.data() + header + 112,
                damagedImage.size() - header - 112));
            const auto digest = FirmwarePackage::ComputeSha256(damagedImage.data() + header + 112,
                damagedImage.size() - header - 112);
            std::copy(digest.begin(), digest.end(), damagedImage.begin() + header + 48);
            rejected.clear();
            if (FirmwareLogo::Canonicalize(damaged, decoded, rejected) || rejected.empty())
                return fail("Checksum-repaired foreign native executor bypassed exact-code guard");
            // Projects keep full immutable originals, placement and image adjustments.
            const auto projectPath = PathUtils::WideToUtf8((root / L"effects.vfwp").wstring());
            const auto saved = FirmwareProject::Save(studio, projectPath);
            if (!saved.saved) return fail(saved.error);
            StudioState reopened;
            if (!FirmwareProject::Open(reopened, projectPath, error)) return false;
            std::vector<uint8_t> reopenedBytes;
            if (!reopened.BuildToBuffer(reopenedBytes, error)) return false;
            if (reopened.screenPatches != studio.screenPatches ||
                reopened.screenPatchSources != studio.screenPatchSources ||
                reopenedBytes != bytes || !reopened.screenPatchSources[0].rgba ||
                *reopened.screenPatchSources[0].rgba != *original.rgba)
                return fail("Native effect project lost originals, placement, adjustments or firmware bytes");
            ++projects;
            auto renderPhase = [&](uint16_t code, uint32_t screen, int phase) {
                FirmwareEmulator::DeviceState device;
                device.hardwareCode = code; device.screen = screen; device.logoPhase = phase;
                FirmwareEmulator::Frame frame;
                const auto context = std::string(FirmwareEditionName(edition)) + " / HW " + std::to_string(code) +
                    " / effect " + std::to_string(static_cast<uint32_t>(effect)) +
                    " / screen " + std::to_string(screen) + " / phase " + std::to_string(phase);
                if (!renderer.Render(studio, device, frame)) return fail(context + ": " + frame.error);
                if (frame.width != 80 || frame.height != 160 || frame.rgba.size() != 80 * 160 ||
                    frame.hardwareCode != code || !frame.spiBytes || !frame.pixelWrites)
                    return fail(context + ": native LCD execution missing");
                std::vector<uint32_t> direct;
                FirmwareLogo::PreviewFrame(settings, uint32_t(phase), direct);
                if (direct != frame.rgba)
                    return fail(context + ": direct preview differs from native Thumb/LCD frame");
                const int visibility = phase < 16 ? phase + 1 : 31 - phase;
                for (int y = 0; y < 160; ++y)
                    for (int x = 0; x < 80; ++x)
                        if (frame.rgba[size_t(y) * 80 + x] != expected(effect, x, y, visibility))
                            return fail(context + ": framebuffer differs at " + std::to_string(x) + "," + std::to_string(y));
                ++frames;
                return true;
            };
            for (const auto& record : studio.firmware.records) {
                const auto code = uint16_t(uint16_t(record.device_code[0]) << 8 | record.device_code[1]);
                // Exit 16/23/30 must be the exact reverse of entrance 14/7/0.
                for (int phase : {0, 7, 14, 15, 16, 23, 30, 31})
                    if (!renderPhase(code, 1, phase)) return false;
                for (uint32_t screen : {3u, 15u})
                    for (int phase : {7, 31})
                        if (!renderPhase(code, screen, phase)) return false;
            }
            settings.bootStages = FirmwareLogo::BootStages::First;
            studio.SetScreenPatch(FirmwareLogo::Target::Boot, settings);
            for (const auto& record : studio.firmware.records) {
                const auto code = uint16_t(uint16_t(record.device_code[0]) << 8 | record.device_code[1]);
                for (int phase : {7, 31})
                    if (!renderPhase(code, 2, phase)) return false;
            }
        }
    }
    const size_t expectedRecords = onlyEdition < 0 ? 16 :
        (onlyEdition == int(FirmwareEdition::Score) ? 7 : 9);
    if (records != expectedRecords) return fail("Native effect regressions missed DRAG S hardware records");
    if (proof) *proof << "Native effects: " << records << " records, " <<
        (onlyEffect == Animation::None ? std::size(effects) : 1) << " effects, " << frames <<
        " actual Thumb/LCD framebuffer phases, four hook entries, reversed exits, protected Flash, " <<
        projects << " original-preserving project round trips\n";
    return true;
}

// Full integer RGB565 parity matrix, including every entrance and exit phase.
inline bool RunNativePreviewParity(std::string& error, std::ostream* proof,
    uint32_t firstEffect, uint32_t endEffect, int onlyEdition) {
    error.clear();
    size_t records = 0, frames = 0;
    struct Observation {
        const FirmwareLogo::Settings& settings;
        std::string& error;
        size_t& frames;
        std::string context;
        uint16_t hardwareCode;
        uint32_t nextPhase = 0;
        uint32_t phaseCount;
        bool valid = true;
        std::vector<uint32_t> direct;
    };
    const FirmwareEmulator::FrameObserver observer = [](uint32_t phase,
        const FirmwareEmulator::Frame& native, void* context) {
        auto& observed = *static_cast<Observation*>(context);
        if (!observed.valid) return;
        auto fail = [&](const std::string& message) {
            observed.error = observed.context + " / phase " + std::to_string(phase) + ": " + message;
            observed.valid = false;
        };
        if (phase != observed.nextPhase || phase >= observed.phaseCount) {
            fail("Native frame ordinal discontinuity; expected " + std::to_string(observed.nextPhase));
            return;
        }
        if (native.width != 80 || native.height != 160 || native.rgba.size() != 80 * 160 ||
            native.hardwareCode != observed.hardwareCode || !native.spiBytes || !native.pixelWrites) {
            fail("Native LCD execution missing");
            return;
        }
        FirmwareLogo::PreviewFrame(observed.settings, phase, observed.direct);
        if (observed.direct != native.rgba) {
            fail("Direct preview differs from native Thumb/LCD frame");
            return;
        }
        ++observed.nextPhase;
        ++observed.frames;
    };
    for (const auto edition : {FirmwareEdition::Score, FirmwareEdition::Factory}) {
        if (onlyEdition >= 0 && onlyEdition != int(edition)) continue;
        FirmwarePackage package;
        StudioState studio;
        if (!LoadEmbeddedFirmware(package, error, FirmwareModel::DragS, edition) ||
            !studio.LoadPackage(std::move(package), "native-preview.zwdf", error)) return false;
        FirmwareLogo::Settings settings;
        settings.enabled = true;
        settings.durationMs = settings.exitDurationMs = 200;
        settings.holdMs = 15;
        settings.pixels.resize(80 * 160 * 2);
        for (int y = 0; y < 160; ++y)
            for (int x = 0; x < 80; ++x) {
                const auto color = uint16_t(((x + y) % 31 + 1) << 11 |
                    ((x * 3 + y) & 63) << 5 | ((x + y * 7) & 31));
                const size_t at = size_t(y * 80 + x) * 2;
                settings.pixels[at] = uint8_t(color);
                settings.pixels[at + 1] = uint8_t(color >> 8);
            }
        FirmwareEmulator::Renderer renderer;
        records += studio.firmware.records.size();
        for (uint32_t id = firstEffect; id < endEffect; ++id) {
            settings.animation = settings.exitAnimation = static_cast<FirmwareLogo::Animation>(id);
            for (size_t target = 0; target < FirmwareLogo::TargetCount; ++target)
                studio.SetScreenPatch(static_cast<FirmwareLogo::Target>(target), settings);
            for (const auto& record : studio.firmware.records) {
                FirmwareEmulator::DeviceState device;
                device.hardwareCode = uint16_t(uint16_t(record.device_code[0]) << 8 | record.device_code[1]);
                const size_t recordFrames = frames;
                for (uint32_t screen : {1u, 2u, 3u, 15u}) {
                    // Screen 1 is the second boot hook; screen 2 is the first.
                    // Select the hook under test rather than comparing stock
                    // VOOPOO art with a patch installed only in the other stage.
                    settings.bootStages = screen == 2 ? FirmwareLogo::BootStages::First : FirmwareLogo::BootStages::Second;
                    studio.SetScreenPatch(FirmwareLogo::Target::Boot, settings);
                    device.screen = screen;
                    std::ostringstream context;
                    context << FirmwareEditionName(edition) << " / HW " << std::hex << device.hardwareCode
                        << std::dec << " / effect " << id << " / screen " << screen;
                    Observation observed{settings, error, frames, context.str(), device.hardwareCode,
                        0, id == 0 ? 1u : 32u};
                    auto renderThrough = [&](int phase, uint32_t expectedNextPhase) {
                        device.logoPhase = phase;
                        FirmwareEmulator::Frame native;
                        const bool rendered = id == 0 ? renderer.Render(studio, device, native) :
                            renderer.Render(studio, device, native, observer, &observed);
                        if (!observed.valid) return false;
                        if (!rendered) {
                            error = observed.context + " / phase " + std::to_string(phase) + ": " + native.error;
                            return false;
                        }
                        // None has no timed native boundaries and remains a single-frame check.
                        if (id == 0) observer(0, native, &observed);
                        if (!observed.valid) return false;
                        if (observed.nextPhase != expectedNextPhase) {
                            error = observed.context + " / phase " + std::to_string(observed.nextPhase) +
                                ": Native frame observer missed completed phases; expected next ordinal " +
                                std::to_string(expectedNextPhase);
                            return false;
                        }
                        return true;
                    };
                    if (id == 0) {
                        if (!renderThrough(0, 1)) return false;
                    } else if (screen == 15) {
                        // CHIPID entrance returns to the native information screen;
                        // phases 16..31 must execute its real InfoExit entry separately.
                        if (!renderThrough(15, 16) || !renderThrough(31, 32)) return false;
                    } else if (!renderThrough(31, 32)) return false;
                }
                PROCESS_MEMORY_COUNTERS_EX memory{};
                MEMORYSTATUSEX system{sizeof(MEMORYSTATUSEX)};
                if (!GetProcessMemoryInfo(GetCurrentProcess(),
                        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) ||
                    !GlobalMemoryStatusEx(&system)) {
                    error = "Cannot measure native preview memory"; return false;
                }
                if (proof) *proof << "Native direct-preview progress: " << FirmwareEditionName(edition)
                    << " / HW " << device.hardwareCode << " / effect " << id
                    << ", four hook entries complete, " << frames - recordFrames
                    << " LCD frames (" << frames << " total), privateMiB=" << memory.PrivateUsage / (1024 * 1024)
                    << ", peakMiB=" << memory.PeakPagefileUsage / (1024 * 1024)
                    << ", availableCommitMiB=" << system.ullAvailPageFile / (1024 * 1024) << '\n' << std::flush;
                if (memory.PrivateUsage > size_t(512) * 1024 * 1024) {
                    error = "Native preview retains over 512 MiB at effect " + std::to_string(id) +
                        ", hardware " + std::to_string(device.hardwareCode);
                    return false;
                }
            }
        }
    }
    const size_t expectedRecords = onlyEdition < 0 ? 16 :
        (onlyEdition == int(FirmwareEdition::Score) ? 7 : 9);
    if (records != expectedRecords) { error = "Preview parity matrix missed DRAG S hardware records"; return false; }
    const size_t effectFrames = size_t(endEffect - firstEffect) * 32 -
        (firstEffect == 0 && endEffect > 0 ? 31 : 0);
    if (frames != expectedRecords * 4 * effectFrames) {
        error = "Preview parity matrix missed native LCD framebuffer phases"; return false;
    }
    if (proof) *proof << "Native direct-preview parity: " << records
        << " records, effects " << firstEffect << ".." << endEffect - 1
        << ", four hook entries, " << frames << " LCD frames\n";
    return true;
}

inline bool RunNativeBasePreviewParity(std::string& error, std::ostream* proof) {
    return RunNativePreviewParity(error, proof, 0, uint32_t(FirmwareLogo::Animation::Count), -1);
}

inline bool RunNativeDirectionRegressions(std::string& error, std::ostream* proof, int onlyEdition = -1) {
    return RunNativePreviewParity(error, proof, 17, uint32_t(FirmwareLogo::Animation::Count), onlyEdition);
}

inline bool RunNativeEffectRegressions(std::string& error, std::ostream* proof = nullptr) {
    return RunNativeBasePreviewParity(error, proof) &&
        RunNativeEffectShard(error, proof, FirmwareLogo::Animation::None, -1);
}
