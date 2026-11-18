#include "firmware_emulator_screens.h"
#include "app_state.h"
#include <algorithm>

namespace FirmwareEmulator {
std::span<const ScreenInfo> AvailableScreens(const StudioState& source, uint16_t hardwareCode) {
    if (!source.hasFirmware) return {};
    switch (source.model) {
    case FirmwareModel::DragS:
    case FirmwareModel::DragX: return SxScreens(source.model, source.edition, hardwareCode);
    case FirmwareModel::Drag1:
    case FirmwareModel::Drag2:
    case FirmwareModel::DragMini:
    case FirmwareModel::Drag2Refresh:
    case FirmwareModel::DragMiniRefresh: return LegacyScreens(source.model, source.edition, hardwareCode);
    case FirmwareModel::Drag3:
    case FirmwareModel::DragMax: return ThreeMaxScreens(source.model, source.edition, hardwareCode);
    case FirmwareModel::DragSPro:
    case FirmwareModel::DragXPro:
    case FirmwareModel::DragXPlus:
    case FirmwareModel::DragXPlusPro: return ProPlusScreens(source.model, source.edition, hardwareCode);
    default: return {};
    }
}
bool ResourceReadTrace::Attach(uc_engine* engine, const StudioState& source, uint16_t code,
    uint32_t flashBase, size_t flashSize, std::string& error) {
    ranges_.clear();
    maximumEnds_.clear();
    used_.assign(source.catalog.size(), false);
    const auto hardware = std::find_if(source.firmware.records.begin(), source.firmware.records.end(),
        [code](const HardwareRecord& record) {
            return (uint16_t(record.device_code[0]) << 8 | record.device_code[1]) == code;
        });
    const size_t recordIndex = static_cast<size_t>(hardware - source.firmware.records.begin());
    for (size_t i = 0; i < source.catalog.size(); ++i) {
        const auto& spec = source.catalog[i];
        uint32_t address = spec.address;
        if (!spec.recordAddresses.empty()) {
            const auto record = std::find_if(spec.recordAddresses.begin(), spec.recordAddresses.end(),
                [code](const auto& item) { return item.first == code; });
            if (record == spec.recordAddresses.end()) continue;
            address = record->second;
        } else if (hardware != source.firmware.records.end() && i < source.locations.size() &&
            recordIndex < source.locations[i].size()) {
            const size_t offset = source.locations[i][recordIndex];
            if (offset == SIZE_MAX || offset > flashSize) continue;
            address = flashBase + static_cast<uint32_t>(offset);
        } else if (spec.sourceCode && spec.sourceCode != code) continue;
        if (!spec.byte_len() || address < flashBase || spec.byte_len() > flashSize ||
            address - flashBase > flashSize - spec.byte_len()) continue;
        ranges_.push_back({address, address + static_cast<uint32_t>(spec.byte_len()), i});
    }
    std::sort(ranges_.begin(), ranges_.end(), [](const Range& a, const Range& b) { return a.first < b.first; });
    uint32_t maximum = 0;
    maximumEnds_.reserve(ranges_.size());
    for (const auto& range : ranges_) {
        maximum = (std::max)(maximum, range.last);
        maximumEnds_.push_back(maximum);
    }
    // Observing code/literal loads breaks Thumb IT execution in Unicorn 2.1.4.
    // Observe only the merged asset intervals, never the whole Flash mapping.
    for (size_t i = 0; i < ranges_.size();) {
        const uint32_t first = ranges_[i].first;
        uint32_t last = ranges_[i++].last;
        while (i < ranges_.size() && ranges_[i].first <= last)
            last = (std::max)(last, ranges_[i++].last);
        uc_hook hook = 0;
        const auto result = uc_hook_add(engine, &hook, UC_HOOK_MEM_READ,
            reinterpret_cast<void*>(Read), this, first, last - 1);
        if (result != UC_ERR_OK) {
            error = std::string("Tracing native resource reads: ") + uc_strerror(result);
            return false;
        }
    }
    return true;
}
void ResourceReadTrace::Read(uc_engine*, uc_mem_type, uint64_t address, int size, int64_t, void* user) {
    auto& self = *static_cast<ResourceReadTrace*>(user);
    if (size <= 0) return;
    const uint64_t end = address + static_cast<unsigned>(size);
    auto found = std::lower_bound(self.ranges_.begin(), self.ranges_.end(), end,
        [](const Range& range, uint64_t last) { return range.first < last; });
    size_t count = static_cast<size_t>(found - self.ranges_.begin());
    while (count) {
        --count;
        if (self.maximumEnds_[count] <= address) break;
        const auto& range = self.ranges_[count];
        if (range.last > address) self.used_[range.index] = true;
    }
}
void ResourceReadTrace::Reset() { std::fill(used_.begin(), used_.end(), false); }
void ResourceReadTrace::Collect(Frame& frame) const {
    frame.resources.clear();
    for (size_t i = 0; i < used_.size(); ++i)
        if (used_[i]) frame.resources.push_back(i);
}
}
