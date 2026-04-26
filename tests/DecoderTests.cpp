#include <gtest/gtest.h>
#include "Decoder.h"
#include "TestConstants.h"

// ── Default state (no file opened) ───────────────────────────────────────────

TEST(DecoderTest, DefaultDimensionsAreZero) {
    Decoder d;
    EXPECT_EQ(d.videoWidth(),  0);
    EXPECT_EQ(d.videoHeight(), 0);
}

TEST(DecoderTest, DefaultDurationIsZero) {
    Decoder d;
    EXPECT_DOUBLE_EQ(d.duration(), 0.0);
}

// ── open() ────────────────────────────────────────────────────────────────────

TEST(DecoderTest, OpenInvalidFileReturnsFalse) {
    Decoder d;
    EXPECT_FALSE(d.open("nonexistent_file.mp4"));
}

TEST(DecoderTest, OpenValidFileReturnsTrue) {
    Decoder d;
    EXPECT_TRUE(d.open(TEST_VIDEO_PATH));
}

TEST(DecoderTest, OpenValidFilePopulatesDimensions) {
    Decoder d;
    ASSERT_TRUE(d.open(TEST_VIDEO_PATH));
    EXPECT_GT(d.videoWidth(),  0);
    EXPECT_GT(d.videoHeight(), 0);
}

TEST(DecoderTest, OpenValidFilePopulatesDuration) {
    Decoder d;
    ASSERT_TRUE(d.open(TEST_VIDEO_PATH));
    EXPECT_GT(d.duration(), 0.0);
}

// ── readFrame() ───────────────────────────────────────────────────────────────

TEST(DecoderTest, ReadFrameReturnsTrue) {
    Decoder d;
    ASSERT_TRUE(d.open(TEST_VIDEO_PATH));
    DecodedFrame frame;
    EXPECT_TRUE(d.readFrame(frame));
    av_frame_free(&frame.videoFrame);
    av_frame_free(&frame.audioFrame);
}

TEST(DecoderTest, ReadFrameProducesVideoFrame) {
    Decoder d;
    ASSERT_TRUE(d.open(TEST_VIDEO_PATH));
    // Read until we get a video frame (first few packets may be audio).
    DecodedFrame frame;
    bool gotVideo = false;
    for (int i = 0; i < 32 && !gotVideo; ++i) {
        if (!d.readFrame(frame)) {
            break; 
        }
        if (frame.videoFrame) {
            gotVideo = true; 
        }
        av_frame_free(&frame.videoFrame);
        av_frame_free(&frame.audioFrame);
    }
    EXPECT_TRUE(gotVideo);
}

TEST(DecoderTest, ReadFramePtsIsNonNegative) {
    Decoder d;
    ASSERT_TRUE(d.open(TEST_VIDEO_PATH));
    DecodedFrame frame;
    ASSERT_TRUE(d.readFrame(frame));
    EXPECT_GE(frame.pts, 0.0);
    av_frame_free(&frame.videoFrame);
    av_frame_free(&frame.audioFrame);
}

TEST(DecoderTest, ConsecutiveFramesPtsIncrease) {
    Decoder d;
    ASSERT_TRUE(d.open(TEST_VIDEO_PATH));

    // Collect a few PTS values and verify they are non-decreasing.
    double prevPts = -1.0;
    for (int i = 0; i < 16; ++i) {
        DecodedFrame frame;
        if (!d.readFrame(frame)) {
            break;
        }
        EXPECT_GE(frame.pts, prevPts);
        prevPts = frame.pts;
        av_frame_free(&frame.videoFrame);
        av_frame_free(&frame.audioFrame);
    }
}

// ── seek() ────────────────────────────────────────────────────────────────────

TEST(DecoderTest, SeekToBeginningSucceeds) {
    Decoder d;
    ASSERT_TRUE(d.open(TEST_VIDEO_PATH));
    EXPECT_TRUE(d.seek(0.0));
}

TEST(DecoderTest, SeekToMiddleSucceeds) {
    Decoder d;
    ASSERT_TRUE(d.open(TEST_VIDEO_PATH));
    EXPECT_TRUE(d.seek(d.duration() / 2.0));
}

TEST(DecoderTest, SeekProducesFramesNearTarget) {
    Decoder d;
    ASSERT_TRUE(d.open(TEST_VIDEO_PATH));

    double target = d.duration() / 2.0;
    ASSERT_TRUE(d.seek(target));

    // After seeking, the first decoded frame should be within 5 seconds of the
    // target (AVSEEK_FLAG_BACKWARD may land on the prior keyframe).
    DecodedFrame frame;
    ASSERT_TRUE(d.readFrame(frame));
    EXPECT_NEAR(frame.pts, target, TEST_SEEK_TOLERANCE_S);
    av_frame_free(&frame.videoFrame);
    av_frame_free(&frame.audioFrame);
}

TEST(DecoderTest, SeekAndReadContinuesAfterSeek) {
    Decoder d;
    ASSERT_TRUE(d.open(TEST_VIDEO_PATH));
    ASSERT_TRUE(d.seek(d.duration() / 4.0));

    DecodedFrame frame;
    EXPECT_TRUE(d.readFrame(frame));
    av_frame_free(&frame.videoFrame);
    av_frame_free(&frame.audioFrame);
}
