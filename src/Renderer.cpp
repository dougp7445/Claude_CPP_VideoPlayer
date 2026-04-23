#include "Renderer.h"
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <vector>

Renderer::Renderer() = default;

Renderer::~Renderer() {
    shutdown();
}

// ── Initialisation ───────────────────────────────────────────────────────────

bool Renderer::initWindow(const std::string& title, int width, int height) {
    m_width  = width;
    m_height = height;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Scale the initial window down to fit within 85% of the primary display.
    int winW = width, winH = height;
    SDL_DisplayID disp = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(disp);
    if (mode) {
        int maxW = static_cast<int>(mode->w * 0.85f);
        int maxH = static_cast<int>(mode->h * 0.85f);
        if (winW > maxW || winH > maxH) {
            float sc = std::min(static_cast<float>(maxW) / winW,
                                static_cast<float>(maxH) / winH);
            winW = static_cast<int>(winW * sc);
            winH = static_cast<int>(winH * sc);
        }
    }

    m_window = SDL_CreateWindow(title.c_str(), winW, winH, SDL_WINDOW_RESIZABLE);
    if (!m_window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return false;
    }
    return true;
}

bool Renderer::initRenderer() {
    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (!m_renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        return false;
    }

    m_texture = SDL_CreateTexture(m_renderer,
        SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING,
        m_width, m_height);
    if (!m_texture) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_AudioSpec spec{};
    spec.format   = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq     = 44100;
    m_bytesPerSecond = spec.freq * spec.channels * 2;

    m_audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                              &spec, nullptr, nullptr);
    if (m_audioStream) {
        SDL_ResumeAudioStreamDevice(m_audioStream);
        measureAudioLatency();
    }

    return true;
}

void Renderer::measureAudioLatency() {
    int probeBytes = m_bytesPerSecond / 100;
    std::vector<uint8_t> silence(probeBytes, 0);
    SDL_PutAudioStreamData(m_audioStream, silence.data(), probeBytes);

    int      initial    = SDL_GetAudioStreamQueued(m_audioStream);
    uint64_t probeStart = SDL_GetTicks();

    while (SDL_GetTicks() - probeStart < 500) {
        SDL_Delay(1);
        if (SDL_GetAudioStreamQueued(m_audioStream) < initial) {
            m_audioLatencyS = static_cast<double>(SDL_GetTicks() - probeStart) / 1000.0;
            break;
        }
    }

    SDL_FlushAudioStream(m_audioStream);
    m_totalBytesQueued = 0;

    SDL_AudioDeviceID devId = SDL_GetAudioStreamDevice(m_audioStream);
    if (devId != 0) {
        SDL_AudioSpec deviceSpec{};
        int sampleFrames = 0;
        if (SDL_GetAudioDeviceFormat(devId, &deviceSpec, &sampleFrames) && sampleFrames > 0) {
            int freq = (deviceSpec.freq > 0) ? deviceSpec.freq : 44100;
            m_audioLatencyS += static_cast<double>(sampleFrames) / freq;
        }
    }

    std::cout << "Audio latency: " << static_cast<int>(m_audioLatencyS * 1000.0) << " ms\n";
}

void Renderer::shutdown() {
    if (m_audioStream) { SDL_DestroyAudioStream(m_audioStream); m_audioStream = nullptr; }
    if (m_texture)     { SDL_DestroyTexture(m_texture);         m_texture     = nullptr; }
    if (m_renderer)    { SDL_DestroyRenderer(m_renderer);       m_renderer    = nullptr; }
    if (m_window)      { SDL_DestroyWindow(m_window);           m_window      = nullptr; }
    SDL_Quit();
}

// ── Video rendering ──────────────────────────────────────────────────────────

void Renderer::renderFrame(const uint8_t* yPlane, int yPitch,
                            const uint8_t* uPlane, int uPitch,
                            const uint8_t* vPlane, int vPitch) {
    SDL_UpdateYUVTexture(m_texture, nullptr,
        yPlane, yPitch,
        uPlane, uPitch,
        vPlane, vPitch);

    SDL_RenderClear(m_renderer);

    int outW = 0, outH = 0;
    SDL_GetRenderOutputSize(m_renderer, &outW, &outH);
    float scale = std::min(static_cast<float>(outW) / m_width,
                           static_cast<float>(outH) / m_height);
    SDL_FRect dst = {(outW - m_width  * scale) * 0.5f,
                     (outH - m_height * scale) * 0.5f,
                     m_width * scale, m_height * scale};
    SDL_RenderTexture(m_renderer, m_texture, nullptr, &dst);
}

void Renderer::renderUI(double currentPts, double duration, bool paused) {
    // When paused, re-draw the video texture (renderFrame isn't being called).
    if (paused) {
        SDL_RenderClear(m_renderer);
        int pw = 0, ph = 0;
        SDL_GetRenderOutputSize(m_renderer, &pw, &ph);
        float ps = std::min(static_cast<float>(pw) / m_width,
                            static_cast<float>(ph) / m_height);
        SDL_FRect pdst = {(pw - m_width  * ps) * 0.5f,
                          (ph - m_height * ps) * 0.5f,
                          m_width * ps, m_height * ps};
        SDL_RenderTexture(m_renderer, m_texture, nullptr, &pdst);
    }

    m_ui.render(m_renderer, currentPts, duration, paused);
}

void Renderer::present() {
    SDL_RenderPresent(m_renderer);
}

// ── Audio ────────────────────────────────────────────────────────────────────

void Renderer::queueAudio(const uint8_t* data, int size) {
    if (!m_audioStream)
    {
        return;
    }
    
    SDL_PutAudioStreamData(m_audioStream, data, size);
    m_totalBytesQueued += static_cast<uint64_t>(size);
}

void Renderer::flushAudio() {
    if (!m_audioStream)
    {
        return;
    }
    
    SDL_FlushAudioStream(m_audioStream);
    m_totalBytesQueued = 0;
}

void Renderer::pauseAudio()  
{ 
    if (m_audioStream) { 
        SDL_PauseAudioStreamDevice(m_audioStream); 
    }  
}

void Renderer::resumeAudio() 
{ 
    if (m_audioStream) { 
        SDL_ResumeAudioStreamDevice(m_audioStream); 
    } 
}

void Renderer::adjustVolume(float delta) {
    m_ui.adjustVolume(delta);
    syncAudio();
}

void Renderer::toggleFullscreen() {
    m_fullscreen = !m_fullscreen;
    SDL_SetWindowFullscreen(m_window, m_fullscreen);
}

double Renderer::getAudioClock() const {
    if (!m_audioStream || m_bytesPerSecond == 0) {
        return -1.0;
    }

    // SDL's device buffer can still report queued bytes briefly after a flush,
    // making played transiently negative. Clamp to zero so the clock never goes
    // below the last known playback position.
    int64_t played = static_cast<int64_t>(m_totalBytesQueued) - SDL_GetAudioStreamQueued(m_audioStream);
    return std::max(0.0, static_cast<double>(played) / m_bytesPerSecond);
}

void Renderer::syncAudio() {

    if (!m_audioStream)
    {
        return;
    }

    SDL_SetAudioStreamGain(m_audioStream, m_ui.isMuted() ? 0.0f : m_ui.getVolume());
    SDL_SetAudioStreamFrequencyRatio(m_audioStream, m_ui.getSpeed());
}

// ── Events ───────────────────────────────────────────────────────────────────

PlayerEvent Renderer::pollEvents() {
    SDL_Event event;
    PlayerEvent playerEvent = PlayerEvent::None;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) > 0) {
        switch (event.type) {

        case SDL_EVENT_QUIT:
            playerEvent = PlayerEvent::Quit;
            break;
        case SDL_EVENT_KEY_DOWN:
            m_ui.onActivity();
            switch (event.key.key) {
                case SDLK_ESCAPE: 
                    playerEvent = PlayerEvent::Quit;
                    break;
                case SDLK_SPACE:  
                    playerEvent = PlayerEvent::TogglePause;
                    break;
                case SDLK_RIGHT:  
                    playerEvent = PlayerEvent::SeekForward;
                    break;
                case SDLK_LEFT:   
                    playerEvent = PlayerEvent::SeekBackward;
                    break;
                case SDLK_UP:     
                    m_ui.adjustVolume( 0.1f); 
                    syncAudio(); 
                    break;
                case SDLK_DOWN:   
                    m_ui.adjustVolume(-0.1f); 
                    syncAudio(); 
                    break;
                case SDLK_F:      
                    playerEvent = PlayerEvent::ToggleFullscreen;
                    break;
                case SDLK_S:      
                    m_ui.cycleSpeed(); 
                    syncAudio(); 
                    playerEvent = PlayerEvent::SpeedChange;
                    break;
                default: 
                    break;
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            m_ui.onActivity();
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                m_ui.onActivity();
                playerEvent = m_ui.handleMouseClick(event.button.x, event.button.y);
                syncAudio();
            }
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            m_ui.onActivity();
            m_ui.adjustVolume(event.wheel.y * 0.05f);
            syncAudio();
            break;

        default: 
            break;
        }
    }
    return playerEvent;
}
