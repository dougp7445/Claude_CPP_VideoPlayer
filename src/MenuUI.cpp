#include "MenuUI.h"

static constexpr float MENU_H        = 22.0f;
static constexpr float MENU_ITEM_W   = 40.0f;
static constexpr float MENU_PAD      =  6.0f;
static constexpr float DROP_W        = 220.0f;
static constexpr float DROP_ITEM_H   = 20.0f;
static constexpr float DROP_SEP_H    =  5.0f;
static constexpr size_t NAME_MAX     = 26;      // chars before truncation

static std::string truncatedName(const std::string& path) {
    size_t sep = path.find_last_of("/\\");
    std::string name = (sep != std::string::npos) ? path.substr(sep + 1) : path;
    if (name.size() > NAME_MAX)
        name = name.substr(0, NAME_MAX - 3) + "...";
    return name;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void MenuUI::setRecentFiles(const std::vector<std::string>& files) {
    m_recentFiles = files;
}

std::string MenuUI::takePendingPath() {
    return std::move(m_pendingPath);
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void MenuUI::render(SDL_Renderer* renderer, float W) {
    // ── Menu bar ──
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_FRect menuBg = {0.0f, 0.0f, W, MENU_H};
    SDL_RenderFillRect(renderer, &menuBg);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    m_fileMenuRect = {MENU_PAD, 0.0f, MENU_ITEM_W, MENU_H};
    bool fileHovered = m_menuOpen
        || (m_mouseX >= MENU_PAD && m_mouseX < MENU_PAD + MENU_ITEM_W
            && m_mouseY >= 0.0f  && m_mouseY < MENU_H);
    if (fileHovered) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 40);
        SDL_RenderFillRect(renderer, &m_fileMenuRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, MENU_PAD + 4.0f, (MENU_H - 8.0f) * 0.5f, "File");

    if (!m_menuOpen) return;

    // ── Dropdown panel (two fixed rows: Open File + Recent Files) ──
    float dx = MENU_PAD;
    float dy = MENU_H;
    float panelH = DROP_ITEM_H * 2.0f;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 28, 28, 28, 230);
    SDL_FRect panel = {dx, dy, DROP_W, panelH};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 90, 90, 90, 200);
    SDL_RenderRect(renderer, &panel);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // ── "Open File" row ──
    m_openFileDropRect = {dx, dy, DROP_W, DROP_ITEM_H};
    bool openHovered = (m_mouseX >= dx && m_mouseX < dx + DROP_W
                     && m_mouseY >= dy && m_mouseY < dy + DROP_ITEM_H);
    if (openHovered) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 35);
        SDL_RenderFillRect(renderer, &m_openFileDropRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, dx + 8.0f,
                        dy + (DROP_ITEM_H - 8.0f) * 0.5f, "Open File");
    SDL_RenderDebugText(renderer, dx + DROP_W - 6.0f * 8.0f - 8.0f,
                        dy + (DROP_ITEM_H - 8.0f) * 0.5f, "Ctrl+O");

    // ── "Recent Files" row ──
    float rfY = dy + DROP_ITEM_H;
    m_recentFilesRowRect = {dx, rfY, DROP_W, DROP_ITEM_H};

    float subX  = dx + DROP_W;
    float subH  = static_cast<float>(m_recentFiles.size()) * DROP_ITEM_H;

    bool rfRowHovered = (m_mouseX >= dx   && m_mouseX < dx + DROP_W
                      && m_mouseY >= rfY  && m_mouseY < rfY + DROP_ITEM_H);
    bool subPanelHovered = !m_recentFiles.empty()
                        && (m_mouseX >= subX && m_mouseX < subX + DROP_W
                         && m_mouseY >= rfY  && m_mouseY < rfY + subH);
    m_subMenuVisible = !m_recentFiles.empty() && (rfRowHovered || subPanelHovered);

    if (rfRowHovered || subPanelHovered) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 35);
        SDL_RenderFillRect(renderer, &m_recentFilesRowRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    uint8_t rfAlpha = m_recentFiles.empty() ? 100 : 255;
    SDL_SetRenderDrawColor(renderer, rfAlpha, rfAlpha, rfAlpha, 255);
    SDL_RenderDebugText(renderer, dx + 8.0f,
                        rfY + (DROP_ITEM_H - 8.0f) * 0.5f, "Recent Files");
    SDL_RenderDebugText(renderer, dx + DROP_W - 8.0f - 8.0f,
                        rfY + (DROP_ITEM_H - 8.0f) * 0.5f, ">");

    if (!m_subMenuVisible) return;

    // ── Submenu panel ──
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 28, 28, 28, 230);
    SDL_FRect subPanel = {subX, rfY, DROP_W, subH};
    SDL_RenderFillRect(renderer, &subPanel);
    SDL_SetRenderDrawColor(renderer, 90, 90, 90, 200);
    SDL_RenderRect(renderer, &subPanel);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    m_recentItemRects.resize(m_recentFiles.size());
    float itemY = rfY;
    for (size_t i = 0; i < m_recentFiles.size(); ++i) {
        SDL_FRect& rect = m_recentItemRects[i];
        rect = {subX, itemY, DROP_W, DROP_ITEM_H};

        bool hovered = (m_mouseX >= subX && m_mouseX < subX + DROP_W
                     && m_mouseY >= itemY && m_mouseY < itemY + DROP_ITEM_H);
        if (hovered) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 35);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDebugText(renderer, subX + 8.0f,
                            itemY + (DROP_ITEM_H - 8.0f) * 0.5f,
                            truncatedName(m_recentFiles[i]).c_str());

        itemY += DROP_ITEM_H;
    }
}

// ── Hit testing ───────────────────────────────────────────────────────────────

PlayerEvent MenuUI::handleMouseClick(float mx, float my, bool& consumed) {
    auto hit = [](float x, float y, const SDL_FRect& r) {
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    };

    if (m_menuOpen) {
        m_menuOpen = false;
        consumed = true;

        if (hit(mx, my, m_openFileDropRect)) {
            m_pendingPath.clear();
            return PlayerEvent::OpenFile;
        }

        if (m_subMenuVisible) {
            for (size_t i = 0; i < m_recentItemRects.size(); ++i) {
                if (hit(mx, my, m_recentItemRects[i])) {
                    m_pendingPath = m_recentFiles[i];
                    return PlayerEvent::OpenFile;
                }
            }
        }

        return PlayerEvent::None;
    }

    if (hit(mx, my, m_fileMenuRect)) {
        m_menuOpen = true;
        consumed = true;
        return PlayerEvent::None;
    }

    consumed = false;
    return PlayerEvent::None;
}

void MenuUI::handleMouseMotion(float mx, float my) {
    m_mouseX = mx;
    m_mouseY = my;
}
