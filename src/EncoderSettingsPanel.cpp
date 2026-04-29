#include "EncoderSettingsPanel.h"
#include <cstring>

namespace {
    constexpr float PANEL_W    = 400.0f;
    constexpr float PANEL_H    = 226.0f;
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
    constexpr float BTN_ROW_Y  = 186.0f; // y within panel

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

    return Result::None;
}

void EncoderSettingsPanel::handleMouseMotion(float mx, float my) {
    m_mouseX = mx;
    m_mouseY = my;
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
    return s;
}
