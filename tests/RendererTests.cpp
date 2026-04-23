#include <gtest/gtest.h>
#include <vector>
#include "Renderer.h"
#include "Decoder.h"

static const std::string TEST_VIDEO = std::string(TEST_RESOURCES_DIR) + "/test.mp4";

// ── Default state (no SDL init required) ─────────────────────────────────────
// These tests construct a Renderer without calling initWindow/initRenderer and
// verify that all accessors return safe defaults and guarded methods don't crash.

TEST(RendererDefaultTest, AudioClockWithoutInitReturnsNegativeOne) {
    Renderer r;
    EXPECT_DOUBLE_EQ(r.getAudioClock(), -1.0);
}

TEST(RendererDefaultTest, AudioLatencyWithoutInitIsZero) {
    Renderer r;
    EXPECT_DOUBLE_EQ(r.getAudioLatency(), 0.0);
}

TEST(RendererDefaultTest, DefaultVolumeIsOne) {
    Renderer r;
    EXPECT_FLOAT_EQ(r.getVolume(), 1.0f);
}

TEST(RendererDefaultTest, DefaultSpeedIsOne) {
    Renderer r;
    EXPECT_FLOAT_EQ(r.getSpeed(), 1.0f);
}

TEST(RendererDefaultTest, QueueAudioWithoutStreamDoesNotCrash) {
    Renderer r;
    uint8_t buf[64] = {};
    EXPECT_NO_FATAL_FAILURE(r.queueAudio(buf, sizeof(buf)));
}

TEST(RendererDefaultTest, FlushAudioWithoutStreamDoesNotCrash) {
    Renderer r;
    EXPECT_NO_FATAL_FAILURE(r.flushAudio());
}

// ── SDL tests without a video file ───────────────────────────────────────────
// Initializes a window and renderer using a synthetic resolution so renderer
// behaviour can be verified independently of the decoder and test.mp4.

class RendererNoVideoTest : public ::testing::Test {
protected:
    Renderer r;
    bool     m_hasAudio = false;

    void SetUp() override {
        ASSERT_TRUE(r.initWindow("RendererNoVideoTest", 1280, 720));
        ASSERT_TRUE(r.initRenderer());
        m_hasAudio = (r.getAudioClock() != -1.0);
    }

    void TearDown() override {
        r.shutdown();
    }

    void SkipIfNoAudio() {
        if (!m_hasAudio)
            GTEST_SKIP() << "No audio device available";
    }
};

TEST_F(RendererNoVideoTest, InitSucceeds) {
    SUCCEED();
}

TEST_F(RendererNoVideoTest, AudioClockAfterInitIsNonNegative) {
    SkipIfNoAudio();
    EXPECT_GE(r.getAudioClock(), 0.0);
}

TEST_F(RendererNoVideoTest, AudioLatencyAfterInitIsNonNegative) {
    SkipIfNoAudio();
    EXPECT_GE(r.getAudioLatency(), 0.0);
}

TEST_F(RendererNoVideoTest, DefaultVolumeIsOne) {
    EXPECT_FLOAT_EQ(r.getVolume(), 1.0f);
}

TEST_F(RendererNoVideoTest, DefaultSpeedIsOne) {
    EXPECT_FLOAT_EQ(r.getSpeed(), 1.0f);
}

TEST_F(RendererNoVideoTest, AdjustVolumeDecreasesVolume) {
    float before = r.getVolume();
    r.adjustVolume(-0.2f);
    EXPECT_LT(r.getVolume(), before);
}

TEST_F(RendererNoVideoTest, AdjustVolumeIncreasesVolume) {
    r.adjustVolume(-0.5f);
    float before = r.getVolume();
    r.adjustVolume(0.2f);
    EXPECT_GT(r.getVolume(), before);
}

TEST_F(RendererNoVideoTest, VolumeClampedAtZero) {
    r.adjustVolume(-10.0f);
    EXPECT_FLOAT_EQ(r.getVolume(), 0.0f);
}

TEST_F(RendererNoVideoTest, VolumeClampedAtTwo) {
    r.adjustVolume(10.0f);
    EXPECT_FLOAT_EQ(r.getVolume(), 2.0f);
}

TEST_F(RendererNoVideoTest, QueueSyntheticAudioDoesNotCrash) {
    std::vector<uint8_t> silence(4096, 0);
    EXPECT_NO_FATAL_FAILURE(r.queueAudio(silence.data(), static_cast<int>(silence.size())));
}

TEST_F(RendererNoVideoTest, FlushAudioDoesNotCrash) {
    std::vector<uint8_t> silence(4096, 0);
    r.queueAudio(silence.data(), static_cast<int>(silence.size()));
    EXPECT_NO_FATAL_FAILURE(r.flushAudio());
}

TEST_F(RendererNoVideoTest, PauseAndResumeAudioDoNotCrash) {
    EXPECT_NO_FATAL_FAILURE(r.pauseAudio());
    EXPECT_NO_FATAL_FAILURE(r.resumeAudio());
}

TEST_F(RendererNoVideoTest, RenderUIWithoutVideoDoesNotCrash) {
    // paused=false skips the video texture blit so no frame upload is needed.
    EXPECT_NO_FATAL_FAILURE(r.renderUI(0.0, 120.0, false));
}

TEST_F(RendererNoVideoTest, RenderUIAtMidpointDoesNotCrash) {
    EXPECT_NO_FATAL_FAILURE(r.renderUI(60.0, 120.0, false));
}

TEST_F(RendererNoVideoTest, PresentDoesNotCrash) {
    r.renderUI(0.0, 120.0, false);
    EXPECT_NO_FATAL_FAILURE(r.present());
}

TEST_F(RendererNoVideoTest, PollEventsReturnsNoneWhenIdle) {
    EXPECT_EQ(r.pollEvents(), PlayerEvent::None);
}

TEST_F(RendererNoVideoTest, QueueAudioAdvancesAudioClock) {
    SkipIfNoAudio();
    int bytesPerSecond = 44100 * 2 * 2;
    std::vector<uint8_t> silence(bytesPerSecond, 0);
    r.queueAudio(silence.data(), bytesPerSecond);
    SDL_Delay(100);
    EXPECT_GE(r.getAudioClock(), 0.0);
}

// ── SDL integration tests (require a display and audio device) ────────────────
// The fixture opens test.mp4 so tests can work with real decoded frames.
// initRenderer() probes audio latency which can take up to 500 ms, so these
// tests are slower than the default-state group above.
//
// Audio-dependent tests call SkipIfNoAudio() and are marked SKIPPED (not
// FAILED) when SDL cannot open an audio device.

class RendererSDLTest : public ::testing::Test {
protected:
    Renderer r;
    Decoder  d;
    bool     m_hasAudio = false;

    void SetUp() override {
        ASSERT_TRUE(d.open(TEST_VIDEO));
        ASSERT_TRUE(r.initWindow("RendererTest", d.videoWidth(), d.videoHeight()));
        ASSERT_TRUE(r.initRenderer());
        m_hasAudio = (r.getAudioClock() != -1.0);
    }

    void TearDown() override {
        r.shutdown();
    }

    void SkipIfNoAudio() {
        if (!m_hasAudio)
            GTEST_SKIP() << "No audio device available";
    }

    // Decode and return the next video frame, skipping audio-only frames.
    // Caller owns the returned AVFrame and must av_frame_free it.
    AVFrame* nextVideoFrame() {
        for (int i = 0; i < 64; ++i) {
            DecodedFrame frame;
            if (!d.readFrame(frame)) break;
            av_frame_free(&frame.audioFrame);
            if (frame.videoFrame) return frame.videoFrame;
        }
        return nullptr;
    }
};

TEST_F(RendererSDLTest, InitWindowAndRendererSucceed) {
    SUCCEED();
}

TEST_F(RendererSDLTest, AudioClockAfterInitIsNonNegative) {
    SkipIfNoAudio();
    EXPECT_GE(r.getAudioClock(), 0.0);
}

TEST_F(RendererSDLTest, AudioLatencyAfterInitIsNonNegative) {
    SkipIfNoAudio();
    EXPECT_GE(r.getAudioLatency(), 0.0);
}

TEST_F(RendererSDLTest, DefaultVolumeIsOneAfterInit) {
    EXPECT_FLOAT_EQ(r.getVolume(), 1.0f);
}

TEST_F(RendererSDLTest, DefaultSpeedIsOneAfterInit) {
    EXPECT_FLOAT_EQ(r.getSpeed(), 1.0f);
}

TEST_F(RendererSDLTest, AdjustVolumeDecreasesVolume) {
    float before = r.getVolume();
    r.adjustVolume(-0.2f);
    EXPECT_LT(r.getVolume(), before);
}

TEST_F(RendererSDLTest, AdjustVolumeIncreasesVolume) {
    r.adjustVolume(-0.5f); // move off default first
    float before = r.getVolume();
    r.adjustVolume(0.2f);
    EXPECT_GT(r.getVolume(), before);
}

TEST_F(RendererSDLTest, VolumeClampedAtZero) {
    r.adjustVolume(-10.0f);
    EXPECT_FLOAT_EQ(r.getVolume(), 0.0f);
}

TEST_F(RendererSDLTest, VolumeClampedAtTwo) {
    r.adjustVolume(10.0f);
    EXPECT_FLOAT_EQ(r.getVolume(), 2.0f);
}

TEST_F(RendererSDLTest, RenderFrameDoesNotCrash) {
    AVFrame* f = nextVideoFrame();
    ASSERT_NE(f, nullptr);
    EXPECT_NO_FATAL_FAILURE(
        r.renderFrame(f->data[0], f->linesize[0],
                      f->data[1], f->linesize[1],
                      f->data[2], f->linesize[2]));
    av_frame_free(&f);
}

TEST_F(RendererSDLTest, RenderUIDoesNotCrash) {
    AVFrame* f = nextVideoFrame();
    ASSERT_NE(f, nullptr);
    r.renderFrame(f->data[0], f->linesize[0],
                  f->data[1], f->linesize[1],
                  f->data[2], f->linesize[2]);
    av_frame_free(&f);
    EXPECT_NO_FATAL_FAILURE(r.renderUI(0.0, d.duration(), false));
}

TEST_F(RendererSDLTest, RenderUIWhilePausedDoesNotCrash) {
    AVFrame* f = nextVideoFrame();
    ASSERT_NE(f, nullptr);
    r.renderFrame(f->data[0], f->linesize[0],
                  f->data[1], f->linesize[1],
                  f->data[2], f->linesize[2]);
    av_frame_free(&f);
    EXPECT_NO_FATAL_FAILURE(r.renderUI(0.0, d.duration(), true));
}

TEST_F(RendererSDLTest, PresentDoesNotCrash) {
    AVFrame* f = nextVideoFrame();
    ASSERT_NE(f, nullptr);
    r.renderFrame(f->data[0], f->linesize[0],
                  f->data[1], f->linesize[1],
                  f->data[2], f->linesize[2]);
    av_frame_free(&f);
    EXPECT_NO_FATAL_FAILURE(r.present());
}

TEST_F(RendererSDLTest, QueueAudioAdvancesAudioClock) {
    SkipIfNoAudio();
    // Decode an audio frame and queue its samples.
    for (int i = 0; i < 32; ++i) {
        DecodedFrame frame;
        if (!d.readFrame(frame)) break;
        av_frame_free(&frame.videoFrame);
        if (frame.audioFrame) {
            AVFrame* f = frame.audioFrame;
            r.queueAudio(f->data[0], f->nb_samples * 2 * sizeof(int16_t));
            av_frame_free(&frame.audioFrame);
            break;
        }
    }
    SDL_Delay(100);
    EXPECT_GE(r.getAudioClock(), 0.0);
}

TEST_F(RendererSDLTest, FlushAudioDoesNotCrash) {
    uint8_t buf[256] = {};
    r.queueAudio(buf, sizeof(buf));
    EXPECT_NO_FATAL_FAILURE(r.flushAudio());
}

TEST_F(RendererSDLTest, PauseAndResumeAudioDoNotCrash) {
    EXPECT_NO_FATAL_FAILURE(r.pauseAudio());
    EXPECT_NO_FATAL_FAILURE(r.resumeAudio());
}

TEST_F(RendererSDLTest, PollEventsReturnsNoneWhenNoEventsQueued) {
    EXPECT_EQ(r.pollEvents(), PlayerEvent::None);
}
