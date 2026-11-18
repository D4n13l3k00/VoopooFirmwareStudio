#pragma once
#include "patch_playback.h"
#include <ostream>

inline bool RunPatchPlaybackRegressions(std::string& error, std::ostream* log) {
    using UiFirmwarePatches::Playback;
    FirmwareLogo::Settings settings;
    settings.animation = FirmwareLogo::Animation::SoftWipeUpLeft;
    settings.exitAnimation = FirmwareLogo::Animation::BlindsUpLeft;
    settings.durationMs = 800; settings.holdMs = 500; settings.exitDurationMs = 1200;
    auto close = [](double a, double b) { return std::abs(a-b) < 1e-8; };
    auto check = [&](bool ok, const char* message) { if (!ok) error = message; return ok; };
    if (!check(close(Playback::Duration(settings), 2.5), "Player duration lost entrance/hold/exit timing")) return false;
    Playback player;
    player.Play(settings); player.Tick(settings, 8, false);
    if (!check(player.playing && player.position == 0, "Preparing frames consumed playback time")) return false;
    player.Tick(settings, .25, true); player.Pause(); player.Tick(settings, 1, true);
    if (!check(close(player.position,.25), "Pause did not freeze playback position")) return false;
    player.Play(settings); player.Tick(settings,.25,true);
    if (!check(close(player.position,.5), "Resume restarted instead of continuing")) return false;
    player.Seek(settings, .9);
    if (!check(!player.playing && player.Phase(settings)==15, "Seeking into hold selected the wrong native phase")) return false;
    player.Step(settings, 1);
    if (!check(player.Phase(settings)==16 && close(player.position,1.3), "Next-frame skipped the hold boundary")) return false;
    player.Step(settings, -1);
    if (!check(player.Phase(settings)==15 && close(player.position,.75), "Previous-frame selected the wrong native phase")) return false;
    player.Play(settings); player.Tick(settings,10,true);
    if (!check(!player.playing && close(player.position,2.5) && player.Phase(settings)==31, "Player did not stop at final frame")) return false;
    player.Play(settings);
    if (!check(player.position==0 && player.playing, "Play at end did not restart")) return false;
    player.loop=true; player.Tick(settings,2.75,true);
    if (!check(player.playing && close(player.position,.25), "Loop did not preserve time past the boundary")) return false;
    player.speed=2; player.Tick(settings,.25,true);
    if (!check(close(player.position,.75), "Playback speed did not scale time")) return false;
    player.Stop();
    if (!check(!player.playing && !player.showAnimation && player.position==0, "Stop did not return to editing preview")) return false;
    settings.animation=FirmwareLogo::Animation::None; settings.holdMs=0;
    player.Seek(settings,0); player.Step(settings,-1);
    if (!check(player.Phase(settings)==0, "Manual stepping cannot inspect the zero-duration static entrance")) return false;
    for (uint32_t id = 1; id < uint32_t(FirmwareLogo::Animation::Count); ++id) {
        settings.animation = static_cast<FirmwareLogo::Animation>(id);
        settings.exitAnimation = static_cast<FirmwareLogo::Animation>(
            uint32_t(FirmwareLogo::Animation::Count) - id);
        settings.holdMs = 500;
        player.Stop(); player.speed = 1; player.loop = false;
        if (!check(FirmwareLogo::FrameCount(settings) == 32 && close(Playback::Duration(settings), 2.5),
            "Directional animation changed the complete sequence timing")) return false;
        player.Seek(settings, Playback::PhaseStart(settings, 16));
        if (!check(player.Phase(settings) == 16,
            "Directional entrance/exit lost their independent phase boundary")) return false;
        player.Seek(settings, Playback::Duration(settings));
        if (!check(player.Phase(settings) == 31,
            "Directional animation could not seek to its final exit frame")) return false;
        settings.animation = FirmwareLogo::Animation::None;
        if (!check(FirmwareLogo::FrameCount(settings) == 17 && close(Playback::Duration(settings), 1.7),
            "Directional exit-only animation lost static entrance or hold timing")) return false;
        player.Seek(settings, 0); player.Step(settings, 1);
        if (!check(player.Phase(settings) == 1 && close(player.position, .5),
            "Directional exit-only stepping skipped the static entrance hold")) return false;
    }
    if (log) *log << "PATCH PLAYBACK REGRESSIONS OK (timing, pause/resume, seek, step, loop, speed, preparation)\n";
    return true;
}
