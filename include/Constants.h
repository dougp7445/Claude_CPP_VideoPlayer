#pragma once

// ── Audio pipeline ────────────────────────────────────────────────────────────
// Shared between Decoder (resampling target) and Renderer (output spec).
static constexpr int   AUDIO_SAMPLE_RATE      = 44100;
static constexpr int   AUDIO_CHANNELS         = 2;
static constexpr int   AUDIO_BYTES_PER_SAMPLE = 2;  // 16-bit PCM (int16_t)

// ── Volume ────────────────────────────────────────────────────────────────────
static constexpr float VOLUME_MIN             = 0.0f;
static constexpr float VOLUME_MAX             = 2.0f;  // 2× allows amplification above unity
static constexpr float VOLUME_KEY_DELTA       = 0.1f;  // per up/down keypress
static constexpr float VOLUME_WHEEL_DELTA     = 0.05f; // per mouse-wheel tick

// ── Playback ──────────────────────────────────────────────────────────────────
static constexpr double SEEK_STEP_SECONDS     = 10.0;

// ── Display ───────────────────────────────────────────────────────────────────
// Initial window is scaled down so it fits within this fraction of the display.
static constexpr float WINDOW_MAX_DISPLAY_FRACTION = 0.85f;

// Reference height used to scale all UI elements proportionally.
// At this height the layout constants in PlayerUI.cpp are 1:1 pixels.
static constexpr float UI_REFERENCE_HEIGHT = 720.0f;

// ── Standard video resolutions ────────────────────────────────────────────────
static constexpr int RES_480P_WIDTH  =  854; static constexpr int RES_480P_HEIGHT  =  480; // SD
static constexpr int RES_720P_WIDTH  = 1280; static constexpr int RES_720P_HEIGHT  =  720; // HD
static constexpr int RES_1080P_WIDTH = 1920; static constexpr int RES_1080P_HEIGHT = 1080; // FHD
static constexpr int RES_1440P_WIDTH = 2560; static constexpr int RES_1440P_HEIGHT = 1440; // QHD
static constexpr int RES_4K_WIDTH    = 3840; static constexpr int RES_4K_HEIGHT    = 2160; // UHD
