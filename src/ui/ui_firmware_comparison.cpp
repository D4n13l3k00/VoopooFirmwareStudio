#include "ui_tabs.h"
#include "ui_firmware_comparison.h"
#include "app_state.h"
#include "path_utils.h"
#include "resource_i18n.h"
#include "../imgui/imgui.h"
#include "ui_controls.h"
#include <cstdio>

namespace UiFirmwareComparison {
namespace {
using namespace FirmwareComparison;

i18n::Str StatusText(Status status) {
    switch (status) {
    case Status::Changed: return i18n::Str::CompareChanged;
    case Status::Added: return i18n::Str::CompareAdded;
    case Status::Removed: return i18n::Str::CompareRemoved;
    case Status::Equal: return i18n::Str::CompareEqual;
    case Status::Incompatible: return i18n::Str::CompareIncompatible;
    default: return i18n::Str::CompareDecodeError;
    }
}

const Snapshot& SourceSnapshot(const State& state, bool right) {
    const auto source = right ? state.rightSource : state.leftSource;
    if (source == Source::Opened) return state.editor;
    if (source == Source::Reference) return state.reference;
    return right ? state.rightFile : state.leftFile;
}

const Snapshot& Left(const State& state) { return SourceSnapshot(state, false); }
const Snapshot& Right(const State& state) { return SourceSnapshot(state, true); }

i18n::Str SourceText(Source source) {
    switch (source) {
    case Source::Opened: return i18n::Str::CompareSourceOpened;
    case Source::Reference: return i18n::Str::CompareSourceReference;
    default: return i18n::Str::CompareSourceFile;
    }
}

void RefreshReference(State& state, const StudioState& studio) {
    if (!studio.hasFirmware) {
        if (state.referenceCached) {
            state.reference = Snapshot{};
            state.referenceCached = false;
            state.referenceError.clear();
            state.comparisonDirty = true;
        }
        return;
    }
    if (state.referenceCached && state.referenceModel == studio.model &&
        state.referenceEdition == studio.edition) return;
    state.referenceCached = true;
    state.referenceModel = studio.model;
    state.referenceEdition = studio.edition;
    if (!LoadReference(studio.model, studio.edition, state.reference, state.referenceError))
        state.reference = Snapshot{};
    state.comparisonDirty = true;
}

void PickSource(State& state, bool right, HWND hwnd) {
    std::string path;
    if (!PathUtils::PickFileDialog(hwnd,
        L"VOOPOO Firmware / Project (*.zwdf;*.swdf;*.xwdf;*.vfwp)\0*.zwdf;*.swdf;*.xwdf;*.vfwp\0All Files (*.*)\0*.*\0", path)) return;
    auto& destination = right ? state.rightFile : state.leftFile;
    auto& error = right ? state.rightError : state.leftError;
    if (!LoadFile(path, destination, error)) {
        // A failed replacement must not leave an old successful comparison visible.
        destination = Snapshot{};
    }
    state.comparisonDirty = true;
}

void SourcePanel(State& state, bool right, const StudioState& studio, HWND hwnd) {
    ImGui::PushID(right ? "RightSource" : "LeftSource");
    ImGui::TextUnformatted(Tr(right ? i18n::Str::CompareRight : i18n::Str::CompareLeft));
    auto& kind = right ? state.rightSource : state.leftSource;
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##Source", Tr(SourceText(kind)))) {
        const Source options[] = {Source::Opened, Source::Reference, Source::File};
        for (const auto option : options) {
            if (right && option == Source::Opened) continue;
            const bool selected = kind == option;
            if (ImGui::Selectable(Tr(SourceText(option)), selected) && !selected) {
                kind = option;
                state.comparisonDirty = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (kind == Source::Opened && state.editor.Update(studio)) state.comparisonDirty = true;
    if (kind == Source::Reference) RefreshReference(state, studio);
    if (kind == Source::File && ImGui::Button(Tr(i18n::Str::CompareChooseFile))) PickSource(state, right, hwnd);
    const auto& source = SourceSnapshot(state, right);
    if (source.loaded) {
        ImGui::TextWrapped("%s", source.path.c_str());
        ImGui::TextDisabled("%s / %s", FirmwareModelName(source.model), FirmwareEditionName(source.edition));
    } else if (kind != Source::File && !studio.hasFirmware) {
        ImGui::TextWrapped("%s", Tr(i18n::Str::CompareEditorUnavailable));
    } else if (kind == Source::File) {
        ImGui::TextDisabled("%s", Tr(i18n::Str::CompareChooseFile));
    }
    const auto& error = kind == Source::Reference ? state.referenceError : right ? state.rightError : state.leftError;
    if (kind != Source::Opened && !error.empty()) ImGui::TextWrapped(Tr(i18n::Str::MsgOpenError), error.c_str());
    ImGui::PopID();
}

void RefreshComparison(State& state) {
    if (!state.comparisonDirty) return;
    // Keep selection by semantic identity across live edits and file reloads.
    std::string selectedId;
    size_t selectedOrdinal = 0;
    const auto& left = Left(state);
    if (state.selected < state.entries.size()) {
        const auto& old = state.entries[state.selected];
        const auto* resource = old.right < Right(state).resources.size() ? &Right(state).resources[old.right] :
            old.left < left.resources.size() ? &left.resources[old.left] : nullptr;
        if (resource) {
            selectedId = resource->spec.id;
            selectedOrdinal = resource->spec.ordinal;
        }
    }
    state.entries = Compare(left, Right(state));
    state.selected = MissingIndex;
    if (!selectedId.empty()) {
        for (size_t i = 0; i < state.entries.size(); ++i) {
            const auto& entry = state.entries[i];
            const auto& resource = entry.right < Right(state).resources.size() ? Right(state).resources[entry.right] : left.resources[entry.left];
            if (resource.spec.id == selectedId && resource.spec.ordinal == selectedOrdinal) {
                state.selected = i;
                break;
            }
        }
    }
    state.comparisonDirty = false;
    state.filterDirty = true;
    state.previewDirty = true;
}

void RefreshFilter(State& state) {
    if (!state.filterDirty) return;
    state.visible.clear();
    const auto query = PathUtils::Utf8ToWide(state.query);
    for (size_t i = 0; i < state.entries.size(); ++i) {
        const auto& entry = state.entries[i];
        if (state.statusFilter && static_cast<int>(entry.status) + 1 != state.statusFilter) continue;
        if (MatchesQuery(entry, Left(state), Right(state), query)) state.visible.push_back(i);
    }
    if (std::find(state.visible.begin(), state.visible.end(), state.selected) == state.visible.end()) {
        state.selected = state.visible.empty() ? MissingIndex : state.visible.front();
        state.previewDirty = true;
    }
    state.filterDirty = false;
}

bool Upload(Dx11Texture& texture, const Resource* resource, int rank, bool cyan,
            ID3D11Device* device, ID3D11DeviceContext* context) {
    const auto* image = resource ? resource->GetImage(static_cast<size_t>(rank), cyan) : nullptr;
    if (!image) {
        texture.Release();
        return true;
    }
    return texture.Update(device, context, image->rgba.data(), resource->spec.width, resource->spec.height);
}

void RefreshPreview(State& state, ID3D11Device* device) {
    if (state.textureDevice != device) {
        state.textureDevice = device;
        state.previewDirty = true;
        state.leftTexture.Release();
        state.rightTexture.Release();
        state.differenceTexture.Release();
    }
    if (!state.previewDirty) return;
    state.previewDirty = false;
    state.textureError = false;
    state.differencePixels.clear();
    state.previewDifferentPixels = 0;
    if (state.selected >= state.entries.size()) {
        state.leftTexture.Release();
        state.rightTexture.Release();
        state.differenceTexture.Release();
        return;
    }
    const auto& entry = state.entries[state.selected];
    const auto& left = Left(state);
    const auto* a = entry.left < left.resources.size() ? &left.resources[entry.left] : nullptr;
    const auto* b = entry.right < Right(state).resources.size() ? &Right(state).resources[entry.right] : nullptr;
    ID3D11DeviceContext* context = nullptr;
    if (device) device->GetImmediateContext(&context);
    const bool leftOk = Upload(state.leftTexture, a, state.rank, state.cyan, device, context);
    const bool rightOk = Upload(state.rightTexture, b, state.rank, state.cyan, device, context);
    state.textureError = !leftOk || !rightOk;
    if (a && b && BuildDifference(*a, *b, static_cast<size_t>(state.rank), state.cyan,
                                 state.differencePixels, state.previewDifferentPixels)) {
        if (!state.differenceTexture.Update(device, context, state.differencePixels.data(), a->spec.width, a->spec.height))
            state.textureError = true;
    } else state.differenceTexture.Release();
    if (context) context->Release();
}

void ImagePanel(const char* id, i18n::Str title, const Resource* resource,
                const Dx11Texture& texture, int zoom, float height) {
    ImGui::PushID(id);
    ImGui::TextUnformatted(Tr(title));
    if (resource) {
        ImGui::TextWrapped("%s: %s", Tr(i18n::Str::LabelId), resource->spec.id.c_str());
        ImGui::TextWrapped("%s: %s", Tr(i18n::Str::LabelGroup), i18n::ResourceText(resource->spec.group));
        ImGui::Text("%s: %u x %u", Tr(i18n::Str::LabelDimensions), resource->spec.width, resource->spec.height);
    }
    ImGui::BeginChild("Canvas", ImVec2(0, height), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (!resource) ImGui::TextWrapped("%s", Tr(i18n::Str::CompareMissingResource));
    else if (!resource->decoded) ImGui::TextWrapped("%s", Tr(i18n::Str::CompareImageUnavailable));
    else if (texture.IsValid()) {
        ImGui::Image((ImTextureID)texture.GetSRV(), ImVec2(static_cast<float>(resource->spec.width * zoom),
            static_cast<float>(resource->spec.height * zoom)));
        if (ImGui::IsItemHovered()) {
            const ImVec2 origin = ImGui::GetItemRectMin();
            const ImVec2 mouse = ImGui::GetMousePos();
            const auto x = static_cast<uint32_t>((mouse.x - origin.x) / zoom);
            const auto y = static_cast<uint32_t>((mouse.y - origin.y) / zoom);
            const auto* image = resource->GetImage();
            if (image && x < resource->spec.width && y < resource->spec.height)
                ImGui::SetTooltip("(%u, %u)", x, y);
        }
    }
    ImGui::EndChild();
    ImGui::PopID();
}

void Inspector(State& state, ID3D11Device* device) {
    if (state.selected >= state.entries.size()) {
        ImGui::TextWrapped("%s", Tr(i18n::Str::SelectResourcePrompt));
        return;
    }
    const auto& entry = state.entries[state.selected];
    const auto& left = Left(state);
    const Resource* a = entry.left < left.resources.size() ? &left.resources[entry.left] : nullptr;
    const Resource* b = entry.right < Right(state).resources.size() ? &Right(state).resources[entry.right] : nullptr;
    const auto* resource = b ? b : a;
    ImGui::TextWrapped("%s", i18n::ResourceText(resource->spec.title));
    ImGui::TextDisabled("%s", Tr(StatusText(entry.status)));
    if (entry.status == Status::Changed || entry.status == Status::Equal)
        ImGui::Text(Tr(i18n::Str::CompareDifferentPixels), entry.differentPixels, entry.totalPixels);
    if (entry.status == Status::Incompatible)
        ImGui::TextWrapped("%s", Tr(i18n::Str::CompareIncompatibleHint));
    if (Ui::BeginControlTable("ComparisonPreviewControls",
        (std::min)(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 30))) {
        Ui::ControlRow(Tr(i18n::Str::LabelZoom));
        Ui::ZoomSlider("##Zoom", state.zoom);
        Ui::ControlRow(Tr(i18n::Str::LabelPreviewRank));
        if (ImGui::SliderInt("##Rank", &state.rank, 0, 7)) state.previewDirty = true;
        ImGui::EndTable();
    }
    const auto& style = ImGui::GetStyle();
    if (ImGui::Checkbox(Tr(i18n::Str::LabelPreviewCyanMode), &state.cyan)) state.previewDirty = true;
    const float highlightWidth = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x +
        ImGui::CalcTextSize(Tr(i18n::Str::CompareHighlightPixels)).x;
    if (ImGui::GetItemRectSize().x + style.ItemSpacing.x + highlightWidth <= ImGui::GetContentRegionAvail().x)
        ImGui::SameLine();
    ImGui::Checkbox(Tr(i18n::Str::CompareHighlightPixels), &state.highlight);
    RefreshPreview(state, device);
    if (state.textureError) ImGui::TextWrapped("%s", Tr(i18n::Str::CompareTextureError));
    const bool showDifference = state.highlight && state.differenceTexture.IsValid();
    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    const float captionsHeight = lineHeight * (showDifference ? 6.0f : 4.0f) +
        ImGui::GetStyle().WindowPadding.y * 2.0f;
    const float canvasHeight = (std::max)(32.0f,
        (ImGui::GetContentRegionAvail().y - captionsHeight) / (showDifference ? 2.0f : 1.0f));
    if (ImGui::BeginTable("SideBySide", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImagePanel("LeftImage", i18n::Str::CompareLeft, a, state.leftTexture, state.zoom, canvasHeight);
        ImGui::TableSetColumnIndex(1);
        ImagePanel("RightImage", i18n::Str::CompareRight, b, state.rightTexture, state.zoom, canvasHeight);
        ImGui::EndTable();
    }
    if (showDifference) {
        ImGui::Separator();
        ImGui::TextUnformatted(Tr(i18n::Str::CompareDifferencePreview));
        ImGui::Text(Tr(i18n::Str::CompareDifferentPixels), state.previewDifferentPixels,
                    state.differencePixels.size());
        ImGui::BeginChild("DifferenceCanvas", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::Image((ImTextureID)state.differenceTexture.GetSRV(),
            ImVec2(static_cast<float>(resource->spec.width * state.zoom), static_cast<float>(resource->spec.height * state.zoom)));
        ImGui::EndChild();
    }
}

void ResourceList(State& state) {
    if (Ui::BeginControlTable("ComparisonFilter")) {
        Ui::ControlRow(Tr(i18n::Str::LabelSearch));
        if (ImGui::InputTextWithHint("##Search", Tr(i18n::Str::SearchPlaceholder), state.query, sizeof(state.query)))
            state.filterDirty = true;
        ImGui::EndTable();
    }
    const i18n::Str options[] = {i18n::Str::CompareAllStatuses, i18n::Str::CompareChanged,
        i18n::Str::CompareAdded, i18n::Str::CompareRemoved, i18n::Str::CompareEqual,
        i18n::Str::CompareIncompatible, i18n::Str::CompareDecodeError};
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##StatusFilter", Tr(options[state.statusFilter]))) {
        for (int i = 0; i < static_cast<int>(std::size(options)); ++i) {
            if (ImGui::Selectable(Tr(options[i]), state.statusFilter == i)) {
                state.statusFilter = i;
                state.filterDirty = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Tr(i18n::Str::CompareStatusFilter));
    RefreshFilter(state);
    if (state.visible.empty()) {
        ImGui::TextWrapped("%s", Tr(i18n::Str::NoResourcesMatchFilter));
        return;
    }
    if (ImGui::BeginTable("ComparisonResources", 2,
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable,
        ImVec2(0, 0))) {
        ImGui::TableSetupColumn("##Resource", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##Status", ImGuiTableColumnFlags_WidthFixed, 105.0f);
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(state.visible.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const size_t index = state.visible[row];
                const auto& entry = state.entries[index];
                const auto& resource = entry.right < Right(state).resources.size() ? Right(state).resources[entry.right] : Left(state).resources[entry.left];
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                char label[256];
                snprintf(label, sizeof(label), "%s (%u x %u)", i18n::ResourceText(resource.spec.title),
                    resource.spec.width, resource.spec.height);
                if (ImGui::Selectable(label, state.selected == index, ImGuiSelectableFlags_SpanAllColumns)) {
                    state.selected = index;
                    state.previewDirty = true;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s", resource.spec.id.c_str(), i18n::ResourceText(resource.spec.group));
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", Tr(StatusText(entry.status)));
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
}
}

void RenderTab(State& state, const StudioState& studio, HWND hwnd, ID3D11Device* device) {
    state.activeTab = false;
    char tabTitle[256];
    snprintf(tabTitle, sizeof(tabTitle), "%s###TabFwComparison", Tr(i18n::Str::TabFirmwareComparison));
    if (!Ui::BeginTabItem(tabTitle)) return;
    state.activeTab = true;
    ImGui::TextDisabled("%s", Tr(i18n::Str::CompareReadOnlyNote));
    if (ImGui::BeginTable("ComparisonSources", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        SourcePanel(state, false, studio, hwnd);
        ImGui::TableSetColumnIndex(1);
        SourcePanel(state, true, studio, hwnd);
        ImGui::EndTable();
    }
    ImGui::Separator();
    RefreshComparison(state);
    if (!Left(state).loaded || !Right(state).loaded) {
        ImGui::TextWrapped("%s", Tr(i18n::Str::CompareChooseSources));
        RefreshPreview(state, device); // Release textures from an unavailable source.
        ImGui::EndTabItem();
        return;
    }
    const float width = ImGui::GetContentRegionAvail().x;
    const float listWidth = (std::min)(420.0f, (std::max)(220.0f, width * 0.36f));
    ImGui::BeginChild("ComparisonList", ImVec2(listWidth, 0), true);
    ResourceList(state);
    ImGui::EndChild();
    ImGui::SameLine();
    RefreshPreview(state, device);
    ImGui::BeginChild("ComparisonInspector", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    Inspector(state, device);
    ImGui::EndChild();
    ImGui::EndTabItem();
}

} // namespace UiFirmwareComparison
