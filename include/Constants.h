#pragma once

// ── Audio pipeline ────────────────────────────────────────────────────────────
// Shared between Decoder (resampling target) and Renderer (output spec).

// Common sample rates (Hz)
static constexpr int   SAMPLE_RATE_8K         =   8000; // Telephone
static constexpr int   SAMPLE_RATE_11K        =  11025; // Quarter CD
static constexpr int   SAMPLE_RATE_22K        =  22050; // Half CD
static constexpr int   SAMPLE_RATE_44K        =  44100; // CD
static constexpr int   SAMPLE_RATE_48K        =  48000; // DVD / professional
static constexpr int   SAMPLE_RATE_88K        =  88200; // Double CD
static constexpr int   SAMPLE_RATE_96K        =  96000; // High resolution
static constexpr int   SAMPLE_RATE_192K       = 192000; // Very high resolution

// Common bytes per sample (bit depth)
static constexpr int   BYTES_PER_SAMPLE_8BIT  = 1; //  8-bit
static constexpr int   BYTES_PER_SAMPLE_16BIT = 2; // 16-bit
static constexpr int   BYTES_PER_SAMPLE_24BIT = 3; // 24-bit
static constexpr int   BYTES_PER_SAMPLE_32BIT = 4; // 32-bit

// Common audio channel counts
static constexpr int   CHANNELS_MONO          = 1; // Mono
static constexpr int   CHANNELS_STEREO        = 2; // Stereo
static constexpr int   CHANNELS_QUAD          = 4; // Quadraphonic
static constexpr int   CHANNELS_5_1           = 6; // 5.1 Surround
static constexpr int   CHANNELS_7_1           = 8; // 7.1 Surround

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
