#pragma once
#include "app_state.h"
#include "ui_firmware_device.h"
#include "imgui_internal.h"
#include "ui_capture.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ostream>
#include <sstream>
#include <thread>

// The production panel, worker, Unicorn CPU and WARP renderer; no desktop window.
inline bool RunDeviceDiagnosticsUiRegressions(std::string& error, std::ostream* report) {
    error.clear();
    const auto directory = std::filesystem::temp_directory_path() /
        ("vfs-device-diagnostics-" + std::to_string(GetCurrentProcessId()) + "-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* gpu = nullptr;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &device, nullptr, &gpu))) {
        std::filesystem::remove(directory);
        error = "Device diagnostics UI could not create WARP device"; return false;
    }
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGuiContext* context = ImGui::CreateContext();
    struct Cleanup {
        ImGuiContext *context, *previous;
        ID3D11Device* device;
        ID3D11DeviceContext* gpu;
        std::filesystem::path directory;
        bool renderer = false;
        ~Cleanup() {
            if (renderer) ImGui_ImplDX11_Shutdown();
            ImGui::DestroyContext(context); ImGui::SetCurrentContext(previous);
            gpu->Release(); device->Release();
            std::error_code ignored; std::filesystem::remove_all(directory, ignored);
        }
    } cleanup{context, previous, device, gpu, directory};
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.DisplaySize = ImVec2(1500, 1000); io.DeltaTime = 1.0f / 60;
    io.ConfigInputTrickleEventQueue = false;
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 16, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    if (!ImGui_ImplDX11_Init(device, gpu)) {
        error = "Device diagnostics UI renderer initialization failed"; return false;
    }
    cleanup.renderer = true;
    FirmwarePackage package;
    StudioState document;
    if (!LoadEmbeddedFirmware(package, error, FirmwareModel::DragS, FirmwareEdition::Score) ||
        !document.LoadPackage(std::move(package), "diagnostics UI native firmware", error)) return false;
    UiFirmwareDevice::State state{FirmwareDevice::Worker(directory)};
    state.loadedDocument = document.DocumentIdentity();
    state.loadedRevision = document.ResourceRevision();
    state.worker.Load(document.CreateEmulationSnapshot(), state.hardwareCode, directory / "device.nvram", false);
    const auto describe = [&](const char* operation) {
        const auto& view = state.worker.Current();
        std::ostringstream text;
        text << FirmwareEditionName(document.edition) << " / HW " << std::hex << state.hardwareCode <<
            std::dec << " / " << operation << ": actual{document=" << view.documentIdentity <<
            ", revision=" << view.revision << ", HW=" << std::hex << view.hardwareCode <<
            ", pc=0x" << view.state.pc << ", sp=0x" << view.state.sp << std::dec <<
            ", sequence=" << view.sequence << ", loaded=" << view.loaded << ", running=" << view.running <<
            ", wantsRunning=" << state.wantsRunning << ", busy=" << state.worker.Busy() <<
            ", us=" << view.state.virtualMicroseconds << ", instructions=" << view.state.instructions <<
            ", clock=" << view.state.modeledClockHz << ", sleeping=" << view.state.sleeping <<
            ", halted=" << view.state.halted << ", frame=" << view.state.frameGeneration <<
            ", pixels=" << view.state.display.pixelWrites << ", error=" << view.error <<
            ", MCU error=" << view.state.error << "} expected{document=" << document.DocumentIdentity() <<
            ", revision=" << state.loadedRevision << "}";
        return text.str();
    };
    const auto wait = [&](const char* operation, auto ready) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        do {
            state.worker.Poll();
            const auto& current = state.worker.Current();
            if (current.documentIdentity && (current.documentIdentity != document.DocumentIdentity() ||
                current.revision != state.loadedRevision || current.hardwareCode != state.hardwareCode)) {
                error = describe(operation) + ": published stale document/revision/hardware telemetry"; return false;
            }
            if (!state.worker.Current().error.empty()) { error = describe(operation); return false; }
            if (ready(state.worker.Current())) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < deadline);
        error = describe(operation) + ": timed out waiting for a real worker snapshot"; return false;
    };
    if (!wait("initial paused load", [](const auto& view) { return view.loaded; })) return false;
    state.worker.SetRunning(true);
    if (!wait("initial resume", [](const auto& view) { return view.running && view.state.virtualMicroseconds >= 5000; })) return false;
    state.worker.SetRunning(false);
    if (!wait("initial pause", [](const auto& view) { return !view.running; })) return false;
    const auto& snapshot = state.worker.Current().state;
    if (snapshot.registers.validGeneral != 0xffff || (snapshot.registers.validSpecial & 7) != 7 ||
        !snapshot.modeledClockHz || snapshot.recentAccess.empty()) {
        error = "Device UI fixture lacks actual CPU/peripheral telemetry"; return false;
    }
    ImGuiID panelsId = 0;
    auto frame = [&](ImVec2 mouse = ImVec2(-100, -100), bool pressed = false) {
        io.AddMousePosEvent(mouse.x, mouse.y); io.AddMouseButtonEvent(0, pressed);
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Device diagnostics UI", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        panelsId = ImGui::GetID("LiveDevicePanels");
        UiFirmwareDevice::Render(state, document, device, gpu);
        ImGui::End(); ImGui::Render();
    };
    auto child = [&](const char* name) -> ImGuiWindow* {
        for (auto* window : context->Windows)
            if (window->LastFrameActive == context->FrameCount && std::strstr(window->Name, name)) return window;
        return nullptr;
    };
    auto checkPanels = [&] {
        auto* controls = child("LiveDeviceControls");
        auto* display = child("LiveDeviceDisplay");
        auto* diagnostics = child("LiveDeviceDiagnostics");
        if (!controls || !display || !diagnostics || controls->Pos.x + controls->Size.x > display->Pos.x + 1 ||
            display->Pos.x + display->Size.x > diagnostics->Pos.x + 1 || !state.texture.IsValid()) {
            error = "Device controls/display/diagnostics are not distinct left/center/right panels"; return false;
        }
        auto* logBody = child("DeviceLogBody");
        auto* registerBody = child("DeviceRegistersBody");
        auto* stackBody = child("DeviceStackBody");
        if (!logBody || !registerBody || !stackBody || logBody->ParentWindow != diagnostics ||
            registerBody->ParentWindow != diagnostics || stackBody->ParentWindow != diagnostics ||
            logBody->WindowBorderSize != 0 || registerBody->WindowBorderSize != 0 ||
            !(logBody->Flags & ImGuiWindowFlags_NoBackground) ||
            !(registerBody->Flags & ImGuiWindowFlags_NoBackground)) {
            error = "Joined diagnostic sections lost their single background/border or sidebar ownership"; return false;
        }
        if (diagnostics->ScrollMax.x > 0 || logBody->ScrollMax.x > 0 || registerBody->ScrollMax.x > 0) {
            error = "Joined section horizontal overflow: sidebar=" + std::to_string(diagnostics->ScrollMax.x) +
                " log=" + std::to_string(logBody->ScrollMax.x) +
                " registers=" + std::to_string(registerBody->ScrollMax.x) +
                " width=" + std::to_string(diagnostics->Size.x) +
                " content=" + std::to_string(diagnostics->ContentSize.x);
            if (report) {
                for (auto* window : {diagnostics, logBody, registerBody, stackBody})
                    *report << window->Name << " width=" << window->Size.x << " content=" << window->ContentSize.x
                        << " work=" << window->WorkRect.GetWidth() << " padding=" << window->WindowPadding.x
                        << " bars=" << window->ScrollbarX << "," << window->ScrollbarY
                        << " scroll=" << window->ScrollMax.x << "," << window->ScrollMax.y << '\n';
                if (const auto* table = ImGui::TableFindByID(logBody->GetID("DeviceAccessLog")))
                    *report << "log table outer=" << table->OuterRect.GetWidth() <<
                        " inner=" << table->InnerRect.GetWidth() << '\n';
            }
            return false;
        }
        auto* log = ImGui::TableFindByID(logBody->GetID("DeviceAccessLog"));
        auto* registers = ImGui::TableFindByID(registerBody->GetID("DeviceRegisters"));
        if (!log || log->OuterWindow != logBody || !registers || registers->OuterWindow != registerBody ||
            !(log->Flags & ImGuiTableFlags_ScrollX) || !(log->Flags & ImGuiTableFlags_ScrollY) ||
            log->LastFrameActive != context->FrameCount || registers->LastFrameActive != context->FrameCount) {
            error = "CPU registers or peripheral log escaped their joined scrollable right-sidebar sections"; return false;
        }
        return true;
    };
    // Auto-height bodies publish their size on the following frame. Let the
    // nested table/body/sidebar scrollbar measurements settle before checking
    // persistent overflow; the third frame still contains previous-frame widths.
    for (unsigned i = 0; i < 8; ++i) frame();
    if (!checkPanels()) return false;
    auto* registersBody = child("DeviceRegistersBody");
    const ImVec2 registersHeader(registersBody->Pos.x + 20,
        registersBody->Pos.y - ImGui::GetFrameHeight() * .5f);
    frame(registersHeader); frame(registersHeader, true); frame(registersHeader); frame();
    auto* remainingLog = child("DeviceLogBody");
    if (child("DeviceRegistersBody") || !remainingLog ||
        remainingLog->ParentWindow != child("LiveDeviceDiagnostics")) {
        error = "Collapsing CPU registers retained its body or detached the peripheral section"; return false;
    }
    frame(registersHeader); frame(registersHeader, true); frame(registersHeader);
    for (unsigned i = 0; i < 3; ++i) frame();
    if (!checkPanels()) return false;
    char capturePath[MAX_PATH]{};
    const bool capture = GetEnvironmentVariableA("VFS_DEVICE_UI_CAPTURE", capturePath, MAX_PATH) > 0;
    const auto wideCapture = directory / "diagnostics-wide.png";
    if (!CaptureUiDrawData(device, gpu, capture ? capturePath : wideCapture.string().c_str(), error)) return false;
    auto* diagnostics = child("LiveDeviceDiagnostics");
    const float oldWidth = diagnostics->Size.x;
    auto* widePanels = ImGui::TableFindByID(panelsId);
    if (!widePanels) { error = "Device panel resize table is missing"; return false; }
    const ImVec2 divider(widePanels->Columns[1].MaxX, diagnostics->Pos.y + 80);
    frame(divider); frame(divider, true);
    frame(ImVec2(divider.x - 50, divider.y), true);
    frame(ImVec2(divider.x - 50, divider.y)); frame();
    if (!checkPanels()) return false;
    if (child("LiveDeviceDiagnostics")->Size.x <= oldWidth + 10) {
        error = "Device diagnostics column could not be widened using its divider"; return false;
    }
    io.DisplaySize = ImVec2(900, 700);
    for (unsigned i = 0; i < 3; ++i) frame();
    auto* panels = ImGui::TableFindByID(panelsId);
    diagnostics = child("LiveDeviceDiagnostics");
    if (!panels || !panels->InnerWindow || panels->InnerWindow->ScrollMax.x <= 0 ||
        !diagnostics || diagnostics->ScrollMax.y <= 0) {
        error = "Narrow device diagnostics lost horizontal layout or vertical sidebar scrolling"; return false;
    }
    const auto narrowCapture = directory / "diagnostics-narrow.png";
    if (!CaptureUiDrawData(device, gpu, narrowCapture.string().c_str(), error)) return false;
    io.DisplaySize = ImVec2(1500, 1000);
    for (unsigned i = 0; i < 8; ++i) frame();
    auto activateControl = [&](const char* label) {
        auto* controls = child("LiveDeviceControls");
        if (!controls) { error = "Device controls disappeared before activation"; return false; }
        const auto seed = std::string_view(label) == "##LiveHardware" ?
            controls->GetID("LiveHardwareSelection") : controls->ID;
        ImGui::ActivateItemByID(ImHashStr(label, 0, seed));
        frame();
        return true;
    };
    auto zoomGeometry = [&] {
        auto* display = child("LiveDeviceDisplay");
        if (!display) { error = "Device LCD display child disappeared"; return false; }
        const auto texture = (ImTextureID)state.texture.GetSRV();
        const float expectedWidth = float(state.worker.Current().state.display.width * state.zoom);
        const float expectedHeight = float(state.worker.Current().state.display.height * state.zoom);
        // Inspect the actual Image quad submitted to WARP, not a separately
        // calculated preview or a test-owned synthetic texture.
        for (const auto& command : display->DrawList->CmdBuffer) {
            if (command.GetTexID() != texture) continue;
            for (unsigned offset = command.IdxOffset; offset + 5 < command.IdxOffset + command.ElemCount;
                offset += 6) {
                const auto& a = display->DrawList->VtxBuffer[
                    command.VtxOffset + display->DrawList->IdxBuffer[offset]];
                const auto& c = display->DrawList->VtxBuffer[
                    command.VtxOffset + display->DrawList->IdxBuffer[offset + 2]];
                if (a.uv.x == 0 && a.uv.y == 0 && c.uv.x == 1 && c.uv.y == 1 &&
                    std::abs(c.pos.x - a.pos.x - expectedWidth) < 1 &&
                    std::abs(c.pos.y - a.pos.y - expectedHeight) < 1) return true;
            }
        }
        error = "Production device LCD Image did not honor the selected zoom"; return false;
    };
    auto checkShortcuts = [&](uint16_t code) {
        auto* sidebar = child("LiveDeviceDiagnostics");
        if (!sidebar) { error = "Native shortcut sidebar is missing"; return false; }
        const float savedScroll = sidebar->Scroll.y;
        ImGui::SetScrollY(sidebar, 0); frame();
        const ImGuiID header = ImHashStr("##SectionHeader", 0, sidebar->GetID("DeviceShortcutsBody"));
        const auto instructions = state.worker.Current().state.instructions;
        ImGui::ActivateItemByID(header); frame(); frame();
        auto* body = child("DeviceShortcutsBody");
        if (!body || body->ParentWindow != sidebar) {
            error = "Native shortcuts did not open as a joined sidebar section"; return false;
        }
        auto activeTable = [&](const char* id) {
            const auto* table = ImGui::TableFindByID(body->GetID(id));
            return table && table->LastFrameActive == context->FrameCount;
        };
        const bool history = code == 0x3013 || code == 0x3014;
        if (!activeTable("NativeMainShortcuts") ||
            activeTable("NativeHistoryShortcuts") != history ||
            activeTable("NativeClockShortcuts") != history ||
            activeTable("NativeServiceShortcuts") == history ||
            activeTable("PatchedMenuShortcuts") ||
            state.worker.Current().state.instructions != instructions) {
            error = "Shortcut help exposed another revision's controls, inactive patch, or resumed the CPU"; return false;
        }
        const char* prefix = std::getenv("VFS_DEVICE_UI_CAPTURE");
        if (prefix && *prefix) {
            char suffix[80];
            std::snprintf(suffix, sizeof(suffix), ".shortcuts.%s.%04X.png",
                document.edition == FirmwareEdition::Factory ? "factory" : "score", unsigned(code));
            if (!CaptureUiDrawData(device, gpu, (std::string(prefix) + suffix).c_str(), error)) return false;
        }
        ImGui::ActivateItemByID(header); frame();
        ImGui::SetScrollY(sidebar, savedScroll);
        for (unsigned i = 0; i < 8; ++i) frame();
        return checkPanels();
    };

    size_t revisionViews = 0;
    for (const auto edition : {FirmwareEdition::Score, FirmwareEdition::Factory}) {
        FirmwarePackage revisionPackage;
        if (!LoadEmbeddedFirmware(revisionPackage, error, FirmwareModel::DragS, edition) ||
            !document.LoadPackage(std::move(revisionPackage), "diagnostics hardware matrix", error)) return false;
        frame();
        for (const auto& record : document.firmware.records) {
            const uint16_t code = uint16_t(record.device_code[0]) << 8 | record.device_code[1];
            if (state.hardwareCode != code) {
                if (!activateControl("##LiveHardware")) return false;
                ImGuiWindow* popup = nullptr;
                if (!context->OpenPopupStack.empty()) popup = context->OpenPopupStack.back().Window;
                if (!popup) { error = "Production hardware revision combo did not open"; return false; }
                char label[48];
                std::snprintf(label, sizeof(label), "HW %04X / %s", unsigned(code),
                    record.linked_base == 0x08003000 ? "GD32F3x0" : "N32G43x");
                // The ninth Factory row can be outside the combo's default
                // eight-row viewport. Scroll the actual popup before activation.
                const auto row = static_cast<size_t>(&record - document.firmware.records.data());
                ImGui::SetScrollY(popup, float(row) * ImGui::GetTextLineHeightWithSpacing());
                frame();
                ImGui::ActivateItemByID(popup->GetID(label));
                frame();
                if (state.hardwareCode != code) {
                    error = "Production hardware revision combo did not select the requested native record"; return false;
                }
            }
            if (!wait("hardware selection startup clock", [&](const auto& view) {
                return view.loaded && view.running && view.state.virtualMicroseconds >= 5000 &&
                    view.state.modeledClockHz != 0;
            })) return false;
            const auto beforePauseTime = state.worker.Current().state.virtualMicroseconds;
            const auto beforePauseSequence = state.worker.Current().sequence;
            const auto beforePauseClock = state.worker.Current().state.modeledClockHz;
            if (!activateControl(Tr(i18n::Str::DevicePause))) return false;
            if (state.wantsRunning) { error = describe("hardware selection pause control did not activate"); return false; }
            if (!wait("hardware selection pause", [](const auto& view) { return !view.running; })) return false;
            const auto pausedClockTime = state.worker.Current().state.virtualMicroseconds;
            const auto pausedClock = state.worker.Current().state.modeledClockHz;
            for (unsigned i = 0; i < 8; ++i) frame();
            const auto& view = state.worker.Current();
            const bool hardwareMatches = view.hardwareCode == code;
            const bool lcdHardwareMatches = view.state.display.hardwareCode == code;
            const bool loadedDocumentMatches = state.loadedDocument == document.DocumentIdentity();
            const bool loadedRevisionMatches = state.loadedRevision == document.ResourceRevision();
            // Reset/startup legitimately switches to low clocks before power-on.
            // A clock belongs to a particular guest snapshot, not the board's
            // eventual full-speed setting. Paused UI frames must preserve it.
            const bool clockMatches = view.state.modeledClockHz != 0 &&
                view.state.virtualMicroseconds == pausedClockTime && view.state.modeledClockHz == pausedClock &&
                (view.state.virtualMicroseconds != beforePauseTime || view.state.modeledClockHz == beforePauseClock);
            const bool generalRegistersValid = view.state.registers.validGeneral == 0xffff;
            const bool specialRegistersValid = (view.state.registers.validSpecial & 7) == 7;
            const bool hasPeripheralTelemetry = !view.state.recentAccess.empty();
            const bool textureValid = state.texture.IsValid();
            const bool textureGenerationMatches = state.textureGeneration == view.state.frameGeneration;
            if (!hardwareMatches || !lcdHardwareMatches || !loadedDocumentMatches || !loadedRevisionMatches ||
                !clockMatches || !generalRegistersValid || !specialRegistersValid || !hasPeripheralTelemetry ||
                !textureValid || !textureGenerationMatches) {
                std::ostringstream detail;
                detail << describe("revision ownership after pause") <<
                    " matches{hardware=" << hardwareMatches << ", lcdHardware=" << lcdHardwareMatches <<
                    ", loadedDocument=" << loadedDocumentMatches << ", loadedRevision=" << loadedRevisionMatches <<
                    ", clock=" << clockMatches << ", generalRegisters=" << generalRegistersValid <<
                    ", specialRegisters=" << specialRegistersValid << ", peripheralTelemetry=" << hasPeripheralTelemetry <<
                    ", texture=" << textureValid << ", textureGeneration=" << textureGenerationMatches <<
                    "} ownership{expectedHW=0x" << std::hex << code << ", workerHW=0x" << view.hardwareCode <<
                    ", lcdHW=0x" << view.state.display.hardwareCode << std::dec <<
                    ", expectedDocument=" << document.DocumentIdentity() << ", loadedDocument=" << state.loadedDocument <<
                    ", workerDocument=" << view.documentIdentity << ", expectedRevision=" << document.ResourceRevision() <<
                    ", loadedRevision=" << state.loadedRevision << ", workerRevision=" << view.revision <<
                    ", expectedPausedClock=" << pausedClock << ", expectedPausedUs=" << pausedClockTime <<
                    ", actualClock=" << view.state.modeledClockHz <<
                    ", expectedGeneralFlags=0xffff, actualGeneralFlags=0x" << std::hex << view.state.registers.validGeneral <<
                    ", expectedSpecialMask=0x7, actualSpecialFlags=0x" << unsigned(view.state.registers.validSpecial) <<
                    std::dec << ", expectedTextureGeneration=" << view.state.frameGeneration <<
                    ", actualTextureGeneration=" << state.textureGeneration <<
                    ", peripheralAccessCount=" << view.state.recentAccess.size() <<
                    ", beforePauseUs=" << beforePauseTime << ", beforePauseSequence=" << beforePauseSequence <<
                    ", beforePauseClock=" << beforePauseClock <<
                    ", display=" << view.state.display.width << 'x' << view.state.display.height <<
                    ", rgbaCount=" << view.state.display.rgba.size() << ", textureDeviceMatches=" <<
                    (state.textureDevice == device) << "}";
                error = detail.str();
                return false;
            }
            if (!checkPanels()) return false;
            if (!checkShortcuts(code)) return false;
            for (const int zoom : {StudioPreferences::MinimumZoom, StudioPreferences::MaximumZoom}) {
                state.zoom = zoom;
                for (unsigned i = 0; i < 3; ++i) frame();
                if (!zoomGeometry()) return false;
            }
            auto* lcd = child("LiveDeviceDisplay");
            if (!lcd || lcd->ScrollMax.x <= 0 || lcd->ScrollMax.y <= 0) {
                error = "Maximum zoom clipped the hardware LCD without both display scrollbars"; return false;
            }
            char captureSuffix[64];
            std::snprintf(captureSuffix, sizeof(captureSuffix), ".%s.%04X.png",
                edition == FirmwareEdition::Factory ? "factory" : "score", unsigned(code));
            const char* capturePrefix = std::getenv("VFS_DEVICE_UI_CAPTURE");
            const auto image = capturePrefix && *capturePrefix ?
                std::filesystem::path(std::string(capturePrefix) + captureSuffix) :
                directory / (std::string("revision") + captureSuffix);
            if (!CaptureUiDrawData(device, gpu, image.string().c_str(), error)) return false;
            const auto pausedInstructions = state.worker.Current().state.instructions;
            const auto stepSequence = state.worker.Current().sequence;
            if (!activateControl(Tr(i18n::Str::DeviceStep)) ||
                !wait("single step", [&](const auto& stepped) {
                    return !stepped.running && stepped.sequence > stepSequence;
                })) return false;
            if (state.worker.Current().state.instructions != pausedInstructions + 1) {
                error = "Production Step did not execute one instruction in the scheduled-IRQ fixture"; return false;
            }
            const auto appliedRevision = state.loadedRevision;
            const auto pausedTime = state.worker.Current().state.virtualMicroseconds;
            const auto accent = std::find_if(document.palette.begin(), document.palette.end(),
                [](const PaletteState& color) { return std::string_view(color.spec.id) == "accent_red"; });
            if (accent == document.palette.end()) {
                error = describe("missing supported foreground palette edit fixture"); return false;
            }
            document.PushUndo("Diagnostics pending device revision");
            document.SetPaletteColor(static_cast<size_t>(accent - document.palette.begin()), Rgb8(0, 255, 0));
            document.EndUndoAction();
            frame();
            if (document.ResourceRevision() == appliedRevision || state.loadedRevision != appliedRevision ||
                state.worker.Current().revision != appliedRevision ||
                state.worker.Current().state.virtualMicroseconds != pausedTime) {
                error = "Editing a document silently replaced or resumed the paused native device"; return false;
            }
            if (!activateControl(Tr(i18n::Str::DeviceApply))) return false;
            if (state.loadedRevision != document.ResourceRevision() || state.texture.IsValid() ||
                state.worker.Current().documentIdentity) {
                error = "Applying a document revision retained stale diagnostics or the previous LCD texture"; return false;
            }
            if (!wait("applied revision startup clock", [&](const auto& applied) {
                return applied.loaded && applied.running && applied.revision == document.ResourceRevision() &&
                    applied.state.virtualMicroseconds >= 5000 && applied.state.modeledClockHz != 0;
            })) return false;
            if (!activateControl(Tr(i18n::Str::DevicePause))) return false;
            if (state.wantsRunning) { error = describe("applied revision pause control did not activate"); return false; }
            if (!wait("applied revision pause", [](const auto& applied) { return !applied.running; })) return false;
            const auto appliedPausedTime = state.worker.Current().state.virtualMicroseconds;
            const auto appliedPausedClock = state.worker.Current().state.modeledClockHz;
            for (unsigned i = 0; i < 8; ++i) frame();
            if (!appliedPausedClock || state.worker.Current().state.virtualMicroseconds != appliedPausedTime ||
                state.worker.Current().state.modeledClockHz != appliedPausedClock) {
                error = describe("applied revision paused clock changed without guest execution"); return false;
            }
            if (!checkPanels() || !zoomGeometry()) return false;
            document.Undo();
            ++revisionViews;
        }
    }
    if (revisionViews != 16) { error = "Diagnostics UI did not cover all 7 Score and 9 Factory revisions"; return false; }
    if (report) *report << "DEVICE DIAGNOSTICS UI REGRESSIONS OK (16 native Score/Factory revisions, actual hardware combo/step/apply/pause, isolated worker storage, live clock/register/log ownership, zoom 1/16 Image geometry, WARP captures, divider resize and narrow scrolling)\n";
    return true;
}
