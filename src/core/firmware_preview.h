#pragma once
#include "firmware_emulator.h"
#include "firmware_logo_patch.h"
#include <functional>

namespace FirmwarePreview {
// Both frames execute native firmware with one identical DeviceState. The opened
// package is loaded into a separate document; no factory reference is substituted.
class Cache {
public:
    Cache();
    ~Cache();
    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;
    enum class Mode { Comparison, CurrentOnly };
    explicit Cache(Mode mode);

    // An optional CPU executor also permits deterministic host-side regressions.
    // It is invoked only by the asynchronous worker, never by Render().
    using FrameExecutor = std::function<bool(const StudioState&,
        const FirmwareEmulator::DeviceState&, FirmwareEmulator::Frame&)>;
    explicit Cache(FrameExecutor executor);

    struct Publication {
        std::shared_ptr<const FirmwareEmulator::Frame> before, after;
        std::shared_ptr<const StudioState> original;
    };
    // Normal calls use DocumentIdentity() and ResourceRevision() for constant-time
    // invalidation. force also compares package bytes and redraws both frames.
    bool Render(const StudioState& source, const FirmwareEmulator::DeviceState& device, bool force = false);

    // Main-thread API. Request replaces the single latest mailbox; call it before
    // Poll so obsolete document/revision/device results cannot become visible.
    // Overrides are copied synchronously only when the source, override presence/
    // revision, or force changes. Increment screenPatchRevision after draft edits;
    // the pointer is never retained. Device-only changes reuse the same snapshot.
    // No native preparation or execution occurs on the requesting thread.
    void Request(const StudioState& source, const FirmwareEmulator::DeviceState& device, bool force = false,
        const FirmwareLogo::Patches* screenPatches = nullptr, uint64_t screenPatchRevision = 0);
    bool Poll();
    bool Busy() const;
    // Holding this handle keeps immutable frames/original alive across Poll().
    std::shared_ptr<const Publication> Published() const;
    // References/pointers below remain stable until the next Poll() or Render().
    const FirmwareEmulator::Frame& Before() const;
    const FirmwareEmulator::Frame& After() const;
    const StudioState* Original() const;
    uint64_t BeforeGeneration() const;
    uint64_t AfterGeneration() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
