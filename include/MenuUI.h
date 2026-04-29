#ifndef MENU_UI_H
#define MENU_UI_H

#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "PlayerEvent.h"

class MenuUI {
public:
    void render(SDL_Renderer* renderer, float W);

    // Returns the triggered event (if any). Sets consumed=true if the click
    // was handled by the menu and should not propagate to other controls.
    PlayerEvent handleMouseClick(float mx, float my, bool& consumed);

    void handleMouseMotion(float mx, float my);

    void setRecentFiles(const std::vector<std::string>& files);

    // Returns the path selected from the recent-files list (empty if none).
    // Clears the stored value on read.
    std::string takePendingPath();

private:
    bool  m_menuOpen = false;
    float m_mouseX   = 0.0f;
    float m_mouseY   = 0.0f;
    SDL_FRect m_fileMenuRect      = {};
    SDL_FRect m_openFileDropRect   = {};
    SDL_FRect m_recentFilesRowRect = {};
    SDL_FRect m_exportVideoDropRect = {};
    bool      m_subMenuVisible    = false;

    std::vector<std::string> m_recentFiles;
    std::vector<SDL_FRect>   m_recentItemRects;
    std::string              m_pendingPath;
};

#endif // MENU_UI_H
