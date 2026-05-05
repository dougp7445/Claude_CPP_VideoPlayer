#ifndef RENDERER_H
#define RENDERER_H

#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "PlayerEvent.h"
#include "PlayerUI.h"
#include "VideoPlayerConfig.h"

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Apply feature flags; call before initWindow().
    void setConfig(const VideoPlayerConfig& cfg);

    // Call initWindow() from the main thread before spawning the render thread.
    bool initWindow(const std::string& title, int width, int height);

    // Call initRenderer() from the render thread.
    bool initRenderer();

    void shutdown();

    // Recreate the YUV texture for a new video without destroying the window.
    // Call this from the main thread after the render thread has been joined.
    bool reloadVideo(const std::string& title, int width, int height);

    // Render the decoded YUV video frame (does not present).
    void renderFrame(const uint8_t* yPlane, int yPitch,
                     const uint8_t* uPlane, int uPitch,
                     const uint8_t* vPlane, int vPitch);

    // Draw the control bar overlay.
    void renderUI(double currentPts, double duration, bool paused);

    // Flip the back buffer to the screen.
    void present();

    void queueAudio(const uint8_t* data, int size);
    void flushAudio();
    void pauseAudio();
    void resumeAudio();
    void adjustVolume(float delta);
    void toggleFullscreen();

    void   setRecentFiles(const std::vector<std::string>& files);
    std::string takePendingOpenPath();

    double getAudioClock()   const;
    double getAudioLatency() const { return m_audioLatencyS; }
    float  getVolume()       const { return m_ui.getVolume(); }
    float  getSpeed()        const { return m_ui.getSpeed(); }
    double getSeekTarget()   const { return m_ui.getSeekTarget(); }

    // Uses SDL_PeepEvents (no pump) — safe to call from non-main threads.
    PlayerEvent pollEvents();

    void onActivity() { m_ui.onActivity(); }

    SDL_Renderer* getSDLRenderer() const { return m_renderer; }
    void getWindowSize(int& w, int& h) const { SDL_GetWindowSize(m_window, &w, &h); }

private:
    SDL_Window*      m_window      = nullptr;
    SDL_Renderer*    m_renderer    = nullptr;
    SDL_Texture*     m_texture     = nullptr;
    SDL_AudioStream* m_audioStream = nullptr;

    int      m_width            = 0;
    int      m_height           = 0;
    int      m_bytesPerSecond   = 0;
    uint64_t m_totalBytesQueued = 0;
    double   m_audioLatencyS    = 0.0;
    bool     m_fullscreen       = false;

    VideoPlayerConfig m_config;
    PlayerUI          m_ui;

    void measureAudioLatency();
    // Sync SDL audio gain and speed ratio from current UI state.
    void syncAudio();
};

#endif // RENDERER_H
