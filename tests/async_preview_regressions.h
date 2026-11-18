#pragma once
#include "app_state.h"
#include "firmware_preview.h"
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace AsyncPreviewRegression {
struct Gate {
    struct Call {
        uint64_t document = 0, revision = 0;
        int watts = 0;
        std::thread::id thread;
        const StudioState* snapshot = nullptr;
        uint32_t patchDuration = 0;
        uint8_t patchPixel = 0;
    };
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<Call> calls;
    bool entered = false, released = false;

    bool Execute(const StudioState& source, const FirmwareEmulator::DeviceState& device,
        FirmwareEmulator::Frame& frame) {
        {
            std::unique_lock lock(mutex);
            calls.push_back({source.DocumentIdentity(), source.ResourceRevision(), device.watts,
                std::this_thread::get_id(), &source, source.screenPatches[0].durationMs,
                source.screenPatches[0].pixels.empty() ? uint8_t{} : source.screenPatches[0].pixels.front()});
            if (calls.size() == 1) {
                entered = true;
                changed.notify_all();
                changed.wait(lock, [&] { return released; });
            }
        }
        frame = {};
        frame.width = frame.height = 1;
        frame.hardwareCode = device.hardwareCode;
        frame.rgba = {static_cast<uint32_t>(device.watts)};
        frame.resources = {static_cast<size_t>(source.DocumentIdentity())};
        if (device.screen == UINT32_MAX) return false;
        if (device.screen == UINT32_MAX - 1) throw std::runtime_error("Controlled worker exception");
        return true;
    }
    bool AwaitEntry() {
        std::unique_lock lock(mutex);
        return changed.wait_for(lock, std::chrono::seconds(30), [&] { return entered; });
    }
    void Release() {
        std::lock_guard lock(mutex);
        released = true;
        changed.notify_all();
    }
};

// Release before Cache's destructor on every early-return path. Deadlines only
// bound a broken test: correctness never depends on a render duration threshold.
struct ReleaseGate {
    Gate& gate;
    ~ReleaseGate() { gate.Release(); }
};
inline bool AwaitIdle(FirmwarePreview::Cache& cache) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (cache.Busy()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    cache.Poll();
    return true;
}
}

inline bool RunAsyncPreviewRegressions(std::string& error) {
    using namespace FirmwareEmulator;
    using namespace AsyncPreviewRegression;
    error.clear();
    auto fail = [&](const char* message) { error = message; return false; };
    FirmwarePackage package;
    StudioState source;
    if (!LoadEmbeddedFirmware(package, error, FirmwareModel::DragS, FirmwareEdition::Score) ||
        !source.LoadPackage(package, "async preview regression", error)) return false;
    const auto mainThread = std::this_thread::get_id();
    source.PushUndo("Snapshot isolation");
    source.SetBackgroundColor(Rgb8(8, 16, 24));
    source.EndUndoAction();
    const auto snapshot = source.CreateEmulationSnapshot();
    if (snapshot->DocumentIdentity() != source.DocumentIdentity() ||
        snapshot->ResourceRevision() != source.ResourceRevision() ||
        snapshot->originals != source.originals || snapshot->firmware.source_bytes != package.source_bytes ||
        snapshot->screenPatches != source.screenPatches || !snapshot->undoStack.empty() ||
        !snapshot->redoStack.empty() || snapshot->selectedVisibleIdx != -1 ||
        !snapshot->visibleAssetIndices.empty() || !snapshot->firmwarePath.empty())
        return fail("Emulation snapshot copied editor state or omitted CPU inputs/identity");
    const auto frozenBackground = snapshot->backgroundColor.ToRgb565();
    source.SetBackgroundColor(Rgb8(32, 40, 48));
    source.EndUndoAction();
    if (snapshot->backgroundColor.ToRgb565() != frozenBackground)
        return fail("Emulation snapshot aliases mutable editor resources");

    Gate gate;
    FirmwarePreview::Cache cache([&](const StudioState& document, const DeviceState& device, Frame& frame) {
        return gate.Execute(document, device, frame);
    });
    ReleaseGate release{gate};
    DeviceState device;
    device.hardwareCode = 0x3103;
    device.watts = 11;
    cache.Request(source, device);
    if (!gate.AwaitEntry()) return fail("Async executor never entered its worker");
    // While the first frame is deliberately held, all these requests must be
    // replaceable without joining it or accumulating a queue of native work.
    for (int watts = 12; watts != 32; ++watts) {
        device.watts = watts;
        cache.Request(source, device);
    }
    const auto oldIdentity = source.DocumentIdentity();
    const auto* reusedAddress = &source;
    if (!source.LoadPackage(package, "same-address replacement", error)) return false;
    if (&source != reusedAddress || source.DocumentIdentity() == oldIdentity)
        return fail("Stale-document fixture did not replace the same document slot");
    source.SetBackgroundColor(Rgb8(56, 64, 72));
    source.EndUndoAction();
    device.watts = 91;
    cache.Request(source, device);
    cache.Poll();
    if (cache.Published() || cache.Original() || !cache.Before().rgba.empty() || !cache.After().rgba.empty())
        return fail("An obsolete document frame became visible before the latest request completed");
    gate.Release();
    if (!AwaitIdle(cache)) return fail("Latest asynchronous request never completed");
    const auto retained = cache.Published();
    if (!retained || !retained->before || !retained->after ||
        retained->before->rgba != std::vector<uint32_t>{91} ||
        retained->after->rgba != std::vector<uint32_t>{91} || !retained->original ||
        retained->after->resources != std::vector<size_t>{static_cast<size_t>(source.DocumentIdentity())})
        return fail("Latest document/device request did not publish its immutable pair");
    {
        std::lock_guard lock(gate.mutex);
        if (gate.calls.size() != 3 || gate.calls.front().watts != 11 ||
            gate.calls[1].watts != 91 || gate.calls[2].watts != 91 ||
            gate.calls[2].document != source.DocumentIdentity() ||
            gate.calls[2].revision != source.ResourceRevision())
            return fail("Latest mailbox queued intermediates or failed cancellation between frames");
        for (const auto& call : gate.calls)
            if (call.thread == mainThread || call.thread != gate.calls.front().thread)
                return fail("Asynchronous CPU execution ran on the UI thread or escaped its worker");
    }
    const auto beforeGeneration = cache.BeforeGeneration(), afterGeneration = cache.AfterGeneration();
    const auto* stableOriginal = cache.Original();
    for (int i = 0; i != 32; ++i) cache.Request(source, device);
    if (cache.Busy() || cache.Poll() || cache.BeforeGeneration() != beforeGeneration ||
        cache.AfterGeneration() != afterGeneration || cache.Original() != stableOriginal)
        return fail("Unchanged requests allocated/redrew or invalidated published CPU state");

    // Advance the revision while preserving the document slot and native device.
    source.SetBackgroundColor(Rgb8(80, 88, 96));
    source.EndUndoAction();
    cache.Request(source, device);
    if (!AwaitIdle(cache)) return fail("Resource revision request never completed");
    {
        std::lock_guard lock(gate.mutex);
        if (gate.calls.size() != 4 || gate.calls.back().revision != source.ResourceRevision())
            return fail("Revision invalidation reran the opened original or used an old snapshot");
    }
    if (retained->after->rgba != std::vector<uint32_t>{91} || retained->original.get() != stableOriginal)
        return fail("A later Poll mutated previously retained frame/original storage");

    // Draft revisions are independent of live document/history revisions. The
    // worker must own a frozen copy, even if the UI edits its buffer immediately.
    {
        const auto livePatches = source.screenPatches;
        const auto liveRevision = source.ResourceRevision();
        const auto undoCount = source.undoStack.size(), redoCount = source.redoStack.size();
        Gate draftGate;
        FirmwarePreview::Cache draftCache([&](const StudioState& document, const DeviceState& input, Frame& frame) {
            return draftGate.Execute(document, input, frame);
        });
        ReleaseGate releaseDraft{draftGate};
        auto draft = livePatches;
        draft[0].durationMs = 800;
        draft[0].pixels = {17};
        auto draftDevice = device;
        draftCache.Request(source, draftDevice, false, &draft, 1);
        if (!draftGate.AwaitEntry()) return fail("Draft snapshot fixture never entered its worker");
        draft[0].durationMs = 900;
        draft[0].pixels[0] = 23;
        draftGate.Release();
        if (!AwaitIdle(draftCache) || draftGate.calls.size() != 2 ||
            draftGate.calls.back().patchDuration != 800 || draftGate.calls.back().patchPixel != 17)
            return fail("Draft patches were not copied synchronously before worker execution");
        const auto* frozen = draftGate.calls.back().snapshot;
        for (int i = 0; i != 32; ++i) draftCache.Request(source, draftDevice, false, &draft, 1);
        if (draftCache.Busy() || draftCache.Poll() || draftGate.calls.size() != 2)
            return fail("Unchanged draft revisions scanned pixels or scheduled native work");
        draftDevice.logoPhase = 1;
        draftCache.Request(source, draftDevice, false, &draft, 1);
        if (!AwaitIdle(draftCache) || draftGate.calls.size() != 4 ||
            draftGate.calls.back().snapshot != frozen || draftGate.calls.back().patchPixel != 17)
            return fail("Animation phases cloned or reread the draft snapshot");
        draftCache.Request(source, draftDevice, false, &draft, 2);
        if (!AwaitIdle(draftCache) || draftGate.calls.size() != 5 ||
            draftGate.calls.back().snapshot == frozen || draftGate.calls.back().patchDuration != 900 ||
            draftGate.calls.back().patchPixel != 23)
            return fail("Draft-only revision changes failed to redraw only the edited frame");
        draftCache.Request(source, draftDevice);
        if (!AwaitIdle(draftCache) || draftGate.calls.size() != 6 ||
            draftGate.calls.back().patchDuration != livePatches[0].durationMs ||
            draftGate.calls.back().patchPixel != (livePatches[0].pixels.empty() ? 0 : livePatches[0].pixels.front()))
            return fail("Removing draft overrides did not restore committed patches");
        draftCache.Request(source, draftDevice, false, &draft);
        if (!AwaitIdle(draftCache) || draftGate.calls.size() != 7 ||
            draftGate.calls.back().patchPixel != 23)
            return fail("Adding an override with revision zero did not invalidate native preview");
        draft[0].pixels[0] = 31;
        draftCache.Request(source, draftDevice, true, &draft);
        if (!AwaitIdle(draftCache) || draftGate.calls.size() != 9 || draftGate.calls.back().patchPixel != 31)
            return fail("Forced refresh did not recopy draft pixels and redraw both frames");
        if (source.screenPatches != livePatches || source.ResourceRevision() != liveRevision ||
            source.undoStack.size() != undoCount || source.redoStack.size() != redoCount)
            return fail("Draft preview mutated the live document or editor history");
    }
    device.screen = UINT32_MAX;
    cache.Request(source, device);
    if (!AwaitIdle(cache) || cache.Before().error.empty() || cache.After().error.empty() ||
        !cache.Before().rgba.empty() || !cache.After().rgba.empty())
        return fail("A failed worker frame retained old pixels or supplied a fallback");
    device.screen = UINT32_MAX - 1;
    cache.Request(source, device);
    if (!AwaitIdle(cache) || cache.Before().error != "Controlled worker exception" ||
        cache.After().error != "Controlled worker exception" || cache.Original() ||
        !cache.Before().rgba.empty() || !cache.After().rgba.empty())
        return fail("Worker exceptions were hidden or published a partial/stale result");

    // Exercise the real native asynchronous executor too, not just mailbox
    // synchronization. Blocking Render remains a separate headless API.
    source.SetBackgroundColor(Rgb8(0, 0, 0));
    source.EndUndoAction();
    device.screen = 0;
    FirmwarePreview::Cache native;
    native.Request(source, device);
    if (!AwaitIdle(native) || !native.Published() || !native.Before().error.empty() ||
        !native.After().error.empty()) {
        error = "Native async rendering: " + native.Before().error + native.After().error;
        return false;
    }
    FirmwarePreview::Cache blocking;
    if (!blocking.Render(source, device) || native.Before().rgba != blocking.Before().rgba ||
        native.After().rgba != blocking.After().rgba)
        return fail("Async and blocking native preview APIs execute different frame inputs");
    const auto unmodifiedNative = native.Published()->after;
    auto oversizedDraft = source.screenPatches;
    for (size_t target = 0; target < oversizedDraft.size(); ++target) {
        auto& patch = oversizedDraft[target];
        patch.enabled = true;
        patch.pixels.resize(FirmwareLogo::Width * FirmwareLogo::Height * 2);
        for (size_t pixel = 0; pixel < patch.pixels.size() / 2; ++pixel) {
            const auto color = static_cast<uint16_t>(pixel + target * FirmwareLogo::Width * FirmwareLogo::Height);
            patch.pixels[pixel * 2] = static_cast<uint8_t>(color);
            patch.pixels[pixel * 2 + 1] = static_cast<uint8_t>(color >> 8);
        }
    }
    native.Request(source, device, false, &oversizedDraft, 1);
    if (!AwaitIdle(native) || !native.Before().error.empty() || !native.After().rgba.empty() ||
        native.After().error.find("safe Flash capacity") == std::string::npos)
        return fail("Draft native preview did not propagate the full-package Flash capacity error");
    for (auto& patch : oversizedDraft) std::fill(patch.pixels.begin(), patch.pixels.end(), 0);
    native.Request(source, device, false, &oversizedDraft, 2);
    if (!AwaitIdle(native) || !native.After().error.empty() || native.After().rgba.empty())
        return fail("Draft-only changes failed to rebuild a cached native capacity failure");
    native.Request(source, device);
    if (!AwaitIdle(native) || !native.After().error.empty() || native.After().rgba != unmodifiedNative->rgba)
        return fail("Removing a native draft override reused its prepared firmware");
    device.screen = UINT32_MAX;
    native.Request(source, device);
    if (!AwaitIdle(native) || native.Before().error.empty() || native.After().error.empty() ||
        !native.Before().rgba.empty() || !native.After().rgba.empty())
        return fail("Failed native asynchronous execution displayed substitute pixels");

    // Shutdown may join; document changes above may not. Destruction must safely
    // own an in-flight executor and release all worker captures before returning.
    Gate shutdownGate;
    auto lifetime = std::make_shared<int>(1);
    std::weak_ptr<int> expired = lifetime;
    auto shuttingDown = std::make_unique<FirmwarePreview::Cache>(
        [&, lifetime](const StudioState& document, const DeviceState& input, Frame& frame) {
            return shutdownGate.Execute(document, input, frame);
        });
    lifetime.reset();
    ReleaseGate releaseShutdown{shutdownGate};
    device.screen = 0;
    shuttingDown->Request(source, device);
    if (!shutdownGate.AwaitEntry()) return fail("Shutdown fixture never entered CPU execution");
    std::thread shutdown([&] { shuttingDown.reset(); });
    shutdownGate.Release();
    shutdown.join();
    if (!expired.expired()) return fail("Shutdown returned while worker-owned executor captures remained alive");
    return true;
}
