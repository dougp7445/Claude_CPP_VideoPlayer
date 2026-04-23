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

// ── Common video bitrates (bits per second) ───────────────────────────────────
// Organized by resolution tier with low / medium / high quality levels.
// Values match typical streaming and encoding targets (e.g. YouTube, Netflix).
static constexpr int BITRATE_480P_LOW    = 500000; //  0.5 Mbps
static constexpr int BITRATE_480P_MED    = 1000000; //  1.0 Mbps
static constexpr int BITRATE_480P_HIGH   = 2000000; //  2.0 Mbps

static constexpr int BITRATE_720P_LOW    = 1500000; //  1.5 Mbps
static constexpr int BITRATE_720P_MED    = 2500000; //  2.5 Mbps
static constexpr int BITRATE_720P_HIGH   = 4000000; //  4.0 Mbps

static constexpr int BITRATE_1080P_LOW   = 3000000; //  3.0 Mbps
static constexpr int BITRATE_1080P_MED   = 5000000; //  5.0 Mbps
static constexpr int BITRATE_1080P_HIGH  = 8000000; //  8.0 Mbps

static constexpr int BITRATE_1440P_LOW   =  6000000; //  6.0 Mbps
static constexpr int BITRATE_1440P_MED   = 10000000; // 10.0 Mbps
static constexpr int BITRATE_1440P_HIGH  = 16000000; // 16.0 Mbps

static constexpr int BITRATE_4K_LOW      = 15000000; // 15.0 Mbps
static constexpr int BITRATE_4K_MED      = 25000000; // 25.0 Mbps
static constexpr int BITRATE_4K_HIGH     = 45000000; // 45.0 Mbps
