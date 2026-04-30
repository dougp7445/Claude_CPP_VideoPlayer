#include <gtest/gtest.h>
#include "Decoder.h"
#include "Demuxer.h"
#include "TestConstants.h"

// Helper: open a Demuxer + Decoder pair from a file path.
static bool openPair(const std::string& path, Demuxer& demuxer, Decoder& decoder) {
    if (!demuxer.open(path)) { return false; }
    return decoder.open(demuxer.videoCodecParameters(),
                        demuxer.audioCodecParameters(),
                        demuxer.videoTimeBase(),
                        demuxer.audioTimeBase());
}

// Helper: pull packets from the demuxer and feed them to the decoder until a
// frame is produced or the file ends. Caller must free the returned frame.
static bool readFrame(Demuxer& demuxer, Decoder& decoder, DecodedFrame& out) {
    AVPacket* pkt = av_packet_alloc();
    bool gotFrame = false;
    while (!gotFrame) {
        if (!demuxer.readPacket(pkt)) { break; }
        gotFrame = decoder.decode(pkt, demuxer.isVideoPacket(pkt), out);
    }
    av_packet_free(&pkt);
    return gotFrame;
}

// ── Default state (no file opened) ───────────────────────────────────────────

TEST(DecoderTest, DefaultDimensionsAreZero) {
    Decoder d;
    EXPECT_EQ(d.videoWidth(),  0);
    EXPECT_EQ(d.videoHeight(), 0);
}

TEST(DecoderTest, DefaultDurationIsZero) {
    Demuxer demuxer;
    EXPECT_DOUBLE_EQ(demuxer.duration(), 0.0);
}

// ── open() ────────────────────────────────────────────────────────────────────

TEST(DecoderTest, OpenInvalidFileReturnsFalse) {
    Demuxer demuxer;
    EXPECT_FALSE(demuxer.open("nonexistent_file.mp4"));
}

TEST(DecoderTest, OpenValidFileReturnsTrue) {
    Demuxer demuxer;
    Decoder d;
    EXPECT_TRUE(openPair(TEST_VIDEO_PATH, demuxer, d));
}

TEST(DecoderTest, OpenValidFilePopulatesDimensions) {
    Demuxer demuxer;
    Decoder d;
    ASSERT_TRUE(openPair(TEST_VIDEO_PATH, demuxer, d));
    EXPECT_GT(d.videoWidth(),  0);
    EXPECT_GT(d.videoHeight(), 0);
}

TEST(DecoderTest, OpenValidFilePopulatesDuration) {
    Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(TEST_VIDEO_PATH));
    EXPECT_GT(demuxer.duration(), 0.0);
}

// ── readFrame() ───────────────────────────────────────────────────────────────

TEST(DecoderTest, ReadFrameReturnsTrue) {
    Demuxer demuxer;
    Decoder d;
    ASSERT_TRUE(openPair(TEST_VIDEO_PATH, demuxer, d));
    DecodedFrame frame;
    EXPECT_TRUE(readFrame(demuxer, d, frame));
    av_frame_free(&frame.videoFrame);
    av_frame_free(&frame.audioFrame);
}

TEST(DecoderTest, ReadFrameProducesVideoFrame) {
    Demuxer demuxer;
    Decoder d;
    ASSERT_TRUE(openPair(TEST_VIDEO_PATH, demuxer, d));
    bool gotVideo = false;
    for (int i = 0; i < 32 && !gotVideo; ++i) {
        DecodedFrame frame;
        if (!readFrame(demuxer, d, frame)) { break; }
        if (frame.videoFrame) { gotVideo = true; }
        av_frame_free(&frame.videoFrame);
        av_frame_free(&frame.audioFrame);
    }
    EXPECT_TRUE(gotVideo);
}

TEST(DecoderTest, ReadFramePtsIsNonNegative) {
    Demuxer demuxer;
    Decoder d;
    ASSERT_TRUE(openPair(TEST_VIDEO_PATH, demuxer, d));
    DecodedFrame frame;
    ASSERT_TRUE(readFrame(demuxer, d, frame));
    EXPECT_GE(frame.pts, 0.0);
    av_frame_free(&frame.videoFrame);
    av_frame_free(&frame.audioFrame);
}

TEST(DecoderTest, ConsecutiveFramesPtsIncrease) {
    Demuxer demuxer;
    Decoder d;
    ASSERT_TRUE(openPair(TEST_VIDEO_PATH, demuxer, d));
    double prevPts = -1.0;
    for (int i = 0; i < 16; ++i) {
        DecodedFrame frame;
        if (!readFrame(demuxer, d, frame)) { break; }
        EXPECT_GE(frame.pts, prevPts);
        prevPts = frame.pts;
        av_frame_free(&frame.videoFrame);
        av_frame_free(&frame.audioFrame);
    }
}

// ── seek() ────────────────────────────────────────────────────────────────────

TEST(DecoderTest, SeekToBeginningSucceeds) {
    Demuxer demuxer;
    Decoder d;
    ASSERT_TRUE(openPair(TEST_VIDEO_PATH, demuxer, d));
    EXPECT_TRUE(demuxer.seek(0.0));
}

TEST(DecoderTest, SeekToMiddleSucceeds) {
    Demuxer demuxer;
    Decoder d;
    ASSERT_TRUE(openPair(TEST_VIDEO_PATH, demuxer, d));
    EXPECT_TRUE(demuxer.seek(demuxer.duration() / 2.0));
}

TEST(DecoderTest, SeekProducesFramesNearTarget) {
    Demuxer demuxer;
    Decoder d;
    ASSERT_TRUE(openPair(TEST_VIDEO_PATH, demuxer, d));

    double target = demuxer.duration() / 2.0;
    ASSERT_TRUE(demuxer.seek(target));
    d.flushBuffers();

    DecodedFrame frame;
    ASSERT_TRUE(readFrame(demuxer, d, frame));
    EXPECT_NEAR(frame.pts, target, TEST_SEEK_TOLERANCE_S);
    av_frame_free(&frame.videoFrame);
    av_frame_free(&frame.audioFrame);
}

TEST(DecoderTest, SeekAndReadContinuesAfterSeek) {
    Demuxer demuxer;
    Decoder d;
    ASSERT_TRUE(openPair(TEST_VIDEO_PATH, demuxer, d));
    ASSERT_TRUE(demuxer.seek(demuxer.duration() / 4.0));
    d.flushBuffers();

    DecodedFrame frame;
    EXPECT_TRUE(readFrame(demuxer, d, frame));
    av_frame_free(&frame.videoFrame);
    av_frame_free(&frame.audioFrame);
}
