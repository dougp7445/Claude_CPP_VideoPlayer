#include "EncoderSettingsPanel.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "nanosvg.h"
#include "nanosvgrast.h"

static SDL_Texture* loadFolderIconTex(SDL_Renderer* renderer) {
    const char* base = SDL_GetBasePath();
    std::string path = (base ? std::string(base) : "") + "icons/folder.svg";

    NSVGimage* image = nsvgParseFromFile(path.c_str(), "px", 96.0f);
    if (!image) { return nullptr; }

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    if (!rast) { nsvgDelete(image); return nullptr; }

    constexpr int SIZE = 18;
    std::vector<uint8_t> pixels(SIZE * SIZE * 4, 0);
    float scale = static_cast<float>(SIZE) / std::max(image->width, image->height);
    nsvgRasterize(rast, image, 0.0f, 0.0f, scale, pixels.data(), SIZE, SIZE, SIZE * 4);
    nsvgDeleteRasterizer(rast);
    nsvgDelete(image);

    for (size_t i = 0; i < pixels.size(); i += 4) {
        if (pixels[i + 3] > 0) {
            pixels[i] = pixels[i + 1] = pixels[i + 2] = 255;
        }
    }

    SDL_Surface* surf = SDL_CreateSurfaceFrom(SIZE, SIZE, SDL_PIXELFORMAT_RGBA32,
                                              pixels.data(), SIZE * 4);
    if (!surf) { return nullptr; }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    if (tex) { SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND); }
    return tex;
}

namespace {
    constexpr float PANEL_W    = 400.0f;
    constexpr float PANEL_H    = 298.0f;
    constexpr float TITLE_H    = 30.0f;
    constexpr float ROW_H      = 36.0f;
    constexpr float ARROW_W    = 20.0f;
    constexpr float ARROW_H    = 22.0f;
    constexpr float ARROW_L_X  = 155.0f;
    constexpr float ARROW_R_X  = PANEL_W - 15.0f - ARROW_W;
    constexpr float VAL_MID_X  = (ARROW_L_X + ARROW_W + ARROW_R_X) * 0.5f;
    constexpr float LABEL_X    =  10.0f;
    constexpr float BTN_W      = 100.0f;
    constexpr float BTN_H      =  26.0f;
    constexpr float BTN_ROW_Y  = 258.0f;

    const char* CODEC_NAMES[]     = {"H264", "MPEG4"};
    constexpr int CODEC_COUNT     = 2;

    const char* FORMAT_NAMES[]    = {"MP4", "TS"};
    constexpr int FORMAT_COUNT    = 2;

    const int   VIDEO_BITRATES[]  = {500, 1000, 2000, 4000};
    constexpr int VIDEO_BR_COUNT  = 4;
    const char* VIDEO_BR_LABELS[] = {"500 kbps", "1000 kbps", "2000 kbps", "4000 kbps"};

    const int   AUDIO_BITRATES[]  = {64, 128, 192};
    constexpr int AUDIO_BR_COUNT  = 3;
    const char* AUDIO_BR_LABELS[] = {"64 kbps", "128 kbps", "192 kbps"};

    // Range slider geometry
    constexpr float SLIDER_H       =  4.0f;
    constexpr float THUMB_W        = 10.0f;
    constexpr float RNG_TRACK_X    = 120.0f;
    constexpr float RNG_TRACK_W    = 170.0f;
}

static bool hitTest(float mx, float my, const SDL_FRect& r) {
    return mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h;
}

static void renderRow(SDL_Renderer* renderer,
                      float px, float rowTopY,
                      const char* label, const char* value,
                      SDL_FRect& prevRect, SDL_FRect& nextRect,
                      float mouseX, float mouseY) {
    float midY = rowTopY + (ROW_H - 8.0f) * 0.5f;
    float arrY = rowTopY + (ROW_H - ARROW_H) * 0.5f;

    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    SDL_RenderDebugText(renderer, px + LABEL_X, midY, label);

    prevRect = {px + ARROW_L_X, arrY, ARROW_W, ARROW_H};
    nextRect = {px + ARROW_R_X, arrY, ARROW_W, ARROW_H};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (hitTest(mouseX, mouseY, prevRect)) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 40);
        SDL_RenderFillRect(renderer, &prevRect);
    }
    if (hitTest(mouseX, mouseY, nextRect)) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 40);
        SDL_RenderFillRect(renderer, &nextRect);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, prevRect.x + (ARROW_W - 8.0f) * 0.5f, midY, "<");
    SDL_RenderDebugText(renderer, nextRect.x + (ARROW_W - 8.0f) * 0.5f, midY, ">");

    size_t len = strlen(value);
    float valX = px + VAL_MID_X - static_cast<float>(len) * 4.0f;
    SDL_RenderDebugText(renderer, valX, midY, value);
}

void EncoderSettingsPanel::render(SDL_Renderer* renderer, float W, float H) {
    float px = (W - PANEL_W) * 0.5f;
    float py = (H - PANEL_H) * 0.5f;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
    SDL_FRect overlay = {0.0f, 0.0f, W, H};
    SDL_RenderFillRect(renderer, &overlay);

    SDL_SetRenderDrawColor(renderer, 28, 28, 28, 240);
    SDL_FRect panel = {px, py, PANEL_W, PANEL_H};
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 90, 90, 90, 200);
    SDL_RenderRect(renderer, &panel);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    const char* title = "Export Settings";
    size_t titleLen = strlen(title);
    float titleX = px + (PANEL_W - static_cast<float>(titleLen) * 8.0f) * 0.5f;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, titleX, py + (TITLE_H - 8.0f) * 0.5f, title);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 90, 90, 90, 200);
    SDL_FRect sep = {px, py + TITLE_H - 1.0f, PANEL_W, 1.0f};
    SDL_RenderFillRect(renderer, &sep);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    renderRow(renderer, px, py + TITLE_H + 0.0f * ROW_H,
              "Video Codec:", CODEC_NAMES[m_codecIdx],
              m_codecPrevRect, m_codecNextRect, m_mouseX, m_mouseY);

    renderRow(renderer, px, py + TITLE_H + 1.0f * ROW_H,
              "Output Format:", FORMAT_NAMES[m_fmtIdx],
              m_fmtPrevRect, m_fmtNextRect, m_mouseX, m_mouseY);

    renderRow(renderer, px, py + TITLE_H + 2.0f * ROW_H,
              "Video Bitrate:", VIDEO_BR_LABELS[m_videoBrIdx],
              m_videoBrPrevRect, m_videoBrNextRect, m_mouseX, m_mouseY);

    renderRow(renderer, px, py + TITLE_H + 3.0f * ROW_H,
              "Audio Bitrate:", AUDIO_BR_LABELS[m_audioBrIdx],
              m_audioBrPrevRect, m_audioBrNextRect, m_mouseX, m_mouseY);

    // Combined range slider (start + end)
    {
        float rowTopY = py + TITLE_H + 4.0f * ROW_H;
        float midY    = rowTopY + (ROW_H - 8.0f) * 0.5f;
        float trackY  = rowTopY + (ROW_H - SLIDER_H) * 0.5f;
        float thumbY  = rowTopY + (ROW_H - ARROW_H)  * 0.5f;

        float dur    = static_cast<float>(m_videoDuration);
        float startT = (dur > 0.0f) ? std::min(1.0f, m_startValue / dur) : 0.0f;
        float endT   = (dur > 0.0f) ? std::min(1.0f, m_endValue   / dur) : 1.0f;

        m_rangeTrack = {px + RNG_TRACK_X,                                    trackY, RNG_TRACK_W, SLIDER_H};
        m_startThumb = {px + RNG_TRACK_X + startT * RNG_TRACK_W - THUMB_W * 0.5f, thumbY, THUMB_W, ARROW_H};
        m_endThumb   = {px + RNG_TRACK_X + endT   * RNG_TRACK_W - THUMB_W * 0.5f, thumbY, THUMB_W, ARROW_H};

        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
        SDL_RenderDebugText(renderer, px + LABEL_X, midY, "Range:");

        // Track background
        SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
        SDL_RenderFillRect(renderer, &m_rangeTrack);

        // Filled region between thumbs
        SDL_FRect fill = {
            px + RNG_TRACK_X + startT * RNG_TRACK_W,
            trackY,
            (endT - startT) * RNG_TRACK_W,
            SLIDER_H
        };
        SDL_SetRenderDrawColor(renderer, 30, 100, 180, 255);
        SDL_RenderFillRect(renderer, &fill);

        // Thumbs
        bool startHov = hitTest(m_mouseX, m_mouseY, m_startThumb);
        bool endHov   = hitTest(m_mouseX, m_mouseY, m_endThumb);
        uint8_t sc = startHov ? 255 : 200;
        uint8_t ec = endHov   ? 255 : 200;
        SDL_SetRenderDrawColor(renderer, sc, sc, sc, 255);
        SDL_RenderFillRect(renderer, &m_startThumb);
        SDL_SetRenderDrawColor(renderer, ec, ec, ec, 255);
        SDL_RenderFillRect(renderer, &m_endThumb);

        // Time labels: start right-aligned before track, end left-aligned after track
        char startBuf[8], endBuf[8];
        {
            int s = static_cast<int>(m_startValue + 0.5f);
            snprintf(startBuf, sizeof(startBuf), "%d:%02d", s / 60, s % 60);
        }
        {
            int s = static_cast<int>(m_endValue + 0.5f);
            snprintf(endBuf, sizeof(endBuf), "%d:%02d", s / 60, s % 60);
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        float startTextX = px + RNG_TRACK_X - static_cast<float>(strlen(startBuf)) * 8.0f - 4.0f;
        SDL_RenderDebugText(renderer, startTextX, midY, startBuf);
        SDL_RenderDebugText(renderer, px + RNG_TRACK_X + RNG_TRACK_W + 4.0f, midY, endBuf);
    }

    // Output folder row
    {
        constexpr float FIELD_X      = 70.0f;
        constexpr float BROWSE_BTN_W = 26.0f;
        constexpr float FIELD_W      = PANEL_W - FIELD_X - BROWSE_BTN_W - 14.0f;

        float rowTopY = py + TITLE_H + 5.0f * ROW_H;
        float midY    = rowTopY + (ROW_H - 8.0f)   * 0.5f;
        float fieldY  = rowTopY + (ROW_H - ARROW_H) * 0.5f;

        m_fileFieldRect = {px + FIELD_X,                        fieldY, FIELD_W,      ARROW_H};
        m_browseRect      = {px + FIELD_X + FIELD_W + 4.0f,       fieldY, BROWSE_BTN_W, ARROW_H};

        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
        SDL_RenderDebugText(renderer, px + LABEL_X, midY, "Output:");

        // Text field
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 240);
        SDL_RenderFillRect(renderer, &m_fileFieldRect);
        SDL_SetRenderDrawColor(renderer,
            m_fileFocused ? 150 : 80,
            m_fileFocused ? 150 : 80,
            m_fileFocused ? 220 : 80, 255);
        SDL_RenderRect(renderer, &m_fileFieldRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        constexpr float TEXT_PAD = 4.0f;
        int maxChars = static_cast<int>((FIELD_W - TEXT_PAD * 2.0f) / 8.0f);
        std::string display = m_filePath;
        if (m_fileFocused) { display += '|'; }
        if (static_cast<int>(display.size()) > maxChars) {
            display = display.substr(display.size() - maxChars);
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(renderer, px + FIELD_X + TEXT_PAD, midY, display.c_str());

        // Browse button
        if (!m_browseIconTex) { m_browseIconTex = loadFolderIconTex(renderer); }
        bool browseHov = hitTest(m_mouseX, m_mouseY, m_browseRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, browseHov ? 220 : 160);
        SDL_RenderFillRect(renderer, &m_browseRect);
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 200);
        SDL_RenderRect(renderer, &m_browseRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        if (m_browseIconTex) {
            constexpr float ICON_SIZE = 16.0f;
            SDL_FRect iconDst = {
                m_browseRect.x + (BROWSE_BTN_W - ICON_SIZE) * 0.5f,
                m_browseRect.y + (ARROW_H       - ICON_SIZE) * 0.5f,
                ICON_SIZE, ICON_SIZE
            };
            SDL_RenderTexture(renderer, m_browseIconTex, nullptr, &iconDst);
        }
    }

    float btnY = py + BTN_ROW_Y;
    m_cancelRect = {px + 20.0f,                   btnY, BTN_W, BTN_H};
    m_exportRect = {px + PANEL_W - 20.0f - BTN_W, btnY, BTN_W, BTN_H};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    auto btnColor = [&](const SDL_FRect& r, uint8_t rr, uint8_t gg, uint8_t bb) {
        bool hov = hitTest(m_mouseX, m_mouseY, r);
        SDL_SetRenderDrawColor(renderer, rr, gg, bb, hov ? 220 : 160);
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 200);
        SDL_RenderRect(renderer, &r);
    };
    btnColor(m_cancelRect, 60,  60,  60);
    btnColor(m_exportRect, 30,  90, 160);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer,
        m_cancelRect.x + (BTN_W - 6.0f * 8.0f) * 0.5f,
        btnY + (BTN_H - 8.0f) * 0.5f, "Cancel");
    SDL_RenderDebugText(renderer,
        m_exportRect.x + (BTN_W - 6.0f * 8.0f) * 0.5f,
        btnY + (BTN_H - 8.0f) * 0.5f, "Export");
}

EncoderSettingsPanel::Result EncoderSettingsPanel::handleMouseClick(float mx, float my) {
    if (hitTest(mx, my, m_cancelRect)) { return Result::Cancel; }
    if (hitTest(mx, my, m_exportRect)) { return Result::Export; }

    if (hitTest(mx, my, m_codecPrevRect)) {
        m_codecIdx = (m_codecIdx - 1 + CODEC_COUNT) % CODEC_COUNT;
    } else if (hitTest(mx, my, m_codecNextRect)) {
        m_codecIdx = (m_codecIdx + 1) % CODEC_COUNT;
    }

    if (hitTest(mx, my, m_fmtPrevRect)) {
        m_fmtIdx = (m_fmtIdx - 1 + FORMAT_COUNT) % FORMAT_COUNT;
    } else if (hitTest(mx, my, m_fmtNextRect)) {
        m_fmtIdx = (m_fmtIdx + 1) % FORMAT_COUNT;
    }

    if (hitTest(mx, my, m_videoBrPrevRect)) {
        m_videoBrIdx = (m_videoBrIdx - 1 + VIDEO_BR_COUNT) % VIDEO_BR_COUNT;
    } else if (hitTest(mx, my, m_videoBrNextRect)) {
        m_videoBrIdx = (m_videoBrIdx + 1) % VIDEO_BR_COUNT;
    }

    if (hitTest(mx, my, m_audioBrPrevRect)) {
        m_audioBrIdx = (m_audioBrIdx - 1 + AUDIO_BR_COUNT) % AUDIO_BR_COUNT;
    } else if (hitTest(mx, my, m_audioBrNextRect)) {
        m_audioBrIdx = (m_audioBrIdx + 1) % AUDIO_BR_COUNT;
    }

    if (m_videoDuration > 0.0 && m_rangeTrack.w > 0.0f) {
        SDL_FRect hitArea = {m_rangeTrack.x, m_startThumb.y, m_rangeTrack.w, m_startThumb.h};
        if (hitTest(mx, my, hitArea)) {
            float t      = std::max(0.0f, std::min(1.0f, (mx - m_rangeTrack.x) / m_rangeTrack.w));
            float newVal = t * static_cast<float>(m_videoDuration);
            float startCx = m_startThumb.x + m_startThumb.w * 0.5f;
            float endCx   = m_endThumb.x   + m_endThumb.w   * 0.5f;
            if (std::abs(mx - startCx) <= std::abs(mx - endCx)) {
                m_startValue    = std::min(newVal, m_endValue);
                m_draggingStart = true;
            } else {
                m_endValue    = std::max(newVal, m_startValue);
                m_draggingEnd = true;
            }
        }
    }

    if (hitTest(mx, my, m_browseRect)) { return Result::BrowseFile; }
    m_fileFocused = hitTest(mx, my, m_fileFieldRect);
    return Result::None;
}

void EncoderSettingsPanel::handleMouseMotion(float mx, float my) {
    m_mouseX = mx;
    m_mouseY = my;
    if (m_draggingStart && m_videoDuration > 0.0 && m_rangeTrack.w > 0.0f) {
        float t      = std::max(0.0f, std::min(1.0f, (mx - m_rangeTrack.x) / m_rangeTrack.w));
        m_startValue = std::min(t * static_cast<float>(m_videoDuration), m_endValue);
    }
    if (m_draggingEnd && m_videoDuration > 0.0 && m_rangeTrack.w > 0.0f) {
        float t    = std::max(0.0f, std::min(1.0f, (mx - m_rangeTrack.x) / m_rangeTrack.w));
        m_endValue = std::max(t * static_cast<float>(m_videoDuration), m_startValue);
    }
}

void EncoderSettingsPanel::handleMouseButtonUp(float /*mx*/, float /*my*/) {
    m_draggingStart = false;
    m_draggingEnd   = false;
}

EncoderSettingsPanel::~EncoderSettingsPanel() {
    if (m_browseIconTex) { SDL_DestroyTexture(m_browseIconTex); }
}

void EncoderSettingsPanel::setVideoDuration(double d) {
    m_videoDuration = d;
    m_startValue    = 0.0f;
    m_endValue      = static_cast<float>(d);
}

void EncoderSettingsPanel::setOutputFilePath(const std::string& filePath) {
    m_filePath = filePath;
}

void EncoderSettingsPanel::handleTextInput(const char* text) {
    if (m_fileFocused) {
        m_filePath += text;
    }
}

void EncoderSettingsPanel::handleKeyDown(SDL_Keycode key) {
    if (!m_fileFocused) { return; }
    if (key == SDLK_BACKSPACE && !m_filePath.empty()) {
        m_filePath.pop_back();
    }
}

EncoderSettings EncoderSettingsPanel::getSettings() const {
    EncoderSettings s;
    s.videoCodec = (m_codecIdx == 0)
        ? EncoderSettings::VideoCodec::H264
        : EncoderSettings::VideoCodec::MPEG4;
    s.outputFormat = (m_fmtIdx == 0)
        ? EncoderSettings::OutputFormat::MP4
        : EncoderSettings::OutputFormat::TS;
    s.videoBitRateKbps = VIDEO_BITRATES[m_videoBrIdx];
    s.audioBitRateKbps = AUDIO_BITRATES[m_audioBrIdx];
    s.exportStartTime  = m_startValue;
    bool isFull        = m_videoDuration <= 0.0 ||
                         m_endValue >= static_cast<float>(m_videoDuration) - 0.5f;
    s.exportDuration   = isFull ? 0.0f : (m_endValue - m_startValue);
    s.outputFilePath   = m_filePath;
    return s;
}
