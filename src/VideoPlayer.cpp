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
    if (!m_demuxer.open(filePath)) {
        return false;
    }
    if (!m_decoder.open(m_demuxer.videoCodecParameters(),
                        m_demuxer.audioCodecParameters(),
                        m_demuxer.videoTimeBase(),
                        m_demuxer.audioTimeBase())) {
        return false;
    }
    m_filePath = filePath;
    m_recentFiles.add(filePath);
    m_renderer.setRecentFiles(m_recentFiles.entries());
    return m_renderer.initWindow("VideoPlayer - " + filePath,
                                 m_decoder.videoWidth(),
                                 m_decoder.videoHeight());
}

bool VideoPlayer::seek(double seconds) {
    if (!m_demuxer.seek(seconds)) {
        return false;
    }
    m_decoder.flushBuffers();
    return true;
}

void VideoPlayer::renderLoop() {
    if (!m_renderer.initRenderer()) {
        m_quit = true;
        return;
    }

    const double audioLatency = m_renderer.getAudioLatency();
    const double SEEK_STEP    = SEEK_STEP_SECONDS;
    const double duration     = m_demuxer.duration();

    bool     clocked    = false;
    bool     reclock    = false;
    uint64_t startTick  = 0;
    double   startPts   = 0.0;
    bool     paused     = false;
    double   currentPts = 0.0;
    float    lastSpeed  = 1.0f;
    double   target     = 0.0;

    bool wasSuppress = false;
    while (!m_quit) {
        bool inDialog = m_showExportDialog.load();
        bool suppress = inDialog || m_encodingActive.load();
        if (suppress) { m_renderer.onActivity(); }
        PlayerEvent ev = suppress ? PlayerEvent::None : m_renderer.pollEvents();
        if (!suppress && wasSuppress) { reclock = true; }
        wasSuppress = suppress;

        if (ev == PlayerEvent::Quit) {
            break;
        }

        if (ev == PlayerEvent::OpenFile) {
            m_requestOpenFile = true;
            break;
        }

        if (ev == PlayerEvent::ExportVideo && !inDialog) {
            m_showExportDialog = true;
        }

        switch (ev) {
            case PlayerEvent::TogglePause:
                paused = !paused;
                if (paused) {
                    m_renderer.pauseAudio();
                } else {
                    m_renderer.resumeAudio();
                    reclock = true;
                }
                break;

            case PlayerEvent::SeekForward:
            case PlayerEvent::SeekBackward:
                target = currentPts + (ev == PlayerEvent::SeekForward ? SEEK_STEP : -SEEK_STEP);
                target = std::max(0.0, std::min(target, duration));
                if (seek(target)) {
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
                if (seek(target)) {
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

        // Pull packets from the demuxer and feed them to the decoder until
        // a frame comes out (needed for codecs that buffer across packets).
        DecodedFrame frame;
        bool gotFrame = false;
        AVPacket* pkt = av_packet_alloc();
        while (!gotFrame) {
            if (!m_demuxer.readPacket(pkt)) {
                break;
            }
            gotFrame = m_decoder.decode(pkt, m_demuxer.isVideoPacket(pkt), frame);
        }
        av_packet_free(&pkt);

        if (!gotFrame) {
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

            if (targetWall > wall) {
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
        m_requestExport    = false;
        m_showExportDialog = false;
        m_encodingActive   = false;

        std::thread rt([this]() { renderLoop(); });

        while (!m_quit) {
            SDL_PumpEvents();
            if (m_showExportDialog) { runExportDialog(); }
            if (m_requestExport)    { m_requestExport = false; runExportProgress(); }
            SDL_Delay(1);
        }

        rt.join();

        if (!m_requestOpenFile) { break; }

        std::string newPath = m_renderer.takePendingOpenPath();
        if (newPath.empty()) {
            newPath = filePicker();
        }
        if (newPath.empty()) {
            continue;
        }

        m_demuxer.close();
        m_decoder.close();
        if (!m_demuxer.open(newPath)) { break; }
        if (!m_decoder.open(m_demuxer.videoCodecParameters(),
                            m_demuxer.audioCodecParameters(),
                            m_demuxer.videoTimeBase(),
                            m_demuxer.audioTimeBase())) {
            break;
        }

        m_filePath = newPath;
        m_recentFiles.add(newPath);
        m_renderer.setRecentFiles(m_recentFiles.entries());

        if (!m_renderer.reloadVideo("VideoPlayer - " + newPath,
                                    m_decoder.videoWidth(),
                                    m_decoder.videoHeight())) { break; }
    }
}

void VideoPlayer::runExportDialog() {
    std::string defaultFolder;
    if (!m_filePath.empty()) {
        defaultFolder = std::filesystem::path(m_filePath).parent_path().string();
    }
    EncoderSettings settings;
    if (showExportDialog(m_renderer.getSDLRenderer(), m_demuxer.duration(),
                         defaultFolder, m_quit, settings)) {
        m_exportSettings = settings;
        m_requestExport  = true;
    }
    m_showExportDialog = false;
}

void VideoPlayer::runExportProgress() {
    std::string ext = (m_exportSettings.outputFormat == EncoderSettings::OutputFormat::TS)
        ? "ts" : "mp4";

    std::string savePath;
    if (!m_exportSettings.outputFolder.empty()) {
        std::string stem = std::filesystem::path(m_filePath).stem().string();
        savePath = (std::filesystem::path(m_exportSettings.outputFolder) /
                    (stem + "_export." + ext)).string();
        std::filesystem::create_directories(m_exportSettings.outputFolder);
    } else {
        savePath = saveFileDialog(ext);
    }
    if (savePath.empty()) { return; }

    std::atomic<bool>  encodingDone{false};
    std::atomic<bool>  cancelEncoding{false};
    std::atomic<float> encodingProgress{0.0f};

    std::thread encThread([&]() {
        Demuxer encDemuxer;
        if (!encDemuxer.open(m_filePath)) {
            Logger::instance().error("VideoPlayer: failed to open demuxer for encoding");
            encodingDone = true;
            return;
        }
        Decoder encDecoder;
        if (!encDecoder.open(encDemuxer.videoCodecParameters(),
                             encDemuxer.audioCodecParameters(),
                             encDemuxer.videoTimeBase(),
                             encDemuxer.audioTimeBase())) {
            Logger::instance().error("VideoPlayer: failed to open decoder for encoding");
            encodingDone = true;
            return;
        }
        runEncoding(encDemuxer, encDecoder, savePath, m_exportSettings,
                    cancelEncoding, encodingProgress);
        encodingDone = true;
    });

    m_encodingActive = true;

    SDL_Window*   mainWin   = SDL_GetRenderWindow(m_renderer.getSDLRenderer());
    SDL_WindowID  mainWinID = mainWin ? SDL_GetWindowID(mainWin) : 0;

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
            if ((ev.type == SDL_EVENT_QUIT) ||
                (ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                 ev.window.windowID == mainWinID)) {
                m_quit         = true;
                cancelEncoding = true;
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
    m_encodingActive = false;

    if (cancelEncoding) {
        std::filesystem::remove(savePath);
        Logger::instance().info("VideoPlayer: export cancelled, partial file removed");
    }
}

void VideoPlayer::runEncoding(Demuxer& demuxer, Decoder& decoder,
                              const std::string& savePath, const EncoderSettings& settings,
                              std::atomic<bool>& cancel, std::atomic<float>& progress) {
    double exportStart = static_cast<double>(settings.exportStartTime);

    if (!demuxer.seek(exportStart)) {
        Logger::instance().error("VideoPlayer: failed to seek to start for encoding");
        return;
    }
    decoder.flushBuffers();

    int  sampleRate = demuxer.hasAudio() ? 44100 : 0;
    int  channels   = demuxer.hasAudio() ? 2 : 0;
    bool preferH264 = settings.videoCodec == EncoderSettings::VideoCodec::H264;

    Muxer muxer;
    if (!muxer.open(savePath)) {
        Logger::instance().error("VideoPlayer: failed to open muxer for " + savePath);
        return;
    }

    Encoder encoder;
    if (!encoder.open(muxer,
                      decoder.videoWidth(), decoder.videoHeight(),
                      demuxer.videoTimeBase(),
                      sampleRate, channels,
                      settings.videoBitRateKbps, settings.audioBitRateKbps,
                      preferH264)) {
        Logger::instance().error("VideoPlayer: failed to open encoder for " + savePath);
        return;
    }

    if (!muxer.beginFile()) {
        Logger::instance().error("VideoPlayer: failed to begin muxer file for " + savePath);
        return;
    }

    double duration    = demuxer.duration();
    double exportLimit = (settings.exportDuration > 0.0f)
                         ? exportStart + static_cast<double>(settings.exportDuration)
                         : duration;
    double clipLength  = exportLimit - exportStart;

    AVRational vtb = demuxer.videoTimeBase();
    int64_t videoPtsOffset = static_cast<int64_t>(exportStart * vtb.den / vtb.num);

    AVPacket* pkt = av_packet_alloc();
    while (!cancel) {
        if (!demuxer.readPacket(pkt)) {
            break;
        }

        bool isVideo = demuxer.isVideoPacket(pkt);
        DecodedFrame frame;
        if (!decoder.decode(pkt, isVideo, frame)) {
            continue;
        }

        if (clipLength > 0.0) {
            float p = static_cast<float>((frame.pts - exportStart) / clipLength);
            progress = p < 0.0f ? 0.0f : (p < 1.0f ? p : 1.0f);
        }
        if (settings.exportDuration > 0.0f && frame.pts >= exportLimit) {
            av_frame_free(&frame.videoFrame);
            av_frame_free(&frame.audioFrame);
            break;
        }
        // Skip pre-roll frames that arrive before exportStart due to keyframe alignment.
        if (frame.pts < exportStart) {
            av_frame_free(&frame.videoFrame);
            av_frame_free(&frame.audioFrame);
            continue;
        }
        if (frame.videoFrame) {
            frame.videoFrame->pts -= videoPtsOffset;
            if (frame.videoFrame->pts < 0) { frame.videoFrame->pts = 0; }
            encoder.writeVideoFrame(frame.videoFrame);
            av_frame_free(&frame.videoFrame);
        }
        if (frame.audioFrame) {
            encoder.writeAudioFrame(frame.audioFrame);
            av_frame_free(&frame.audioFrame);
        }
    }
    av_packet_free(&pkt);

    encoder.close();
    muxer.endFile();

    if (!cancel) {
        progress = 1.0f;
        Logger::instance().info("VideoPlayer: export complete — " + savePath);
    }
}
