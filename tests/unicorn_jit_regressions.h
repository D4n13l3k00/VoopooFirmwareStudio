#pragma once
#include <unicorn/unicorn.h>
#include <unicorn/arm.h>
#include "../src/core/firmware_emulator_runtime.h"
#include <array>
#include <ostream>
#include <string>
#include <windows.h>
#include <psapi.h>

inline bool RunUnicornJitRegressions(std::string& error, std::ostream* report = nullptr) {
    uc_engine* cpu = nullptr;
    auto check = [&](uc_err result, const char* operation) {
        if (result == UC_ERR_OK) return true;
        error = std::string(operation) + ": " + uc_strerror(result);
        return false;
    };
    if (!check(uc_open(UC_ARCH_ARM, static_cast<uc_mode>(UC_MODE_THUMB | UC_MODE_MCLASS), &cpu),
        "Opening JIT boundary fixture")) return false;
    struct Close { uc_engine* cpu; ~Close() { uc_close(cpu); } } close{cpu};
    // Smaller than the Windows VEH's 4-MiB commit step: an unbounded lazy
    // commit crosses the reserved region on the very first native translation.
    if (!check(uc_ctl_set_cpu_model(cpu, UC_CPU_ARM_CORTEX_M4), "Selecting JIT fixture CPU") ||
        !check(uc_ctl_set_tcg_buffer_size(cpu, uint32_t(2 * 1024 * 1024)), "Bounding JIT allocation") ||
        !check(uc_mem_map(cpu, 0x10000, 4096, UC_PROT_READ | UC_PROT_EXEC), "Mapping JIT fixture code")) return false;
    const std::array<uint8_t, 4> code{0x01, 0x30, 0x70, 0x47}; // ADDS r0,#1; BX lr.
    if (!check(uc_mem_write(cpu, 0x10000, code.data(), code.size()), "Loading real Thumb instructions")) return false;
    for (uint32_t iteration = 0; iteration < 4096; ++iteration) {
        uint32_t value = iteration, link = 0x10005;
        if (!check(uc_reg_write(cpu, UC_ARM_REG_R0, &value), "Seeding JIT fixture input") ||
            !check(uc_reg_write(cpu, UC_ARM_REG_LR, &link), "Seeding JIT fixture return") ||
            !check(uc_emu_start(cpu, 0x10001, 0x10004, 0, 16), "Executing native JIT fixture") ||
            !check(uc_reg_read(cpu, UC_ARM_REG_R0, &value), "Reading native JIT fixture output")) return false;
        uint32_t pc = 0;
        if (!check(uc_reg_read(cpu, UC_ARM_REG_PC, &pc), "Reading native JIT fixture PC")) return false;
        if (value != iteration + 1 || pc != 0x10004) {
            error = "Repeated JIT invalidation changed real Thumb execution or failed its native return";
            return false;
        }
        if (!check(uc_ctl_remove_cache(cpu, 0x10000, 0x10004), "Invalidating completed JIT fixture")) return false;
    }
    if (report) *report << "Windows JIT bounded commit and 4096 native retranslations: OK\n";
    PROCESS_MEMORY_COUNTERS_EX before{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&before), sizeof(before))) {
        error = "Cannot measure native engine lifetime memory"; return false;
    }
    for (uint32_t iteration = 0; iteration < 64; ++iteration) {
        uc_engine* next = nullptr;
        if (!check(uc_open(UC_ARCH_ARM, static_cast<uc_mode>(UC_MODE_THUMB | UC_MODE_MCLASS), &next),
            "Opening repeated native engine")) return false;
        Close owner{next};
        if (!check(uc_ctl_set_cpu_model(next, UC_CPU_ARM_CORTEX_M4), "Selecting repeated CPU") ||
            !check(uc_ctl_set_tcg_buffer_size(next, uint32_t(2 * 1024 * 1024)), "Bounding repeated JIT") ||
            !check(uc_mem_map(next, 0x10000, 4096, UC_PROT_READ | UC_PROT_EXEC), "Mapping repeated code") ||
            !check(uc_mem_write(next, 0x10000, code.data(), code.size()), "Loading repeated code")) return false;
        uint32_t value = iteration, link = 0x10005;
        uc_context* saved = nullptr;
        if (!check(uc_context_alloc(next, &saved), "Allocating repeated context")) return false;
        struct Context { uc_context* value; ~Context() { uc_context_free(value); } } context{saved};
        if (!check(uc_reg_write(next, UC_ARM_REG_R0, &value), "Seeding repeated input") ||
            !check(uc_reg_write(next, UC_ARM_REG_LR, &link), "Seeding repeated return") ||
            !check(uc_context_save(next, saved), "Saving repeated CPU") ||
            !check(uc_emu_start(next, 0x10001, 0x10004, 0, 16), "Executing repeated engine") ||
            !check(uc_reg_read(next, UC_ARM_REG_R0, &value), "Reading repeated output") ||
            value != iteration + 1) {
            if (error.empty()) error = "Repeated native engine changed the Thumb result";
            return false;
        }
    }
    PROCESS_MEMORY_COUNTERS_EX after{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&after), sizeof(after))) {
        error = "Cannot measure completed native engine lifetimes"; return false;
    }
    if (report) *report << "Native engine lifetimes=64 privateBefore=" << before.PrivateUsage
        << " privateAfter=" << after.PrivateUsage << '\n' << std::flush;
    if (after.PrivateUsage > before.PrivateUsage + size_t(128) * 1024 * 1024) {
        error = "Closing native engines retains over 128 MiB of committed memory"; return false;
    }
    FirmwareEmulator::ArmRuntime runtime;
    const std::array<uint8_t, 6> boundedCode{0x01, 0x30, 0x70, 0x47, 0xfe, 0xe7}; // ADDS; BX lr; B self.
    if (!runtime.Open(boundedCode, 0x10000, 0x20000000, 4096, UC_CPU_ARM_CORTEX_M4, {}, error)) return false;
    bool delayed = false;
    uc_hook delayHook = 0;
    const auto delayOnce = +[](uc_engine*, uint64_t, uint32_t, void* data) {
        auto& delayed = *static_cast<bool*>(data);
        if (!delayed) { delayed = true; Sleep(650); }
    };
    if (!check(uc_hook_add(runtime.Raw(), &delayHook, UC_HOOK_CODE,
        reinterpret_cast<void*>(delayOnce), &delayed, 1, 0), "Simulating slow native host")) return false;
    if (!runtime.Call(0x10000, 0x20001000, {41, 0, 0, 0}, error)) return false;
    uint32_t slowResult = 0;
    if (!check(uc_reg_read(runtime.Raw(), UC_ARM_REG_R0, &slowResult), "Reading delayed native result") ||
        !check(uc_hook_del(runtime.Raw(), delayHook), "Removing host delay hook")) return false;
    if (!delayed || slowResult != 42) {
        error = "Host scheduling delay changed a valid native call result"; return false;
    }
    if (runtime.Call(0x10004, 0x20001000, {}, error) ||
        error.find("instruction limit") == std::string::npos) {
        error = "Native instruction budget did not stop an infinite guest loop"; return false;
    }
    error.clear();
    if (report) *report << "Native runtime: delayed host preserves result; infinite guest remains instruction-bounded: OK\n";
    return true;
}
