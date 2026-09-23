#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

namespace voopoo {

// Device Information queried over USB HID
struct DeviceInfo {
    bool connected = false;
    std::string manufacturer;
    std::string product;
    std::string serial;
    uint16_t device_address = 0xAAAA;
    std::string hardware_id;      // e.g. "3103"
    std::string firmware_version; // e.g. "V1.0.0.1"
    bool in_bootloader = false;
};

// Flashing status and progress
enum class FlashState {
    Idle,
    CheckingFile,
    EnteringBootloader,
    WaitingBootloader,
    InitSectors,
    WritingBlocks,
    Finalizing,
    FinishedSuccess,
    Failed
};

struct FlashProgress {
    FlashState state = FlashState::Idle;
    float progress = 0.0f;        // 0.0f to 1.0f
    int current_block = 0;
    int total_blocks = 0;
    std::string status_text;
    std::string error_message;
};

class UsbFlasher {
public:
    UsbFlasher();
    ~UsbFlasher();

    // Start background device monitoring thread
    void StartMonitor();
    void StopMonitor();
    void StartMonitoring(int /*interval_ms*/ = 1000) { StartMonitor(); }
    void StopMonitoring() { StopMonitor(); }

    // Query current device info (thread-safe)
    DeviceInfo GetDeviceInfo();

    // Query current flashing progress (thread-safe)
    FlashProgress GetFlashProgress();

    // Start flashing async
    bool StartFlash(const std::vector<uint8_t>& zwdf_data, uint32_t target_record_code = 0x3103);
    bool StartFlashAsync(const std::vector<uint8_t>& zwdf_data, uint32_t target_record_code = 0x3103) {
        return StartFlash(zwdf_data, target_record_code);
    }

    // Cancel in-progress flashing
    void CancelFlash();

private:
    void MonitorLoop();
    void FlashWorker(std::vector<uint8_t> zwdf_data, uint32_t target_record_code);

    // Low-level Windows HID communication
    void* OpenDevice();
    bool QueryIdentity(void* handle, DeviceInfo& info, std::string& error);
    void CloseDevice(void* handle);
    bool WriteReport(void* handle, const uint8_t* data, size_t len);
    int ReadReport(void* handle, uint8_t* buffer, size_t max_len, int timeout_ms);
    bool TransmitFrame(void* handle, const std::vector<uint8_t>& frame, std::vector<uint8_t>& response, int timeout_ms, std::string& error);

    std::atomic<bool> m_running{false};
    std::thread m_monitor_thread;

    std::atomic<bool> m_flashing{false};
    std::atomic<bool> m_cancel_requested{false};
    std::thread m_flash_thread;

    std::mutex m_io_mutex;
    std::mutex m_device_mutex;
    DeviceInfo m_device_info;

    std::mutex m_progress_mutex;
    FlashProgress m_progress;
};

} // namespace voopoo
