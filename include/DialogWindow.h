#ifndef DIALOG_WINDOW_H
#define DIALOG_WINDOW_H

#include <SDL3/SDL.h>
#include <atomic>

class DialogWindow {
public:
    DialogWindow(const char* title, int w, int h);
    virtual ~DialogWindow() = default;

    // Creates the window, runs the event loop, then destroys it.
    // Sets quit=true if SDL_EVENT_QUIT or the main window is closed.
    void run(SDL_Renderer* mainRenderer, std::atomic<bool>& quit);

protected:
    virtual void onRender(SDL_Renderer* renderer, float w, float h) = 0;
    virtual void onMouseMotion(float x, float y) {}
    virtual void onMouseButtonDown(float x, float y) {}
    virtual void onMouseButtonUp(float x, float y) {}

    void close() { m_done = true; }

private:
    const char* m_title;
    int         m_w;
    int         m_h;
    bool        m_done = false;
};

#endif // DIALOG_WINDOW_H
