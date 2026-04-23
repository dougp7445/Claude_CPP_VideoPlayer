#include "VideoPlayer.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <thread>

extern "C" {
#include <libavutil/frame.h>
}

VideoPlayer::VideoPlayer() = default;
VideoPlayer::~VideoPlayer() = default;

bool VideoPlayer::load(const std::string& filePath) {
    if (!m_decoder.open(filePath)) {
        return false;
    } 
    // initWindow must run on the main thread (OS window creation).
    return m_renderer.initWindow("VideoPlayer - " + filePath,
                                 m_decoder.videoWidth(),
                                 m_decoder.videoHeight());
}

// Runs on the render thread: creates the renderer, decodes and presents frames.
void VideoPlayer::renderLoop() {
    
    if (!m_renderer.initRenderer()) { 
        m_quit = true; 
        return; 
    }

    const double audioLatency = m_renderer.getAudioLatency();
    const double SEEK_STEP    = 10.0;
    const double duration     = m_decoder.duration();

    bool     clocked    = false;
    bool     reclock    = false;
    uint64_t startTick  = 0;
    double   startPts   = 0.0;
    bool     paused     = false;
    double   currentPts = 0.0;
    float    lastSpeed  = 1.0f;
    double   target     = 0.0;

    while (!m_quit) {
        PlayerEvent ev = m_renderer.pollEvents();

        if (ev == PlayerEvent::Quit) {
            break;
        }

        switch (ev) {
            case PlayerEvent::TogglePause:
                paused = !paused;
                if (paused) { 
                    m_renderer.pauseAudio();
                }
                else      { 
                    m_renderer.resumeAudio(); 
                    reclock = true; 
                }
                break;

            case PlayerEvent::SeekForward:
            case PlayerEvent::SeekBackward: 
                target = currentPts + (ev == PlayerEvent::SeekForward ? SEEK_STEP : -SEEK_STEP);
                target = std::max(0.0, std::min(target, duration));
                if (m_decoder.seek(target)) {
                    m_renderer.flushAudio();
                    reclock = true;

                    if (paused) { 
                        paused = false; 
                        m_renderer.resumeAudio(); 
                    }
                }
                break;
            case PlayerEvent::SeekTo:
                target = m_renderer.getSeekTarget() * duration;
                target = std::max(0.0, std::min(target, duration));
                
                if (m_decoder.seek(target)) {
                    m_renderer.flushAudio();
                    reclock = true;

                    if (paused) { 
                        paused = false; 
                        m_renderer.resumeAudio(); 
                    }
                }
                break;
            case PlayerEvent::VolumeUp:        
                m_renderer.adjustVolume( 0.1f); 
                break;
            case PlayerEvent::VolumeDown:
                m_renderer.adjustVolume(-0.1f); 
                break;
            case PlayerEvent::ToggleFullscreen: 
                m_renderer.toggleFullscreen();  
                break;
            case PlayerEvent::SpeedChange:      
                reclock = true;                 
                break;
            default: 
                break;
        }

        float curSpeed = m_renderer.getSpeed();
        if (curSpeed != lastSpeed) { lastSpeed = curSpeed; reclock = true; }

        if (paused) {
            m_renderer.renderUI(currentPts, duration, true);
            m_renderer.present();
            SDL_Delay(16);
            continue;
        }

        DecodedFrame frame;
        if (!m_decoder.readFrame(frame)) {
            break;
        }

        if (frame.audioFrame) {
            AVFrame* f = frame.audioFrame;
            m_renderer.queueAudio(f->data[0], f->nb_samples * 2 * sizeof(int16_t));
            av_frame_free(&frame.audioFrame);
        }

        if (frame.videoFrame) {
            AVFrame* f = frame.videoFrame;
            currentPts = frame.pts;

            if (!clocked || reclock) {
                startTick = SDL_GetTicks();
                startPts  = frame.pts;
                clocked   = true;
                reclock   = false;
            }

            float  speed      = m_renderer.getSpeed();
            double targetWall = (frame.pts - startPts) / speed + audioLatency;
            double wall       = static_cast<double>(SDL_GetTicks() - startTick) / 1000.0;
            
            if (targetWall > wall){
                SDL_Delay(static_cast<Uint32>((targetWall - wall) * 1000.0));
            }

            m_renderer.renderFrame(
                f->data[0], f->linesize[0],
                f->data[1], f->linesize[1],
                f->data[2], f->linesize[2]);

            m_renderer.renderUI(currentPts, duration, false);
            m_renderer.present();

            av_frame_free(&frame.videoFrame);
        }
    }

    m_quit = true;
}

void VideoPlayer::run() {
    m_quit = false;

    // Render thread owns the SDL renderer and does all drawing.
    std::thread rt([this]() { renderLoop(); });

    // Main thread pumps OS window messages so SDL's event queue stays fed.
    // SDL_PeepEvents (used by the render thread) is thread-safe and reads
    // from this queue without needing to be on the main thread.
    while (!m_quit) {
        SDL_PumpEvents();
        SDL_Delay(1);
    }

    rt.join();
}
