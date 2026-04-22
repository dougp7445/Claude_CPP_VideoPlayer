#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include "PlayerEvent.h"

class PlayerUI {
public:
    // Draw the control bar overlay onto renderer.
    void render(SDL_Renderer* renderer, double currentPts, double duration, bool paused);

    // Process a left-mouse click at (mx, my). Updates mute, volume, and speed
    // state internally; caller is responsible for applying audio side-effects
    // by reading isMuted(), getVolume(), and getSpeed() afterwards.
    PlayerEvent handleMouseClick(float mx, float my);

    // Signal that user activity occurred (shows bar, resets hide timer).
    void onActivity();

    // Adjust volume by delta, clamped to [0, 2].
    void adjustVolume(float delta);

    // Advance to the next speed step (0.5x → 1x → 1.5x → 2x, wraps).
    void cycleSpeed();

    double getSeekTarget() const { return m_seekTarget; }
    float  getSpeed()      const { return m_speed; }
    float  getVolume()     const { return m_volume; }
    bool   isMuted()       const { return m_muted; }

private:
    // Playback speed
    int   m_speedIndex    = 1;     // index into SPEEDS[]
    float m_speed         = 1.0f;

    // Volume / mute
    float m_volume        = 1.0f;
    bool  m_muted         = false;

    // Visibility / activity
    bool     m_uiVisible      = true;
    uint64_t m_lastActivityMs = 0;   // 0 = not yet initialised

    double m_seekTarget = 0.0;

    // Hit-test rects — updated every render() call
    SDL_FRect m_playPauseRect = {};
    float m_progressBarX = 0, m_progressBarY = 0;
    float m_progressBarW = 0, m_progressBarH = 0;
    float m_volumeBarX   = 0, m_volumeBarY   = 0;
    float m_volumeBarW   = 0, m_volumeBarH   = 0;
    SDL_FRect m_muteRect  = {};
    SDL_FRect m_speedRect = {};
};
