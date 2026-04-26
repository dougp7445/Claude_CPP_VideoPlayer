#include "VideoPlayer.h"
#include "Constants.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#endif

static std::string executableDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0) return "";
    std::wstring wpath(buf, len);
    auto pos = wpath.rfind(L'\\');
    if (pos != std::wstring::npos) wpath.resize(pos + 1);
    int n = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string result(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, result.data(), n, nullptr, nullptr);
    return result;
#else
    return "";
#endif
}

static std::string openFileDialog() {
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    std::string result;
    IFileOpenDialog* pfd = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&pfd)))) {
        COMDLG_FILTERSPEC filters[] = {
            { L"Video Files", L"*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.flv;*.webm;*.m4v;*.ts;*.mpg;*.mpeg" },
            { L"All Files",   L"*.*" }
        };
        pfd->SetFileTypes(2, filters);
        pfd->SetDefaultExtension(L"mp4");

        if (SUCCEEDED(pfd->Show(nullptr))) {
            IShellItem* psi = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psi))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0) {
                        result.resize(len - 1);
                        WideCharToMultiByte(CP_UTF8, 0, path, -1, result.data(), len, nullptr, nullptr);
                    }
                    CoTaskMemFree(path);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }

    CoUninitialize();
    return result;
#else
    return {};
#endif
}

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

    while (!m_quit) {
        PlayerEvent ev = m_renderer.pollEvents();

        if (ev == PlayerEvent::Quit) {
            break;
        }

        if (ev == PlayerEvent::OpenFile) {
            m_requestOpenFile = true;
            break;
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

void VideoPlayer::run() {
    while (true) {
        m_quit            = false;
        m_requestOpenFile = false;

        std::thread rt([this]() { renderLoop(); });

        while (!m_quit) {
            SDL_PumpEvents();
            SDL_Delay(1);
        }

        rt.join();

        if (!m_requestOpenFile) break;

        // Check if the user selected a recent file from the menu; otherwise show the picker.
        std::string newPath = m_renderer.takePendingOpenPath();
        if (newPath.empty()) newPath = openFileDialog();
        if (newPath.empty()) continue;

        m_decoder.close();
        if (!m_decoder.open(newPath)) break;

        m_recentFiles.add(newPath);
        m_renderer.setRecentFiles(m_recentFiles.entries());

        if (!m_renderer.reloadVideo("VideoPlayer - " + newPath,
                                    m_decoder.videoWidth(),
                                    m_decoder.videoHeight())) break;
    }
}
