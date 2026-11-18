#include "firmware_emulator_runtime.h"
#include <cstdio>

namespace FirmwareEmulator {
namespace {
constexpr uint32_t ReturnAddress = 0x0fff0000;
bool Check(uc_err result, const char* operation, std::string& error) {
    if (result == UC_ERR_OK) return true;
    error = std::string(operation) + ": " + uc_strerror(result);
    return false;
}
}
ArmRuntime::~ArmRuntime() { Close(); }
void ArmRuntime::Close() {
    if (resetCpu_) uc_context_free(resetCpu_);
    resetCpu_ = nullptr;
    if (engine_) uc_close(engine_);
    engine_ = nullptr;
}
bool ArmRuntime::Open(std::span<const uint8_t> image, uint32_t flashBase, uint32_t ramBase,
    size_t ramSize, int cpuModel, std::span<const uint32_t> peripheralPages, std::string& error) {
    Close();
    error.clear();
    if (image.empty() || image.size() > 0x200000 || !ramSize || ramSize > 0x100000 ||
        (ramBase & 4095) || (ramSize & 4095)) {
        error = "Invalid native emulator memory layout";
        return false;
    }
    auto fail = [&] { Close(); return false; };
    if (!Check(uc_open(UC_ARCH_ARM, static_cast<uc_mode>(UC_MODE_THUMB | UC_MODE_MCLASS), &engine_), "Opening ARM emulator", error) ||
        !Check(uc_ctl_set_cpu_model(engine_, cpuModel), "Selecting Cortex-M CPU", error)) return fail();
    const uint32_t alignedBase = flashBase & ~uint32_t(4095);
    const size_t flashSize = (image.size() + flashBase - alignedBase + 4095) & ~size_t(4095);
    if (!Check(uc_mem_map(engine_, alignedBase, flashSize, UC_PROT_READ | UC_PROT_EXEC), "Mapping native flash", error) ||
        !Write(flashBase, image.data(), image.size(), error) ||
        !Check(uc_mem_map(engine_, ramBase, ramSize, UC_PROT_READ | UC_PROT_WRITE), "Mapping native RAM", error) ||
        !Check(uc_mem_map(engine_, ReturnAddress, 4096, UC_PROT_READ | UC_PROT_EXEC), "Mapping native return", error)) return fail();
    const uint16_t breakpoint = 0xbe00;
    if (!Write(ReturnAddress, breakpoint, error)) return fail();
    pages_.assign(peripheralPages.begin(), peripheralPages.end());
    for (uint32_t page : pages_) {
        if ((page & 4095) || !Check(uc_mem_map(engine_, page, 4096, UC_PROT_READ | UC_PROT_WRITE), "Mapping native peripheral", error))
            return fail();
    }
    const uint32_t xpsr = 0x01000000;
    if (!Check(uc_reg_write(engine_, UC_ARM_REG_XPSR, &xpsr), "Initializing Thumb state", error) ||
        !Check(uc_context_alloc(engine_, &resetCpu_), "Allocating reset CPU state", error) ||
        !Check(uc_context_save(engine_, resetCpu_), "Saving reset CPU state", error)) return fail();
    ramBase_ = ramBase;
    zeroRam_.assign(ramSize, 0);
    return true;
}
bool ArmRuntime::Write(uint32_t address, const void* data, size_t size, std::string& error) {
    if (!engine_) { error = "Native emulator is not initialized"; return false; }
    return Check(uc_mem_write(engine_, address, data, size), "Writing native memory", error);
}
bool ArmRuntime::Reset(std::string& error) {
    if (!engine_ || !resetCpu_) { error = "Native emulator is not initialized"; return false; }
    if (!Check(uc_context_restore(engine_, resetCpu_), "Resetting native CPU", error) ||
        !Write(ramBase_, zeroRam_.data(), zeroRam_.size(), error)) return false;
    const std::array<uint8_t, 4096> zeroPage{};
    for (uint32_t page : pages_)
        if (!Write(page, zeroPage.data(), zeroPage.size(), error)) return false;
    return true;
}
bool ArmRuntime::Call(uint32_t address, uint32_t stack,
    const std::array<uint32_t, 4>& arguments, std::string& error) {
    if (!engine_) { error = "Native emulator is not initialized"; return false; }
    const std::array<int, 4> regs{UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3};
    for (size_t i = 0; i < regs.size(); ++i)
        if (!Check(uc_reg_write(engine_, regs[i], &arguments[i]), "Setting native arguments", error)) return false;
    const uint32_t link = ReturnAddress | 1;
    if (!Check(uc_reg_write(engine_, UC_ARM_REG_SP, &stack), "Setting native stack", error) ||
        !Check(uc_reg_write(engine_, UC_ARM_REG_LR, &link), "Setting native return", error)) return false;
    const auto result = uc_emu_start(engine_, address | 1, ReturnAddress, 500000, 20000000);
    uint32_t pc = 0;
    if (!Check(uc_reg_read(engine_, UC_ARM_REG_PC, &pc), "Reading native PC", error)) return false;
    if (result != UC_ERR_OK || pc != ReturnAddress) {
        char location[32];
        snprintf(location, sizeof(location), " at 0x%08X", pc);
        error = std::string(result != UC_ERR_OK ? uc_strerror(result) : "Native execution interrupted or exceeded its limit") + location;
        return false;
    }
    return true;
}
}
