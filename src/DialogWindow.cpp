#include "DialogWindow.h"
#include "Logger.h"
#include <string>

DialogWindow::DialogWindow(const char* title, int w, int h)
    : m_title(title), m_w(w), m_h(h) {}

void DialogWindow::run(SDL_Renderer* mainRenderer, std::atomic<bool>& quit) {
    SDL_Window*   win    = SDL_CreateWindow(m_title, m_w, m_h, 0);
    SDL_Renderer* ren    = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
    SDL_WindowID  winID  = win ? SDL_GetWindowID(win) : 0;
    SDL_Window*   mainWin = mainRenderer ? SDL_GetRenderWindow(mainRenderer) : nullptr;
    SDL_WindowID  mainID  = mainWin ? SDL_GetWindowID(mainWin) : 0;

    bool wasCursorVisible = SDL_CursorVisible();
    SDL_ShowCursor();
    m_done = false;

    Logger::instance().trace(std::string("DialogWindow '") + m_title + "' entering render loop");

    while (!quit && !m_done) {
        SDL_Event ev;
        while (!m_done && SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) { quit = true; m_done = true; }
            if (ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                if (ev.window.windowID == winID)  { m_done = true; }
                if (ev.window.windowID == mainID) { quit = true; m_done = true; }
            }
            if (ev.type == SDL_EVENT_MOUSE_MOTION && ev.motion.windowID == winID) {
                onMouseMotion(ev.motion.x, ev.motion.y);
            }
            if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                ev.button.windowID == winID &&
                ev.button.button == SDL_BUTTON_LEFT) {
                onMouseButtonDown(ev.button.x, ev.button.y);
            }
            if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                ev.button.windowID == winID &&
                ev.button.button == SDL_BUTTON_LEFT) {
                onMouseButtonUp(ev.button.x, ev.button.y);
            }
        }
        if (!m_done && ren) {
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
            onRender(ren, static_cast<float>(m_w), static_cast<float>(m_h));
            SDL_RenderPresent(ren);
        }
        if (!m_done) { SDL_Delay(16); }
    }

    Logger::instance().trace(std::string("DialogWindow '") + m_title + "' exited render loop");

    if (ren) { SDL_DestroyRenderer(ren); }
    if (win) { SDL_DestroyWindow(win); }
    if (!wasCursorVisible) {
        if (!SDL_HideCursor()) {
            Logger::instance().error(std::string("SDL_HideCursor failed: ") + SDL_GetError());
        }
    }
}
