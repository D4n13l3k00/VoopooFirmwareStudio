#include "firmware_device_worker.h"
#include "app_state.h"
#include "atomic_file.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <optional>
#include <thread>
#include <stdexcept>
#include <utility>
#include <cstdio>

namespace FirmwareDevice {
namespace {
std::filesystem::path DefaultStorageFile(FirmwareEdition edition, uint16_t hardwareCode,
    const std::filesystem::path& storageDirectory) {
    std::filesystem::path root = storageDirectory;
    if (root.empty()) {
        PWSTR directory = nullptr;
        const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &directory);
        if (FAILED(result)) throw std::runtime_error("Cannot locate virtual device storage directory");
        root = directory;
        CoTaskMemFree(directory);
        root /= L"VoopooFirmwareStudio";
        root /= L"emulator";
    }
    char name[64];
    std::snprintf(name, sizeof(name), "drag-s-%s-%04x-v2.nvram",
        edition == FirmwareEdition::Factory ? "factory" : "score", unsigned(hardwareCode));
    return root / name;
}
bool ReadStorage(const std::filesystem::path& path, std::vector<uint8_t>& bytes, std::string& error) {
    std::error_code code;
    if (!std::filesystem::exists(path, code)) {
        if (!code) return true;
        error = "Cannot inspect virtual Flash: " + code.message();
        return false;
    }
    const auto size = std::filesystem::file_size(path, code);
    if (code || !size || size > 65536) {
        error = "Virtual Flash storage is empty, unreadable or exceeds 64 KiB";
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    bytes.resize(static_cast<size_t>(size));
    if (!file || (size && !file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)))) {
        error = "Cannot read virtual Flash storage";
        return false;
    }
    return true;
}
}

struct Worker::Impl {
    struct LoadRequest {
        std::shared_ptr<const StudioState> source;
        std::filesystem::path file;
        uint16_t hardwareCode;
        uint64_t epoch;
    };
    enum class Action { Inputs, Reset, Step };
    struct Command {
        Action action;
        FirmwareDevice::Inputs inputs{};
        std::chrono::steady_clock::time_point captured = std::chrono::steady_clock::now();
    };
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::optional<LoadRequest> load;
    std::deque<Command> commands;
    std::shared_ptr<const View> published;
    std::shared_ptr<const View> visible = std::make_shared<View>();
    const std::filesystem::path storageDirectory;
    std::thread thread;
    uint64_t epoch = 0;
    bool stopping = false;
    bool requestedRunning = false;
    bool inputOverflow = false;
    std::atomic<bool> busy{false};

    explicit Impl(std::filesystem::path directory) : storageDirectory(std::move(directory)) {
        thread = std::thread([this] { Run(); });
    }
    ~Impl() {
        { std::lock_guard lock(mutex); stopping = true; }
        wake.notify_one();
        if (thread.joinable()) thread.join();
    }
    void Push(Command command) {
        {
            std::lock_guard lock(mutex);
            const bool sameButtons = command.action == Action::Inputs && !commands.empty() &&
                commands.back().action == Action::Inputs &&
                commands.back().inputs.fire == command.inputs.fire &&
                commands.back().inputs.plus == command.inputs.plus &&
                commands.back().inputs.minus == command.inputs.minus &&
                commands.back().inputs.usb == command.inputs.usb &&
                commands.back().inputs.airflow == command.inputs.airflow &&
                commands.back().inputs.atomizerConnected == command.inputs.atomizerConnected;
            if (sameButtons) commands.back().inputs = command.inputs;
            else if (commands.size() == 128) inputOverflow = true;
            else commands.push_back(command);
        }
        wake.notify_one();
    }
    void Run() noexcept {
        using Clock = std::chrono::steady_clock;
        Machine machine;
        View view;
        uint64_t activeEpoch = 0;
        struct TimedInput { uint64_t time; Inputs inputs; };
        std::deque<TimedInput> scheduledInputs;
        std::optional<Clock::time_point> inputAnchor;
        uint64_t inputAnchorTime = 0;
        Inputs latestInputs;
        std::filesystem::path file;
        std::vector<uint8_t> savedStorage;
        auto lastPublish = Clock::now();
        auto lastSave = lastPublish;
        auto nextRun = lastPublish;
        uint64_t savedRevision = UINT64_MAX;
        auto publish = [&] {
            view.busy = false;
            ++view.sequence;
            auto next = std::make_shared<View>(view);
            if (view.loaded) next->state = machine.Current();
            std::lock_guard lock(mutex);
            if (activeEpoch == epoch) published = std::move(next);
        };
        auto save = [&] {
            if (!view.loaded || file.empty()) return;
            const uint64_t revision = machine.StorageRevision();
            if (revision == savedRevision) return;
            auto bytes = machine.SaveStorage();
            if (bytes == savedStorage) { savedRevision = revision; return; }
            std::error_code directoryError;
            std::filesystem::create_directories(file.parent_path(), directoryError);
            if (directoryError) {
                view.error = "Cannot create virtual Flash directory: " + directoryError.message();
                view.running = false;
                return;
            }
            AtomicFile::Error writeError;
            if (!AtomicFile::Write(file, bytes.data(), bytes.size(), false, writeError)) {
                view.error = "Cannot save virtual Flash (Windows error " + std::to_string(writeError.code) + ")";
                view.running = false;
                return;
            }
            savedStorage = std::move(bytes);
            savedRevision = revision;
        };
        try {
            for (;;) {
                std::optional<LoadRequest> request;
                std::deque<Command> pending;
                bool run = false, stop = false, overflow = false;
                {
                    std::unique_lock lock(mutex);
                    if (!view.running && !load && commands.empty() && !inputOverflow && !stopping &&
                        requestedRunning == view.running)
                        wake.wait(lock, [&] { return stopping || load || !commands.empty() ||
                            inputOverflow || requestedRunning != view.running; });
                    if (view.running && !load && commands.empty() && !inputOverflow && !stopping &&
                        requestedRunning == view.running)
                        wake.wait_until(lock, nextRun, [&] { return stopping || load ||
                            !commands.empty() || inputOverflow || requestedRunning != view.running; });
                    stop = stopping;
                    request = std::move(load);
                    load.reset();
                    pending.swap(commands);
                    run = requestedRunning;
                    overflow = std::exchange(inputOverflow, false);
                }
                if (stop) { save(); break; }
                busy.store(true, std::memory_order_relaxed);
                bool changed = false;
                if (request) {
                    save();
                    activeEpoch = request->epoch;
                    view = {};
                    view.documentIdentity = request->source->DocumentIdentity();
                    view.revision = request->source->ResourceRevision();
                    view.hardwareCode = request->hardwareCode;
                    file = request->file.empty() ? DefaultStorageFile(request->source->edition,
                        request->hardwareCode, storageDirectory) : std::move(request->file);
                    savedStorage.clear();
                    savedRevision = UINT64_MAX;
                    nextRun = Clock::now();
                    scheduledInputs.clear();
                    inputAnchor.reset();
                    latestInputs = {};
                    view.loaded = ReadStorage(file, savedStorage, view.error) &&
                        machine.Open(*request->source, request->hardwareCode, savedStorage, view.error);
                    changed = true;
                }
                if (overflow) {
                    view.error = "Virtual button event queue overflow; device paused";
                    view.running = false;
                    std::lock_guard lock(mutex);
                    requestedRunning = false;
                    changed = true;
                }
                if (view.loaded && view.error.empty()) {
                    for (const auto& command : pending) {
                        if (command.action == Action::Inputs) {
                            latestInputs = command.inputs;
                            if (!run) machine.SetInputs(command.inputs);
                            else {
                                if (!inputAnchor) {
                                    inputAnchor = command.captured;
                                    inputAnchorTime = machine.Current().virtualMicroseconds;
                                }
                                if (scheduledInputs.size() == 128) {
                                    view.error = "Virtual button timeline overflow; device paused";
                                    break;
                                }
                                const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                                    command.captured - *inputAnchor).count();
                                scheduledInputs.push_back({inputAnchorTime +
                                    static_cast<uint64_t>((std::max)(int64_t(0), elapsed)), command.inputs});
                            }
                        } else if (command.action == Action::Reset) {
                            save();
                            scheduledInputs.clear();
                            inputAnchor.reset();
                            if (!view.error.empty() || !machine.Reset(view.error)) break;
                        } else if (!machine.Step(view.error)) break;
                        changed = true;
                    }
                    view.running = run && view.error.empty() && !overflow;
                    if (!view.running) {
                        scheduledInputs.clear();
                        inputAnchor.reset();
                        machine.SetInputs(latestInputs);
                    } else {
                        // Keep both press durations and gaps on the guest clock.
                        // A slow LCD draw may delay delivery, never collapse edges.
                        while (!scheduledInputs.empty() &&
                            scheduledInputs.front().time <= machine.Current().virtualMicroseconds) {
                            machine.SetInputs(scheduledInputs.front().inputs);
                            scheduledInputs.pop_front();
                        }
                        const auto hostNow = Clock::now();
                        const uint64_t target = inputAnchor ? inputAnchorTime + static_cast<uint64_t>(
                            (std::max)(int64_t(0), std::chrono::duration_cast<std::chrono::microseconds>(
                                hostNow - *inputAnchor).count())) : machine.Current().virtualMicroseconds;
                        if (machine.Current().virtualMicroseconds < target) nextRun = hostNow;
                        if (hostNow >= nextRun) {
                            uint32_t quantum = 2000;
                            if (!scheduledInputs.empty())
                                quantum = static_cast<uint32_t>((std::min)(uint64_t(quantum),
                                    scheduledInputs.front().time - machine.Current().virtualMicroseconds));
                            if (!machine.RunFor(quantum, view.error)) view.running = false;
                            const uint64_t current = machine.Current().virtualMicroseconds;
                            const uint64_t ahead = current > target ? (std::min)(current - target, uint64_t(2000)) : 0;
                            nextRun = hostNow + std::chrono::microseconds(ahead);
                        }
                    }
                    if (!view.running || Clock::now() - lastPublish >= std::chrono::milliseconds(33)) {
                        changed = true;
                    }
                } else view.running = false;
                if (!view.error.empty()) {
                    std::lock_guard lock(mutex);
                    requestedRunning = false;
                }
                const auto now = Clock::now();
                if (!view.running || now - lastSave >= std::chrono::milliseconds(500)) {
                    save();
                    lastSave = now;
                }
                if (changed || !view.running) { publish(); lastPublish = now; }
                busy.store(false, std::memory_order_relaxed);
            }
        } catch (const std::exception& exception) {
            view.error = exception.what();
            view.running = false;
            publish();
        } catch (...) {
            view.error = "Unexpected device worker failure";
            view.running = false;
            publish();
        }
        busy.store(false, std::memory_order_relaxed);
    }
};

Worker::Worker(std::filesystem::path storageDirectory) : impl_(std::make_unique<Impl>(std::move(storageDirectory))) {}
Worker::~Worker() = default;
void Worker::Load(std::shared_ptr<const StudioState> source, std::filesystem::path storageFile,
    bool initiallyRunning) {
    Load(std::move(source), 0x3103, std::move(storageFile), initiallyRunning);
}
void Worker::Load(std::shared_ptr<const StudioState> source, uint16_t hardwareCode,
    std::filesystem::path storageFile, bool initiallyRunning) {
    auto& worker = *impl_;
    {
        std::lock_guard lock(worker.mutex);
        const uint64_t epoch = ++worker.epoch;
        worker.load = Impl::LoadRequest{std::move(source), std::move(storageFile),
            hardwareCode ? hardwareCode : uint16_t(0x3103), epoch};
        worker.commands.clear();
        worker.inputOverflow = false;
        worker.requestedRunning = initiallyRunning;
        worker.published.reset();
    }
    worker.visible = std::make_shared<View>();
    worker.wake.notify_one();
}
void Worker::SetRunning(bool running) {
    { std::lock_guard lock(impl_->mutex); impl_->requestedRunning = running; }
    impl_->wake.notify_one();
}
void Worker::SetInputs(const Inputs& inputs) { impl_->Push({Impl::Action::Inputs, inputs}); }
void Worker::Reset() { impl_->Push({Impl::Action::Reset}); }
void Worker::Step() { SetRunning(false); impl_->Push({Impl::Action::Step}); }
bool Worker::Poll() {
    std::lock_guard lock(impl_->mutex);
    if (!impl_->published || impl_->published == impl_->visible) return false;
    impl_->visible = impl_->published;
    return true;
}
const View& Worker::Current() const { return *impl_->visible; }
bool Worker::Busy() const { return impl_->busy.load(std::memory_order_relaxed); }
}
