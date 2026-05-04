#include "MainWindow.h"
#include "Constants.h"
#include "ExportSettingsDialog.h"
#include "FileOperations.h"
#include "Logger.h"
#include <filesystem>
#include <thread>

MainWindow::MainWindow() {}
MainWindow::~MainWindow() = default;

bool MainWindow::load(const std::string& filePath) {
    return m_player.load(filePath);
}

void MainWindow::run(std::function<std::string()> filePicker) {
    while (true) {
        m_player.resetState();
        m_requestExport = false;

        std::thread rt([this]() { m_player.renderLoop(); });

        while (!m_player.quit().load()) {
            SDL_PumpEvents();
            if (m_player.wantsExportDialog()) { runExportDialog(); }
            if (m_requestExport)              { m_requestExport = false; runExportProgress(); }
            SDL_Delay(1);
        }

        rt.join();

        if (!m_player.requestedOpenFile()) { break; }

        std::string newPath = m_player.takePendingOpenPath();
        if (newPath.empty()) {
            newPath = filePicker();
        }
        if (newPath.empty()) {
            continue;
        }

        if (!m_player.reload(newPath)) { break; }
    }
}

void MainWindow::runExportDialog() {
    std::string defaultFilePath;
    if (!m_player.filePath().empty()) {
        auto p = std::filesystem::path(m_player.filePath());
        std::string dir = m_player.lastExportDir().empty()
            ? p.parent_path().string()
            : m_player.lastExportDir();
        defaultFilePath = (std::filesystem::path(dir) /
            (p.stem().string() + EXPORT_DEFAULT_STEM_SUFFIX + "." + EXPORT_DEFAULT_EXTENSION)).string();
    }
    EncoderSettings settings;
    if (showExportDialog(m_player.sdlRenderer(), m_player.duration(),
                         defaultFilePath, m_player.quit(), settings)) {
        m_exportSettings = settings;
        m_requestExport  = true;
    }
    m_player.clearWantsExportDialog();
}

void MainWindow::runExportProgress() {
    std::string ext = (m_exportSettings.outputFormat == EncoderSettings::OutputFormat::TS)
        ? "ts" : "mp4";

    std::string savePath = m_exportSettings.outputFilePath;
    if (savePath.empty()) {
        savePath = saveFileDialog(ext);
    }
    if (!savePath.empty()) {
        auto parent = std::filesystem::path(savePath).parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
            m_player.setLastExportDir(parent.string());
        }
    }
    if (savePath.empty()) { return; }

    m_exporter.start(m_player.filePath(), savePath, m_exportSettings);
    m_player.setEncodingActive(true);

    SDL_Window*   mainWin   = SDL_GetRenderWindow(m_player.sdlRenderer());
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

    while (!m_exporter.isDone()) {
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
                m_exporter.cancel();
            }
            if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                ev.button.windowID == progWinID &&
                ev.button.button == SDL_BUTTON_LEFT) {
                if (ev.button.x >= bx && ev.button.x < bx + BTN_W &&
                    ev.button.y >= by && ev.button.y < by + BTN_H) {
                    m_exporter.cancel();
                }
            }
            if ((ev.type == SDL_EVENT_QUIT) ||
                (ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                 ev.window.windowID == mainWinID)) {
                m_player.quit().store(true);
                m_exporter.cancel();
            }
        }

        if (progRenderer) {
            float progress = m_exporter.progress();
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

    m_exporter.wait();
    m_player.setEncodingActive(false);

    if (m_exporter.cancelled()) {
        std::filesystem::remove(savePath);
        Logger::instance().info("MainWindow: export cancelled, partial file removed");
    }
}
