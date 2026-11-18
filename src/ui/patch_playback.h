#pragma once
#include "firmware_logo_patch.h"
#include <algorithm>
#include <cmath>

namespace UiFirmwarePatches {
// Playback owns time independently of background native validation.
struct Playback {
    bool playing = false, loop = false, showAnimation = false;
    float speed = 1.0f;
    double position = 0;
    int manualPhase = -1;
    FirmwareLogo::Target target = FirmwareLogo::Target::Boot;

    static double PhaseDuration(const FirmwareLogo::Settings& settings, int phase,
        FirmwareLogo::Target target = FirmwareLogo::Target::Boot) {
        const bool entranceAnimated = settings.animation != FirmwareLogo::Animation::None;
        const int entrance = entranceAnimated ? int(FirmwareLogo::PhaseCount) : 1;
        double milliseconds = phase < entrance ?
            (entranceAnimated ? settings.durationMs / double(FirmwareLogo::PhaseCount) : 0.0) :
            settings.exitDurationMs / double(FirmwareLogo::PhaseCount);
        if (phase == entrance - 1 && target != FirmwareLogo::Target::DeviceInfo) milliseconds += settings.holdMs;
        return milliseconds / 1000.0;
    }
    static double PhaseStart(const FirmwareLogo::Settings& settings, int phase,
        FirmwareLogo::Target target = FirmwareLogo::Target::Boot) {
        double start = 0;
        for (int i = 0; i < phase; ++i) start += PhaseDuration(settings, i, target);
        return start;
    }
    static double Duration(const FirmwareLogo::Settings& settings,
        FirmwareLogo::Target target = FirmwareLogo::Target::Boot) {
        return PhaseStart(settings, int(FirmwareLogo::FrameCount(settings)), target);
    }
    int Phase(const FirmwareLogo::Settings& settings) const {
        const int count = int(FirmwareLogo::FrameCount(settings));
        if (manualPhase >= 0) return (std::min)(manualPhase, count - 1);
        double boundary = 0;
        for (int i = 0; i < count - 1; ++i) {
            boundary += PhaseDuration(settings, i, target);
            if (position < boundary) return i;
        }
        return count - 1;
    }
    void Play(const FirmwareLogo::Settings& settings) {
        if (position >= Duration(settings, target)) position = 0;
        manualPhase = -1;
        showAnimation = true;
        playing = Duration(settings, target) > 0;
    }
    void Pause() { playing = false; }
    void Stop() { playing = showAnimation = false; position = 0; manualPhase = -1; }
    void Seek(const FirmwareLogo::Settings& settings, double seconds) {
        position = std::clamp(seconds, 0.0, Duration(settings, target));
        manualPhase = -1;
        playing = false;
        showAnimation = true;
    }
    void Step(const FirmwareLogo::Settings& settings, int delta) {
        const int phase = std::clamp(Phase(settings) + delta, 0, int(FirmwareLogo::FrameCount(settings)) - 1);
        Seek(settings, PhaseStart(settings, phase, target));
        manualPhase = phase;
    }
    void Tick(const FirmwareLogo::Settings& settings, double elapsed, bool ready) {
        if (!playing || !ready) return;
        const double duration = Duration(settings, target);
        if (duration <= 0) { playing = false; return; }
        position += (std::max)(elapsed, 0.0) * speed;
        if (position >= duration) {
            if (loop) position = std::fmod(position, duration);
            else { position = duration; playing = false; }
        }
    }
};
} // namespace UiFirmwarePatches
