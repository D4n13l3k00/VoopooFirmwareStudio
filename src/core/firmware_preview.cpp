#include "firmware_preview.h"
#include "app_state.h"
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>

namespace FirmwarePreview {
namespace {
bool SamePackage(const FirmwarePackage& a, const FirmwarePackage& b) {
    if (a.format != b.format || a.source_sha256 != b.source_sha256 ||
        a.records.size() != b.records.size() || a.source_bytes != b.source_bytes) return false;
    for (size_t i = 0; i < a.records.size(); ++i) {
        const auto& x = a.records[i];
        const auto& y = b.records[i];
        if (x.device_code[0] != y.device_code[0] || x.device_code[1] != y.device_code[1] ||
            x.linked_base != y.linked_base || x.sp != y.sp || x.reset != y.reset ||
            x.marker != y.marker || x.image != y.image) return false;
    }
    return true;
}

void Fail(FirmwareEmulator::Frame& frame, const std::string& error) {
    frame = {};
    frame.error = error;
}

std::shared_ptr<const FirmwareEmulator::Frame> FailedFrame(const std::string& error) {
    auto frame = std::make_shared<FirmwareEmulator::Frame>();
    Fail(*frame, error);
    return frame;
}

// Mutable native execution state belongs either to a headless caller or
// exclusively to the worker. Completed frames are shared read-only.
struct BlockingCache {
    std::shared_ptr<StudioState> original;
    std::optional<FirmwarePackage> failedPackage;
    std::optional<FirmwareEmulator::Renderer> beforeRenderer, afterRenderer;
    std::shared_ptr<FirmwareEmulator::Frame> before, after;
    FirmwareEmulator::DeviceState device;
    const StudioState* source = nullptr;
    uint64_t revision = UINT64_MAX, snapshotSerial = 0;
    uint64_t documentIdentity = 0;
    uint64_t beforeGeneration = 0, afterGeneration = 0;
    FirmwareModel model = FirmwareModel::DragS;
    FirmwareEdition edition = FirmwareEdition::Score;
    bool hasFirmware = false;
    bool initialized = false;
    bool deviceInitialized = false;
    std::string originalError;

    bool SameSource(const StudioState& state, const StudioState* identity, bool inspectPackage) const {
        if (!initialized || source != identity || documentIdentity != state.DocumentIdentity() ||
            hasFirmware != state.hasFirmware || model != state.model || edition != state.edition) return false;
        if (!inspectPackage) return true;
        const auto* package = original ? &original->firmware :
            (failedPackage ? &*failedPackage : nullptr);
        return package && SamePackage(*package, state.firmware);
    }

    void LoadOriginal(const StudioState& state, const StudioState* identity, bool includeOriginal) {
        beforeRenderer.reset();
        afterRenderer.reset();
        original.reset();
        failedPackage.reset();
        originalError.clear();
        source = identity;
        documentIdentity = state.DocumentIdentity();
        model = state.model;
        edition = state.edition;
        hasFirmware = state.hasFirmware;
        initialized = true;
        deviceInitialized = false;
        if (!includeOriginal) return;
        if (state.hasFirmware) {
            auto loaded = std::make_shared<StudioState>();
            if (loaded->LoadPackage(state.firmware, "", originalError)) {
                original = std::move(loaded);
                return;
            }
        } else {
            originalError = "No firmware loaded";
        }
        failedPackage = state.firmware;
    }

    template<class Cancelled>
    bool Render(const StudioState& state, const StudioState* identity,
        const FirmwareEmulator::DeviceState& input, bool force, Cancelled cancelled,
        const Cache::FrameExecutor& executor = {}, bool includeOriginal = true, uint64_t inputSnapshotSerial = 0) {
        if (cancelled()) return false;
        const bool changedSource = !SameSource(state, identity, force);
        if (changedSource) LoadOriginal(state, identity, includeOriginal);
        const bool changedSnapshot = snapshotSerial != inputSnapshotSerial;
        // Snapshot revisions also distinguish draft-only edits. Do not depend on
        // allocation addresses or rebuild native firmware for device-only phases.
        if (changedSnapshot && !changedSource) afterRenderer.reset();
        if (cancelled()) return false;
        const bool changedDevice = changedSource || !deviceInitialized || device != input;
        auto render = [&](std::optional<FirmwareEmulator::Renderer>& renderer, const StudioState& document,
            FirmwareEmulator::Frame& output) {
            if (!executor && !renderer) renderer.emplace();
            const bool ok = executor ? executor(document, input, output) : renderer->Render(document, input, output);
            if (!ok || !output.error.empty()) {
                const auto error = output.error.empty() ? "Native preview rendering failed" : output.error;
                Fail(output, error);
            }
        };
        if (includeOriginal && (changedDevice || force)) {
            auto frame = before && before.use_count() == 1 ? before : std::make_shared<FirmwareEmulator::Frame>();
            if (original) render(beforeRenderer, *original, *frame);
            else Fail(*frame, originalError);
            before = std::move(frame);
            ++beforeGeneration;
        }
        // Commit the completed before frame's key even when the request is
        // superseded. The next request must not mistake it for an older device.
        device = input;
        deviceInitialized = true;
        if (cancelled()) {
            revision = UINT64_MAX;
            return false;
        }
        if (changedDevice || changedSnapshot || force || revision != state.ResourceRevision()) {
            auto frame = after && after.use_count() == 1 ? after : std::make_shared<FirmwareEmulator::Frame>();
            render(afterRenderer, state, *frame);
            after = std::move(frame);
            ++afterGeneration;
        }
        revision = state.ResourceRevision();
        snapshotSerial = inputSnapshotSerial;
        return !cancelled();
    }
};
}

struct Cache::Impl {
    struct Request {
        std::shared_ptr<const StudioState> snapshot;
        const StudioState* identity = nullptr; // Compared only; never dereferenced.
        FirmwareEmulator::DeviceState device;
        uint64_t epoch = 0;
        bool force = false;
        uint64_t refreshSerial = 0, snapshotSerial = 0;
    };
    std::unique_ptr<BlockingCache> blocking;
    FrameExecutor executor;
    Mode mode = Mode::Comparison;
    std::mutex mutex;
    std::condition_variable wake;
    std::optional<Request> pending;
    std::shared_ptr<const Publication> completed;
    uint64_t completedEpoch = 0;
    std::thread worker;
    std::atomic<uint64_t> latestEpoch{0};
    std::atomic<bool> stopping{false};
    std::atomic<bool> busy{false};
    std::shared_ptr<const StudioState> snapshot;
    std::shared_ptr<const Publication> published;
    const StudioState* source = nullptr;
    uint64_t documentIdentity = 0, revision = UINT64_MAX, screenPatchRevision = 0;
    FirmwareModel model = FirmwareModel::DragS;
    FirmwareEdition edition = FirmwareEdition::Score;
    bool hasFirmware = false, requested = false, invalidate = false, hasScreenPatchOverride = false;
    FirmwareEmulator::DeviceState device;
    uint64_t beforeGeneration = 0, afterGeneration = 0;
    uint64_t refreshSerial = 0, snapshotSerial = 0;

    ~Impl() {
        stopping.store(true, std::memory_order_release);
        wake.notify_one();
        if (worker.joinable()) worker.join(); // Shutdown only, never on document changes.
    }

    void Run() {
        // Even renderer construction/destruction and original-package discovery
        // stay here, not on the UI thread.
        std::unique_ptr<BlockingCache> cache;
        uint64_t refreshed = 0;
        for (;;) {
            Request request;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [&] { return stopping.load(std::memory_order_acquire) || pending.has_value(); });
                if (stopping.load(std::memory_order_acquire)) return;
                request = std::move(*pending);
                pending.reset();
            }
            const auto cancelled = [&] {
                return stopping.load(std::memory_order_acquire) ||
                    latestEpoch.load(std::memory_order_acquire) != request.epoch;
            };
            std::shared_ptr<Publication> result;
            try {
                if (!cache) cache = std::make_unique<BlockingCache>();
                if (!cache->Render(*request.snapshot, request.identity, request.device,
                    request.force || request.refreshSerial != refreshed, cancelled, executor,
                    mode == Mode::Comparison, request.snapshotSerial)) continue;
                refreshed = request.refreshSerial;
                result = std::make_shared<Publication>();
                result->before = cache->before;
                result->after = cache->after;
                result->original = cache->original;
            } catch (const std::exception& error) {
                result = std::make_shared<Publication>();
                result->before = FailedFrame(error.what());
                result->after = result->before;
                cache.reset();
            } catch (...) {
                result = std::make_shared<Publication>();
                result->before = FailedFrame("Native preview worker failed");
                result->after = result->before;
                cache.reset();
            }
            if (cancelled()) continue;
            {
                std::lock_guard lock(mutex);
                if (cancelled()) continue;
                completed = std::move(result);
                completedEpoch = request.epoch;
                busy.store(false, std::memory_order_release);
            }
        }
    }
};

Cache::Cache() : impl_(std::make_unique<Impl>()) {}
Cache::Cache(Mode mode) : Cache() { impl_->mode = mode; }
Cache::Cache(FrameExecutor executor) : Cache() { impl_->executor = std::move(executor); }
Cache::~Cache() = default;

bool Cache::Render(const StudioState& source, const FirmwareEmulator::DeviceState& device, bool force) {
    auto& cache = *impl_;
    // Render is an intentionally blocking, independent API for headless callers.
    if (!cache.blocking) cache.blocking = std::make_unique<BlockingCache>();
    cache.blocking->Render(source, &source, device, force, [] { return false; });
    cache.requested = false;
    return cache.blocking->before->error.empty() && cache.blocking->after->error.empty();
}

void Cache::Request(const StudioState& source, const FirmwareEmulator::DeviceState& device, bool force,
    const FirmwareLogo::Patches* screenPatches, uint64_t screenPatchRevision) {
    auto& cache = *impl_;
    const bool changedSource = !cache.requested || cache.source != &source ||
        cache.documentIdentity != source.DocumentIdentity() || cache.model != source.model ||
        cache.edition != source.edition || cache.hasFirmware != source.hasFirmware;
    const bool changedRevision = changedSource || cache.revision != source.ResourceRevision();
    const bool hasScreenPatchOverride = screenPatches != nullptr;
    const bool changedOverride = cache.hasScreenPatchOverride != hasScreenPatchOverride ||
        (hasScreenPatchOverride && cache.screenPatchRevision != screenPatchRevision);
    if (!changedRevision && !changedOverride && cache.device == device && !force) return;
    cache.source = &source;
    cache.documentIdentity = source.DocumentIdentity();
    cache.revision = source.ResourceRevision();
    cache.model = source.model;
    cache.edition = source.edition;
    cache.hasFirmware = source.hasFirmware;
    cache.hasScreenPatchOverride = hasScreenPatchOverride;
    cache.screenPatchRevision = screenPatchRevision;
    cache.device = device;
    cache.requested = true;
    cache.invalidate = true;
    const uint64_t epoch = cache.latestEpoch.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (force) ++cache.refreshSerial;
    try {
        if (changedRevision || changedOverride || force || !cache.snapshot) {
            cache.snapshot = source.CreateEmulationSnapshot(screenPatches);
            ++cache.snapshotSerial;
        }
        {
            std::lock_guard lock(cache.mutex);
            cache.completed.reset();
            cache.pending = Impl::Request{cache.snapshot, &source, device, epoch, force,
                cache.refreshSerial, cache.snapshotSerial};
            cache.busy.store(true, std::memory_order_release);
        }
        if (!cache.worker.joinable()) cache.worker = std::thread([&cache] { cache.Run(); });
        cache.wake.notify_one();
    } catch (const std::exception& error) {
        auto failure = std::make_shared<Publication>();
        failure->before = FailedFrame(error.what());
        failure->after = failure->before;
        std::lock_guard lock(cache.mutex);
        cache.pending.reset();
        cache.completed = std::move(failure);
        cache.completedEpoch = epoch;
        cache.busy.store(false, std::memory_order_release);
    }
}

bool Cache::Poll() {
    auto& cache = *impl_;
    std::shared_ptr<const Publication> result;
    {
        std::lock_guard lock(cache.mutex);
        if (cache.completed && cache.completedEpoch == cache.latestEpoch.load(std::memory_order_acquire))
            result = std::move(cache.completed);
    }
    if (!result && !cache.invalidate) return false;
    cache.invalidate = false;
    if (result == cache.published) return false;
    cache.published = std::move(result);
    ++cache.beforeGeneration;
    ++cache.afterGeneration;
    return true;
}

bool Cache::Busy() const { return impl_->busy.load(std::memory_order_acquire); }
std::shared_ptr<const Cache::Publication> Cache::Published() const { return impl_->published; }
const FirmwareEmulator::Frame& Cache::Before() const {
    static const FirmwareEmulator::Frame empty;
    if (impl_->requested) return impl_->published && impl_->published->before ? *impl_->published->before : empty;
    return impl_->blocking && impl_->blocking->before ? *impl_->blocking->before : empty;
}
const FirmwareEmulator::Frame& Cache::After() const {
    static const FirmwareEmulator::Frame empty;
    if (impl_->requested) return impl_->published && impl_->published->after ? *impl_->published->after : empty;
    return impl_->blocking && impl_->blocking->after ? *impl_->blocking->after : empty;
}
const StudioState* Cache::Original() const {
    if (impl_->requested) return impl_->published ? impl_->published->original.get() : nullptr;
    return impl_->blocking ? impl_->blocking->original.get() : nullptr;
}
uint64_t Cache::BeforeGeneration() const {
    return impl_->requested ? impl_->beforeGeneration :
        (impl_->blocking ? impl_->blocking->beforeGeneration : 0);
}
uint64_t Cache::AfterGeneration() const {
    return impl_->requested ? impl_->afterGeneration :
        (impl_->blocking ? impl_->blocking->afterGeneration : 0);
}
}
