#include "VideoPlayer.h"
#include "Constants.h"
#include "Encoder.h"
#include "ExportSettingsDialog.h"
#include "FileOperations.h"
#include "Logger.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>

extern "C" {
#include <libavutil/frame.h>
}

VideoPlayer::VideoPlayer()
    : m_recentFiles(executableDir() + "recent_files.json")
{}
VideoPlayer::~VideoPlayer() = default;

bool VideoPlayer::load(const std::string& filePath) {
    if (!m_decoder.open(filePath)) {
        return false;
    }
    m_recentFiles.add(filePath);
    m_renderer.setRecentFiles(m_recentFiles.entries());
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
    const double SEEK_STEP    = SEEK_STEP_SECONDS;
    const double duration     = m_decoder.duration();

    bool     clocked    = false;
    bool     reclock    = false;
    uint64_t startTick  = 0;
    double   startPts   = 0.0;
    bool     paused     = false;
    double   currentPts = 0.0;
    float    lastSpeed  = 1.0f;
    double   target     = 0.0;

    bool wasInDialog = false;
    while (!m_quit) {
        bool inDialog = m_showExportDialog.load();
        if (inDialog) { m_renderer.onActivity(); }
        PlayerEvent ev = inDialog ? PlayerEvent::None : m_renderer.pollEvents();
        if (!inDialog && wasInDialog) { reclock = true; }
        wasInDialog = inDialog;

        if (ev == PlayerEvent::Quit) {
            break;
        }

        if (ev == PlayerEvent::OpenFile) {
            m_requestOpenFile = true;
            break;
        }

        if (ev == PlayerEvent::ExportVideo && !m_showExportDialog) {
            m_showExportDialog = true;
        }

        switch (ev) {
            case PlayerEvent::TogglePause:
                paused = !paused;
                if (paused) { 
                    m_renderer.pauseAudio();
                }
                else { 
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
                m_renderer.adjustVolume( VOLUME_KEY_DELTA);
                break;
            case PlayerEvent::VolumeDown:
                m_renderer.adjustVolume(-VOLUME_KEY_DELTA);
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

void VideoPlayer::run(std::function<std::string()> filePicker) {
    while (true) {
        m_quit             = false;
        m_requestOpenFile  = false;
        m_showExportDialog = false;

        std::thread rt([this]() { renderLoop(); });

        while (!m_quit) {
            SDL_PumpEvents();
            if (m_showExportDialog) { runExportDialog(); }
            SDL_Delay(1);
        }

        rt.join();

        if (m_requestExport) {
            m_requestExport = false;
            {
                std::string ext = (m_exportSettings.outputFormat == EncoderSettings::OutputFormat::TS)
                    ? "ts" : "mp4";
                std::string savePath = saveFileDialog(ext);
                if (!savePath.empty()) {
                    std::atomic<bool>  encodingDone{false};
                    std::atomic<bool>  cancelEncoding{false};
                    std::atomic<float> encodingProgress{0.0f};
                    std::thread encThread([&]() {
                        runEncoding(savePath, m_exportSettings, cancelEncoding, encodingProgress);
                        encodingDone = true;
                    });

                    constexpr int   PROG_WIN_W = 360;
                    constexpr int   PROG_WIN_H = 150;
                    constexpr float BAR_W      = 300.0f;
                    constexpr float BAR_H      =  16.0f;
                    constexpr float BTN_W      = 100.0f;
                    constexpr float BTN_H      =  26.0f;
                    constexpr float fW         = static_cast<float>(PROG_WIN_W);
                    constexpr float fH         = static_cast<float>(PROG_WIN_H);
                    constexpr float barX       = (fW - BAR_W) * 0.5f;
                    constexpr float barY       = fH * 0.5f - 8.0f;
                    constexpr float bx         = (fW - BTN_W) * 0.5f;
                    constexpr float by         = fH * 0.5f + 26.0f;
                    constexpr const char* MSG  = "Exporting... Please wait.";
                    constexpr float MSG_W      = 25.0f * 8.0f;

                    SDL_Window*   progWin      = SDL_CreateWindow("Exporting", PROG_WIN_W, PROG_WIN_H, 0);
                    SDL_Renderer* progRenderer = progWin ? SDL_CreateRenderer(progWin, nullptr) : nullptr;
                    SDL_WindowID  progWinID    = progWin ? SDL_GetWindowID(progWin) : 0;
                    float mouseX = 0.0f, mouseY = 0.0f;
                    char  pctBuf[8] = "0%";

                    while (!encodingDone) {
                        SDL_Event ev;
                        while (SDL_PollEvent(&ev)) {
                            if (ev.type == SDL_EVENT_MOUSE_MOTION &&
                                ev.motion.windowID == progWinID) {
                                mouseX = ev.motion.x;
                                mouseY = ev.motion.y;
                            }
                            if (ev.type == SDL_EVENT_KEY_DOWN &&
                                ev.key.windowID == progWinID &&
                                ev.key.key == SDLK_ESCAPE) {
                                cancelEncoding = true;
                            }
                            if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                                ev.button.windowID == progWinID &&
                                ev.button.button == SDL_BUTTON_LEFT) {
                                if (ev.button.x >= bx && ev.button.x < bx + BTN_W &&
                                    ev.button.y >= by && ev.button.y < by + BTN_H) {
                                    cancelEncoding = true;
                                }
                            }
                        }

                        if (progRenderer) {
                            float progress = encodingProgress.load();
                            int   pct      = static_cast<int>(progress * 100.0f);
                            snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);

                            SDL_SetRenderDrawColor(progRenderer, 0, 0, 0, 255);
                            SDL_RenderClear(progRenderer);

                            SDL_SetRenderDrawColor(progRenderer, 255, 255, 255, 255);
                            SDL_RenderDebugText(progRenderer,
                                (fW - MSG_W) * 0.5f, fH * 0.5f - 34.0f, MSG);

                            SDL_FRect bgBar   = {barX, barY, BAR_W, BAR_H};
                            SDL_FRect fillBar = {barX, barY, BAR_W * progress, BAR_H};
                            SDL_SetRenderDrawBlendMode(progRenderer, SDL_BLENDMODE_BLEND);
                            SDL_SetRenderDrawColor(progRenderer, 50, 50, 50, 255);
                            SDL_RenderFillRect(progRenderer, &bgBar);
                            SDL_SetRenderDrawColor(progRenderer, 30, 100, 180, 255);
                            SDL_RenderFillRect(progRenderer, &fillBar);
                            SDL_SetRenderDrawColor(progRenderer, 120, 120, 120, 200);
                            SDL_RenderRect(progRenderer, &bgBar);
                            SDL_SetRenderDrawBlendMode(progRenderer, SDL_BLENDMODE_NONE);

                            size_t pctLen = strlen(pctBuf);
                            SDL_SetRenderDrawColor(progRenderer, 255, 255, 255, 255);
                            SDL_RenderDebugText(progRenderer,
                                barX + (BAR_W - static_cast<float>(pctLen) * 8.0f) * 0.5f,
                                barY + (BAR_H - 8.0f) * 0.5f, pctBuf);

                            bool hovered  = mouseX >= bx && mouseX < bx + BTN_W &&
                                            mouseY >= by && mouseY < by + BTN_H;
                            SDL_FRect btn = {bx, by, BTN_W, BTN_H};
                            SDL_SetRenderDrawBlendMode(progRenderer, SDL_BLENDMODE_BLEND);
                            SDL_SetRenderDrawColor(progRenderer, 60, 60, 60, hovered ? 220 : 160);
                            SDL_RenderFillRect(progRenderer, &btn);
                            SDL_SetRenderDrawColor(progRenderer, 200, 200, 200, 200);
                            SDL_RenderRect(progRenderer, &btn);
                            SDL_SetRenderDrawBlendMode(progRenderer, SDL_BLENDMODE_NONE);
                            SDL_SetRenderDrawColor(progRenderer, 255, 255, 255, 255);
                            SDL_RenderDebugText(progRenderer,
                                bx + (BTN_W - 6.0f * 8.0f) * 0.5f,
                                by + (BTN_H - 8.0f) * 0.5f, "Cancel");

                            SDL_RenderPresent(progRenderer);
                        }
                        SDL_Delay(16);
                    }

                    if (progRenderer) { SDL_DestroyRenderer(progRenderer); }
                    if (progWin)      { SDL_DestroyWindow(progWin); }

                    encThread.join();

                    if (cancelEncoding) {
                        std::filesystem::remove(savePath);
                        Logger::instance().info("VideoPlayer: export cancelled, partial file removed");
                    }
                }
            }
            m_decoder.seek(0.0);
            continue;
        }

        if (!m_requestOpenFile) { break; }

        // Check if the user selected a recent file from the menu; otherwise show the picker.
        std::string newPath = m_renderer.takePendingOpenPath();
        if (newPath.empty()) {
            newPath = filePicker();
        }
        if (newPath.empty())
        {
            continue;
        } 

        m_decoder.close();
        if (!m_decoder.open(newPath)) { break; }

        m_recentFiles.add(newPath);
        m_renderer.setRecentFiles(m_recentFiles.entries());

        if (!m_renderer.reloadVideo("VideoPlayer - " + newPath,
                                    m_decoder.videoWidth(),
                                    m_decoder.videoHeight())) break;
    }
}


void VideoPlayer::runExportDialog() {
    EncoderSettings settings;
    if (showExportDialog(m_renderer.getSDLRenderer(), m_decoder.duration(), m_quit, settings)) {
        m_exportSettings = settings;
        m_requestExport  = true;
        m_quit           = true;
    }
    m_showExportDialog = false;
}

void VideoPlayer::runEncoding(const std::string& savePath, const EncoderSettings& settings,
                              std::atomic<bool>& cancel, std::atomic<float>& progress) {
    if (!m_decoder.seek(0.0)) {
        Logger::instance().error("VideoPlayer: failed to seek to start for encoding");
        return;
    }

    int  sampleRate = m_decoder.hasAudio() ? 44100 : 0;
    int  channels   = m_decoder.hasAudio() ? 2 : 0;
    bool preferH264 = settings.videoCodec == EncoderSettings::VideoCodec::H264;

    Encoder encoder;
    if (!encoder.open(savePath,
                      m_decoder.videoWidth(), m_decoder.videoHeight(),
                      m_decoder.videoTimeBase(),
                      sampleRate, channels,
                      settings.videoBitRateKbps, settings.audioBitRateKbps,
                      preferH264)) {
        Logger::instance().error("VideoPlayer: failed to open encoder for " + savePath);
        return;
    }

    double duration    = m_decoder.duration();
    double exportLimit = (settings.exportDuration > 0.0f)
                         ? static_cast<double>(settings.exportDuration) : duration;
    DecodedFrame frame;
    while (!cancel && m_decoder.readFrame(frame)) {
        if (exportLimit > 0.0) {
            float p = static_cast<float>(frame.pts / exportLimit);
            progress = p < 1.0f ? p : 1.0f;
        }
        if (settings.exportDuration > 0.0f && frame.pts >= exportLimit) {
            av_frame_free(&frame.videoFrame);
            av_frame_free(&frame.audioFrame);
            break;
        }
        if (frame.videoFrame) {
            encoder.writeVideoFrame(frame.videoFrame);
            av_frame_free(&frame.videoFrame);
        }
        if (frame.audioFrame) {
            encoder.writeAudioFrame(frame.audioFrame);
            av_frame_free(&frame.audioFrame);
        }
    }

    encoder.close();

    if (!cancel) {
        progress = 1.0f;
        Logger::instance().info("VideoPlayer: export complete — " + savePath);
    }
}
