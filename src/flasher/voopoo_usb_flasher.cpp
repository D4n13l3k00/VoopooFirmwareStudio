#include "voopoo_usb_flasher.h"
#include "zwdf_package.h"
#include "flash_protocol.h"
#include <memory>
#include <stdexcept>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>

#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

namespace voopoo {

static constexpr uint16_t VENDOR_ID = 0x0483;
static constexpr uint16_t PRODUCT_ID = 0x5750;
static constexpr uint8_t REPORT_ID = 0x02;
static constexpr size_t REPORT_SIZE = 64;
static constexpr size_t MAX_PAYLOAD_PER_REPORT = 63;

UsbFlasher::UsbFlasher() {
    // Monitoring is explicitly enabled by the interactive UI only.
}

UsbFlasher::~UsbFlasher() {
    CancelFlash();
    StopMonitor();
    if (m_flash_thread.joinable()) {
        m_flash_thread.join();
    }
}

void UsbFlasher::StartMonitor() {
    if (!m_running.exchange(true)) {
        m_monitor_thread = std::thread(&UsbFlasher::MonitorLoop, this);
    }
}

void UsbFlasher::StopMonitor() {
    if (m_running.exchange(false)) {
        if (m_monitor_thread.joinable()) {
            m_monitor_thread.join();
        }
    }
}

DeviceInfo UsbFlasher::GetDeviceInfo() {
    std::lock_guard<std::mutex> lock(m_device_mutex);
    return m_device_info;
}

FlashProgress UsbFlasher::GetFlashProgress() {
    std::lock_guard<std::mutex> lock(m_progress_mutex);
    return m_progress;
}

void* UsbFlasher::OpenDevice() {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO devInfo = SetupDiGetClassDevsA(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfo == INVALID_HANDLE_VALUE) return nullptr;

    HANDLE selected = nullptr;
    bool ambiguous = false;
    SP_DEVICE_INTERFACE_DATA devInterfaceData;
    devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(devInfo, NULL, &hidGuid, i, &devInterfaceData); i++) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailA(devInfo, &devInterfaceData, NULL, 0, &requiredSize, NULL);
        if (requiredSize == 0) continue;

        std::vector<uint8_t> detailBuffer(requiredSize);
        PSP_DEVICE_INTERFACE_DETAIL_DATA_A pDetail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_A>(detailBuffer.data());
        pDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        if (!SetupDiGetDeviceInterfaceDetailA(devInfo, &devInterfaceData, pDetail, requiredSize, NULL, NULL)) {
            continue;
        }

        HANDLE hDev = CreateFileA(pDetail->DevicePath,
                                  GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL,
                                  OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED,
                                  NULL);

        if (hDev != INVALID_HANDLE_VALUE) {
            HIDD_ATTRIBUTES attrib;
            attrib.Size = sizeof(HIDD_ATTRIBUTES);
            if (HidD_GetAttributes(hDev, &attrib)) {
                if (attrib.VendorID == VENDOR_ID && attrib.ProductID == PRODUCT_ID) {
                    PHIDP_PREPARSED_DATA pp = nullptr;
                    HIDP_CAPS caps{};
                    bool validCaps = false;
                    if (HidD_GetPreparsedData(hDev, &pp)) {
                        validCaps = HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS &&
                            caps.InputReportByteLength == 64 && caps.OutputReportByteLength == 64;
                        HidD_FreePreparsedData(pp);
                    }
                    wchar_t product[256]{};
                    const bool validProduct = HidD_GetProductString(hDev, product, sizeof(product)) &&
                        std::wstring(product).rfind(L"VOOPOO", 0) == 0;
                    if (validCaps && validProduct) {
                        if (selected) { CloseHandle(hDev); ambiguous = true; break; }
                        selected = hDev;
                        continue;
                    }
                }
            }
            CloseHandle(hDev);
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    if (ambiguous) { CloseHandle(selected); return nullptr; }
    if (selected && !HidD_FlushQueue(selected)) { CloseHandle(selected); return nullptr; }
    return selected;
}

void UsbFlasher::CloseDevice(void* handle) {
    if (handle && handle != INVALID_HANDLE_VALUE) {
        CloseHandle(reinterpret_cast<HANDLE>(handle));
    }
}

bool UsbFlasher::WriteReport(void* handle, const uint8_t* data, size_t len) {
    if (!handle || handle == INVALID_HANDLE_VALUE || len > 63) return false;

    uint8_t report[REPORT_SIZE] = {0};
    report[0] = REPORT_ID;
    size_t copy_len = (len < MAX_PAYLOAD_PER_REPORT) ? len : MAX_PAYLOAD_PER_REPORT;
    if (data && copy_len > 0) {
        memcpy(report + 1, data, copy_len);
    }

    OVERLAPPED ov = {0};
    ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!ov.hEvent) return false;

    DWORD written = 0;
    BOOL res = WriteFile(reinterpret_cast<HANDLE>(handle), report, REPORT_SIZE, &written, &ov);
    if (!res && GetLastError() == ERROR_IO_PENDING) {
        if (WaitForSingleObject(ov.hEvent, 1000) == WAIT_OBJECT_0) {
            if (!GetOverlappedResult(reinterpret_cast<HANDLE>(handle), &ov, &written, FALSE)) written = 0;
        } else {
            CancelIoEx(reinterpret_cast<HANDLE>(handle), &ov);
            GetOverlappedResult(reinterpret_cast<HANDLE>(handle), &ov, &written, TRUE);
            written = 0;
        }
    }
    CloseHandle(ov.hEvent);
    return written == REPORT_SIZE;
}

int UsbFlasher::ReadReport(void* handle, uint8_t* buffer, size_t max_len, int timeout_ms) {
    if (!handle || handle == INVALID_HANDLE_VALUE) return 0;

    uint8_t report[REPORT_SIZE] = {0};
    OVERLAPPED ov = {0};
    ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!ov.hEvent) return 0;

    DWORD bytesRead = 0;
    BOOL res = ReadFile(reinterpret_cast<HANDLE>(handle), report, REPORT_SIZE, &bytesRead, &ov);
    if (!res && GetLastError() == ERROR_IO_PENDING) {
        if (WaitForSingleObject(ov.hEvent, timeout_ms) == WAIT_OBJECT_0) {
            if (!GetOverlappedResult(reinterpret_cast<HANDLE>(handle), &ov, &bytesRead, FALSE)) bytesRead = 0;
        } else {
            CancelIoEx(reinterpret_cast<HANDLE>(handle), &ov);
            GetOverlappedResult(reinterpret_cast<HANDLE>(handle), &ov, &bytesRead, TRUE);
            bytesRead = 0;
        }
    }
    CloseHandle(ov.hEvent);

    if (bytesRead == REPORT_SIZE && report[0] == REPORT_ID) {
        size_t offset = 1;
        size_t count = (bytesRead > offset) ? (bytesRead - offset) : 0;
        if (count > max_len) count = max_len;
        if (count > 0 && buffer) {
            memcpy(buffer, report + offset, count);
        }
        return static_cast<int>(count);
    }
    return 0;
}

bool UsbFlasher::TransmitFrame(void* handle, const std::vector<uint8_t>& frame, std::vector<uint8_t>& response, int timeout_ms, std::string& error) {
    response.clear(); error.clear();
    protocol::Frame decoded;
    if (!handle || !protocol::Decode(frame, decoded)) { error = "Invalid outgoing frame"; return false; }
    for (size_t offset = 0; offset < frame.size(); offset += 63) {
        if (!WriteReport(handle, frame.data() + offset, (std::min)(size_t(63), frame.size() - offset))) {
            error = "USB write failed; update stopped without retry"; return false;
        }
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    size_t expected = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        uint8_t chunk[63];
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        const int count = ReadReport(handle, chunk, sizeof(chunk), int((std::max)(int64_t(1), remaining)));
        if (!count) break;
        response.insert(response.end(), chunk, chunk + count);
        if (!expected && response.size() >= 6) {
            expected = 9 + (size_t(response[4]) << 8) + response[5];
            if (response[0] != 0x68 || expected > 4096) { error = "Malformed USB reply"; return false; }
        }
        if (expected && response.size() >= expected) {
            response.resize(expected);
            return protocol::Reply(frame, response, error);
        }
    }
    error = "USB reply timeout or disconnected device; completion is unknown";
    return false;
}

bool UsbFlasher::QueryIdentity(void* handle, DeviceInfo& info, std::string& error) {
    info = {};
    wchar_t wide[256]{}; char utf8[1024]{};
    if (!HidD_GetProductString(handle, wide, sizeof(wide))) { error = "Missing USB product"; return false; }
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), nullptr, nullptr); info.product = utf8;
    if (info.product.rfind("VOOPOO", 0) != 0) { error = "Unsupported USB product"; return false; }
    protocol::Bytes reply;
    bool ok = TransmitFrame(handle, protocol::Encode(0xAAAA, 0x44), reply, 500, error);
    if (!ok) ok = TransmitFrame(handle, protocol::Encode(0xAAAA, 0x36), reply, 500, error);
    protocol::Frame frame;
    if (!ok || !protocol::Decode(reply, frame)) return false;
    info.device_address = frame.address;
    char text[32]; snprintf(text, sizeof(text), "%04X", frame.address); info.hardware_id = text;
    if (frame.payload.size() >= 2) {
        snprintf(text, sizeof(text), "0x%02X%02X", frame.payload[0], frame.payload[1]); info.firmware_version = text;
    }
    if (HidD_GetSerialNumberString(handle, wide, sizeof(wide))) {
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), nullptr, nullptr); info.serial = utf8;
    }
    if (HidD_GetManufacturerString(handle, wide, sizeof(wide))) {
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof(utf8), nullptr, nullptr); info.manufacturer = utf8;
    }
    info.in_bootloader = info.product.rfind("VOOPOO-BOOT", 0) == 0;
    info.connected = true; return true;
}

void UsbFlasher::MonitorLoop() {
    while (m_running) {
        if (!m_flashing) {
            std::lock_guard io(m_io_mutex);
            if (!m_flashing) {
                DeviceInfo info;
                void* handle = nullptr;
                try { handle = OpenDevice(); std::string error; if (handle) QueryIdentity(handle, info, error); }
                catch (...) { info = {}; }
                CloseDevice(handle);
                std::lock_guard lock(m_device_mutex); m_device_info = std::move(info);
            }
        }
        for (int i = 0; i < 10 && m_running; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool UsbFlasher::StartFlash(const std::vector<uint8_t>& bytes, uint32_t code) {
    if (bytes.empty() || code > 0xFFFF || m_flashing.exchange(true)) return false;
    if (m_flash_thread.joinable()) m_flash_thread.join();
    m_cancel_requested = false;
    { std::lock_guard lock(m_progress_mutex); m_progress = {}; m_progress.state = FlashState::CheckingFile; }
    try { m_flash_thread = std::thread(&UsbFlasher::FlashWorker, this, bytes, code); }
    catch (...) { m_flashing = false; return false; }
    return true;
}
void UsbFlasher::CancelFlash() { m_cancel_requested = true; }
void UsbFlasher::FlashWorker(std::vector<uint8_t> bytes, uint32_t code) {
    const auto state = [this](FlashState s, float p, const std::string& text, const std::string& error = "") {
        std::lock_guard lock(m_progress_mutex);
        m_progress.state = s; m_progress.progress = p; m_progress.status_text = text; m_progress.error_message = error;
    };
    void* handle = nullptr;
    try {
        std::lock_guard io(m_io_mutex);
        FirmwarePackage package; protocol::Plan plan; std::string error;
        if (!FirmwarePackage::Parse(bytes, package, error) || !protocol::MakePlan(package, uint16_t(code), plan, error)) throw std::runtime_error(error);
        handle = OpenDevice(); DeviceInfo info;
        if (!handle) throw std::runtime_error("Connect exactly one supported VOOPOO HID device (64-byte reports)");
        if (!QueryIdentity(handle, info, error)) throw std::runtime_error(error);
        if (info.device_address != plan.code) throw std::runtime_error("Device hardware ID differs from selected record; no update commands sent");
        if (m_cancel_requested) throw std::runtime_error("Update cancelled before bootloader entry");
        const auto serial = info.serial;
        if (!info.in_bootloader) {
            state(FlashState::EnteringBootloader, .05f, "Requesting bootloader...");
            const protocol::Bytes unlock{'v','o','o','p','o','-','t','e','s','t','-','0','0','0','0',0};
            protocol::Bytes response;
            const bool acknowledged = TransmitFrame(handle, protocol::Encode(plan.code, 0x37, unlock), response, 1000, error);
            // A050 application may reset without an ACK. A received rejection or
            // corrupt reply is never ignored; an empty reply requires fresh boot identity.
            if (!acknowledged && !response.empty()) throw std::runtime_error(error);
            CloseDevice(handle); handle = nullptr;
            state(FlashState::WaitingBootloader, .10f, "Waiting for matching VOOPOO-BOOT device...");
            for (int i = 0; i < 40 && !m_cancel_requested; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                handle = OpenDevice();
                if (handle && QueryIdentity(handle, info, error) && info.in_bootloader && info.device_address == plan.code &&
                    (serial.empty() || info.serial.empty() || info.serial == serial)) break;
                CloseDevice(handle); handle = nullptr;
            }
            if (!handle) throw std::runtime_error(m_cancel_requested ? "Update cancelled" : "Matching bootloader did not reconnect; no image blocks written");
        }
        state(FlashState::InitSectors, .15f, "Preparing firmware storage...");
        auto exchange = [&](const protocol::Bytes& request, protocol::Bytes& response, int timeout, std::string& err) {
            if (request[3] == 0x20) state(FlashState::Finalizing, .95f, "Sending final image length and checksum...");
            return TransmitFrame(handle, request, response, timeout, err);
        };
        auto progress = [&](size_t block, size_t total) {
            std::lock_guard lock(m_progress_mutex);
            m_progress.state = FlashState::WritingBlocks; m_progress.current_block = int(block); m_progress.total_blocks = int(total);
            m_progress.progress = .2f + .7f * float(block) / float(total);
            m_progress.status_text = "Writing block " + std::to_string(block) + " / " + std::to_string(total);
        };
        if (!protocol::Program(plan, info.device_address, info.in_bootloader, exchange,
                [this] { return m_cancel_requested.load(); }, progress, error)) throw std::runtime_error(error);
        state(FlashState::FinishedSuccess, 1.f, "Device acknowledged all blocks and final length/checksum.");
    } catch (const std::exception& e) { state(FlashState::Failed, 0.f, "Update stopped", e.what()); }
      catch (...) { state(FlashState::Failed, 0.f, "Update stopped", "Unexpected error; device state is unknown"); }
    CloseDevice(handle);
    m_flashing = false;
}
} // namespace voopoo
