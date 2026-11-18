#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unicorn/unicorn.h>

namespace FirmwareEmulator {
// Explicit VM inputs, not a bootloader dump and never part of exported flash.
// Unicorn requires page-granular mappings; the read hook narrows access to the
// two recovered data objects. The page has neither write nor execute permission.
class LegacyLoaderData {
public:
    static constexpr uint32_t Page = 0x08002000, Version = 0x08002c04, Identity = 0x08002888;
    static bool AllowsRead(uint64_t address, int size) {
        if (size <= 0) return false;
        return (address >= Version && address <= Version + 4 && uint64_t(size) <= Version + 4 - address) ||
            (address >= Identity && address <= Identity + 8 && uint64_t(size) <= Identity + 8 - address);
    }
    bool Open(uc_engine* engine, std::string& error) {
        fault_.clear();
        uc_hook read = 0;
        if (uc_mem_map(engine, Page, 4096, UC_PROT_READ) != UC_ERR_OK ||
            uc_hook_add(engine, &read, UC_HOOK_MEM_READ, reinterpret_cast<void*>(Read), this,
                Page, Page + 4095) != UC_ERR_OK) {
            error = "Unable to map guarded emulated loader metadata";
            return false;
        }
        return true;
    }
    bool Seed(uc_engine* engine, const std::array<uint8_t, 4>& version,
        const std::array<uint8_t, 8>& identity, std::string& error) const {
        if (uc_mem_write(engine, Version, version.data(), version.size()) != UC_ERR_OK ||
            uc_mem_write(engine, Identity, identity.data(), identity.size()) != UC_ERR_OK) {
            error = "Unable to seed explicit emulated loader metadata";
            return false;
        }
        return true;
    }
    void ResetError() { fault_.clear(); }
    const std::string& Error() const { return fault_; }
private:
    std::string fault_;
    static void Read(uc_engine* engine, uc_mem_type, uint64_t address, int size, int64_t, void* user) {
        auto& self = *static_cast<LegacyLoaderData*>(user);
        if (AllowsRead(address, size)) return;
        self.fault_ = "Native firmware read outside the 12 emulated loader metadata bytes";
        uc_emu_stop(engine);
    }
};
}
