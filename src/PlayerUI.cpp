#include "PlayerUI.h"
#include "Constants.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

// ── Layout constants (all in reference pixels at 720p) ───────────────────────
static constexpr float BAR_H       = 58.0f;
static constexpr float PAD         = 8.0f;
static constexpr float BTN_W       = 44.0f;
static constexpr float PROG_H      = 8.0f;
static constexpr float TIME_W      = 160.0f; // fixed: debug font is always 8 px/char
static constexpr float SPEED_BTN_W = 40.0f;
static constexpr float MUTE_BTN_W  = 30.0f;
static constexpr float VOL_BAR_W   = 64.0f;
static constexpr uint64_t UI_HIDE_MS = 3000;

static constexpr float       SPEEDS[]       = {0.5f, 1.0f, 1.5f, 2.0f};
static constexpr int         SPEED_COUNT    = 4;
static constexpr const char* SPEED_LABELS[] = {"0.5x", "1x", "1.5x", "2x"};
// ─────────────────────────────────────────────────────────────────────────────

static std::string formatTime(double secs) {
    int t = static_cast<int>(secs);
    int h = t / 3600;
    int m = (t % 3600) / 60;
    int s = t % 60;
    char buf[16];
    if (h > 0) snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
    else        snprintf(buf, sizeof(buf), "%02d:%02d",    m, s);
    return buf;
}

// ── SVG texture loading ───────────────────────────────────────────────────────

static SDL_Texture* loadSVGTexture(SDL_Renderer* renderer, const char* path, int size) {
    NSVGimage* image = nsvgParseFromFile(path, "px", 96.0f);
    if (!image) return nullptr;

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    if (!rast) { nsvgDelete(image); return nullptr; }

    std::vector<uint8_t> pixels(size * size * 4, 0);
    float scale = static_cast<float>(size) / std::max(image->width, image->height);
    nsvgRasterize(rast, image, 0.0f, 0.0f, scale, pixels.data(), size, size, size * 4);
    nsvgDeleteRasterizer(rast);
    nsvgDelete(image);

    // SVGs use fill="#000000"; recolor non-transparent pixels to white so they
    // composite correctly over the dark control bar.
    for (size_t i = 0; i < pixels.size(); i += 4) {
        if (pixels[i + 3] > 0)
            pixels[i] = pixels[i + 1] = pixels[i + 2] = 255;
    }

    SDL_Surface* surf = SDL_CreateSurfaceFrom(size, size, SDL_PIXELFORMAT_RGBA32,
                                              pixels.data(), size * 4);
    if (!surf) return nullptr;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    return tex;
}

void PlayerUI::initTextures(SDL_Renderer* renderer) {
    const char* base = SDL_GetBasePath();
    std::string dir = base ? std::string(base) : "";

    m_volFullTex  = loadSVGTexture(renderer, (dir + "icons/volume-full.svg").c_str(),  64);
    m_volMutedTex = loadSVGTexture(renderer, (dir + "icons/volume-muted.svg").c_str(), 64);
}

PlayerUI::~PlayerUI() {
    SDL_DestroyTexture(m_volFullTex);
    SDL_DestroyTexture(m_volMutedTex);
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void PlayerUI::onActivity() {
    m_lastActivityMs = SDL_GetTicks();
    if (!m_uiVisible) {
        m_uiVisible = true;
        SDL_ShowCursor();
    }
}

void PlayerUI::adjustVolume(float delta) {
    m_volume = SDL_clamp(m_volume + delta, VOLUME_MIN, VOLUME_MAX);
}

void PlayerUI::cycleSpeed() {
    m_speedIndex = (m_speedIndex + 1) % SPEED_COUNT;
    m_speed      = SPEEDS[m_speedIndex];
}

// ── Rendering ────────────────────────────────────────────────────────────────

void PlayerUI::render(SDL_Renderer* renderer,
                      double currentPts, double duration, bool paused) {
    // Lazy-init the activity timer so the bar is visible at startup.
    if (m_lastActivityMs == 0) m_lastActivityMs = SDL_GetTicks();

    if (!m_texturesLoaded) { initTextures(renderer); m_texturesLoaded = true; }

    if (paused) {
        // Video redraw while paused is handled by Renderer::renderUI (needs the texture).
        m_uiVisible      = true;
        m_lastActivityMs = SDL_GetTicks();
    }

    // Auto-hide after inactivity while playing.
    uint64_t now = SDL_GetTicks();
    if (!paused && now - m_lastActivityMs > UI_HIDE_MS) {
        if (m_uiVisible) {
            m_uiVisible = false;
            SDL_HideCursor();
        }
    }

    if (!m_uiVisible) return;

    int outW = 0, outH = 0;
    SDL_GetRenderOutputSize(renderer, &outW, &outH);
    const float W = static_cast<float>(outW);
    const float H = static_cast<float>(outH);

    // Scale relative to a 720p reference height.
    const float sc = SDL_clamp(H / UI_REFERENCE_HEIGHT, 0.5f, 3.0f);

    const float barH      = BAR_H       * sc;
    const float pad       = PAD         * sc;
    const float btnW      = BTN_W       * sc;
    const float progH     = PROG_H      * sc;
    const float speedBtnW = SPEED_BTN_W * sc;
    const float muteBtnW  = MUTE_BTN_W  * sc;
    const float volBarW   = VOL_BAR_W   * sc;
    const float timeW     = TIME_W;            // fixed — debug font doesn't scale

    const float barY = H - barH;

    // ── Background bar ──
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_FRect barBg = {0.0f, barY, W, barH};
    SDL_RenderFillRect(renderer, &barBg);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    const float btnCX = pad + btnW * 0.5f;
    const float btnCY = barY + barH * 0.5f;
    const float ic    = 9.0f * sc;
    const float ih    = 12.0f * sc;

    // ── Play / Pause icon ──
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    if (paused) {
        SDL_Vertex tri[3] = {
            {{btnCX - ic,             btnCY - ih}, {255,255,255,255}, {0,0}},
            {{btnCX + ic + 4.0f * sc, btnCY},      {255,255,255,255}, {0,0}},
            {{btnCX - ic,             btnCY + ih}, {255,255,255,255}, {0,0}},
        };
        SDL_RenderGeometry(renderer, nullptr, tri, 3, nullptr, 0);
    } else {
        float bw = 7.0f * sc;
        SDL_FRect lb = {btnCX - ic,                   btnCY - ih, bw, ih * 2.0f};
        SDL_FRect rb = {btnCX - ic + bw + 2.0f * sc,  btnCY - ih, bw, ih * 2.0f};
        SDL_RenderFillRect(renderer, &lb);
        SDL_RenderFillRect(renderer, &rb);
    }
    m_playPauseRect = {pad, barY, btnW, barH};

    // ── Progress bar ──
    float progX = pad + btnW + pad;
    float progW = W - progX - pad - timeW - pad - speedBtnW - muteBtnW - volBarW - pad;
    float progY = barY + (barH - progH) * 0.5f;

    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_FRect progBg = {progX, progY, progW, progH};
    SDL_RenderFillRect(renderer, &progBg);

    float fillW = (duration > 0.0)
        ? static_cast<float>(currentPts / duration) * progW : 0.0f;
    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
    SDL_FRect progFill = {progX, progY, fillW, progH};
    SDL_RenderFillRect(renderer, &progFill);

    float handleW = 10.0f * sc;
    float handleH = progH + 8.0f * sc;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_FRect handle = {progX + fillW - handleW * 0.5f, progY - 4.0f * sc, handleW, handleH};
    SDL_RenderFillRect(renderer, &handle);

    m_progressBarX = progX; m_progressBarW = progW;
    m_progressBarY = barY;  m_progressBarH = barH;

    // ── Time display ──
    float timeX = progX + progW + pad;
    float timeY = barY + (barH - 8.0f) * 0.5f;
    std::string timeStr = formatTime(currentPts) + " / " + formatTime(duration);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, timeX, timeY, timeStr.c_str());

    // ── Speed button ──
    float speedX = timeX + timeW;
    m_speedRect = {speedX, barY, speedBtnW, barH};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 30);
    float speedBgH = 20.0f * sc;
    SDL_FRect speedBg = {speedX + 2.0f * sc, barY + (barH - speedBgH) * 0.5f,
                         speedBtnW - 4.0f * sc, speedBgH};
    SDL_RenderFillRect(renderer, &speedBg);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    const char* label = SPEED_LABELS[m_speedIndex];
    float labelLen = static_cast<float>(SDL_strlen(label)) * 8.0f;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer,
        speedX + (speedBtnW - labelLen) * 0.5f,
        barY + (barH - 8.0f) * 0.5f, label);

    // ── Mute / Speaker icon ──
    float volLabelX = speedX + speedBtnW;
    float volBarX   = volLabelX + muteBtnW + 4.0f * sc;
    float volBarY   = barY + (barH - progH) * 0.5f;

    m_muteRect = {volLabelX, barY, muteBtnW, barH};

    SDL_Texture* iconTex = m_muted ? m_volMutedTex : m_volFullTex;
    if (iconTex) {
        float iconSize = muteBtnW * 0.8f;
        SDL_FRect iconDst = {volLabelX + (muteBtnW - iconSize) * 0.5f,
                             barY      + (barH     - iconSize) * 0.5f,
                             iconSize, iconSize};
        SDL_RenderTexture(renderer, iconTex, nullptr, &iconDst);
    }

    // ── Volume bar ──
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_FRect volBg = {volBarX, volBarY, volBarW, progH};
    SDL_RenderFillRect(renderer, &volBg);

    float volFillW = SDL_clamp(m_volume / VOLUME_MAX, VOLUME_MIN, 1.0f) * volBarW;
    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
    SDL_FRect volFill = {volBarX, volBarY, volFillW, progH};
    SDL_RenderFillRect(renderer, &volFill);

    m_volumeBarX = volBarX; m_volumeBarW = volBarW;
    m_volumeBarY = barY;    m_volumeBarH = barH;
}

// ── Hit testing ──────────────────────────────────────────────────────────────

PlayerEvent PlayerUI::handleMouseClick(float mx, float my) {
    auto hit = [](float x, float y, const SDL_FRect& r) {
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    };
    auto hitXYWH = [](float x, float y, float rx, float ry, float rw, float rh) {
        return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
    };

    if (hit(mx, my, m_playPauseRect))
        return PlayerEvent::TogglePause;

    if (hitXYWH(mx, my, m_progressBarX, m_progressBarY, m_progressBarW, m_progressBarH)) {
        m_seekTarget = std::max(0.0, std::min(1.0,
            static_cast<double>((mx - m_progressBarX) / m_progressBarW)));
        return PlayerEvent::SeekTo;
    }

    if (hit(mx, my, m_speedRect)) {
        cycleSpeed();
        return PlayerEvent::SpeedChange;
    }

    if (hit(mx, my, m_muteRect)) {
        m_muted = !m_muted;
        return PlayerEvent::None;
    }

    if (hitXYWH(mx, my, m_volumeBarX, m_volumeBarY, m_volumeBarW, m_volumeBarH)) {
        m_volume = SDL_clamp((mx - m_volumeBarX) / m_volumeBarW * 2.0f, 0.0f, 2.0f);
        return PlayerEvent::None;
    }

    return PlayerEvent::None;
}
