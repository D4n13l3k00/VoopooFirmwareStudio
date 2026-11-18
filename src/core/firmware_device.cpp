#include "firmware_device.h"
#include "firmware_device_regression.h"
#include "app_state.h"
#include "firmware_layout.h"
#include "firmware_logo_patch.h"
#include "firmware_menu_patch.h"
#include "firmware_emulator_sx_score.h"
#include "firmware_emulator_sx_factory.h"
#include <unicorn/unicorn.h>
#include <unicorn/arm.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sstream>
#include <utility>

namespace FirmwareDevice {
namespace {
constexpr uint32_t RamBase = 0x20000000, RamSize = 0x4000;
constexpr uint32_t SettingsBase = 0x0801f000, SettingsSize = 0x1000;
constexpr unsigned AccessLimit = 96;
constexpr std::array<uint32_t, 8> TimerBases{
    0x40000000, 0x40000400, 0x40000800, 0x40012c00,
    0x40002000, 0x40014000, 0x40014400, 0x40014800};
constexpr std::array<unsigned, 8> N32TimerIrqs{28, 29, 30, 25, 0, 0, 0, 0};
constexpr std::array<unsigned, 8> Gd32TimerIrqs{15, 16, 0, 13, 19, 20, 21, 22};
constexpr uint32_t FirePin = 1u << 11, PlusPin = 1u << 2, MinusPin = 1u << 10;
constexpr uint32_t UsbPin = 1u << 7, AirflowPin = 1u << 4;
uint32_t ButtonLevels(const Inputs& inputs) {
    // Both MCU board families wire the active-low switches to PB11/PB2/PB10.
    return (FirePin | PlusPin | MinusPin) & ~(inputs.fire ? FirePin : 0) &
        ~(inputs.plus ? PlusPin : 0) & ~(inputs.minus ? MinusPin : 0);
}
uint32_t PortALevels(const Inputs& inputs) {
    // Native GPIO_ReadInputDataBit(PA4) returns one on draw activation:
    // Score3013 08006d8e..6da2; Factory3013 08003cb6..3cc0.
    return (inputs.usb ? UsbPin : 0) | (inputs.airflow ? AirflowPin : 0);
}
uint32_t Le32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void Put32(std::vector<uint8_t>& bytes, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<uint8_t>(v >> (i * 8)));
}
std::string Hex(uint32_t value) { char text[16]; std::snprintf(text, sizeof(text), "0x%08X", value); return text; }
const HardwareRecord* Record(const FirmwarePackage& p, uint16_t code) {
    const auto it = std::find_if(p.records.begin(), p.records.end(), [code](const HardwareRecord& r) {
        return (uint16_t(r.device_code[0]) << 8 | r.device_code[1]) == code;
    });
    return it == p.records.end() ? nullptr : &*it;
}
uint32_t Pixel(uint16_t v) {
    const unsigned r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
    return 0xff000000u | (r << 3 | r >> 2) | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2) << 16;
}
bool PrepareImage(const StudioState& source, uint16_t code, std::vector<uint8_t>& image,
    uint32_t& imageBase, uint32_t& menuStateAddress, std::string& error) {
    if (!source.hasFirmware || source.model != FirmwareModel::DragS ||
        (source.edition != FirmwareEdition::Score && source.edition != FirmwareEdition::Factory)) {
        error = "Whole-device mode supports normal DRAG S Score and Factory firmware"; return false;
    }
    std::vector<uint8_t> bytes;
    FirmwarePackage built;
    if (!source.BuildZwdfBytes(bytes, error) || !FirmwarePackage::Parse(bytes, built, error)) return false;
    const auto* actual = Record(built, code);
    FirmwarePackage stock;
    if (!LoadEmbeddedFirmware(stock, error, FirmwareModel::DragS, source.edition)) return false;
    const auto* baseline = Record(stock, code);
    if (!actual || !baseline || actual->linked_base != baseline->linked_base ||
        actual->sp != baseline->sp || actual->reset != baseline->reset ||
        actual->image.size() < baseline->image.size() ||
        uint64_t(actual->linked_base) + actual->image.size() >= SettingsBase) {
        error = "Unsupported hardware record, reset vector, stack, or native image size for " + Hex(code); return false;
    }
    imageBase = actual->linked_base;
    menuStateAddress = FirmwareMenu::StateAddress(*actual);
    // Rebuild the trusted executable for this edition and revision independently.
    // Resource data may differ; all executable extensions must match the builder.
    StudioState trusted;
    if (!trusted.LoadPackage(std::move(stock), "device executable baseline", error)) return false;
    for (auto& color : trusted.palette) {
        const auto found = std::find_if(source.palette.begin(), source.palette.end(), [&](const PaletteState& c) {
            return std::string_view(c.spec.id) == color.spec.id;
        });
        if (found != source.palette.end()) color.current = found->current;
    }
    trusted.backgroundColor = source.backgroundColor;
    trusted.layoutOffsets = source.layoutOffsets;
    trusted.screenPatches = source.screenPatches;
    trusted.menuPatch = source.menuPatch;
    FirmwarePackage expected;
    if (!trusted.BuildZwdfBytes(bytes, error) || !FirmwarePackage::Parse(bytes, expected, error)) return false;
    const auto* reference = Record(expected, code);
    if (!reference || reference->image.size() != actual->image.size()) { error = "Invalid embedded hardware baseline"; return false; }
    std::vector<bool> resources(actual->image.size(), false);
    const auto* stockRecord = Record(trusted.firmware, code);
    const size_t index = static_cast<size_t>(stockRecord - trusted.firmware.records.data());
    for (size_t i = 0; i < trusted.catalog.size(); ++i) {
        if (i >= trusted.locations.size() || index >= trusted.locations[i].size()) continue;
        // Locations contain one address per record, not arbitrary caller-supplied intervals.
        const size_t offset = trusted.locations[i][index], size = trusted.catalog[i].byte_len();
        if (offset <= resources.size() && size <= resources.size() - offset)
            std::fill(resources.begin() + offset, resources.begin() + offset + size, true);
    }
    for (size_t i = 0; i < resources.size(); ++i) {
        if (!resources[i] && actual->image[i] != reference->image[i]) {
            error = "Unsupported whole-device executable change at " + Hex(imageBase + static_cast<uint32_t>(i)); return false;
        }
    }
    image = actual->image;
    return true;
}

// Recovered from each image's power-on tail, scheduler back edge, settings
// loads/stores and ADC consumer. Screen entrypoints reuse the native renderer's
// independently mapped records. These sites only observe executed instructions.
struct ObservationProfile {
    FirmwareEdition edition;
    uint16_t code;
    uint32_t bootTail, scheduler, settings, sensors, page, usbTail;
    bool history;
};
constexpr ObservationProfile ObservationProfiles[] = {
    {FirmwareEdition::Score, 0x314d, 0x080083ba, 0x0800609e, 0x200018a8, 0x200019d0, 0x2000008c, 0x0800cd6a, false},
    {FirmwareEdition::Score, 0x3014, 0x08005e88, 0x08004260, 0x20001530, 0x20001660, 0x2000005c, 0x0800f916, true},
    {FirmwareEdition::Score, 0x302a, 0x08005c82, 0x08004310, 0x20001520, 0x20001624, 0x2000005c, 0x0800e916, false},
    {FirmwareEdition::Score, 0x3013, 0x08005e88, 0x08004260, 0x20001530, 0x20001660, 0x2000005c, 0x0800f906, true},
    {FirmwareEdition::Score, 0x3029, 0x08005c82, 0x08004310, 0x20001520, 0x20001624, 0x2000005c, 0x0800e906, false},
    {FirmwareEdition::Score, 0x3103, 0x080083ba, 0x0800609e, 0x200018a8, 0x200019d0, 0x2000008c, 0x0800cd92, false},
    {FirmwareEdition::Score, 0x3105, 0x080083ba, 0x0800609e, 0x200018a8, 0x200019d0, 0x2000008c, 0x0800cd92, false},
    {FirmwareEdition::Factory, 0x3148, 0x08005b2e, 0x08003bfc, 0x20001518, 0x20001618, 0x20000084, 0x0800d2c6, false},
    {FirmwareEdition::Factory, 0x3014, 0x08005708, 0x08003b4c, 0x20001528, 0x20001654, 0x2000008e, 0x0800e76e, true},
    {FirmwareEdition::Factory, 0x3104, 0x080088ba, 0x08006198, 0x20001498, 0x200015bc, 0x2000008c, 0x0800c8fe, false},
    {FirmwareEdition::Factory, 0x3013, 0x08005708, 0x08003b4c, 0x20001528, 0x20001654, 0x2000008e, 0x0800e75e, true},
    {FirmwareEdition::Factory, 0x3029, 0x08005b2e, 0x08003bfc, 0x20001518, 0x20001618, 0x20000084, 0x0800d1fa, false},
    {FirmwareEdition::Factory, 0x3103, 0x08008a7e, 0x08006138, 0x20001498, 0x200015bc, 0x2000008c, 0x0800c9d6, false},
    {FirmwareEdition::Factory, 0x3034, 0x08005b2e, 0x08003bfc, 0x20001518, 0x20001618, 0x20000084, 0x0800d486, false},
    {FirmwareEdition::Factory, 0x3105, 0x080088ba, 0x08006198, 0x20001498, 0x200015bc, 0x2000008c, 0x0800c8f2, false},
    {FirmwareEdition::Factory, 0x314d, 0x080088f6, 0x0800619c, 0x20001498, 0x200015bc, 0x2000008c, 0x0800c912, false},
};
struct VoltageObservationProfile {
    FirmwareEdition edition;
    uint16_t code;
    uint32_t address;
    bool combined;
};
constexpr VoltageObservationProfile VoltageObservationProfiles[] = {
    {FirmwareEdition::Score,0x3013,0x08008f0c,false}, {FirmwareEdition::Score,0x3014,0x08008f14,false},
    {FirmwareEdition::Score,0x3029,0x080088d4,false}, {FirmwareEdition::Score,0x302a,0x080088dc,false},
    {FirmwareEdition::Score,0x3103,0x0800ba80,false}, {FirmwareEdition::Score,0x3105,0x0800ba80,false},
    {FirmwareEdition::Score,0x314d,0x0800ba58,false},
    {FirmwareEdition::Factory,0x3013,0x0800886c,false}, {FirmwareEdition::Factory,0x3014,0x08008874,false},
    {FirmwareEdition::Factory,0x3029,0x08007ad8,true}, {FirmwareEdition::Factory,0x3034,0x08007ad8,true},
    {FirmwareEdition::Factory,0x3148,0x08007b68,true}, {FirmwareEdition::Factory,0x3103,0x0800b05c,true},
    {FirmwareEdition::Factory,0x3104,0x0800b168,true}, {FirmwareEdition::Factory,0x3105,0x0800b15c,true},
    {FirmwareEdition::Factory,0x314d,0x0800b174,true},
};
} // namespace

// High-base records use Nations N32G43x; low-base records use GD32F3x0.
// Register maps and IRQs follow the vendors' CMSIS/SPL, not a shared 3103 ABI:
// https://github.com/axfnull/N32G43x_CodeLib/tree/main/firmware
// https://github.com/CommunityGD32Cores/gd32-pio-spl-package/tree/main/gd32
// Exact package/pin-count cannot be established from an update image alone.
// Timing is deterministic, not cycle accurate: each instruction consumes one
// programmed HCLK tick, including false IT predicates. Native SystemInit selects
// 72 MHz (N32) or IRC48M /2 *4 = 96 MHz (GD32).
// Oscillator, ADC calibration and Flash acknowledge after one virtual us.
// No guest function is replaced, skipped, or called by the host.
// Peripheral counters synchronize at MMIO/timed events, not per microsecond;
// WFI advances directly to the next real peripheral deadline or RunFor target.
struct Machine::Impl {
    uc_engine* cpu = nullptr;
    std::vector<uint8_t> image;
    uint32_t imageBase = 0;
    uint32_t menuStateAddress = 0;
    uint16_t hardwareCode = 0x3103;
    FirmwareEdition edition = FirmwareEdition::Score;
    const ObservationProfile* observation = nullptr;
    const FirmwareEmulator::SxScreenProfile* screens = nullptr;
    uint32_t offEntry = 0, warningFirst = 0, warningLast = 0;
    bool Gd32() const { return imageBase == 0x08003000; }
    uint32_t GpioA() const { return Gd32() ? 0x48000000 : 0x40010800; }
    uint32_t GpioB() const { return Gd32() ? 0x48000400 : 0x40010c00; }
    uint32_t Spi() const { return Gd32() ? 0x40003800 : 0x40013c00; }
    uint32_t AdcBase() const { return Gd32() ? 0x40012400 : 0x40020800; }
    uint32_t FlashPageSize() const { return Gd32() ? 0x400 : 0x800; }
    unsigned DmaIrq() const { return Gd32() ? 9 : 11; }
    unsigned TimerIrq(size_t i) const { return Gd32() ? Gd32TimerIrqs[i] : N32TimerIrqs[i]; }
    bool HasTimer(size_t i) const { return Gd32() ? i != 2 : i < 4; }
    bool ShiftedPanel() const {
        return hardwareCode == 0x3014 || hardwareCode == 0x302a || hardwareCode == 0x3104;
    }
    unsigned LcdX() const { return ShiftedPanel() ? 26 : 24; }
    unsigned LcdY() const { return ShiftedPanel() ? 1 : 0; }
    bool VendorPanel() const { return hardwareCode == 0x3105 || hardwareCode == 0x3034; }
    bool FeEfPanel() const { return hardwareCode == 0x314d || hardwareCode == 0x3148; }
    unsigned IrqCount() const { return Gd32() ? 68u : 64u; }
    bool NvicBank(uint32_t word, uint32_t base) const {
        return word >= base && word < base + (Gd32() ? 12u : 8u);
    }
    std::array<uint32_t, 0x800 / 4> gpioBus{};
    std::array<uint8_t, SettingsSize> settings{};
    std::array<uint32_t, 0x30000 / 4> bus{};
    std::array<uint32_t, 0x1000 / 4> usbBus{};
    uint64_t usbResetDue = 0;
    std::array<uint32_t, 0x2000 / 4> core{};
    std::array<uint64_t, TimerBases.size()> timerCycles{};
    Snapshot snapshot;
    Inputs inputs;
    std::array<Access, AccessLimit> log{};
    size_t logNext = 0, logSize = 0;
    uint64_t revision = 0, target = 0, stepTarget = 0, systickCycles = 0, adcDue = 0, watchdogDue = 0;
    uint64_t clockAck = 0, adcAck = 0, flashAck = 0;
    uint64_t rtcCycles = 0, rtcAck = 0;
    unsigned rtcKey = 0;
    uint32_t dwtControl = 0, dwtCount = 0;
    uint64_t dwtFraction = 0;
    uint64_t peripheralTime = 0, nextEvent = (std::numeric_limits<uint64_t>::max)();
    std::array<uint64_t, 84> exceptionInstructions{};
    uint64_t bootCompletions = 0, mainIterations = 0, serviceEntries = 0, powerOffEntries = 0;
    uint64_t usbInitializations = 0;
    std::array<uint64_t, 17> screenEntries{};
    uint64_t adcConversions = 0, pwmStarts = 0;
    const VoltageObservationProfile* voltageObservation = nullptr;
    uint32_t voltageCentivolts = 0;
    uint64_t voltageRenders = 0;
    std::array<uint64_t, 2> comparatorDue{};
    std::array<bool, 2> comparatorTarget{};
    bool adcConverting = false;
    bool anyEnabledPending = false;
    uint32_t pendingSystem = 0, flashKey = 0, activeDepth = 0;
    uint32_t hclk = 16000000;
    uint16_t thermistorSample = 2047;
    std::array<uint32_t, 16> active{};
    uint32_t cpuPhase = 0;
    bool powerRemoved = false;
    unsigned interrupt = 0;
    bool opened = false, stepping = false, watchdog = false, resetRequested = false, instructionPending = false;
    bool dc = false, panelOn = false, panelSleep = true, backlightOn = false, highPending = false;
    uint8_t command = 0, high = 0;
    std::array<uint8_t, 32> params{};
    unsigned paramCount = 0, paramExpected = 0;
    uint16_t x0 = 0, x1 = 0, y0 = 0, y1 = 0, x = 0, y = 0;
    bool haveX = false, haveY = false;
    std::vector<uint32_t> gram;
    ~Impl() { if (cpu) uc_close(cpu); }
    uint32_t Reg(int id) const { uint32_t value = 0; uc_reg_read(cpu, id, &value); return value; }
    void Reg(int id, uint32_t value) { uc_reg_write(cpu, id, &value); }
    uint32_t& B(uint32_t address) {
        if (address >= 0x50000000) return usbBus[(address - 0x50000000) / 4];
        return address >= 0x48000000 ? gpioBus[(address - 0x48000000) / 4] :
            bus[(address - 0x40000000) / 4];
    }
    uint32_t& C(uint32_t address) { return core[(address - 0xe000e000) / 4]; }
    uint32_t Pc() const { return cpu ? Reg(UC_ARM_REG_PC) : 0; }
    bool Fail(std::string text) {
        if (!snapshot.halted) { snapshot.error = std::move(text); snapshot.halted = true; }
        if (cpu) uc_emu_stop(cpu);
        return false;
    }
    void RemovePower() {
        // PA15 holds the board supply on both families. Releasing it without
        // VBUS cuts power (Score3014 OFF, Factory3103 charging detach).
        // Fire/VBUS can supply a new power-on reset; no guest call is skipped.
        powerRemoved = true; snapshot.sleeping = true;
        panelOn = false; panelSleep = true; SetVisible();
        if (cpu) uc_emu_stop(cpu);
    }
    bool Check(uc_err result, const char* action) {
        return result == UC_ERR_OK || Fail(std::string(action) + ": " + uc_strerror(result) + " at PC " + Hex(Pc()));
    }
    void Trace(uint32_t address, unsigned size, bool write, uint32_t value) {
        // The last SPI data/status transfer is uninformative beside 25,600 pixels.
        // Keep it once, without letting a raster evict clocks/buttons/ADC/flash.
        if (address == Spi() + 8 || address == Spi() + 12) return;
        Access entry{snapshot.virtualMicroseconds, Pc(), address, value, static_cast<uint8_t>(size), write};
        if (logSize) {
            auto& last = log[(logNext + AccessLimit - 1) % AccessLimit];
            if (last.pc == entry.pc && last.address == address && last.size == size && last.write == write && last.value == value) {
                last.virtualMicroseconds = entry.virtualMicroseconds; return;
            }
        }
        log[logNext] = entry; logNext = (logNext + 1) % AccessLimit;
        logSize = (std::min)(logSize + 1, size_t(AccessLimit));
    }
    void FaultAccess(uint32_t address, unsigned size, bool write, uint32_t value, const char* type = "Unknown MMIO") {
        Trace(address, size, write, value);
        Fail(std::string(type) + " " + (write ? "write" : "read") + " at PC " + Hex(Pc()) +
            ", address " + Hex(address) + ", width " + std::to_string(size) + ", value " + Hex(value));
    }
    static bool TimerRegister(uint32_t offset, unsigned size) {
        if (offset == 0 || offset == 4 || offset == 0x10 || offset == 0x20) return size == 4 || size == 2;
        return offset <= 0x4c && !(offset & 3) && (size == 2 || size == 4);
    }
    // GD32F3x0 manual 23.7: device-mode USBFS registers only. VBUS supplies
    // power; no host, SOF, reset token, enumeration or packet traffic is invented.
    static bool UsbRegister(uint32_t offset, bool write) {
        switch (offset) {
        case 0: case 4: case 8: case 0xc: case 0x10: case 0x14: case 0x18:
        case 0x24: case 0x28: case 0x38: case 0x3c: case 0x104: case 0x108: case 0x10c:
        case 0x800: case 0x804: case 0x810: case 0x814: case 0x81c:
        case 0x828: case 0x82c: case 0x834: case 0xe00: case 0x818: return true;
        case 0x1c: case 0x20: case 0x2c: case 0x808: return !write;
        }
        if ((offset >= 0x900 && offset < 0x980) || (offset >= 0xb00 && offset < 0xb80)) {
            switch (offset & 0x1f) {
            case 0: case 8: case 0x10: return true;
            case 0x18: return offset < 0x980 && !write;
            }
        }
        return false;
    }
    void ResetUsb() {
        usbBus.fill(0); usbResetDue = 0;
        B(0x5000000c) = 0xa80;
        B(0x50000010) = 0x80000000;
        B(0x50000024) = 0x200;
        B(0x50000028) = 0x02000400;
        B(0x5000003c) = 0x1000;
        B(0x50000828) = 0x17d7; B(0x5000082c) = 0x5b8;
        B(0x50000900) = B(0x50000b00) = 0x8000;
        for (unsigned endpoint = 0; endpoint < 4; ++endpoint) {
            B(0x50000908 + endpoint * 0x20) = 0x80;
            B(0x50000918 + endpoint * 0x20) = 0x200;
        }
    }
    uint32_t ReadUsb(uint32_t word) {
        if (usbResetDue && snapshot.virtualMicroseconds >= usbResetDue) {
            if (B(0x50000010) & 1) ResetUsb();
            else { B(0x50000010) &= ~0x37u; usbResetDue = 0; }
        }
        if (word == 0x50000000)
            return (B(word) & ~0xd0000u) | 0x10000u | (inputs.usb ? 0xc0000u : 0);
        if (word >= 0x50000918 && word < 0x50000980 && (word & 0x1f) == 0x18) {
            const unsigned endpoint = (word - 0x50000918) / 0x20;
            return B(endpoint ? 0x50000100 + endpoint * 4 : 0x50000028) >> 16;
        }
        return B(word);
    }
    void WriteUsb(uint32_t word, uint32_t value) {
        // DAEPINT is a read-only endpoint summary. The stock SDK still writes
        // all ones while initializing; hardware ignores that write.
        if (word == 0x50000818) return;
        if (word == 0x5000000c && (value & 0x20000000)) {
            FaultAccess(word, 4, true, value, "Unsupported USBFS host mode"); return;
        }
        if (word == 0x50000010) {
            B(word) = 0x80000000u | (value & 0x7f7);
            if (value & 0x37) usbResetDue = snapshot.virtualMicroseconds + 1;
            return;
        }
        if (word == 0x50000004) { B(word) &= ~value; return; }
        if (word == 0x50000014) { B(word) &= ~(value & ~0xc0u); return; }
        if (word == 0x50000804) {
            uint32_t status = B(word) & 0xc;
            if (value & 0x80) status |= 4;
            if (value & 0x100) status &= ~4u;
            if (value & 0x200) status |= 8;
            if (value & 0x400) status &= ~8u;
            B(word) = (value & 0x803) | status;
            B(0x50000014) = (B(0x50000014) & ~0xc0u) | (status << 4);
            return;
        }
        if ((word >= 0x50000900 && word < 0x50000980) ||
            (word >= 0x50000b00 && word < 0x50000b80)) {
            const unsigned field = word & 0x1f;
            if (field == 8) { B(word) &= ~(value & 0x5bu); return; }
            if (!field) {
                uint32_t status = B(word) & 0x20000;
                if (value & 0x08000000) status |= 0x20000;
                if (value & 0x04000000) status &= ~0x20000u;
                const bool disable = (value & 0x40000000) && (B(word) & 0x80000000);
                B(word) = (value & ~0x4c020000u) | status;
                if (!(word & 0xff)) B(word) |= 0x8000;
                if (disable) { B(word) &= ~0x80000000u; B(word + 8) |= 2; }
                return;
            }
        }
        B(word) = value;
    }
    bool Known(uint32_t address, unsigned size, bool write) {
        if ((size != 1 && size != 2 && size != 4) || (address & (size - 1))) return false;
        const uint32_t word = address & ~3u;
        if (address >= 0x50000000 && address < 0x50001000)
            return Gd32() && size == 4 && UsbRegister(address - 0x50000000, write);
        if (address >= 0xe000e000 && address < 0xe000f000) {
            if (word >= 0xe000e400 && address + size <= 0xe000e400 + IrqCount()) return true;
            if (word >= 0xe000ed18 && address + size <= 0xe000ed24) return true;
            if (size != 4) return false;
            if (NvicBank(word, 0xe000e100) || NvicBank(word, 0xe000e180) ||
                NvicBank(word, 0xe000e200) || NvicBank(word, 0xe000e280)) return true;
            if (NvicBank(word, 0xe000e300)) return !write;
            switch (word) {
            case 0xe000e010: case 0xe000e014: case 0xe000e018: case 0xe000e01c:
            case 0xe000ed00: case 0xe000ed04: case 0xe000ed08: case 0xe000ed0c:
            case 0xe000ed10: case 0xe000ed14: case 0xe000ed24: case 0xe000ed28:
            case 0xe000ed2c: case 0xe000ed30: case 0xe000ed34: case 0xe000ed38:
            case 0xe000ed88: case 0xe000edfc: return true;
            default: return false;
            }
        }
        for (size_t i = 0; i < TimerBases.size(); ++i)
            if (HasTimer(i) && address >= TimerBases[i] && address < TimerBases[i] + 0x50)
                return TimerRegister(address - TimerBases[i], size);
        for (const uint32_t gpio : {GpioA(), GpioB()}) {
            if (Gd32() && address == gpio + 0x3c) return size == 4; // GD32 GPIO_OSPD1.
            if (address >= gpio && address < gpio + 0x30)
                return size == 4 && !(address & 3) && (address - gpio != 0x10 || !write);
        }
        if (address >= Spi() && address <= Spi() + 0x20)
            return size == (Gd32() ? 4u : 2u) && !(address & 3) && (address != Spi() + 8 || !write);
        // N32 USB CTRL/STS are 16-bit registers on 32-bit spaced words. Native
        // PowerOn uses STR, while the SDK permits both STR and STRH.
        // VBUS is a charging cable, not a simulated USB host/packet source.
        if (!Gd32() && (address == 0x40005c40 || address == 0x40005c44)) return size == 2 || size == 4;
        if (size != 4 || address != word) return false;
        if (Gd32()) {
            if (word >= 0x40021000 && word <= 0x40021034) return true;
            if (word == 0x400210c0 || word == 0x400210cc) return true;
            if (word >= 0x40012400 && word <= 0x4001244c) return true;
            if (word == 0x40012480) return true;
            if (word >= 0x40010000 && word <= 0x4001001c) return true;
        }
        if (!Gd32() && word >= 0x40021000 && word <= 0x40021044) return word != 0x40021038 && word != 0x4002103c;
        if (!Gd32() && word >= 0x40020800 && word <= 0x4002085c) return true;
        if (word >= 0x40020000 && word <= 0x400200a4) return true;
        if (word >= 0x40010400 && word <= 0x40010414) return true;
        if (word >= 0x40010000 && word <= 0x40010010) return true;
        if (word >= 0x40007000 && word <= 0x40007014) return true;
        if (!Gd32() && word >= 0x40001800 && word <= 0x40001828) return true;
        if (Gd32() && word >= 0x40002800 && word <= 0x40002860) {
            switch (word - 0x40002800) {
            case 0: case 4: case 8: case 0xc: case 0x10: case 0x1c: case 0x24:
            case 0x28: case 0x2c: case 0x30: case 0x34: case 0x38: case 0x3c:
            case 0x40: case 0x44: case 0x50: case 0x54: case 0x58: case 0x5c: case 0x60:
                return true;
            default: return false;
            }
        }
        if (!Gd32() && word >= 0x40002800 && word <= 0x40002860 && word != 0x40002818) return true;
        if (word >= 0x40022000 && word <= 0x40022020) return true;
        if (word == 0x40022030) return true;
        if (!Gd32() && word >= 0x40002400 && word <= 0x40002438 && word != 0x4000241c) return true;
        if (word >= 0x40003000 && word <= 0x4000300c) return true;
        return false;
    }
    unsigned Priority(unsigned exception) {
        if (exception < 4) return 0;
        const uint32_t address = exception < 16 ? 0xe000ed18 + exception - 4 : 0xe000e400 + exception - 16;
        return (C(address & ~3u) >> ((address & 3) * 8)) & 0xf0;
    }
    unsigned NextException() {
        if (!anyEnabledPending) return 0;
        const unsigned current = activeDepth ? active[activeDepth - 1] : 0;
        unsigned best = 0, priority = 256;
        const unsigned group = (C(0xe000ed0c) >> 8) & 7;
        const unsigned preempt = (std::min)(4u, 7u - group), shift = 8 - preempt;
        for (unsigned exc = 11; exc < 16 + IrqCount(); ++exc) {
            const bool pending = exc < 16 ? (pendingSystem & (1u << exc)) != 0 :
                (C(0xe000e200 + ((exc - 16) / 32) * 4) & C(0xe000e100 + ((exc - 16) / 32) * 4) & (1u << ((exc - 16) % 32))) != 0;
            if (!pending) continue;
            const unsigned p = Priority(exc);
            if (current && (p >> shift) >= (Priority(current) >> shift)) continue;
            if (p < priority) { priority = p; best = exc; }
        }
        if (!best || Reg(UC_ARM_REG_PRIMASK) || Reg(UC_ARM_REG_FAULTMASK)) return 0;
        const unsigned basepri = Reg(UC_ARM_REG_BASEPRI);
        return basepri && priority >= basepri ? 0 : best;
    }
    void RefreshPending() {
        anyEnabledPending = pendingSystem || (C(0xe000e200) & C(0xe000e100)) ||
            (C(0xe000e204) & C(0xe000e104)) || (Gd32() && (C(0xe000e208) & C(0xe000e108) & 15));
    }
    void Pend(unsigned irq) {
        C(0xe000e200 + (irq / 32) * 4) |= 1u << (irq % 32);
        RefreshPending();
    }
    bool Enter(unsigned exception) {
        const uint32_t previous = Reg(UC_ARM_REG_IPSR), control = Reg(UC_ARM_REG_CONTROL);
        const int stackReg = !previous && (control & 2) ? UC_ARM_REG_PSP : UC_ARM_REG_MSP;
        const uint32_t oldSp = Reg(stackReg), sp = (oldSp - 32) & ~7u;
        if (sp < RamBase || sp + 32 > RamBase + RamSize || activeDepth == active.size()) return Fail("Exception stacking outside SRAM");
        std::array<uint32_t, 8> frame{Reg(UC_ARM_REG_R0), Reg(UC_ARM_REG_R1), Reg(UC_ARM_REG_R2), Reg(UC_ARM_REG_R3),
            Reg(UC_ARM_REG_R12), Reg(UC_ARM_REG_LR), Pc(), Reg(UC_ARM_REG_XPSR)};
        if (oldSp & 4) frame[7] |= 0x200;
        uint32_t vector = 0;
        const uint32_t vectorAddress = C(0xe000ed08) + exception * 4;
        if (vectorAddress < imageBase || vectorAddress + 4 > imageBase + image.size() ||
            uc_mem_read(cpu, vectorAddress, &vector, 4) != UC_ERR_OK || !(vector & 1) ||
            vector < imageBase || vector >= imageBase + image.size())
            return Fail("Missing/invalid exception vector " + std::to_string(exception) + " at " + Hex(vectorAddress) + ", PC " + Hex(Pc()));
        if (!Check(uc_mem_write(cpu, sp, frame.data(), sizeof(frame)), "Stacking exception")) return false;
        Reg(stackReg, sp);
        Reg(UC_ARM_REG_LR, previous ? 0xfffffff1u : stackReg == UC_ARM_REG_PSP ? 0xfffffffdu : 0xfffffff9u);
        Reg(UC_ARM_REG_XPSR, 0x01000000 | exception);
        Reg(UC_ARM_REG_PC, vector);
        active[activeDepth++] = exception;
        if (exception < 16) pendingSystem &= ~(1u << exception);
        else C(0xe000e200 + ((exception - 16) / 32) * 4) &= ~(1u << ((exception - 16) % 32));
        RefreshPending();
        snapshot.sleeping = false;
        return true;
    }
    bool ReturnException() {
        const uint32_t token = Pc() | 1;
        if (!activeDepth || (token != 0xfffffff1 && token != 0xfffffff9 && token != 0xfffffffd)) return Fail("Invalid Cortex-M EXC_RETURN at " + Hex(Pc()));
        const int stackReg = token & 4 ? UC_ARM_REG_PSP : UC_ARM_REG_MSP;
        const uint32_t sp = Reg(stackReg);
        std::array<uint32_t, 8> frame{};
        if (sp < RamBase || sp + 32 > RamBase + RamSize || !Check(uc_mem_read(cpu, sp, frame.data(), sizeof(frame)), "Unstacking exception"))
            return Fail("Exception return stack outside SRAM");
        if (!(frame[7] & 0x01000000)) return Fail("Exception return without Thumb state");
        Reg(stackReg, sp + 32 + ((frame[7] & 0x200) ? 4 : 0));
        constexpr std::array<int, 6> regs{UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_R12, UC_ARM_REG_LR};
        for (size_t i = 0; i < regs.size(); ++i) Reg(regs[i], frame[i]);
        if (token & 8) Reg(UC_ARM_REG_CONTROL, (Reg(UC_ARM_REG_CONTROL) & ~2u) | ((token & 4) ? 2u : 0u));
        Reg(UC_ARM_REG_XPSR, frame[7] & ~0x200u);
        Reg(UC_ARM_REG_PC, frame[6] | 1);
        --activeDepth;
        return true;
    }
    void SetVisible() {
        const bool visible = panelOn && !panelSleep && backlightOn;
        if (visible) snapshot.display.rgba = gram;
        else std::fill(snapshot.display.rgba.begin(), snapshot.display.rgba.end(), 0xff000000u);
        ++snapshot.frameGeneration;
    }
    void UpdateBacklight() {
        // All supported boards drive the LCD backlight from TIM1/TIMER0 CH2.
        // Preserve GRAM while the native idle handler turns the light off.
        const bool on = (B(0x40021018) & 0x800) && (B(0x40012c00) & 1) &&
            (B(0x40012c20) & 0x100) && (B(0x40012c44) & 0x8000) &&
            (B(0x40012c3c) & 0xffff);
        if (on == backlightOn) return;
        backlightOn = on;
        SetVisible();
    }
    void Display(uint8_t value) {
        ++snapshot.display.spiBytes;
        if (!dc) {
            // A new command aborts an unfinished parameter stream on the panel.
            // Stock E0 is deliberately interrupted by vendor FC before byte 16.
            highPending = false;
            command = value; paramCount = 0; paramExpected = 0;
            // HW314D uses the FE/EF panel's one-byte timing registers and
            // fourteen-byte gamma tables, unlike the ST7735 command lengths.
            if (FeEfPanel()) {
                switch (command) {
                case 0xfe: case 0xef: return;
                case 0xb0: case 0xb2: case 0xb3: case 0xb6: case 0xb7:
                case 0xb8: case 0xa8: case 0xab: case 0xac: case 0xc6:
                case 0xc7: case 0xea: case 0xed:
                    paramExpected = 1; return;
                case 0xf0: case 0xf1: paramExpected = 14; return;
                }
            }
            // HW3105's native init uses a different panel command set:
            // FF/A5, single-byte power/timing registers and COLMOD 65.
            if (VendorPanel()) {
                switch (command) {
                case 0xff: case 0x51: case 0x53: case 0x61: case 0x62:
                case 0x83: case 0x86: case 0x87: case 0x88: case 0x89:
                case 0x91: case 0x93: case 0x94: case 0x95: case 0x96:
                case 0xb2: case 0xb5: case 0xc3:
                    paramExpected = 1; return;
                }
            }
            switch (command) {
            case 0x00: break;
            case 0x01: panelSleep = true; panelOn = false; SetVisible(); break;
            case 0x10: panelSleep = true; SetVisible(); break;
            case 0x11: panelSleep = false; SetVisible(); break;
            case 0x28: panelOn = false; SetVisible(); break;
            case 0x29: panelOn = true; SetVisible(); break;
            case 0x20: case 0x21: case 0x13: break;
            case 0x2a: case 0x2b: paramExpected = 4; break;
            case 0x2c:
                if (!haveX || !haveY) { Fail("LCD RAM write without an address window"); return; }
                x = x0; y = y0; break;
            case 0x26: case 0x36: case 0x3a: case 0xb4: case 0xc1: case 0xc5: paramExpected = 1; break;
            case 0xc2: case 0xc3: case 0xc4: paramExpected = 2; break;
            case 0xb1: case 0xb2: case 0xc0: paramExpected = 3; break;
            case 0xb3: paramExpected = 6; break;
            case 0xe0: case 0xe1: paramExpected = 16; break;
            case 0xfc: paramExpected = ShiftedPanel() ? 15 : 14; break;
            default: Fail("Unsupported LCD command " + Hex(command) + " at PC " + Hex(Pc())); break;
            }
            return;
        }
        if (command == 0x2c) {
            if (!highPending) { high = value; highPending = true; return; }
            highPending = false;
            if (x >= LcdX() && x < LcdX() + 80 && y >= LcdY() && y < LcdY() + 160) {
                const size_t index = size_t(y - LcdY()) * 80 + x - LcdX();
                gram[index] = Pixel(static_cast<uint16_t>(uint16_t(high) << 8 | value));
                if (panelOn && !panelSleep && backlightOn) snapshot.display.rgba[index] = gram[index];
            }
            ++snapshot.display.pixelWrites;
            if (++x > x1) { x = x0; if (++y > y1) { y = y0; ++snapshot.frameGeneration; } }
            return;
        }
        if (!paramExpected || paramCount >= paramExpected) { Fail("Unexpected LCD parameter at PC " + Hex(Pc())); return; }
        params[paramCount++] = value;
        if (paramCount != paramExpected) return;
        if (command == 0x2a || command == 0x2b) {
            const uint16_t first = uint16_t(params[0]) << 8 | params[1], last = uint16_t(params[2]) << 8 | params[3];
            // Controller GRAM is larger than the visible 80x160 glass. The real
            // initialization selects columns 26..105 before the first clear.
            // HW3105 programs a larger startup window before selecting its
            // 80x160 glass. Retain the 16-bit registers; raster writes below
            // are always clipped to the fixed visible buffer.
            if (first > last || (!VendorPanel() && (command == 0x2a ? last >= 132 : last >= 162))) {
                Fail("Invalid LCD address window"); return;
            }
            if (command == 0x2a) { x0 = first; x1 = last; haveX = true; }
            else { y0 = first; y1 = last; haveY = true; }
        }
        if (command == 0x3a && value != 5 && value != 0x55 && !(VendorPanel() && value == 0x65))
            Fail("Unsupported LCD pixel format");
    }
    uint32_t Read(uint32_t address, unsigned size) {
        const uint32_t aligned = address & ~3u;
        bool counter = aligned >= 0xe000e010 && aligned <= 0xe000e018;
        counter |= Gd32() && aligned >= 0x40002800 && aligned <= 0x40002860;
        for (const uint32_t timer : TimerBases) counter |= aligned == timer + 0x24 || aligned == timer + 0x10;
        if (counter) SyncTime();
        if (!Known(address, size, false)) { FaultAccess(address, size, false, 0); return 0; }
        const uint32_t word = address & ~3u;
        if (word >= 0x50000000 && word < 0x50001000) {
            SyncTime();
            const uint32_t result = ReadUsb(word);
            Trace(address, size, false, result); return result;
        }
        uint32_t value;
        if (word >= 0xe000e000) {
            value = C(word);
            if (word == 0xe000ed00) value = 0x410fc241; // Cortex-M4 r0p1, not a fabricated device serial.
            if (word == 0xe000ed04) value = Reg(UC_ARM_REG_IPSR) | ((pendingSystem & (1u << 14)) ? 1u << 28 : 0) | ((pendingSystem & (1u << 15)) ? 1u << 26 : 0);
            if (word == 0xe000e010) C(word) &= ~0x10000u;
            if (NvicBank(word, 0xe000e180)) value = C(word - 0x80);
            if (NvicBank(word, 0xe000e280)) value = C(word - 0x80);
            if (NvicBank(word, 0xe000e300)) {
                value = 0; for (unsigned i = 0; i < activeDepth; ++i) if (active[i] >= 16 && (active[i] - 16) / 32 == (word - 0xe000e300) / 4) value |= 1u << ((active[i] - 16) % 32);
            }
        } else {
            value = B(word);
            if (word == GpioA() + 0x10)
                value = (B(GpioA() + 0x14) & ~(UsbPin | AirflowPin)) | PortALevels(inputs);
            if (word == GpioB() + 0x10) value = (B(GpioB() + 0x14) & ~(FirePin | PlusPin | MinusPin)) | ButtonLevels(inputs);
            if (word == Spi() + 8) value = 2; // TXE, not BUSY; MISO unconnected.
            if (!Gd32() && word == 0x40020858 && snapshot.virtualMicroseconds >= adcAck && (B(0x40020808) & 1)) value |= 0x20;
        }
        value >>= (address & 3) * 8;
        if (size < 4) value &= (1u << (size * 8)) - 1;
        Trace(address, size, false, value);
        return value;
    }
    void Write(uint32_t address, unsigned size, uint32_t value) {
        const bool timed = TimedRegister(address & ~3u);
        if (timed) SyncTime();
        struct Finish {
            Impl& self;
            bool timed, backlight;
            ~Finish() {
                if (backlight) self.UpdateBacklight();
                if (timed) { self.UpdateComparators(); self.UpdateInterrupts(); self.Schedule(); }
            }
        } finish{*this, timed, (address & ~0xffu) == 0x40012c00 ||
            (address & ~3u) == 0x40021018 || (address & ~3u) == 0x4002100c};
        if (!Known(address, size, true)) { FaultAccess(address, size, true, value); return; }
        Trace(address, size, true, value);
        const uint32_t pwm = Gd32() ? 0x40014834 : 0x40000840;
        if ((address & ~3u) == pwm && value && !(B(pwm) & 0xffff)) ++pwmStarts;
        const uint32_t word = address & ~3u;
        const uint32_t mask = size == 4 ? 0xffffffffu : ((1u << (size * 8)) - 1) << ((address & 3) * 8);
        auto merge = [&](uint32_t& reg) { reg = (reg & ~mask) | ((value << ((address & 3) * 8)) & mask); };
        if (word >= 0x50000000 && word < 0x50001000) { WriteUsb(word, value); return; }
        if (word >= 0xe000e000) {
            const uint32_t bits = (word & 0x7f) == 8 ? value & 15 : value;
            if (NvicBank(word, 0xe000e100)) { C(word) |= bits; return; }
            if (NvicBank(word, 0xe000e180)) { C(word - 0x80) &= ~bits; return; }
            if (NvicBank(word, 0xe000e200)) { C(word) |= bits; return; }
            if (NvicBank(word, 0xe000e280)) { C(word - 0x80) &= ~bits; return; }
            if (word == 0xe000ed04) {
                if (value & (1u << 28)) pendingSystem |= 1u << 14;
                if (value & (1u << 27)) pendingSystem &= ~(1u << 14);
                if (value & (1u << 26)) pendingSystem |= 1u << 15;
                if (value & (1u << 25)) pendingSystem &= ~(1u << 15);
                return;
            }
            if (word == 0xe000e018) { C(word) = 0; C(0xe000e010) &= ~0x10000u; systickCycles = 0; return; }
            if (word == 0xe000e014) { C(word) = value & 0xffffff; return; }
            if (word == 0xe000ed0c) {
                if ((value >> 16) != 0x5fa) return;
                if (value & 4) { resetRequested = true; uc_emu_stop(cpu); return; }
                C(word) = value & 0x700; return;
            }
            merge(C(word)); return;
        }
        if (word == Spi() + 12) { B(word) = value; Display(static_cast<uint8_t>(value)); return; }
        if (word == 0x40005c40) {
            // CTRL.FRST release raises STS.RST (SDK usb_pwr.c Suspend polls it).
            // No host is attached to the virtual cable: no SOF, enumeration,
            // transfer-complete or wakeup events are invented.
            const uint32_t previous = B(word);
            B(word) = value & 0xff1f;
            if ((previous & 1) && !(B(word) & 3)) B(0x40005c44) |= 0x400;
            return;
        }
        if (word == 0x40005c44) {
            // STS interrupt events are W0C; CTRS/EP_ID/DIR derive from endpoints,
            // not from software writes. No packet engine is exposed here.
            B(word) &= (value & 0x7f00) | ~0x7f00u;
            return;
        }
        for (const uint32_t gpio : {GpioA(), GpioB()}) {
            if (word == gpio + 0x18 || word == gpio + 0x28) {
                const uint32_t previous = B(gpio + 0x14);
                if (word == gpio + 0x18) B(gpio + 0x14) = (B(gpio + 0x14) | (value & 0xffff)) & ~(value >> 16);
                else B(gpio + 0x14) &= ~value;
                if (gpio == GpioA()) dc = (B(gpio + 0x14) & 0x100) != 0;
                if (gpio == GpioA() && !inputs.usb &&
                    (previous & 0x8000) && !(B(gpio + 0x14) & 0x8000))
                    RemovePower();
                return;
            }
        }
        if (Gd32() && word >= 0x40002800 && word <= 0x40002860) {
            WriteRtc(word, value); return;
        }
        if (Gd32() && word == 0x4001001c) {
            uint32_t next = B(word);
            for (unsigned shift : {0u, 16u}) {
                const uint32_t half = 0xffffu << shift;
                if (!(next & (0x8000u << shift)))
                    next = (next & ~half) | (value & (half & ~(0x4000u << shift))) |
                        (next & (0x4000u << shift));
            }
            B(word) = next; return;
        }
        if (!Gd32() && word >= 0x40002400 && word <= 0x40002438) {
            if (word == 0x4000240c) { B(word) |= value & 3; return; }
            if (word == 0x40002438) { B(word) &= value & 3; return; }
            const unsigned index = word >= 0x40002420 && word <= 0x40002428 ? 1 : 0;
            if (word >= 0x40002410 && word <= 0x40002428 && (B(0x4000240c) & (1u << index))) return;
            if (word == 0x40002410 || word == 0x40002420) value = (value & ~0x80000u) | (B(word) & 0x80000);
            B(word) = value;
            return;
        }
        if (word == 0x40020004) {
            // W1C CGIF clears its entire channel; other flags recompute GIF.
            for (unsigned channel = 0; channel < 8; ++channel) {
                const unsigned shift = channel * 4, clear = (value >> shift) & 15;
                uint32_t flags = (B(0x40020000) >> shift) & 15;
                flags &= ~((clear & 1) ? 15u : clear);
                flags = (flags & ~1u) | ((flags & 14) ? 1u : 0u);
                B(0x40020000) = (B(0x40020000) & ~(15u << shift)) | (flags << shift);
            }
            return;
        }
        if (word == 0x40010414) { B(word) &= ~value; return; }
        for (uint32_t timer : TimerBases) {
            if (word == timer + 0x10) { B(word) &= value | ~mask; return; } // Timer flags are W0C.
        }
        if (word == 0x40022004) {
            if (!flashKey && value == 0x45670123) flashKey = 1;
            else if (flashKey == 1 && value == 0xcdef89ab) { B(0x40022010) &= ~0x80u; flashKey = 0; }
            else flashKey = 0;
            return;
        }
        if (word == 0x4002200c) { B(word) &= ~value; return; }
        if (word == 0x40022010) {
            if ((B(word) & 0x80) && !(value & 0x80)) return;
            B(word) = value;
            if ((value & 0x42) == 0x42) {
                const uint32_t page = B(0x40022014);
                const uint32_t pageSize = FlashPageSize();
                if (page < SettingsBase || page >= SettingsBase + SettingsSize || (page & (pageSize - 1))) {
                    FaultAccess(page, pageSize, true, 0xffffffff, "Disallowed Flash erase"); return;
                }
                const size_t offset = page - SettingsBase;
                std::fill(settings.begin() + offset, settings.begin() + offset + pageSize, 0xff);
                Check(uc_mem_write(cpu, page, settings.data() + offset, pageSize), "Erasing settings page");
                ++revision; B(0x4002200c) = 1; flashAck = snapshot.virtualMicroseconds + 1;
            } else if (value & 0x74) Fail("Unsupported mass/option Flash operation at PC " + Hex(Pc()));
            return;
        }
        if (word == 0x40003000) {
            if (value == 0xcccc) watchdog = true;
            if (value == 0xaaaa || value == 0xcccc) watchdogDue = snapshot.virtualMicroseconds + ((uint64_t(B(0x40003008) & 0xfff) + 1) * (4u << (B(0x40003004) & 7)) * 25);
            return;
        }
        if (word == 0x4002100c || word == 0x40021010 || word == 0x40021028)
            ResetPeripherals(word, value & ~B(word));
        merge(B(word));
        if (word == 0x40021000 || word == 0x40021004 || word == 0x40021020 ||
            word == 0x40021024 || word == 0x40021034 || word == 0x40021040 || word == 0x400210c0)
            clockAck = snapshot.virtualMicroseconds + 1;
        if (word == AdcBase() + 8) {
            adcAck = snapshot.virtualMicroseconds + 1;
            if (!(value & 1)) { adcConverting = false; adcDue = 0; }
            if ((value & 0x400001) == 0x400001) {
                // SWSTART is a hardware-cleared trigger, not a persistent enable.
                B(word) &= ~0x400000u; adcConverting = true;
                adcDue = snapshot.virtualMicroseconds + AdcPeriod();
            }
        }
        for (size_t i = 0; i < TimerBases.size(); ++i) {
            if (word == TimerBases[i] + 0x14 && (value & 1)) { timerCycles[i] = 0; B(TimerBases[i] + 0x24) = 0; B(TimerBases[i] + 0x10) |= 1; }
        }
    }
    uint32_t AliasRead(uint32_t address, unsigned size) {
        const uint32_t offset = address - 0x42000000, byte = 0x40000000 + offset / 32, bit = (offset % 32) / 4;
        if (size != 4 || (address & 3) || !Known(byte & ~3u, 4, false)) { FaultAccess(address, size, false, 0); return 0; }
        return (Read(byte & ~3u, 4) >> (((byte & 3) * 8) + bit)) & 1;
    }
    void AliasWrite(uint32_t address, unsigned size, uint32_t value) {
        const uint32_t offset = address - 0x42000000, byte = 0x40000000 + offset / 32, bit = (byte & 3) * 8 + (offset % 32) / 4;
        if (size != 4 || (address & 3) || value > 1 || !Known(byte & ~3u, 4, true)) { FaultAccess(address, size, true, value); return; }
        const uint32_t old = B(byte & ~3u);
        Write(byte & ~3u, 4, (old & ~(1u << bit)) | (value << bit));
    }
    static uint64_t BusRead(uc_engine*, uint64_t offset, unsigned size, void* user) {
        return static_cast<Impl*>(user)->Read(0x40000000 + static_cast<uint32_t>(offset), size);
    }
    static void BusWrite(uc_engine*, uint64_t offset, unsigned size, uint64_t value, void* user) {
        static_cast<Impl*>(user)->Write(0x40000000 + static_cast<uint32_t>(offset), size, static_cast<uint32_t>(value));
    }
    static uint64_t CoreRead(uc_engine*, uint64_t offset, unsigned size, void* user) {
        return static_cast<Impl*>(user)->Read(0xe000e000 + static_cast<uint32_t>(offset), size);
    }
    static void CoreWrite(uc_engine*, uint64_t offset, unsigned size, uint64_t value, void* user) {
        static_cast<Impl*>(user)->Write(0xe000e000 + static_cast<uint32_t>(offset), size, static_cast<uint32_t>(value));
    }
    static uint64_t DwtRead(uc_engine*, uint64_t offset, unsigned size, void* user) {
        auto& self = *static_cast<Impl*>(user);
        if (size != 4 || (offset != 0 && offset != 4)) {
            self.FaultAccess(0xe0001000 + static_cast<uint32_t>(offset), size, false, 0); return 0;
        }
        self.SyncTime();
        return offset ? self.dwtCount : self.dwtControl;
    }
    static void DwtWrite(uc_engine*, uint64_t offset, unsigned size, uint64_t value, void* user) {
        auto& self = *static_cast<Impl*>(user);
        if (size != 4 || offset != 0) {
            self.FaultAccess(0xe0001000 + static_cast<uint32_t>(offset), size, true, static_cast<uint32_t>(value)); return;
        }
        self.SyncTime();
        self.dwtControl = static_cast<uint32_t>(value);
    }
    static uint64_t BitRead(uc_engine*, uint64_t offset, unsigned size, void* user) {
        return static_cast<Impl*>(user)->AliasRead(0x42000000 + static_cast<uint32_t>(offset), size);
    }
    static void BitWrite(uc_engine*, uint64_t offset, unsigned size, uint64_t value, void* user) {
        static_cast<Impl*>(user)->AliasWrite(0x42000000 + static_cast<uint32_t>(offset), size, static_cast<uint32_t>(value));
    }
    static uint64_t GpioRead(uc_engine*, uint64_t offset, unsigned size, void* user) {
        return static_cast<Impl*>(user)->Read(0x48000000 + static_cast<uint32_t>(offset), size);
    }
    static void GpioWrite(uc_engine*, uint64_t offset, unsigned size, uint64_t value, void* user) {
        static_cast<Impl*>(user)->Write(0x48000000 + static_cast<uint32_t>(offset), size, static_cast<uint32_t>(value));
    }
    static uint64_t UsbRead(uc_engine*, uint64_t offset, unsigned size, void* user) {
        return static_cast<Impl*>(user)->Read(0x50000000 + static_cast<uint32_t>(offset), size);
    }
    static void UsbWrite(uc_engine*, uint64_t offset, unsigned size, uint64_t value, void* user) {
        static_cast<Impl*>(user)->Write(0x50000000 + static_cast<uint32_t>(offset), size, static_cast<uint32_t>(value));
    }
    static bool Invalid(uc_engine*, uc_mem_type type, uint64_t address, int size, int64_t value, void* user) {
        auto& self = *static_cast<Impl*>(user);
        const bool fetch = type == UC_MEM_FETCH_UNMAPPED || type == UC_MEM_FETCH_PROT;
        self.FaultAccess(static_cast<uint32_t>(address), size, type == UC_MEM_WRITE_UNMAPPED || type == UC_MEM_WRITE_PROT,
            static_cast<uint32_t>(value), fetch ? "Code fetch fault" : "Unmapped/protected memory");
        return false;
    }
    static void MissingFlashRead(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t, void* user) {
        auto& s = *static_cast<Impl*>(user);
        s.FaultAccess(static_cast<uint32_t>(address), size, false, 0, "Missing firmware/loader Flash");
    }
    static void IdentityRead(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t, void* user) {
        auto& s = *static_cast<Impl*>(user);
        // Explicit virtual all-zero UID in the selected MCU's identity ROM.
        const uint32_t first = s.Gd32() ? 0x1ffff7ac : 0x1ffff7f5;
        if (address < first || size <= 0 || address + size > first + 12)
            s.FaultAccess(static_cast<uint32_t>(address), size, false, 0, "Unknown identity ROM");
        else s.Trace(static_cast<uint32_t>(address), size, false, 0);
    }
    static void Program(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t value, void* user) {
        auto& s = *static_cast<Impl*>(user);
        s.SyncTime();
        s.Trace(static_cast<uint32_t>(address), size, true, static_cast<uint32_t>(value));
        const unsigned width = s.Gd32() ? 2u : 4u;
        if (size != int(width) || (address & (width - 1)) || (s.B(0x40022010) & 0x81) != 1 || s.B(0x4002200c) & 1) {
            s.FaultAccess(static_cast<uint32_t>(address), size, true, static_cast<uint32_t>(value), "Invalid Flash program"); return;
        }
        const size_t offset = static_cast<size_t>(address - SettingsBase);
        uint32_t old = 0;
        for (unsigned i = 0; i < width; ++i) old |= uint32_t(s.settings[offset + i]) << (i * 8);
        const uint32_t next = static_cast<uint32_t>(value) & (width == 2 ? 0xffffu : 0xffffffffu);
        if ((old & next) != next) { s.Fail("Flash programming attempted a zero-to-one transition"); return; }
        for (unsigned i = 0; i < width; ++i) s.settings[offset + i] = static_cast<uint8_t>(next >> (i * 8));
        ++s.revision; s.B(0x4002200c) = 1; s.flashAck = s.snapshot.virtualMicroseconds + 1;
        s.Schedule();
    }
    static void Intr(uc_engine*, uint32_t number, void* user) {
        auto& s = *static_cast<Impl*>(user); s.interrupt = number; uc_emu_stop(s.cpu);
    }
    uint32_t ClockHz() {
        const uint32_t cfg = B(0x40021004), source = (cfg >> 2) & 3;
        uint32_t hz;
        if (Gd32()) {
            hz = 8000000;
            if (source == 2) {
                const uint32_t cfg1 = B(0x4002102c);
                const unsigned encoded = ((cfg >> 18) & 15) | ((cfg >> 23) & 16) | ((cfg1 >> 26) & 32);
                const unsigned multiplier = encoded < 15 ? encoded + 2 : encoded + 1;
                const uint32_t input = cfg & 0x10000 ?
                    ((cfg1 & 0x40000000) ? 48000000u : 8000000u) / ((cfg1 & 15) + 1) : 4000000u;
                hz = input * multiplier;
                if (cfg & 0x80000000) hz /= 2;
            }
        } else {
            constexpr std::array<uint32_t, 8> msi{100000, 200000, 400000, 800000, 1000000, 2000000, 4000000, 4000000};
            hz = source == 0 ? msi[(B(0x40021024) >> 4) & 7] : source == 2 ? 8000000 : 16000000;
            if (source == 3) {
                const unsigned multiplier = (cfg & 0x08000000) ? ((cfg & 0x083c0000) >> 18) - 495 : ((cfg >> 18) & 15) + 2;
                const bool external = (cfg & 0x10000) != 0;
                hz = (external ? 8000000 : 16000000) / ((external ? cfg & 0x20000 : B(0x40021040) & 1) ? 2 : 1) * multiplier;
                if (B(0x40021040) & 2) hz /= 2;
            }
        }
        const unsigned prescale = (cfg >> 4) & 15;
        if (prescale >= 8) hz >>= (prescale < 12 ? prescale - 7 : prescale - 6);
        return hz;
    }
    double PwmDuty() {
        const uint32_t timer = Gd32() ? 0x40014800 : 0x40000800;
        const unsigned compare = Gd32() ? 0x34 : 0x40;
        // A running counter alone does not drive the PWM pin. GD32's
        // advanced TIMER16 additionally requires its primary output gate.
        if (!(B(timer + 0x20) & (Gd32() ? 1u : 0x1000u)) ||
            (Gd32() && !(B(timer + 0x44) & 0x8000))) return 0;
        return (B(timer) & 1) ? double(B(timer + compare) & 0xffff) / ((B(timer + 0x2c) & 0xffff) + 1) : 0;
    }
    double LoadCurrent(double millivolts) const {
        if (!inputs.atomizerConnected || millivolts <= 0) return 0;
        const int resistance = std::clamp(inputs.resistanceMilliOhms, 0, 20000);
        return resistance ? millivolts * 1000 / resistance : (std::numeric_limits<double>::infinity)();
    }
    void Adc() {
        if (!AdcEnabled()) return;
        const unsigned count = B(0x4002000c) & 0xffff;
        const uint32_t destination = B(0x40020014);
        if (count != 6 || destination < RamBase || destination + 12 > RamBase + RamSize || !(B(0x40020008) & 0x20)) {
            Fail("Unsupported ADC DMA transfer configuration at PC " + Hex(Pc())); return;
        }
        // Virtual electrical bench, not a physical coil/thermal simulator.
        // Family-specific reference/divider gains feed the real ADC/DMA.
        // PWM voltage drives a resistive load; no guest globals are overwritten.
        const double reference = Gd32() ? 1571.0 : 1489.0;
        const int battery = std::clamp(inputs.batteryMillivolts, 0, 6000);
        const int resistance = std::clamp(inputs.resistanceMilliOhms, 0, 20000);
        // The board's current/voltage feedback is sampled during PWM ON.
        // Duty sets delivered energy, not the instantaneous probe amplitude.
        // An ideal weak sense bias lifts the open node; an attached idle coil
        // shunts it. This does not assume a physical pull-up resistor value.
        const double output = PwmDuty() > 0 || !inputs.atomizerConnected ? double(battery) : 0.0;
        const double current = LoadCurrent(output);
        auto raw = [](double v) { return static_cast<uint16_t>(std::clamp(v, 0.0, 4095.0)); };
        const double currentGain = Gd32() ? 24510.0 : 40051.0;
        const double voltageGain = Gd32() ? 2550.0 : 4052.0;
        const double batteryGain = Gd32() ? 2548.0 : 4019.0;
        const std::array<uint16_t, 6> samples{raw(current * reference / currentGain), raw(output * reference / voltageGain),
            raw(inputs.atomizerConnected ? reference * resistance / (resistance + 100.0) : reference), thermistorSample,
            raw(battery * reference / batteryGain), static_cast<uint16_t>(reference)};
        if (!Check(uc_mem_write(cpu, destination, samples.data(), sizeof(samples)), "ADC DMA")) return;
        ++adcConversions;
        B(AdcBase() + 0x4c) = samples.back(); B(AdcBase()) |= 2; B(0x40020000) |= 3;
        if (B(0x40020008) & 2) Pend(DmaIrq());
    }
    uint32_t TimerHz(size_t index) {
        if (!HasTimer(index)) return 0;
        const bool apb2 = index == 3 || index >= 5;
        const unsigned bit = index == 3 ? 11 : index == 4 ? 8 : index >= 5 ? unsigned(index) + 11 : unsigned(index);
        if (!(B(apb2 ? 0x40021018 : 0x4002101c) & (1u << bit)) || !(B(TimerBases[index]) & 1)) return 0;
        const unsigned prescale = (B(0x40021004) >> (apb2 ? 11 : 8)) & 7;
        return prescale < 4 ? hclk : (hclk >> (prescale - 3)) * 2;
    }
    uint32_t AdcPeriod() {
        // N32 ranks 2,6,1,10,9,18; GD32 ranks 1,5,2,9,8,17.
        // Derive conversion time from the programmed clock and sample lengths.
        const uint32_t base = AdcBase();
        constexpr std::array<unsigned, 6> n32Channels{2, 6, 1, 10, 9, 18};
        constexpr std::array<unsigned, 6> gd32Channels{1, 5, 2, 9, 8, 17};
        const auto& channels = Gd32() ? gd32Channels : n32Channels;
        if (((B(base + 0x2c) >> 20) & 15) != 5 ||
            (!Gd32() && ((B(base + 0x58) & 0x13) || (B(base + 0x5c) & 8)))) {
            Fail("Unsupported native ADC resolution/scan at PC " + Hex(Pc())); return 1;
        }
        uint32_t hz;
        if (Gd32()) {
            const uint32_t cfg2 = B(0x40021030);
            if (!(cfg2 & 0x100)) hz = 28000000 / ((cfg2 & 0x10000) ? 1 : 2);
            else {
                // GD32 UM rev2.8 RCU_CFG0.ADCPSC: low encodings divide
                // APB2; high encodings divide AHB, not the APB clock.
                constexpr std::array<unsigned, 8> divisors{2, 4, 6, 8, 3, 5, 7, 9};
                const unsigned index = ((B(0x40021004) >> 14) & 3) | ((cfg2 >> 29) & 4);
                const unsigned apb = (B(0x40021004) >> 11) & 7;
                const uint32_t pclk = apb < 4 ? hclk : hclk >> (apb - 3);
                hz = (index < 4 ? pclk : hclk) / divisors[index];
            }
        } else {
            constexpr std::array<unsigned, 16> divisors{1, 2, 4, 6, 8, 10, 12, 16, 32, 32, 32, 32, 32, 32, 32, 32};
            hz = hclk / divisors[B(0x4002102c) & 15];
        }
        constexpr std::array<unsigned, 8> halfCycles{28, 40, 52, 82, 108, 136, 168, 504};
        unsigned total = 0;
        for (unsigned rank = 0; rank < channels.size(); ++rank) {
            const unsigned channel = (B(base + 0x34) >> (rank * 5)) & 31;
            if (channel != channels[rank]) {
                Fail("Unsupported ADC input routing at PC " + Hex(Pc())); return 1;
            }
            const uint32_t sample = !Gd32() && channel == 18 ? B(base + 0x5c) :
                (channel < 10 ? B(base + 0x10) >> (channel * 3) : B(base + 0x0c) >> ((channel - 10) * 3));
            total += halfCycles[sample & 7];
        }
        return static_cast<uint32_t>((uint64_t(total) * 1000000 + hz * 2 - 1) / (hz * 2));
    }
    bool AdcEnabled() {
        const bool clocks = (B(0x40021014) & 1) &&
            (Gd32() ? (B(0x40021018) & 0x200) != 0 : (B(0x40021014) & 0x1000) != 0);
        return adcConverting && clocks && (B(AdcBase() + 8) & 1) &&
            (B(0x40020008) & 1) && B(0x40020010) == AdcBase() + 0x4c;
    }
    void ResetPeripherals(uint32_t address, uint32_t bits) {
        auto clear = [&](uint32_t base, unsigned length) {
            for (unsigned offset = 0; offset < length; offset += 4) B(base + offset) = 0;
        };
        for (size_t i = 0; i < TimerBases.size(); ++i) {
            if (!HasTimer(i)) continue;
            const bool apb2 = i == 3 || i >= 5;
            const unsigned bit = i == 3 ? 11 : i == 4 ? 8 : i >= 5 ? unsigned(i) + 11 : unsigned(i);
            if (address == (apb2 ? 0x4002100cu : 0x40021010u) && (bits & (1u << bit))) {
                clear(TimerBases[i], 0x50); B(TimerBases[i] + 0x2c) = 0xffff; timerCycles[i] = 0;
            }
        }
        if ((Gd32() && address == 0x4002100c && (bits & 0x200)) ||
            (!Gd32() && address == 0x40021028 && (bits & 0x1000))) {
            clear(AdcBase(), Gd32() ? 0x84 : 0x60);
            B(AdcBase() + 0x24) = 0xfff;
            adcDue = adcAck = 0; adcConverting = false;
        }
        if (address == 0x40021028 && (bits & 1)) clear(0x40020000, 0xa8);
        if (Gd32() && address == 0x40021028) {
            if (bits & 0x20000) { clear(GpioA(), 0x30); B(GpioA()) = 0x28000000; dc = false; }
            if (bits & 0x40000) clear(GpioB(), 0x30);
        } else if (!Gd32() && address == 0x4002100c) {
            if (bits & 4) { clear(GpioA(), 0x30); dc = false; }
            if (bits & 8) clear(GpioB(), 0x30);
        }
        if (address == (Gd32() ? 0x40021010u : 0x4002100cu) &&
            (bits & (Gd32() ? 0x4000u : 0x1000u))) clear(Spi(), 0x24);
    }
    uint32_t RtcHz() {
        if (!Gd32() || !(B(0x40021020) & 0x8000) || (B(0x4000280c) & 0x80)) return 0;
        switch ((B(0x40021020) >> 8) & 3) {
        case 1: return (B(0x40021020) & 2) ? 32768 : 0;
        case 2: return (B(0x40021024) & 2) ? 40000 : 0;
        case 3: return (B(0x40021000) & 0x20000) ? 250000 : 0;
        default: return 0;
        }
    }
    uint64_t RtcPeriod() {
        const uint32_t prescale = B(0x40002810);
        return uint64_t(((prescale >> 16) & 127) + 1) * ((prescale & 0x7fff) + 1) * 1000000;
    }
    void WriteRtc(uint32_t address, uint32_t value) {
        if (address == 0x40002824) {
            if (value == 0xca) rtcKey = 1;
            else if (value == 0x53 && rtcKey == 1) rtcKey = 2;
            else rtcKey = 0;
            return;
        }
        if (address >= 0x40002850) { B(address) = value; return; }
        if (rtcKey != 2) return;
        if (address == 0x4000280c) {
            const uint32_t old = B(address);
            B(address) = (old & ~0x80u) | (value & 0x80);
            B(address) &= value | ~0x7900u; // Event flags are W0C.
            if (!(value & 0x20)) B(address) &= ~0x20u;
            rtcAck = snapshot.virtualMicroseconds + 1;
            return;
        }
        if (address == 0x40002800 || address == 0x40002804 || address == 0x40002810) {
            if (!(B(0x4000280c) & 0x40)) return;
            B(address) = value; rtcCycles = 0;
            if (address == 0x40002804) B(0x4000280c) |= 0x10;
            return;
        }
        if (address == 0x40002808) {
            B(address) = value & ~0x30000u;
            rtcAck = snapshot.virtualMicroseconds + 1;
            return;
        }
        if (address == 0x4000281c || address == 0x40002844) {
            if (B(0x4000280c) & 1) B(address) = value;
            return;
        }
        if (address == 0x40002828 || (address >= 0x40002830 && address <= 0x40002838)) return;
        if (address == 0x4000282c) {
            // The native builds initialize SHIFTCTL to zero. Nonzero phase
            // adjustments require a reference-clock calibration model.
            if (value) Fail("Unsupported RTC phase adjustment at PC " + Hex(Pc()));
            return;
        }
        B(address) = value;
    }
    void RtcSecond() {
        auto decimal = [](uint32_t bcd) { return (bcd & 15) + ((bcd >> 4) & 15) * 10; };
        auto bcd = [](unsigned value) { return (value / 10) * 16 + value % 10; };
        uint32_t& time = B(0x40002800);
        unsigned second = decimal(time & 0x7f), minute = decimal((time >> 8) & 0x7f);
        unsigned hour = decimal((time >> 16) & 0x3f);
        const bool twelve = (B(0x40002808) & 0x40) != 0;
        if (twelve) hour = hour % 12 + ((time & 0x400000) ? 12 : 0);
        bool midnight = false;
        if (++second == 60) {
            second = 0;
            if (++minute == 60) { minute = 0; if (++hour == 24) { hour = 0; midnight = true; } }
        }
        time = bcd(second) | bcd(minute) << 8 |
            bcd(twelve ? (hour % 12 ? hour % 12 : 12) : hour) << 16 |
            (twelve && hour >= 12 ? 0x400000 : 0);
        uint32_t& date = B(0x40002804);
        if (midnight) {
            unsigned day = decimal(date & 0x3f), month = decimal((date >> 8) & 0x1f);
            unsigned year = decimal((date >> 16) & 0xff), weekday = (date >> 13) & 7;
            constexpr std::array<unsigned, 12> lengths{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            if (month < 1 || month > 12) { Fail("Invalid RTC calendar month"); return; }
            const unsigned days = lengths[month - 1] + (month == 2 && year % 4 == 0 ? 1 : 0);
            if (++day > days) { day = 1; if (++month > 12) { month = 1; year = (year + 1) % 100; } }
            weekday = weekday % 7 + 1;
            date = bcd(day) | bcd(month) << 8 | weekday << 13 | bcd(year) << 16;
        }
        const uint32_t alarm = B(0x4000281c);
        if ((B(0x40002808) & 0x100) &&
            ((alarm & 0x80) || (alarm & 0x7f) == (time & 0x7f)) &&
            ((alarm & 0x8000) || (alarm & 0x7f00) == (time & 0x7f00)) &&
            ((alarm & 0x800000) || (alarm & 0x7f0000) == (time & 0x7f0000)) &&
            ((alarm & 0x80000000) || ((alarm >> 24) & 0x3f) ==
                ((alarm & 0x40000000) ? (date >> 13) & 7 : date & 0x3f))) {
            B(0x4000280c) |= 0x100;
            if ((B(0x40002808) & 0x1000) && (B(0x40010408) & 0x20000)) {
                B(0x40010414) |= 0x20000;
                if (B(0x40010400) & 0x20000) Pend(2);
            }
        }
    }
    void UpdateRtc(uint64_t elapsed) {
        if (!Gd32()) return;
        if (rtcAck && snapshot.virtualMicroseconds >= rtcAck) {
            uint32_t& status = B(0x4000280c);
            status = (status & ~0x41u) | ((status & 0x80) ? 0x40 : 0) |
                ((B(0x40002808) & 0x100) ? 0 : 1) | 0x20;
            rtcAck = 0;
        }
        const uint32_t hz = RtcHz();
        if (!hz) return;
        rtcCycles += elapsed * hz;
        const uint64_t period = RtcPeriod();
        while (rtcCycles >= period && !snapshot.halted) { rtcCycles -= period; RtcSecond(); }
        const uint32_t divisor = ((B(0x40002810) >> 16) & 127) + 1;
        B(0x40002828) = (B(0x40002810) & 0x7fff) - static_cast<uint32_t>(rtcCycles / (uint64_t(divisor) * 1000000));
    }
    void UpdateGd32Comparators() {
        uint32_t& state = B(0x4001001c);
        const double current = LoadCurrent(std::clamp(inputs.batteryMillivolts, 0, 6000) *
            std::clamp(PwmDuty(), 0.0, 1.0));
        for (unsigned shift : {0u, 16u}) {
            const uint32_t control = state >> shift;
            bool output = false;
            if ((control & 1) && (B(0x40021018) & 1)) {
                const unsigned negative = (control >> 4) & 7;
                if (negative > 3 || (control & 0x700)) {
                    Fail("Unsupported GD32 comparator routing at PC " + Hex(Pc())); return;
                }
                output = current * 1274 / 24510 > 1274.0 * (negative + 1) / 4;
                if (control & 0x800) output = !output;
            }
            const uint32_t bit = 0x4000u << shift;
            const bool previous = (state & bit) != 0;
            state = (state & ~bit) | (output ? bit : 0);
            const uint32_t line = 1u << (shift ? 22 : 21);
            if (previous != output && ((output ? B(0x40010408) : B(0x4001040c)) & line)) {
                B(0x40010414) |= line;
                if (B(0x40010400) & line) Pend(12);
            }
        }
    }
    static bool TimedRegister(uint32_t word) {
        if (word >= 0xe000e000) return true;
        for (uint32_t timer : TimerBases) if (word >= timer && word <= timer + 0x4c) return true;
        return (word >= 0x40021000 && word <= 0x40021134) ||
            (word >= 0x40020800 && word <= 0x4002085c) ||
            (word >= 0x40012400 && word <= 0x40012480) ||
            word == 0x4001001c || (word >= 0x50000000 && word < 0x50001000) ||
            (word >= 0x40020000 && word <= 0x400200a4) ||
            (word >= 0x40003000 && word <= 0x4000300c) ||
            (word >= 0x40022000 && word <= 0x40022030) ||
            (word >= 0x40002400 && word <= 0x40002438) ||
            (word >= 0x40002800 && word <= 0x40002860) ||
            word == 0x40005c40 || word == 0x40005c44;
    }
    bool ActiveIrq(unsigned irq) const {
        for (unsigned i = 0; i < activeDepth; ++i) if (active[i] == irq + 16) return true;
        return false;
    }
    void UpdateInterrupts() {
        for (size_t i = 0; i < TimerBases.size(); ++i) {
            const uint32_t base = TimerBases[i];
            if (HasTimer(i) && (B(base + 0x10) & B(base + 0x0c) & 1) && !ActiveIrq(TimerIrq(i))) Pend(TimerIrq(i));
        }
        if ((B(0x40020000) & 2) && (B(0x40020008) & 2) && !ActiveIrq(DmaIrq())) Pend(DmaIrq());
        // IRQ22 is routed through EXTI21/22; COMP_INTSTS is a separate W0C
        // latch. Re-pending from that latch starves the native EXTI handler.
        if (!Gd32() && (B(0x40010414) & B(0x40010400) & (3u << 21)) && !ActiveIrq(22)) Pend(22);
        if (!Gd32() && (B(0x4002101c) & 0x800000) && !(B(0x40005c40) & 3) &&
            (B(0x40005c44) & B(0x40005c40) & 0x7f00) && !ActiveIrq(21)) Pend(21);
        if (Gd32() && (B(0x50000008) & 1) && (B(0x50000014) & B(0x50000018)) &&
            !ActiveIrq(67)) Pend(67);
        RefreshPending();
    }
    void ComparatorOutput(unsigned index, bool highOutput) {
        const uint32_t address = 0x40002410 + index * 0x10, line = 1u << (21 + index);
        const bool previous = (B(address) & 0x80000) != 0;
        B(address) = (B(address) & ~0x80000u) | (highOutput ? 0x80000 : 0);
        if (!previous && highOutput && (B(0x40002400) & (1u << index))) B(0x40002438) |= 1u << index;
        if (previous != highOutput && ((highOutput ? B(0x40010408) : B(0x4001040c)) & line)) {
            B(0x40010414) |= line;
            if (B(0x40010400) & line) Pend(22);
        }
    }
    void UpdateComparators() {
        if (Gd32()) {
            UpdateGd32Comparators(); return;
        }
        // N32G43x COMP, not RTC: CTRL/FILC/FILP at +10/+14/+18.
        // PA1 uses the same explicit virtual current transducer as ADC channel 2.
        // Ideal bench VREF1: a 6-bit DAC across 3.3-V VDDA. The manual
        // specifies 64 levels, not a calibrated silicon trim transfer.
        const double duty = PwmDuty();
        const double milliamps = LoadCurrent(std::clamp(inputs.batteryMillivolts, 0, 6000) *
            std::clamp(duty, 0.0, 1.0));
        for (unsigned index = 0; index < 2; ++index) {
            const uint32_t address = 0x40002410 + index * 0x10, control = B(address);
            bool output = false;
            if ((control & 1) && (B(0x4002101c) & 0x40)) {
                if (index != 0 || (control & 0x1e0) != 0x120 || (control & 0xe) != 0xc || (control & 0x100000)) {
                    Fail("Unsupported virtual comparator input routing at PC " + Hex(Pc())); return;
                }
                const uint32_t reference = B(0x40002430);
                const double negative = (reference & 1) ? 3300.0 * ((reference >> 1) & 63) / 64 : 0;
                output = milliamps * 1200 / 40051 > negative;
                if (control & 0x2000) output = !output;
            }
            if (comparatorTarget[index] == output && (comparatorDue[index] || ((control & 0x80000) != 0) == output)) continue;
            comparatorTarget[index] = output;
            if ((B(address + 4) & 1) && (B(0x4002101c) & 0x80)) {
                const uint64_t samples = ((B(address + 4) >> 6) & 31) + 1, divider = (B(address + 8) & 0xffff) + 1;
                const unsigned prescale = (B(0x40021004) >> 8) & 7;
                const uint32_t pclk = prescale < 4 ? hclk : hclk >> (prescale - 3);
                const uint32_t frequency = B(0x40002404) & 1 ? 40000 : pclk;
                comparatorDue[index] = snapshot.virtualMicroseconds + (std::max)(uint64_t(1), (samples * divider * 1000000 + frequency - 1) / frequency);
            } else {
                comparatorDue[index] = 0; ComparatorOutput(index, output);
            }
        }
    }
    void Schedule() {
        const uint64_t now = snapshot.virtualMicroseconds;
        nextEvent = (std::numeric_limits<uint64_t>::max)();
        auto deadline = [&](uint64_t at) { if (at) nextEvent = (std::min)(nextEvent, at > now ? at : now + 1); };
        deadline(clockAck); deadline(adcAck); deadline(flashAck);
        deadline(rtcAck);
        if (const uint32_t hz = RtcHz()) {
            const uint64_t period = RtcPeriod();
            deadline(now + (period - (std::min)(rtcCycles, period - 1) + hz - 1) / hz);
        }
        if (watchdog) deadline(watchdogDue);
        if (AdcEnabled()) { if (!adcDue) adcDue = now + AdcPeriod(); deadline(adcDue); }
        for (uint64_t at : comparatorDue) deadline(at);
        auto cyclesUntil = [&](uint64_t remaining, uint32_t hz) {
            if (hz) deadline(now + (std::max)(uint64_t(1), (remaining + hz - 1) / hz));
        };
        if (C(0xe000e010) & 1) {
            const uint64_t period = (uint64_t(C(0xe000e014)) + 1) * 1000000;
            cyclesUntil(period - (std::min)(systickCycles, period - 1), (C(0xe000e010) & 4) ? hclk : hclk / 8);
        }
        for (size_t i = 0; i < TimerBases.size(); ++i) {
            const uint32_t base = TimerBases[i], hz = TimerHz(i);
            if (!hz || !(B(base + 0x0c) & 1)) continue;
            const uint64_t divider = (uint64_t(B(base + 0x28) & 0xffff) + 1) * 1000000;
            const uint64_t period = (B(base + 0x2c) & 0xffff) + 1;
            const uint64_t remaining = (period - (B(base + 0x24) % period)) * divider;
            cyclesUntil(remaining - (std::min)(timerCycles[i], remaining - 1), hz);
        }
    }
    void SyncTime() {
        const uint64_t now = snapshot.virtualMicroseconds, elapsed = now - peripheralTime;
        if (!elapsed) return;
        peripheralTime = now;
        UpdateRtc(elapsed);
        if ((dwtControl & 1) && (C(0xe000edfc) & 0x1000000)) {
            dwtFraction += uint64_t(hclk) * elapsed;
            dwtCount += static_cast<uint32_t>(dwtFraction / 1000000);
            dwtFraction %= 1000000;
        }
        if (C(0xe000e010) & 1) {
            systickCycles += uint64_t((C(0xe000e010) & 4) ? hclk : hclk / 8) * elapsed;
            const uint64_t period = (uint64_t(C(0xe000e014)) + 1) * 1000000;
            if (systickCycles >= period) {
                systickCycles %= period; C(0xe000e010) |= 0x10000;
                if (C(0xe000e010) & 2) pendingSystem |= 1u << 15;
            }
            C(0xe000e018) = C(0xe000e014) - static_cast<uint32_t>(systickCycles / 1000000);
        }
        for (size_t i = 0; i < TimerBases.size(); ++i) {
            const uint32_t base = TimerBases[i], hz = TimerHz(i);
            if (!hz) continue;
            timerCycles[i] += uint64_t(hz) * elapsed;
            const uint64_t divider = (uint64_t(B(base + 0x28) & 0xffff) + 1) * 1000000;
            const uint64_t count = B(base + 0x24) + timerCycles[i] / divider;
            timerCycles[i] %= divider;
            const uint32_t period = (B(base + 0x2c) & 0xffff) + 1;
            if (count >= period) B(base + 0x10) |= 1;
            B(base + 0x24) = static_cast<uint32_t>(count % period);
        }
        if (clockAck && now >= clockAck) {
            uint32_t& control = B(0x40021000);
            control = (control & ~0x02020002u) | ((control & 1) << 1) | ((control & 0x10000) << 1) | ((control & 0x1000000) << 1);
            B(0x40021004) = (B(0x40021004) & ~0xcu) | ((B(0x40021004) & 3) << 2);
            for (uint32_t address : {0x40021020u, 0x40021024u}) B(address) = (B(address) & ~2u) | ((B(address) & 1) << 1);
            if (Gd32()) {
                B(0x40021034) = (B(0x40021034) & ~2u) | ((B(0x40021034) & 1) << 1);
                B(0x400210c0) = (B(0x400210c0) & ~0x20000u) | ((B(0x400210c0) & 0x10000) << 1);
            }
            const uint32_t nextClock = ClockHz();
            cpuPhase = static_cast<uint32_t>(uint64_t(cpuPhase) * nextClock / hclk);
            hclk = nextClock; clockAck = 0;
        }
        if (adcAck && now >= adcAck) { B(AdcBase() + 8) &= ~12u; adcAck = 0; }
        if (flashAck && now >= flashAck) { B(0x4002200c) = (B(0x4002200c) & ~1u) | 0x20; B(0x40022010) &= ~0x40u; flashAck = 0; }
        if (AdcEnabled() && adcDue && now >= adcDue) {
            Adc();
            if (B(AdcBase() + 8) & 2) adcDue = now + AdcPeriod();
            else { adcConverting = false; adcDue = 0; }
        }
        for (unsigned i = 0; i < comparatorDue.size(); ++i) if (comparatorDue[i] && now >= comparatorDue[i]) {
            comparatorDue[i] = 0; ComparatorOutput(i, comparatorTarget[i]);
        }
        if (watchdog && now >= watchdogDue) { resetRequested = true; if (cpu) uc_emu_stop(cpu); }
        UpdateInterrupts(); Schedule();
    }
    bool AccountInstruction(uint32_t pc) {
        // A low HCLK instruction may span multiple RunFor quanta. Finish its
        // clock ticks before executing it; never charge the same slot twice or
        // enter an IRQ halfway through that instruction.
        if (!instructionPending) {
            cpuPhase += 1000000;
            instructionPending = true;
        }
        while (cpuPhase >= hclk) {
            if (!stepping && snapshot.virtualMicroseconds >= target) return false;
            cpuPhase -= hclk;
            ++snapshot.virtualMicroseconds;
            if (snapshot.virtualMicroseconds >= nextEvent) SyncTime();
        }
        instructionPending = false;
        ++snapshot.instructions;
        ++exceptionInstructions[activeDepth ? active[activeDepth - 1] : 0];
        if (pc == observation->bootTail) ++bootCompletions;
        if (pc == observation->scheduler) ++mainIterations;
        if (voltageObservation && pc == voltageObservation->address + 4) {
            voltageCentivolts = Reg(voltageObservation->combined ? UC_ARM_REG_R2 : UC_ARM_REG_R0);
            ++voltageRenders;
        }
        if (pc == screens->draw[15]) { ++serviceEntries; ++screenEntries[15]; }
        if (pc == screens->draw[0]) ++screenEntries[0];
        if (pc == offEntry) { ++powerOffEntries; ++screenEntries[3]; }
        // Most execution is outside warning-page code. Bound the scan to this
        // record's contiguous draw routines; never search profiles in the hook.
        if (pc >= warningFirst && pc <= warningLast) {
            for (size_t slot = 4; slot <= 14; ++slot)
                if (pc == screens->draw[slot]) { ++screenEntries[slot]; break; }
            if (pc == screens->draw[16]) ++screenEntries[16];
        }
        if (pc == observation->usbTail && pc) ++usbInitializations;
        return true;
    }
    static void Instruction(uc_engine*, uint64_t address, uint32_t, void* user) {
        auto& s = *static_cast<Impl*>(user);
        if (s.snapshot.halted) { uc_emu_stop(s.cpu); return; }
        if ((!s.stepping && s.snapshot.virtualMicroseconds >= s.target) ||
            (s.stepping && s.snapshot.instructions >= s.stepTarget) ||
            (!s.instructionPending && s.NextException())) { uc_emu_stop(s.cpu); return; }
        if (address < s.imageBase || address + 1 >= s.imageBase + s.image.size()) {
            s.Fail("Code fetch outside native image at PC " + Hex(static_cast<uint32_t>(address))); return;
        }
        const size_t offset = static_cast<size_t>(address - s.imageBase);
        const uint16_t opcode = uint16_t(s.image[offset]) | uint16_t(s.image[offset + 1]) << 8;
        if (!s.AccountInstruction(static_cast<uint32_t>(address))) { uc_emu_stop(s.cpu); return; }
        if (opcode == 0xbf30 || opcode == 0xbf20) {
            const uint32_t xpsr = s.Reg(UC_ARM_REG_XPSR);
            const unsigned it = ((xpsr >> 8) & 0xfc) | ((xpsr >> 25) & 3), condition = it >> 4;
            const bool n = (xpsr >> 31) != 0, z = (xpsr & 0x40000000) != 0;
            const bool c = (xpsr & 0x20000000) != 0, v = (xpsr & 0x10000000) != 0;
            const std::array<bool, 8> passed{z, c, n, v, c && !z, n == v, !z && n == v, true};
            if (!it || condition >= 14 || (passed[condition >> 1] != bool(condition & 1)))
                s.snapshot.sleeping = true;
            // Execute the real WFI/WFE. Unicorn synchronizes PC/IT and exits;
            // the peripheral scheduler supplies the subsequent wake event.
        }
    }
    void Publish() {
        snapshot.registers = {};
        snapshot.stack = {};
        snapshot.modeledClockHz = opened ? hclk : 0;
        snapshot.mappedSramBase = opened ? RamBase : 0;
        snapshot.mappedSramBytes = opened ? RamSize : 0;
        if (cpu && opened) {
            constexpr std::array<int, 16> generalIds{
                UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
                UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7,
                UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11,
                UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC};
            auto& registers = snapshot.registers;
            for (size_t i = 0; i < generalIds.size(); ++i)
                if (uc_reg_read(cpu, generalIds[i], &registers.general[i]) == UC_ERR_OK)
                    registers.validGeneral |= uint16_t(1u << i);
            constexpr std::array<int, 7> specialIds{UC_ARM_REG_XPSR, UC_ARM_REG_MSP,
                UC_ARM_REG_PSP, UC_ARM_REG_CONTROL, UC_ARM_REG_PRIMASK, UC_ARM_REG_BASEPRI, UC_ARM_REG_FAULTMASK};
            const std::array<uint32_t*, 7> specialValues{&registers.xpsr, &registers.msp,
                &registers.psp, &registers.control, &registers.primask, &registers.basepri, &registers.faultmask};
            for (size_t i = 0; i < specialIds.size(); ++i)
                if (uc_reg_read(cpu, specialIds[i], specialValues[i]) == UC_ERR_OK)
                    registers.validSpecial |= uint8_t(1u << i);
            snapshot.pc = registers.general[15];
            snapshot.sp = registers.general[13];
            if (registers.validGeneral & (1u << 13)) {
                auto& stack = snapshot.stack;
                stack.address = snapshot.sp;
                // Validate by subtraction before forming a byte count. Never
                // inspect MMIO/Flash or wrap a malicious guest stack pointer.
                if ((stack.address & 3) || stack.address < RamBase ||
                    stack.address - RamBase > RamSize) {
                    stack.status = StackStatus::InvalidPointer;
                } else {
                    stack.count = (std::min)(uint32_t(stack.words.size()),
                        (RamSize - (stack.address - RamBase)) / uint32_t(sizeof(uint32_t)));
                    stack.status = StackStatus::Empty;
                    if (stack.count) {
                        if (uc_mem_read(cpu, stack.address, stack.words.data(),
                            size_t(stack.count) * sizeof(uint32_t)) == UC_ERR_OK)
                            stack.status = StackStatus::Available;
                        else {
                            stack.words.fill(0);
                            stack.count = 0;
                            stack.status = StackStatus::ReadFailed;
                        }
                    }
                }
            }
        } else snapshot.pc = snapshot.sp = 0;
        snapshot.recentAccess.resize(logSize);
        for (size_t i = 0; i < logSize; ++i) snapshot.recentAccess[i] = log[(logNext + AccessLimit - logSize + i) % AccessLimit];
    }
    bool Boot(std::string& error) {
        if (cpu) { uc_close(cpu); cpu = nullptr; }
        opened = false;
        powerRemoved = false;
        snapshot = {}; bus.fill(0); gpioBus.fill(0); core.fill(0); timerCycles.fill(0);
        if (Gd32()) ResetUsb();
        snapshot.display.width = 80; snapshot.display.height = 160; snapshot.display.hardwareCode = hardwareCode;
        snapshot.display.emulatedUid = true;
        snapshot.display.rgba.assign(80 * 160, 0xff000000u); gram.assign(80 * 160, 0xff000000u);
        logNext = logSize = 0; cpuPhase = interrupt = activeDepth = 0; pendingSystem = flashKey = 0;
        exceptionInstructions.fill(0); bootCompletions = mainIterations = serviceEntries = powerOffEntries = 0;
        usbInitializations = 0;
        screenEntries.fill(0); adcConversions = pwmStarts = 0;
        voltageCentivolts = 0; voltageRenders = 0;
        systickCycles = adcDue = clockAck = adcAck = flashAck = 0; watchdog = false; watchdogDue = 0;
        rtcCycles = rtcAck = 0; rtcKey = 0;
        dwtControl = dwtCount = 0; dwtFraction = 0;
        adcConverting = false;
        resetRequested = false;
        instructionPending = false;
        peripheralTime = 0; nextEvent = (std::numeric_limits<uint64_t>::max)();
        comparatorDue.fill(0); comparatorTarget.fill(false);
        anyEnabledPending = false;
        hclk = Gd32() ? 8000000 : 16000000;
        dc = panelOn = backlightOn = highPending = haveX = haveY = false; panelSleep = true; command = 0; paramExpected = paramCount = 0;
        B(0x40021000) = 0x83; B(0x40021004) = Gd32() ? 0 : 0x5;
        if (!Gd32()) B(0x40021040) = 3;
        else { B(0x40021014) = 0x14; B(0x48000000) = 0x28000000; }
        if (Gd32()) {
            B(0x40002804) = 0x2101; B(0x4000280c) = 1; B(0x40002810) = 0x7f00ff;
            B(0x40012424) = 0xfff;
        }
        for (size_t i = 0; i < TimerBases.size(); ++i)
            if (HasTimer(i)) B(TimerBases[i] + 0x2c) = 0xffff;
        B(0x40022010) = 0x80; B(0x40003008) = 0xfff;
        B(0x40005c40) = 3; // USB CTRL reset value: power-down and force-reset.
        C(0xe000ed08) = imageBase; C(0xe000ed14) = 0x200;
        C(0xe000e01c) = 0x80000000; // No independent external SysTick reference clock.
        if (!Check(uc_open(UC_ARCH_ARM, static_cast<uc_mode>(UC_MODE_THUMB | UC_MODE_MCLASS), &cpu), "Opening Cortex-M") ||
            !Check(uc_ctl_set_cpu_model(cpu, UC_CPU_ARM_CORTEX_M4), "Selecting Cortex-M4F core") ||
            !Check(uc_mem_map(cpu, imageBase & ~0xfffu, SettingsBase - (imageBase & ~0xfffu), UC_PROT_READ | UC_PROT_EXEC), "Mapping native Flash") ||
            !Check(uc_mem_write(cpu, imageBase, image.data(), image.size()), "Loading native image") ||
            !Check(uc_mem_map(cpu, RamBase, RamSize, UC_PROT_READ | UC_PROT_WRITE), "Mapping SRAM") ||
            !Check(uc_mem_map(cpu, SettingsBase, SettingsSize, UC_PROT_READ | UC_PROT_WRITE), "Mapping settings Flash") ||
            !Check(uc_mem_write(cpu, SettingsBase, settings.data(), settings.size()), "Loading settings Flash") ||
            !Check(uc_mem_map(cpu, 0x1ffff000, 0x1000, UC_PROT_READ), "Mapping virtual UID ROM") ||
            !Check(uc_mmio_map(cpu, 0x40000000, 0x30000, BusRead, this, BusWrite, this), "Mapping MCU peripheral bus") ||
            !Check(uc_mmio_map(cpu, 0x42000000, 0x600000, BitRead, this, BitWrite, this), "Mapping peripheral bit-band") ||
            !Check(uc_mmio_map(cpu, 0xe0001000, 0x1000, DwtRead, this, DwtWrite, this), "Mapping Cortex-M cycle counter") ||
            !Check(uc_mmio_map(cpu, 0xe000e000, 0x1000, CoreRead, this, CoreWrite, this), "Mapping Cortex-M system control")) {
            error = snapshot.error; Publish(); return false;
        }
        if (Gd32() && !Check(uc_mmio_map(cpu, 0x48000000, 0x1000, GpioRead, this, GpioWrite, this),
            "Mapping GD32 GPIO bus")) { error = snapshot.error; Publish(); return false; }
        if (Gd32() && !Check(uc_mmio_map(cpu, 0x50000000, 0x1000, UsbRead, this, UsbWrite, this),
            "Mapping GD32 USBFS device controller")) { error = snapshot.error; Publish(); return false; }
        uc_hook hook = 0;
        if (!Check(uc_hook_add(cpu, &hook, UC_HOOK_MEM_INVALID, reinterpret_cast<void*>(Invalid), this, 1, 0), "Hooking faults") ||
            !Check(uc_hook_add(cpu, &hook, UC_HOOK_INTR, reinterpret_cast<void*>(Intr), this, 1, 0), "Hooking exceptions") ||
            !Check(uc_hook_add(cpu, &hook, UC_HOOK_CODE, reinterpret_cast<void*>(Instruction), this, 1, 0), "Hooking time") ||
            (imageBase & 0xfff ? !Check(uc_hook_add(cpu, &hook, UC_HOOK_MEM_READ, reinterpret_cast<void*>(MissingFlashRead), this,
                imageBase & ~0xfffu, imageBase - 1), "Guarding missing loader Flash") : false) ||
            !Check(uc_hook_add(cpu, &hook, UC_HOOK_MEM_READ, reinterpret_cast<void*>(MissingFlashRead), this,
                imageBase + image.size(), SettingsBase - 1), "Guarding Flash padding") ||
            !Check(uc_hook_add(cpu, &hook, UC_HOOK_MEM_READ, reinterpret_cast<void*>(IdentityRead), this, 0x1ffff000, 0x1fffffff), "Hooking strict UID ROM") ||
            !Check(uc_hook_add(cpu, &hook, UC_HOOK_MEM_WRITE, reinterpret_cast<void*>(Program), this, SettingsBase, SettingsBase + SettingsSize - 1), "Hooking settings Flash")) {
            error = snapshot.error; Publish(); return false;
        }
        Reg(UC_ARM_REG_MSP, Le32(image.data())); Reg(UC_ARM_REG_SP, Le32(image.data()));
        Reg(UC_ARM_REG_XPSR, 0x01000000); Reg(UC_ARM_REG_PC, Le32(image.data() + 4));
        opened = true; error.clear(); Publish(); return true;
    }
    bool HardwareReset(std::string& error) {
        const uint64_t now = snapshot.virtualMicroseconds, instructions = snapshot.instructions, generation = snapshot.frameGeneration;
        const uint64_t spiBytes = snapshot.display.spiBytes, pixelWrites = snapshot.display.pixelWrites;
        const uint32_t phase = cpuPhase, previousClock = hclk;
        const auto profile = exceptionInstructions;
        const uint64_t boots = bootCompletions, loops = mainIterations, services = serviceEntries, offs = powerOffEntries, usbInits = usbInitializations;
        const auto pageEntries = screenEntries;
        const uint64_t conversions = adcConversions, starts = pwmStarts;
        if (!Boot(error)) return false;
        snapshot.virtualMicroseconds = now; snapshot.instructions = instructions; snapshot.frameGeneration = generation + 1;
        snapshot.display.spiBytes = spiBytes; snapshot.display.pixelWrites = pixelWrites;
        cpuPhase = static_cast<uint32_t>(uint64_t(phase) * hclk / previousClock);
        exceptionInstructions = profile; bootCompletions = boots; mainIterations = loops;
        serviceEntries = services; powerOffEntries = offs;
        usbInitializations = usbInits;
        screenEntries = pageEntries; adcConversions = conversions; pwmStarts = starts;
        peripheralTime = now; Schedule();
        return true;
    }
    bool Run(uint32_t microseconds, bool single, std::string& error) {
        error.clear();
        if (!opened) { error = "No native device is open"; return false; }
        if (snapshot.halted) { error = snapshot.error; return false; }
        if (microseconds > 10000) { error = "RunFor quantum exceeds 10,000 virtual microseconds"; return false; }
        target = snapshot.virtualMicroseconds + microseconds; stepping = single; stepTarget = snapshot.instructions + 1;
        while (!snapshot.halted && (single ? snapshot.instructions < stepTarget : snapshot.virtualMicroseconds < target)) {
            if (resetRequested) { if (!HardwareReset(error)) break; }
            if (powerRemoved) {
                snapshot.virtualMicroseconds = single ? snapshot.virtualMicroseconds + 1 : target;
                break;
            }
            if (!instructionPending) {
                if (const unsigned next = NextException()) { if (!Enter(next)) break; }
            }
            if (snapshot.sleeping) {
                // WFI wakes for an enabled pending interrupt even when PRIMASK
                // prevents exception entry until the following CPSIE.
                if (anyEnabledPending) {
                    snapshot.sleeping = false;
                } else {
                    if (single && nextEvent == (std::numeric_limits<uint64_t>::max)()) break;
                    const uint64_t limit = single ? nextEvent : target;
                    snapshot.virtualMicroseconds = (std::min)(limit, nextEvent);
                    SyncTime();
                    if (resetRequested && !HardwareReset(error)) break;
                    if (single && snapshot.sleeping && !anyEnabledPending) break;
                    continue;
                }
            }
            interrupt = 0;
            const uc_err result = uc_emu_start(cpu, Pc() | 1, 0, 0, 0);
            if (snapshot.halted) break;
            if (resetRequested) { if (!HardwareReset(error)) break; continue; }
            if (interrupt == 2) { if (!Enter(11)) break; }
            else if (interrupt == 8) { if (!ReturnException()) break; }
            else if (interrupt) { Fail("Unhandled CPU exception " + std::to_string(interrupt) + " at PC " + Hex(Pc())); break; }
            else if (!Check(result, "Executing native firmware")) break;
            UpdateInterrupts();
        }
        Publish(); error = snapshot.error; return !snapshot.halted;
    }
};

Machine::Machine() : impl_(std::make_unique<Impl>()) {}
Machine::~Machine() = default;
Machine::Machine(Machine&&) noexcept = default;
Machine& Machine::operator=(Machine&&) noexcept = default;
bool Machine::Open(const StudioState& source, std::span<const uint8_t> storage, std::string& error) {
    return Open(source, 0x3103, storage, error);
}
bool Machine::Open(const StudioState& source, uint16_t hardwareCode,
    std::span<const uint8_t> storage, std::string& error) {
    auto next = std::make_unique<Impl>();
    next->hardwareCode = hardwareCode ? hardwareCode : 0x3103;
    next->edition = source.edition;
    if (!PrepareImage(source, next->hardwareCode, next->image, next->imageBase, next->menuStateAddress, error)) return false;
    for (const auto& profile : ObservationProfiles)
        if (profile.edition == next->edition && profile.code == next->hardwareCode) { next->observation = &profile; break; }
    for (const auto& profile : VoltageObservationProfiles)
        if (profile.edition == next->edition && profile.code == next->hardwareCode) { next->voltageObservation = &profile; break; }
    const auto screenProfiles = next->edition == FirmwareEdition::Score ?
        std::span<const FirmwareEmulator::SxScreenProfile>(FirmwareEmulator::SxScoreScreenProfiles) :
        std::span<const FirmwareEmulator::SxScreenProfile>(FirmwareEmulator::SxFactoryScreenProfiles);
    for (const auto& profile : screenProfiles)
        if (profile.editionIndex == (next->edition == FirmwareEdition::Score ? 0u : 1u) &&
            profile.code == next->hardwareCode) { next->screens = &profile; break; }
    if (!next->observation || !next->screens) { error = "Missing native regression mapping for " + Hex(hardwareCode); return false; }
    next->offEntry = next->screens->draw[1] + 0x60;
    next->warningFirst = (std::min)(next->screens->draw[4], next->screens->draw[5]);
    next->warningLast = (std::max)(next->screens->draw[14], next->screens->draw[16]);
    next->settings.fill(0xff);
    if (!storage.empty()) {
        constexpr size_t HeaderSize = 24;
        if (storage.size() != HeaderSize + SettingsSize || std::memcmp(storage.data(), "VFDN", 4) != 0 ||
            Le32(storage.data() + 4) != 2 ||
            Le32(storage.data() + 8) != (uint32_t(next->edition) << 16 | next->hardwareCode) ||
            Le32(storage.data() + 12) != SettingsBase || Le32(storage.data() + 16) != SettingsSize ||
            Le32(storage.data() + 20) != FirmwarePackage::Crc16Ccitt(storage.data() + HeaderSize, SettingsSize)) {
            error = "Invalid/incompatible revision-and-edition settings storage"; return false;
        }
        std::copy(storage.begin() + HeaderSize, storage.end(), next->settings.begin());
    }
    if (!next->Boot(error)) { impl_ = std::move(next); return false; }
    impl_ = std::move(next); return true;
}
bool Machine::Reset(std::string& error) {
    if (!impl_->opened) { error = "No native device is open"; return false; }
    return impl_->Boot(error);
}
void Machine::SetInputs(const Inputs& inputs) {
    const Inputs previous = impl_->inputs;
    if (inputs == previous) return;
    if (impl_->opened && !impl_->snapshot.halted && !impl_->powerRemoved) impl_->SyncTime();
    if (inputs.temperatureCelsius != previous.temperatureCelsius) {
        const double ntc = 10000 * std::exp(3950.0 * (1.0 / (std::clamp(inputs.temperatureCelsius, -40, 150) + 273.15) - 1.0 / 298.15));
        impl_->thermistorSample = static_cast<uint16_t>(4095 * ntc / (10000 + ntc));
    }
    impl_->inputs = inputs;
    if (!impl_->opened || impl_->snapshot.halted) return;
    if (impl_->powerRemoved) {
        if ((!previous.fire && inputs.fire) || (!previous.usb && inputs.usb)) impl_->resetRequested = true;
        return;
    }
    if (previous.usb && !inputs.usb && !inputs.fire &&
        !(impl_->B(impl_->GpioA() + 0x14) & 0x8000)) {
        impl_->RemovePower(); return;
    }
    impl_->UpdateComparators();
    impl_->UpdateInterrupts();
    impl_->Schedule();
    const uint32_t previousLevels = ButtonLevels(previous) | PortALevels(previous);
    const uint32_t levels = ButtonLevels(inputs) | PortALevels(inputs);
    const uint32_t changed = previousLevels ^ levels;
    if (changed) {
        auto& s = *impl_;
        const uint32_t falling = previousLevels & ~levels;
        const uint32_t rising = ~previousLevels & levels;
        const uint32_t triggered = (falling & s.B(0x4001040c)) | (rising & s.B(0x40010408));
        s.B(0x40010414) |= triggered;
        const uint32_t enabled = triggered & s.B(0x40010400);
        if (s.Gd32()) {
            if (enabled & 3) s.Pend(5);
            if (enabled & 12) s.Pend(6);
            if (enabled & 0xfff0) s.Pend(7);
        } else {
            for (unsigned pin = 0; pin < 5; ++pin) if (enabled & (1u << pin)) s.Pend(6 + pin);
            if (enabled & 0x3e0) s.Pend(23);
            if (enabled & 0xfc00) s.Pend(40);
        }
        if (s.C(0xe000ed10) & 0x10) s.snapshot.sleeping = false;
    }
}
bool Machine::RunFor(uint32_t microseconds, std::string& error) { return impl_->Run(microseconds, false, error); }
bool Machine::Step(std::string& error) { return impl_->Run(0, true, error); }
const Snapshot& Machine::Current() const { return impl_->snapshot; }
uint64_t Machine::StorageRevision() const { return impl_->revision; }
std::vector<uint8_t> Machine::SaveStorage() const {
    if (!impl_->opened) return {};
    std::vector<uint8_t> result;
    result.reserve(24 + SettingsSize); result.insert(result.end(), {'V', 'F', 'D', 'N'});
    Put32(result, 2); Put32(result, uint32_t(impl_->edition) << 16 | impl_->hardwareCode);
    Put32(result, SettingsBase); Put32(result, SettingsSize);
    Put32(result, FirmwarePackage::Crc16Ccitt(impl_->settings.data(), impl_->settings.size()));
    result.insert(result.end(), impl_->settings.begin(), impl_->settings.end()); return result;
}

RegressionObservation ObserveRegressionState(const Machine& machine, std::span<uint8_t> settingsSnapshot) {
    auto& s = *machine.impl_;
    RegressionObservation result{s.bootCompletions, s.mainIterations, s.serviceEntries, s.powerOffEntries};
    if (!s.cpu || !s.observation) return result;
    const auto& profile = *s.observation;
    auto read = [&](uint32_t address, unsigned size) {
        uint32_t value = 0;
        if (uc_mem_read(s.cpu, address, &value, size) != UC_ERR_OK) result.mapped = false;
        return value;
    };
    result.mapped = true;
    result.railPowered = !s.powerRemoved;
    result.historyMenu = profile.history;
    result.usbPeripheral = profile.usbTail != 0;
    const bool score = s.edition == FirmwareEdition::Score;
    const unsigned width = s.Gd32() ? 2 : 4;
    const uint32_t powerOffset = s.Gd32() ? (score ? 0x12 : profile.history ? 0x0e : 0x0a) : score ? 0x20 : 0x14;
    const uint32_t resistanceOffset = s.Gd32() ? (score ? (profile.history ? 0x24 : 0x20) :
        (profile.history ? 0x20 : 0x18)) : score ? 0x3c : 0x34;
    const uint32_t durationOffset = s.Gd32() ? (score ? (profile.history ? 0x28 : 0x24) :
        (profile.history ? 0x24 : 0x1c)) : score ? 0x44 : 0x3c;
    const uint32_t puffOffset = s.Gd32() ? (score ? (profile.history ? 0x50 : 0x2e) :
        (profile.history ? 8 : 0x28)) : score ? 0x50 : 0x4c;
    result.flags = read(s.Gd32() ? 0x20000054 : 0x20000058, 4);
    result.page = read(profile.page, 1);
    result.powerA = read(profile.settings + powerOffset, width);
    result.powerB = read(profile.settings + powerOffset + width, width);
    result.mode = read(profile.settings + width * 2, width);
    result.powerLimit = read(profile.settings + powerOffset + width * 3, width);
    result.locked = read(profile.settings + resistanceOffset + width, width) == 0;
    result.resistanceMilliOhms = read(profile.settings + resistanceOffset, width);
    result.durationCentiseconds = read(profile.settings + durationOffset, 2);
    result.puffs = read(profile.settings + puffOffset, width);
    if (s.Gd32() && !(score && profile.history))
        result.puffs = (result.puffs << 16) | read(profile.settings + puffOffset + 2, 2);
    if (score) {
        result.scoreBestSinglePuff = read(profile.settings + (s.Gd32() ? 0x0a : 0x14), width);
        result.scoreRawTotal = s.Gd32() ?
            (read(profile.settings + 0x0c, 2) << 16) | read(profile.settings + 0x0e, 2) :
            read(profile.settings + 0x18, 4);
    }
    if (!settingsSnapshot.empty()) {
        const size_t bytes = s.Gd32() ?
            (score ? (profile.history ? 0x5e : 0x34) : (profile.history ? 0x5a : 0x2e)) :
            (score ? 0x58 : 0x54);
        if (settingsSnapshot.size() != bytes ||
            uc_mem_read(s.cpu, profile.settings, settingsSnapshot.data(), bytes) != UC_ERR_OK)
            result.mapped = false;
    }
    result.batteryMillivolts = read(profile.sensors + 8, 2);
    result.feedbackCurrentMilliAmps = read(profile.sensors, 2);
    result.feedbackVoltageMillivolts = read(profile.sensors + 2, 2);
    result.feedbackResistanceMilliOhms = read(profile.sensors + 4, 2);
    result.usbInitializations = s.usbInitializations;
    result.usbControl = s.B(s.Gd32() ? 0x50000008 : 0x40005c40);
    result.usbStatus = s.B(s.Gd32() ? 0x50000014 : 0x40005c44);
    const uint32_t timer = s.Gd32() ? 0x40014800 : 0x40000800;
    result.pwmCompare = s.B(timer + (s.Gd32() ? 0x34 : 0x40)) & 0xffff;
    result.pwmPeriod = s.B(timer + 0x2c) & 0xffff;
    result.pwmActive = s.PwmDuty() > 0;
    result.adcConversions = s.adcConversions; result.pwmStarts = s.pwmStarts;
    result.screenEntries = s.screenEntries;
    result.voltageCentivolts = s.voltageCentivolts;
    result.voltageRenders = s.voltageRenders;
    if (s.menuStateAddress) {
        result.menuStateAddress = s.menuStateAddress;
        result.menuMagic = read(s.menuStateAddress, 4);
        result.menuActive = static_cast<uint8_t>(read(s.menuStateAddress + 4, 1));
        result.menuSelection = static_cast<uint8_t>(read(s.menuStateAddress + 5, 1));
        result.menuBatteryVoltage = read(s.menuStateAddress + 6, 1) != 0;
        result.menuSequence = static_cast<uint8_t>(read(s.menuStateAddress + 7, 1));
        result.menuSavedPower = read(s.menuStateAddress + 12, 4);
        if (result.menuActive >= 4 && result.menuActive <= 6) {
            result.menuPuffValue = read(s.menuStateAddress + 32, 4);
            result.menuPuffOriginal = read(s.menuStateAddress + 36, 4);
            result.menuPuffDigit = read(s.menuStateAddress + 40, 4);
            result.menuPuffStatus = static_cast<uint8_t>(read(s.menuStateAddress + 48, 1));
        }
    }
    const uint32_t destination = s.B(0x40020014);
    if (destination >= RamBase && destination + sizeof(result.adcSamples) <= RamBase + RamSize)
        uc_mem_read(s.cpu, destination, result.adcSamples.data(), sizeof(result.adcSamples));
    return result;
}

std::string DescribeRegressionState(const Machine& machine) {
    auto& s = *machine.impl_;
    const auto& state = s.snapshot;
    std::ostringstream text;
    text << "us=" << state.virtualMicroseconds << " instructions=" << state.instructions
        << " storageRevision=" << s.revision << " generation=" << state.frameGeneration
        << " spi=" << state.display.spiBytes << " pixels=" << state.display.pixelWrites
        << " sleeping=" << state.sleeping << " pc=0x" << std::hex << state.pc << " sp=0x" << state.sp;
    if (!s.cpu) return text.str();
    const auto observed = ObserveRegressionState(machine);
    text << " mapped=" << observed.mapped << " flags=0x" << observed.flags << " nativePage=0x" << observed.page
        << " railPowered=" << observed.railPowered
        << " powerA=0x" << observed.powerA << " powerB=0x" << observed.powerB
        << " mode=0x" << observed.mode << " limit=0x" << observed.powerLimit
        << " flashControl=0x" << s.B(0x40022010) << " flashStatus=0x" << s.B(0x4002200c)
        << " dmaControl=0x" << s.B(0x40020008) << " dmaFlags=0x" << s.B(0x40020000)
        << " adcControl=0x" << s.B(s.AdcBase() + 8) << " nvic0=0x" << s.C(0xe000e100)
        << " ipsr=" << std::dec << s.Reg(UC_ARM_REG_IPSR) << " hclk=" << s.hclk
        << " feedbackMa=" << observed.feedbackCurrentMilliAmps << " feedbackMv=" << observed.feedbackVoltageMillivolts
        << " feedbackMilliOhms=" << observed.feedbackResistanceMilliOhms
        << " boots=" << s.bootCompletions << " mainIterations=" << s.mainIterations
        << " serviceEntries=" << s.serviceEntries << " powerOffEntries=" << s.powerOffEntries
        << " exceptionInstructions=[";
    for (size_t i = 0; i < s.exceptionInstructions.size(); ++i)
        if (s.exceptionInstructions[i]) text << i << ':' << s.exceptionInstructions[i] << ',';
    text << ']';
    text << std::dec << " batteryMv=" << observed.batteryMillivolts
        << " resistanceMilliOhms=" << observed.resistanceMilliOhms << " locked=" << observed.locked
        << " puffs=" << observed.puffs << " durationCs=" << observed.durationCentiseconds
        << " scoreRaw=" << observed.scoreRawTotal << " scoreBestPuff=" << observed.scoreBestSinglePuff
        << " pwmCompare=" << observed.pwmCompare << " pwmActive=" << observed.pwmActive << " pwmStarts=" << observed.pwmStarts
        << " adcConversions=" << observed.adcConversions << " adcSamples=[";
    for (uint16_t value : observed.adcSamples) text << value << ',';
    text << "] screenEntries=[";
    for (size_t i = 0; i < observed.screenEntries.size(); ++i)
        if (observed.screenEntries[i]) text << i << ':' << observed.screenEntries[i] << ',';
    text << ']';
    text << " menuActive=" << unsigned(observed.menuActive) << " menuSelection=" << unsigned(observed.menuSelection)
        << " menuSequence=" << unsigned(observed.menuSequence) << " menuSavedPower=" << observed.menuSavedPower
        << " puffEdit=" << observed.menuPuffValue << '/' << observed.menuPuffOriginal
        << " puffDigit=" << observed.menuPuffDigit << " puffStatus=" << unsigned(observed.menuPuffStatus)
        << " panelOn=" << s.panelOn << " panelSleep=" << s.panelSleep << " backlightOn=" << s.backlightOn;
    for (const auto& access : state.recentAccess)
        text << "; access{us=" << std::dec << access.virtualMicroseconds << " pc=0x" << std::hex << access.pc
            << " address=0x" << access.address << " value=0x" << access.value << std::dec
            << " size=" << unsigned(access.size) << " write=" << access.write << '}';
    return text.str();
}

// Exercise the actual Unicorn register bank and mapped SRAM, including pointer
// edges that must never trigger a peripheral read or change guest execution.
bool RunDiagnosticsRegressions(Machine& machine, std::string& error) {
    auto& s = *machine.impl_;
    if (!s.opened) { error = "Diagnostics regression requires an open device"; return false; }
    if (!machine.Reset(error)) return false;
    constexpr std::array<int, 13> ids{UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
        UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7, UC_ARM_REG_R8,
        UC_ARM_REG_R9, UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_R12};
    for (size_t i = 0; i < ids.size(); ++i) s.Reg(ids[i], 0xa5000000u + uint32_t(i));
    s.Reg(UC_ARM_REG_XPSR, 0xa1000000);
    s.Reg(UC_ARM_REG_PSP, RamBase + 0x100);
    s.Reg(UC_ARM_REG_MSP, RamBase + RamSize - 8);
    s.Reg(UC_ARM_REG_LR, 0x08001235);
    std::array<uint32_t, 2> words{0x12345678, 0x89abcdef};
    if (uc_mem_write(s.cpu, RamBase + RamSize - 8, words.data(), sizeof(words)) != UC_ERR_OK) {
        error = "Diagnostics regression could not populate mapped SRAM"; return false;
    }
    s.Publish();
    const Snapshot captured = machine.Current();
    for (size_t i = 0; i < ids.size(); ++i) {
        if (!(captured.registers.validGeneral & (1u << i)) ||
            captured.registers.general[i] != 0xa5000000u + uint32_t(i)) {
            error = "Diagnostics did not capture the actual general register bank"; return false;
        }
    }
    if (captured.registers.validGeneral != 0xffff || (captured.registers.validSpecial & 7) != 7 ||
        captured.registers.xpsr != s.Reg(UC_ARM_REG_XPSR) || captured.registers.msp != s.Reg(UC_ARM_REG_MSP) ||
        captured.registers.psp != s.Reg(UC_ARM_REG_PSP) || captured.registers.general[14] != 0x08001235 ||
        captured.registers.general[15] != s.Pc() || captured.sp != captured.registers.general[13] ||
        captured.modeledClockHz != s.hclk || captured.mappedSramBase != RamBase || captured.mappedSramBytes != RamSize ||
        captured.stack.status != StackStatus::Available || captured.stack.count != 2 ||
        captured.stack.words[0] != words[0] || captured.stack.words[1] != words[1]) {
        error = "Diagnostics register, clock, or SRAM-edge snapshot is incorrect"; return false;
    }
    const auto instructions = s.snapshot.instructions, time = s.snapshot.virtualMicroseconds;
    const size_t logSize = s.logSize, logNext = s.logNext;
    for (const uint32_t pointer : {0u, RamBase - 4, RamBase + RamSize + 4, 0x40021000u, 0xfffffffcu}) {
        s.Reg(UC_ARM_REG_MSP, pointer);
        s.Publish();
        if (s.snapshot.stack.status != StackStatus::InvalidPointer || s.snapshot.stack.count ||
            s.snapshot.halted || s.logSize != logSize || s.logNext != logNext ||
            s.snapshot.instructions != instructions || s.snapshot.virtualMicroseconds != time) {
            error = "Invalid stack pointer caused a read or altered guest execution"; return false;
        }
    }
    s.Reg(UC_ARM_REG_MSP, RamBase + RamSize);
    s.Publish();
    if (s.snapshot.stack.status != StackStatus::Empty || s.snapshot.stack.count) {
        error = "One-past-SRAM stack pointer was not represented as empty"; return false;
    }
    s.Reg(UC_ARM_REG_MSP, RamBase);
    s.Publish();
    if (s.snapshot.stack.status != StackStatus::Available || s.snapshot.stack.count != s.snapshot.stack.words.size() ||
        captured.stack.words[0] != words[0] || captured.registers.general[0] != 0xa5000000) {
        error = "Stack capture was unbounded or changed an earlier immutable snapshot"; return false;
    }
    // Also read the active process stack rather than assuming every RTOS uses MSP.
    s.Reg(UC_ARM_REG_CONTROL, 2);
    s.Publish();
    if (s.snapshot.sp != RamBase + 0x100 || s.snapshot.stack.address != RamBase + 0x100) {
        error = "Diagnostics ignored the active process stack"; return false;
    }
    return machine.Reset(error);
}

// Regression-only friend: linked native instructions exercise IT stop/resume,
// real-vector exception entry/return, strict bus/code faults, and AIRCR reset.
// No executable image, settings, boot path, or public API is substituted.
bool RunFaultRegressions(Machine& machine, std::string& error) {
    auto& s = *machine.impl_;
    // ADC_Cmd's real CMP/LDR/ITE NE/ORRNE/BICEQ sequence checks both predicates.
    // The skipped BIC must still be a stop-able, counted instruction boundary.
    auto itBits = [&] { return s.Reg(UC_ARM_REG_XPSR) & 0x0600fc00; };
    if (!machine.Reset(error)) return false;
    s.Reg(UC_ARM_REG_PRIMASK, 1);
    s.Reg(UC_ARM_REG_R0, 0x40020800); s.Reg(UC_ARM_REG_R1, 1);
    s.Reg(UC_ARM_REG_PC, 0x08005d3f);
    for (unsigned i = 0; i < 4; ++i) if (!machine.Step(error)) return false;
    const uint32_t beforeIt = itBits(), beforeXpsr = s.Reg(UC_ARM_REG_XPSR), beforeSp = s.Reg(UC_ARM_REG_MSP);
    if (s.Pc() != 0x08005d48 || s.Reg(UC_ARM_REG_R1) != 1 || !beforeIt) {
        error = "Native IT true predicate or mid-IT single-step state is incorrect"; return false;
    }
    if (!s.Enter(44)) { error = s.snapshot.error; return false; }
    std::array<uint32_t, 8> stacked{};
    if (uc_mem_read(s.cpu, s.Reg(UC_ARM_REG_MSP), stacked.data(), sizeof(stacked)) != UC_ERR_OK ||
        stacked[6] != 0x08005d48 || stacked[7] != beforeXpsr || itBits()) {
        error = "Native-vector IRQ did not stack the precise interrupted IT/xPSR state"; return false;
    }
    for (unsigned i = 0; i < 256 && s.activeDepth; ++i) if (!machine.Step(error)) return false;
    if (s.activeDepth || s.Pc() != 0x08005d48 || itBits() != beforeIt ||
        s.Reg(UC_ARM_REG_MSP) != beforeSp || s.Reg(UC_ARM_REG_R1) != 1) {
        error = "Real TIM2 handler EXC_RETURN did not restore mid-IT CPU state"; return false;
    }
    const uint64_t beforeFalse = machine.Current().instructions;
    if (!machine.Step(error)) return false;
    if (machine.Current().instructions != beforeFalse + 1 || s.Pc() != 0x08005d4c || itBits() ||
        s.Reg(UC_ARM_REG_R1) != 1) {
        error = "False native IT predicate executed or escaped the exact single-step boundary"; return false;
    }
    if (!machine.Reset(error)) return false;
    s.Reg(UC_ARM_REG_PRIMASK, 1); s.B(0x40020808) = 0x101;
    s.Reg(UC_ARM_REG_R0, 0x40020800); s.Reg(UC_ARM_REG_R1, 0);
    s.Reg(UC_ARM_REG_PC, 0x08005d3f);
    for (unsigned i = 0; i < 5; ++i) if (!machine.Step(error)) return false;
    if (s.Pc() != 0x08005d4c || s.Reg(UC_ARM_REG_R1) != 0x100 || itBits()) {
        error = "Native IT inverted predicate or post-IT state is incorrect"; return false;
    }
    // Peripheral diagnostics complement the real-boot directional key test.
    if (!machine.Reset(error)) return false;
    machine.SetInputs({});
    s.Write(0x40010400, 4, PlusPin | MinusPin);
    s.Write(0x40010408, 4, PlusPin | MinusPin);
    s.Write(0x4001040c, 4, PlusPin | MinusPin);
    for (unsigned button = 0; button < 2; ++button) {
        const uint32_t pin = button ? MinusPin : PlusPin, irq = button ? 40 : 8;
        for (bool pressed : {true, false}) {
            s.B(0x40010414) = 0; s.C(0xe000e200) = s.C(0xe000e204) = 0;
            s.RefreshPending();
            Inputs keys;
            keys.plus = !button && pressed; keys.minus = button && pressed;
            machine.SetInputs(keys);
            const uint32_t levels = s.Read(0x40010c10, 4);
            if ((levels & pin) != (pressed ? 0 : pin) ||
                (levels & (FirePin | PlusPin | MinusPin) & ~pin) != ((FirePin | PlusPin | MinusPin) & ~pin) ||
                s.B(0x40010414) != pin || s.C(0xe000e200 + irq / 32 * 4) != (1u << (irq % 32))) {
                error = "Native Plus/PB2 or Minus/PB10 GPIO/EXTI press/release mapping is inconsistent"; return false;
            }
        }
    }
    // Execute PowerOn's linked instructions, including the reported STR.
    if (!machine.Reset(error)) return false;
    s.Reg(UC_ARM_REG_PRIMASK, 1); s.Reg(UC_ARM_REG_PC, 0x0800aa15);
    if (s.Read(0x40005c40, 4) != 3) { error = "USB CTRL reset value is incorrect"; return false; }
    for (unsigned i = 0; i < 3; ++i) if (!machine.Step(error)) return false;
    if (s.Read(0x40005c40, 2) != 1) { error = "Native USB PowerOn did not assert FRST"; return false; }
    for (unsigned i = 0; i < 3; ++i) if (!machine.Step(error)) return false;
    if (s.Read(0x40005c40, 4) || s.Read(0x40005c44, 4) != 0x400) {
        error = "Native USB FRST release did not raise the reset status"; return false;
    }
    for (unsigned i = 0; i < 5; ++i) if (!machine.Step(error)) return false;
    if (s.Read(0x40005c40, 4) != 0x1c00 || s.Read(0x40005c44, 2)) {
        error = "Native USB PowerOn did not clear STS and retain its interrupt mask"; return false;
    }
    s.B(0x40005c44) = 0x1400;
    s.Write(0x40005c44, 2, 0xfbff);
    if (s.Read(0x40005c44, 4) != 0x1000) { error = "USB STS events are not write-zero-to-clear"; return false; }
    s.Write(0x40005c44, 4, 0xffffffff);
    if (s.Read(0x40005c44, 4) != 0x1000) { error = "USB STS writes invented hardware events"; return false; }
    if (s.Known(0x40005c40, 1, false) || s.Known(0x40005c42, 2, true)) {
        error = "USB register gate accepted unsupported byte/high-halfword access"; return false;
    }
    for (bool write : {false, true}) {
        if (!machine.Reset(error)) return false;
        s.Reg(UC_ARM_REG_PRIMASK, 1); s.Reg(UC_ARM_REG_R0, 0x40005c3c); s.Reg(UC_ARM_REG_R1, 0);
        s.Reg(UC_ARM_REG_PC, write ? 0x08006e45 : 0x0800c241);
        std::string reservedFault;
        if (machine.Step(reservedFault) || !machine.Current().halted ||
            reservedFault.find("Unknown MMIO") == std::string::npos || reservedFault.find("0x40005C3C") == std::string::npos) {
            error = "Reserved USB register did not halt with a strict diagnostic: " + reservedFault; return false;
        }
    }
    if (!machine.Reset(error)) return false;
    s.Reg(UC_ARM_REG_PRIMASK, 1);
    s.Reg(UC_ARM_REG_R0, 0x4002f000);
    s.Reg(UC_ARM_REG_PC, 0x0800c241);
    std::string fault;
    if (machine.Step(fault) || !machine.Current().halted ||
        fault.find("Unknown MMIO") == std::string::npos || fault.find("0x4002F000") == std::string::npos ||
        machine.Current().recentAccess.empty() || machine.Current().recentAccess.back().size != 4) {
        error = "Unknown MMIO did not produce a strict PC/address/width diagnostic: " + fault; return false;
    }
    if (!machine.Reset(error)) return false;
    s.Reg(UC_ARM_REG_PRIMASK, 1);
    s.Reg(UC_ARM_REG_R0, 0x1ffff7f4);
    s.Reg(UC_ARM_REG_PC, 0x0800c241);
    if (machine.Step(fault) || fault.find("Unknown identity ROM") == std::string::npos) {
        error = "Virtual UID ROM silently accepted an out-of-range read: " + fault; return false;
    }
    if (!machine.Reset(error)) return false;
    s.Reg(UC_ARM_REG_PC, 0x08020001);
    if (machine.Step(fault) || !machine.Current().halted || fault.find("Code fetch") == std::string::npos) {
        error = "Unmapped code did not halt the native device: " + fault; return false;
    }
    if (!machine.Reset(error) || !machine.RunFor(1234, error)) return false;
    // A RunFor boundary can split a low-HCLK slot. Complete that native
    // instruction before redirecting PC for this isolated reset diagnostic.
    if (s.instructionPending && !machine.Step(error)) return false;
    const uint64_t beforeTime = machine.Current().virtualMicroseconds, beforeInstructions = machine.Current().instructions;
    const uint32_t beforePhase = s.cpuPhase, beforeClock = s.hclk;
    const uint64_t beforeRevision = machine.StorageRevision();
    s.Reg(UC_ARM_REG_XPSR, 0x01000000);
    s.Reg(UC_ARM_REG_PRIMASK, 1);
    s.Reg(UC_ARM_REG_R0, 0xe000ed0c);
    s.Reg(UC_ARM_REG_R1, 0x05fa0004);
    s.Reg(UC_ARM_REG_PC, 0x08006e45); // Actual linked STR r1,[r0], not a reset stub.
    if (!machine.Step(error)) return false;
    const uint32_t afterPhase = (beforePhase + 1000000) % beforeClock;
    if (machine.Current().virtualMicroseconds != beforeTime + (beforePhase + 1000000) / beforeClock ||
        machine.Current().instructions != beforeInstructions + 1 || machine.Current().pc != 0x08005ab0 ||
        s.cpuPhase != uint64_t(afterPhase) * s.hclk / beforeClock || machine.StorageRevision() != beforeRevision) {
        error = "Native AIRCR reset rewound world time/instructions or modified settings: " +
            DescribeRegressionState(machine) + "; beforeUs=" + std::to_string(beforeTime) +
            " beforeInstructions=" + std::to_string(beforeInstructions) +
            " beforePhase=" + std::to_string(beforePhase) + " afterPhase=" + std::to_string(s.cpuPhase) +
            " beforeHclk=" + std::to_string(beforeClock); return false;
    }
    return machine.Reset(error);
}
} // namespace FirmwareDevice
