#pragma once

#include "firmware_catalog.h"
#include <array>
#include <limits>
#include <string_view>

struct StudioState;

namespace FirmwareComparison {

inline constexpr size_t MissingIndex = (std::numeric_limits<size_t>::max)();
enum class Status { Changed, Added, Removed, Equal, Incompatible, DecodeError };

// Colors are quantized to the device's RGB565 screen representation. A resource
// may have different draw colors in the eight rank and two mode contexts.
struct Image {
    Rgb8 foreground;
    Rgb8 background;
    std::vector<uint32_t> rgba;
};

struct Resource {
    AssetSpec spec;
    std::vector<uint8_t> raw;
    std::vector<uint8_t> mask; // Retained for palette-only recoloring without decoding.
    std::vector<Image> images;
    std::array<uint8_t, 16> contexts{};
    bool decoded = false;

    const Image* GetImage(size_t rank = 0, bool cyan = false) const;
};

struct Snapshot {
    FirmwareModel model = FirmwareModel::DragS;
    FirmwareEdition edition = FirmwareEdition::Score;
    std::string path;
    std::vector<Resource> resources;
    bool loaded = false;

    // Returns true only when the document's resource revision changes. Does not
    // mutate the source. Unchanged resource bytes/colors reuse decoded images.
    bool Update(const StudioState& source);

private:
    const StudioState* sourceIdentity = nullptr;
    uint64_t sourceRevision = (std::numeric_limits<uint64_t>::max)();
};

struct Entry {
    size_t left = MissingIndex;
    size_t right = MissingIndex;
    Status status = Status::Equal;
    // Union of differing pixels across all native draw contexts, not bytes.
    size_t differentPixels = 0;
    size_t totalPixels = 0;
};

// Firmware and .vfwp project snapshots include staged resource edits. Failed
// loads leave destination intact. No USB, save, or live editor operations.
bool LoadFile(const std::string& path, Snapshot& destination, std::string& error);
// Exact bundled model + edition, never a cross-edition fallback.
bool LoadReference(FirmwareModel model, FirmwareEdition edition,
                   Snapshot& destination, std::string& error);

// Exact semantic IDs within known compatible model families; never addresses.
// Ambiguous duplicate identities are deliberately kept as added/removed rows.
bool SameFamily(FirmwareModel left, FirmwareModel right);
std::vector<Entry> Compare(const Snapshot& left, const Snapshot& right);
bool MatchesQuery(const Entry& entry, const Snapshot& left, const Snapshot& right,
                  std::wstring_view query);

// False for missing, undecodable, or dimension-incompatible images. Highlight
// uses opaque magenta for differences and a dimmed right image for equal pixels.
bool BuildDifference(const Resource& left, const Resource& right, size_t rank, bool cyan,
                     std::vector<uint32_t>& rgba, size_t& differentPixels);

} // namespace FirmwareComparison
