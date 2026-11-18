#include "firmware_comparison.h"
#include "app_state.h"
#include "firmware_project.h"
#include "resource_i18n.h"
#include <map>
#include <tuple>

namespace FirmwareComparison {
namespace {

std::pair<Rgb8, Rgb8> DrawColors(const StudioState& source, const AssetSpec& spec,
                                size_t rank, bool cyan) {
    return {source.GetForegroundForAsset(spec, rank, cyan).ScreenColor(),
            source.GetBackgroundForAsset(spec, cyan).ScreenColor()};
}

bool SameSpec(const AssetSpec& a, const AssetSpec& b) {
    return a.id == b.id && a.title == b.title && a.group == b.group &&
        a.ordinal == b.ordinal && a.width == b.width && a.height == b.height &&
        a.format == b.format && a.sourceCode == b.sourceCode;
}

// Keep logical identity independent of dimensions so incompatible resources are
// represented explicitly rather than disappearing into an added/removed pair.
using Identity = std::tuple<std::string, std::string, std::string, size_t, uint16_t>;
Identity Key(const AssetSpec& spec) {
    return {spec.id, spec.title, spec.group, spec.ordinal, spec.sourceCode};
}

bool ValidDimensions(const AssetSpec& spec) {
    // Firmware catalogs are small device images. Bound malformed dimensions
    // before arithmetic/decoder allocation; DX11 uses the same axis limit.
    return spec.width > 0 && spec.height > 0 && spec.width <= 16384 && spec.height <= 16384 &&
        static_cast<uint64_t>(spec.width) * spec.height <= 16 * 1024 * 1024;
}

void UpdateResource(Resource& resource, const StudioState& source, size_t index, bool reset) {
    const auto& spec = source.catalog[index];
    const auto replacement = source.replacements.find(spec.id);
    const std::vector<uint8_t>* bytes = replacement != source.replacements.end() ? &replacement->second :
        index < source.originals.size() ? &source.originals[index] : nullptr;
    std::array<std::pair<Rgb8, Rgb8>, 16> colors{};
    if (spec.format != AssetFormat::Rgb565 && ValidDimensions(spec)) {
        const bool rankDependent = !FirmwareExtensionFor(source.model) && spec.id.starts_with("meter_");
        const bool modeDependent = AssetUsesModeColor(spec, source.model) ||
            spec.id == "factory_legacy_label_puff" || spec.id.starts_with("font_11x20_");
        for (size_t context = 0; context < colors.size(); ++context) {
            const size_t rank = rankDependent ? context / 2 : 0;
            const bool cyan = modeDependent && (context & 1) != 0;
            const size_t canonical = rank * 2 + (cyan ? 1 : 0);
            colors[context] = canonical == context ? DrawColors(source, spec, rank, cyan) : colors[canonical];
        }
    }
    bool colorsEqual = resource.decoded;
    if (spec.format != AssetFormat::Rgb565 && colorsEqual) {
        for (size_t context = 0; context < colors.size(); ++context) {
            const auto* image = resource.GetImage(context / 2, (context & 1) != 0);
            if (!image || image->foreground != colors[context].first || image->background != colors[context].second) {
                colorsEqual = false;
                break;
            }
        }
    }
    const bool sameRaw = !reset && bytes && resource.raw == *bytes;
    if (sameRaw && colorsEqual) return;
    if (!sameRaw) resource.mask.clear();
    resource.spec = spec;
    if (!bytes) resource.raw.clear();
    else if (resource.raw != *bytes) resource.raw = *bytes;
    resource.images.clear();
    resource.contexts.fill(0);
    resource.decoded = false;
    if (!bytes || !ValidDimensions(spec) || resource.raw.size() != spec.byte_len()) return;
    if (spec.format == AssetFormat::Rgb565) {
        Image image;
        if (!FirmwareBitmap::DecodeRgb565(resource.raw.data(), resource.raw.size(), spec.width, spec.height, image.rgba)) return;
        resource.images.push_back(std::move(image));
    } else {
        if (resource.mask.empty() &&
            !FirmwareBitmap::DecodeMask(resource.raw.data(), resource.raw.size(), spec.width, spec.height, resource.mask, spec.format)) return;
        for (size_t context = 0; context < colors.size(); ++context) {
            const auto& [foreground, background] = colors[context];
            size_t imageIndex = 0;
            while (imageIndex < resource.images.size() &&
                (resource.images[imageIndex].foreground != foreground || resource.images[imageIndex].background != background)) ++imageIndex;
            if (imageIndex == resource.images.size()) {
                Image image;
                image.foreground = foreground;
                image.background = background;
                FirmwareBitmap::ColorizeMask(resource.mask.data(), spec.width, spec.height, foreground, background, image.rgba);
                resource.images.push_back(std::move(image));
            }
            resource.contexts[context] = static_cast<uint8_t>(imageIndex);
        }
    }
    resource.decoded = true;
}

Entry ComparePair(size_t leftIndex, size_t rightIndex, const Snapshot& left, const Snapshot& right) {
    Entry entry;
    entry.left = leftIndex;
    entry.right = rightIndex;
    const auto& a = left.resources[leftIndex];
    const auto& b = right.resources[rightIndex];
    if (a.spec.width != b.spec.width || a.spec.height != b.spec.height) {
        entry.status = Status::Incompatible;
        return entry;
    }
    if (!a.decoded || !b.decoded) {
        entry.status = Status::DecodeError;
        return entry;
    }
    entry.totalPixels = static_cast<size_t>(a.spec.width) * a.spec.height;
    std::vector<uint8_t> changed(entry.totalPixels, 0);
    // Deduplicate context pairs: RGB images and fixed-color masks compare once.
    std::array<std::pair<uint8_t, uint8_t>, 16> visited{};
    size_t visitedCount = 0;
    for (size_t context = 0; context < 16; ++context) {
        const auto pair = std::make_pair(a.contexts[context], b.contexts[context]);
        if (std::find(visited.begin(), visited.begin() + visitedCount, pair) != visited.begin() + visitedCount) continue;
        visited[visitedCount++] = pair;
        const auto& pixelsA = a.images[pair.first].rgba;
        const auto& pixelsB = b.images[pair.second].rgba;
        for (size_t pixel = 0; pixel < entry.totalPixels; ++pixel) {
            if (!changed[pixel] && pixelsA[pixel] != pixelsB[pixel]) {
                changed[pixel] = 1;
                ++entry.differentPixels;
            }
        }
    }
    entry.status = entry.differentPixels ? Status::Changed : Status::Equal;
    return entry;
}

bool ResourceMatches(const Resource& resource, std::wstring_view query) {
    return i18n::ResourceTextMatches(resource.spec.id, query) ||
        i18n::ResourceTextMatches(resource.spec.title, query) ||
        i18n::ResourceTextMatches(resource.spec.group, query);
}
}

const Image* Resource::GetImage(size_t rank, bool cyan) const {
    const size_t context = (std::min)(rank, size_t{7}) * 2 + (cyan ? 1 : 0);
    const size_t image = contexts[context];
    return decoded && image < images.size() ? &images[image] : nullptr;
}

bool Snapshot::Update(const StudioState& source) {
    const auto& documentPath = source.projectDocument ? source.projectPath : source.firmwarePath;
    if (path != documentPath) path = documentPath;
    if (sourceIdentity == &source && sourceRevision == source.ResourceRevision() && loaded == source.hasFirmware) return false;
    const bool reset = !loaded || model != source.model || edition != source.edition || resources.size() != source.catalog.size();
    loaded = source.hasFirmware;
    model = source.model;
    edition = source.edition;
    if (!loaded) resources.clear();
    else {
        resources.resize(source.catalog.size());
        for (size_t index = 0; index < resources.size(); ++index)
            UpdateResource(resources[index], source, index, reset || !SameSpec(resources[index].spec, source.catalog[index]));
    }
    sourceIdentity = &source;
    sourceRevision = source.ResourceRevision();
    return true;
}

bool LoadFile(const std::string& path, Snapshot& destination, std::string& error) {
    StudioState independent;
    const bool opened = FirmwareProject::IsProjectPath(path) ?
        FirmwareProject::Open(independent, path, error) : independent.OpenFirmware(path, error);
    if (!opened) return false;
    Snapshot candidate;
    candidate.Update(independent);
    candidate.path = path;
    destination = std::move(candidate);
    error.clear();
    return true;
}

bool LoadReference(FirmwareModel model, FirmwareEdition edition,
                   Snapshot& destination, std::string& error) {
    FirmwarePackage package;
    if (!LoadEmbeddedFirmware(package, error, model, edition)) return false;
    StudioState independent;
    if (!independent.LoadPackage(std::move(package), FirmwareDefaultFilename(model, edition), error)) return false;
    if (independent.model != model || independent.edition != edition) {
        error = "Embedded firmware does not match the requested model and edition";
        return false;
    }
    Snapshot candidate;
    candidate.Update(independent);
    destination = std::move(candidate);
    error.clear();
    return true;
}

bool SameFamily(FirmwareModel left, FirmwareModel right) {
    if (left == right) return true;
    // These pairs intentionally share canonical catalogs and resource semantics.
    return ((left == FirmwareModel::DragS || left == FirmwareModel::DragX) &&
            (right == FirmwareModel::DragS || right == FirmwareModel::DragX)) ||
           ((left == FirmwareModel::DragSPro || left == FirmwareModel::DragXPro) &&
            (right == FirmwareModel::DragSPro || right == FirmwareModel::DragXPro));
}

std::vector<Entry> Compare(const Snapshot& left, const Snapshot& right) {
    std::vector<Entry> entries;
    if (!left.loaded || !right.loaded) return entries;
    std::map<Identity, std::vector<size_t>> leftById, rightById;
    for (size_t i = 0; i < left.resources.size(); ++i) leftById[Key(left.resources[i].spec)].push_back(i);
    for (size_t i = 0; i < right.resources.size(); ++i) rightById[Key(right.resources[i].spec)].push_back(i);
    std::vector<bool> matched(right.resources.size(), false);
    entries.reserve(left.resources.size() + right.resources.size());
    for (size_t i = 0; i < left.resources.size(); ++i) {
        const auto key = Key(left.resources[i].spec);
        const auto found = rightById.find(key);
        if (SameFamily(left.model, right.model) && leftById[key].size() == 1 &&
            found != rightById.end() && found->second.size() == 1) {
            const size_t j = found->second.front();
            matched[j] = true;
            entries.push_back(ComparePair(i, j, left, right));
        } else entries.push_back({i, MissingIndex, Status::Removed});
    }
    for (size_t j = 0; j < right.resources.size(); ++j)
        if (!matched[j]) entries.push_back({MissingIndex, j, Status::Added});
    return entries;
}

bool MatchesQuery(const Entry& entry, const Snapshot& left, const Snapshot& right, std::wstring_view query) {
    return query.empty() ||
        (entry.left < left.resources.size() && ResourceMatches(left.resources[entry.left], query)) ||
        (entry.right < right.resources.size() && ResourceMatches(right.resources[entry.right], query));
}

bool BuildDifference(const Resource& left, const Resource& right, size_t rank, bool cyan,
                     std::vector<uint32_t>& rgba, size_t& differentPixels) {
    rgba.clear();
    differentPixels = 0;
    if (left.spec.width != right.spec.width || left.spec.height != right.spec.height) return false;
    const auto* a = left.GetImage(rank, cyan);
    const auto* b = right.GetImage(rank, cyan);
    if (!a || !b || a->rgba.size() != b->rgba.size()) return false;
    rgba.resize(a->rgba.size());
    for (size_t i = 0; i < rgba.size(); ++i) {
        if (a->rgba[i] != b->rgba[i]) {
            rgba[i] = 0xFFFF00FFu;
            ++differentPixels;
        } else {
            const uint32_t pixel = b->rgba[i];
            rgba[i] = 0xFF000000u | ((pixel & 0x00FCFCFCu) >> 2);
        }
    }
    return true;
}

} // namespace FirmwareComparison
