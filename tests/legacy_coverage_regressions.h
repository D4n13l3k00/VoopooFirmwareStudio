#pragma once
#include "../src/core/app_state.h"
#include "../src/core/embedded_firmware.h"
#include "../src/core/firmware_emulator.h"
#include "../src/core/firmware_emulator_screens.h"
#include "../src/core/firmware_profiles.h"
#include "../src/core/firmware_emulator_runtime.h"
#include "../src/core/firmware_legacy_loader_data.h"
#include <algorithm>
#include <ostream>
#include <set>

inline bool RunLegacyLoaderMemoryRegressions(std::string& error) {
    using FirmwareEmulator::LegacyLoaderData;
    enum class Access { Byte, Word, Write, Execute };
    const auto probe = [&](uint32_t address, Access access, bool allowed, uint32_t expected = 0) {
        std::array<uint8_t, 12> code{0x01, 0x48, 0x00, 0x78, 0x70, 0x47, 0x00, 0xbf};
        if (access == Access::Word) code[3] = 0x68;
        if (access == Access::Write) { code[2] = 0x01; code[3] = 0x70; }
        if (access == Access::Execute) { code[2] = 0x00; code[3] = 0x47; address |= 1; }
        for (unsigned i = 0; i < 4; ++i) code[8 + i] = uint8_t(address >> (i * 8));
        FirmwareEmulator::ArmRuntime runtime;
        LegacyLoaderData loader;
        std::string detail;
        if (!runtime.Open(code, 0x08003000, 0x20000000, 0x10000, UC_CPU_ARM_CORTEX_M3, {}, detail) ||
            !loader.Open(runtime.Raw(), detail) ||
            !loader.Seed(runtime.Raw(), {1, 2, 3, 4}, {0, 1, 2, 3, 4, 5, 6, 7}, detail)) {
            error = "Loader memory probe setup: " + detail;
            return false;
        }
        const bool result = runtime.Call(0x08003000, 0x2000e000, {0, 255, 0, 0}, detail);
        if (result != allowed) { error = "Loader metadata access permission mismatch: " + detail; return false; }
        if (allowed) {
            uint32_t value = 0;
            if (!loader.Error().empty() || uc_reg_read(runtime.Raw(), UC_ARM_REG_R0, &value) != UC_ERR_OK || value != expected) {
                error = "Loader metadata read changed explicit VM input"; return false;
            }
        } else if (access == Access::Write || access == Access::Execute) {
            const char* expectedFault = access == Access::Write ? "UC_ERR_WRITE_PROT" : "UC_ERR_FETCH_PROT";
            if (detail.find(expectedFault) == std::string::npos) {
                error = "Loader metadata page is not read-only/non-executable: " + detail; return false;
            }
        } else if (loader.Error().empty()) {
            error = "Unspecified bytes in the loader page were readable"; return false;
        }
        return true;
    };
    if (!probe(LegacyLoaderData::Version, Access::Byte, true, 1) ||
        !probe(LegacyLoaderData::Version + 3, Access::Byte, true, 4) ||
        !probe(LegacyLoaderData::Identity, Access::Byte, true, 0) ||
        !probe(LegacyLoaderData::Identity + 7, Access::Byte, true, 7) ||
        !probe(LegacyLoaderData::Version, Access::Word, true, 0x04030201)) return false;
    for (uint32_t address : {LegacyLoaderData::Page, LegacyLoaderData::Version - 1,
        LegacyLoaderData::Version + 4, LegacyLoaderData::Identity - 1, LegacyLoaderData::Identity + 8})
        if (!probe(address, Access::Byte, false)) return false;
    return probe(LegacyLoaderData::Version + 1, Access::Word, false) &&
        probe(LegacyLoaderData::Identity + 6, Access::Word, false) &&
        probe(LegacyLoaderData::Version, Access::Write, false) &&
        probe(LegacyLoaderData::Identity, Access::Execute, false);
}
inline bool RunLegacyCoverageRegressions(std::string& error, std::ostream* proof = nullptr) {
    using FirmwareLayout::Point;
    if (!RunLegacyLoaderMemoryRegressions(error)) return false;
    FirmwareEmulator::Renderer renderer;
    size_t releases = 0, pages = 0, moves = 0;
    for (const auto& profile : FirmwareProfiles()) {
        if (profile.model != FirmwareModel::Drag1 && profile.model != FirmwareModel::Drag2 &&
            profile.model != FirmwareModel::DragMini && profile.model != FirmwareModel::Drag2Refresh &&
            profile.model != FirmwareModel::DragMiniRefresh) continue;
        const auto fail = [&](const std::string& why) {
            error = std::string(profile.filename) + ": " + why;
            return false;
        };
        FirmwarePackage package;
        StudioState state;
        if (!LoadEmbeddedFirmware(package, error, profile.model, profile.edition) ||
            !state.LoadPackage(std::move(package), profile.filename, error)) return fail(error);
        if (!state.layoutError.empty() || state.layoutElements.empty()) return fail(state.layoutError);
        if (state.firmware.records.size() != 1) return fail("Unexpected actual legacy hardware inventory");
        const auto& record = state.firmware.records.front();
        const uint16_t code = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
        const auto screens = FirmwareEmulator::LegacyScreens(profile.model, profile.edition, code);
        if (screens.empty() || !FirmwareEmulator::LegacyScreens(profile.model, profile.edition, 0xffff).empty())
            return fail("Hardware-specific screen selection falls back to another record");
        std::set<uint32_t> coordinateOwners;
        for (const auto& element : state.layoutElements) {
            if (element.hardwareCode != code || element.sites.empty()) return fail("Non-native layout element");
            for (const auto& site : element.sites)
                if (!coordinateOwners.insert(site.address).second) return fail("Duplicate ownership of shared native coordinates");
        }
        const bool original = profile.model != FirmwareModel::Drag2Refresh && profile.model != FirmwareModel::DragMiniRefresh;
        // These are complete original LCD streams, including fixed full-screen
        // warning/logo bitmaps; they must not disappear from the screen catalog.
        for (const auto& screen : screens) {
            FirmwareEmulator::DeviceState device;
            device.hardwareCode = code; device.screen = screen.id;
            if (screen.temperatureMode >= 0) device.temperatureControl = screen.temperatureMode != 0;
            FirmwareEmulator::Frame frame;
            if (!renderer.Render(state, device, frame) || frame.width != 128 || frame.height != 32 ||
                frame.pixelWrites < 4096 || frame.spiBytes == 0 || frame.hardwareCode != code || frame.emulatedUid ||
                frame.emulatedLoaderMetadata != (original && (screen.id == 91 || screen.id == 92)))
                return fail("Native page " + std::to_string(screen.id) + ": " + frame.error);
            ++pages;
        }
        const auto metadataPages = std::count_if(screens.begin(), screens.end(),
            [](const auto& page) { return page.id == 91 || page.id == 92; });
        if (metadataPages != (original ? 2 : 0)) return fail("Incorrect native loader metadata screen inventory");
        if (original) {
            if (record.linked_base != 0x08003000) return fail("Unexpected original application base");
            for (uint32_t page : {91u, 92u}) {
                const auto info = std::find_if(screens.begin(), screens.end(), [&](const auto& value) { return value.id == page; });
                if (info->orientation != 0 || info->temperatureMode != -1) return fail("Incorrect loader page UI mode metadata");
                FirmwareEmulator::DeviceState device;
                device.hardwareCode = code; device.screen = page;
                FirmwareEmulator::Frame zero, changed, repeated;
                if (!renderer.Render(state, device, zero) || !zero.emulatedLoaderMetadata)
                    return fail("Native zero loader input: " + zero.error);
                if (page == 91) device.loaderVersion = {1, 2, 3, 4};
                else device.loaderId = {0x0a, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde};
                if (!renderer.Render(state, device, changed) || changed.rgba == zero.rgba || !changed.emulatedLoaderMetadata ||
                    !renderer.Render(state, device, repeated) || repeated.rgba != changed.rgba)
                    return fail("Explicit loader input did not reach native LCD stream: " + changed.error);
                if (page == 91) device.loaderId.fill(255);
                else device.loaderVersion.fill(9);
                if (!renderer.Render(state, device, repeated) || repeated.rgba != changed.rgba)
                    return fail("Native loader page depends on unrelated metadata bytes");
                std::vector<uint8_t> bytes;
                if (!state.BuildToBuffer(bytes, error) || bytes != state.firmware.source_bytes)
                    return fail("Emulated loader metadata leaked into firmware export: " + error);
                FirmwarePackage rebuilt;
                StudioState reopened;
                if (!FirmwarePackage::Parse(bytes, rebuilt, error) ||
                    !reopened.LoadPackage(std::move(rebuilt), profile.filename, error) ||
                    !renderer.Render(reopened, device, repeated) || repeated.rgba != changed.rgba || !repeated.emulatedLoaderMetadata)
                    return fail("Native loader page changed after export/reopen: " + error + repeated.error);
                if (page == 91) {
                    device.loaderVersion.fill(255);
                    FirmwareEmulator::Frame invalid, normalized;
                    if (!renderer.Render(state, device, invalid)) return fail(invalid.error);
                    device.loaderVersion = {1, 0, 0, 0};
                    if (!renderer.Render(state, device, normalized) || normalized.rgba != invalid.rgba)
                        return fail("Native version-byte normalization was replaced with host formatting");
                }
                device.screen = 0;
                if (!renderer.Render(state, device, repeated) || repeated.emulatedLoaderMetadata)
                    return fail("Loader metadata flag leaked into an ordinary native page");
            }
            for (const auto& element : state.layoutElements) {
                if (element.screen != 92) continue;
                const int top = element.id.find(".loader.header.") != std::string::npos ? 0 : 16;
                if (element.position.y + element.minimumOffset.y < top ||
                    element.position.y + element.height + element.maximumOffset.y > top + 16)
                    return fail(element.id + ": layout escapes its native heading/clear window");
            }
        }
        // Each editable block must visibly change a real native page. Search its
        // declared aliases/selection alternatives, not unrelated native screens.
        for (size_t index = 0; index < state.layoutElements.size(); ++index) {
            const auto element = state.layoutElements[index];
            Point delta;
            if (element.maximumOffset.x >= element.stepX) delta.x = element.stepX;
            else if (element.minimumOffset.x <= -element.stepX) delta.x = -element.stepX;
            else if (element.maximumOffset.y >= element.stepY) delta.y = element.stepY;
            else if (element.minimumOffset.y <= -element.stepY) delta.y = -element.stepY;
            if (delta == Point{}) return fail(element.id + ": fake immovable layout element");
            state.PushUndo("Move legacy native block");
            if (!state.SetLayoutOffset(index, delta, error)) return fail(element.id + ": " + error);
            bool visible = false;
            FirmwareEmulator::DeviceState observed;
            FirmwareEmulator::Frame changed;
            for (const auto& screen : screens) {
                if (!FirmwareLayout::OnScreen(element, screen.id)) continue;
                for (int mode = 0; mode < 2 && !visible; ++mode) {
                    if ((element.temperatureMode >= 0 && element.temperatureMode != mode) ||
                        (screen.temperatureMode >= 0 && screen.temperatureMode != mode)) continue;
                    for (unsigned selection = 0; selection < (std::max)(1u, screen.menuItems) && !visible; ++selection) {
                        FirmwareEmulator::DeviceState device;
                        device.hardwareCode = code; device.screen = screen.id; device.temperatureControl = mode != 0;
                        device.menuSelection = static_cast<int>(selection);
                        if (screen.id == 91 || screen.id == 92) {
                            device.loaderVersion = {1, 2, 3, 4};
                            device.loaderId = {0x0a, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde};
                        }
                        FirmwareEmulator::Frame stock;
                        if (!state.SetLayoutOffset(index, {}, error) || !renderer.Render(state, device, stock) ||
                            !state.SetLayoutOffset(index, delta, error) || !renderer.Render(state, device, changed))
                            return fail(element.id + ": " + (error.empty() ? changed.error : error));
                        if (stock.rgba != changed.rgba) { visible = true; observed = device; }
                    }
                }
                if (visible) break;
            }
            if (!visible) return fail(element.id + ": coordinate edits have no native LCD effect");
            std::vector<uint8_t> bytes;
            FirmwarePackage built;
            StudioState reopened;
            if (!state.BuildToBuffer(bytes, error) || !FirmwarePackage::Parse(bytes, built, error) ||
                !reopened.LoadPackage(std::move(built), profile.filename, error) || !reopened.layoutError.empty())
                return fail(element.id + ": reopen: " + error + reopened.layoutError);
            const auto recovered = std::find_if(reopened.layoutElements.begin(), reopened.layoutElements.end(),
                [&](const auto& value) { return value.id == element.id; });
            if (recovered == reopened.layoutElements.end() ||
                recovered->position != Point{element.position.x + delta.x, element.position.y + delta.y} ||
                recovered->sharedScreens != element.sharedScreens) return fail(element.id + ": recovered geometry/aliases changed");
            FirmwareEmulator::Frame frame;
            if (!renderer.Render(reopened, observed, frame) || frame.rgba != changed.rgba ||
                frame.emulatedLoaderMetadata != changed.emulatedLoaderMetadata)
                return fail(element.id + ": exported native stream changed: " + frame.error);
            // Resource replacement and a second layout move must coexist after
            // reopening: descriptors are not resource payload or init metadata.
            if (!reopened.catalog.empty()) {
                auto bitmap = reopened.originals.front();
                if (!bitmap.empty()) {
                    bitmap.front() ^= 1;
                    reopened.SetBitmapReplacement(0, std::move(bitmap));
                    const size_t reopenedIndex = static_cast<size_t>(recovered - reopened.layoutElements.begin());
                    if (!reopened.SetLayoutOffset(reopenedIndex, {-delta.x, -delta.y}, error))
                        return fail(element.id + ": second native move: " + error);
                    if (!reopened.BuildToBuffer(bytes, error) || !FirmwarePackage::Parse(bytes, built, error))
                        return fail(element.id + ": resource/layout combination: " + error);
                    StudioState combined;
                    if (!combined.LoadPackage(std::move(built), profile.filename, error) || !combined.layoutError.empty() ||
                        !renderer.Render(combined, observed, frame)) return fail(element.id + ": resource guard: " + error + frame.error);
                }
            }
            state.EndUndoAction();
            if (!state.Undo() || std::any_of(state.layoutOffsets.begin(), state.layoutOffsets.end(),
                [](const auto& item) { return item.second != Point{}; }) ||
                !state.BuildToBuffer(bytes, error) || bytes != state.firmware.source_bytes)
                return fail(element.id + ": undo lost original firmware bytes");
            ++moves;
        }
        auto corrupted = state.firmware;
        corrupted.records.front().image[0x144] ^= 0x80;
        std::vector<FirmwareLayout::Element> rejected;
        std::string guardError;
        if (FirmwareLayout::Discover(corrupted, rejected, guardError)) return fail("Unrelated executable mutation passed native guard");
        ++releases;
    }
    if (releases != 8) { error = "Legacy release inventory changed without coverage"; return false; }
    if (proof) *proof << "Legacy native releases=" << releases << " pages=" << pages << " moves=" << moves << '\n';
    error.clear();
    return true;
}
