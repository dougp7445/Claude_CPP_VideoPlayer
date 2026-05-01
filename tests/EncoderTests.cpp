#include "Encoder.h"
#include "Muxer.h"
#include "LockingQueue.h"
#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include <string>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
}

namespace fs = std::filesystem;

class EncoderTest : public ::testing::Test {
protected:
    fs::path    testDir;
    std::string outPath;

    void SetUp() override {
        testDir = fs::temp_directory_path() / "VideoPlayerEncoderTests";
        fs::remove_all(testDir);
        fs::create_directories(testDir);
        outPath = (testDir / "out.mp4").string();
    }

    void TearDown() override {
        fs::remove_all(testDir);
    }

    // Open a muxer+encoder pair ready for writing frames.
    // On success muxer.beginFile() has been called and the async write thread
    // is running; caller must call encoder.close() then muxer.endFile() when done.
    bool openPair(Muxer& muxer, Encoder& enc, LockingQueue<AVPacket*>& queue,
                  int w = 320, int h = 240,
                  int sampleRate = 44100, int channels = 2) {
        return muxer.open(outPath) &&
               enc.open(muxer, queue, w, h, {1, 12800}, sampleRate, channels) &&
               muxer.beginFile() &&
               muxer.startAsync(queue);
    }

    AVFrame* makeSyntheticVideoFrame(int width, int height, int64_t pts = 0) {
        AVFrame* frame = av_frame_alloc();
        frame->format = AV_PIX_FMT_YUV420P;
        frame->width  = width;
        frame->height = height;
        frame->pts    = pts;
        av_frame_get_buffer(frame, 0);
        av_frame_make_writable(frame);
        memset(frame->data[0], 0,   frame->linesize[0] * height);
        memset(frame->data[1], 128, frame->linesize[1] * height / 2);
        memset(frame->data[2], 128, frame->linesize[2] * height / 2);
        return frame;
    }

    AVFrame* makeSyntheticAudioFrame(int nbSamples = 1024, int sampleRate = 44100) {
        AVFrame* frame = av_frame_alloc();
        frame->format      = AV_SAMPLE_FMT_S16;
        frame->sample_rate = sampleRate;
        frame->nb_samples  = nbSamples;
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        av_channel_layout_copy(&frame->ch_layout, &stereo);
        av_frame_get_buffer(frame, 0);
        av_frame_make_writable(frame);
        memset(frame->data[0], 0, frame->linesize[0]);
        return frame;
    }
};

TEST_F(EncoderTest, IsNotOpenByDefault) {
    Encoder enc;
    EXPECT_FALSE(enc.isOpen());
}

TEST_F(EncoderTest, OpenCreatesOutputFile) {
    LockingQueue<AVPacket*> queue;
    Muxer muxer;
    Encoder enc;
    ASSERT_TRUE(openPair(muxer, enc, queue));
    enc.close();
    muxer.endFile();
    EXPECT_TRUE(fs::exists(outPath));
}

TEST_F(EncoderTest, IsOpenAfterOpen) {
    LockingQueue<AVPacket*> queue;
    Muxer muxer;
    Encoder enc;
    ASSERT_TRUE(openPair(muxer, enc, queue));
    EXPECT_TRUE(enc.isOpen());
    enc.close();
    muxer.endFile();
}

TEST_F(EncoderTest, IsClosedAfterClose) {
    LockingQueue<AVPacket*> queue;
    Muxer muxer;
    Encoder enc;
    ASSERT_TRUE(openPair(muxer, enc, queue));
    enc.close();
    muxer.endFile();
    EXPECT_FALSE(enc.isOpen());
}

TEST_F(EncoderTest, CloseOnUnopenedReturnsFalse) {
    Encoder enc;
    EXPECT_FALSE(enc.close());
}

TEST_F(EncoderTest, OpenWithBadPathReturnsFalse) {
    LockingQueue<AVPacket*> queue;
    Muxer muxer;
    Encoder enc;
    std::string bad = (testDir / "nonexistent" / "out.mp4").string();
    ASSERT_TRUE(muxer.open(bad));
    ASSERT_TRUE(enc.open(muxer, queue, 320, 240, {1, 12800}));
    EXPECT_FALSE(muxer.beginFile());
}

TEST_F(EncoderTest, WriteVideoFrameDoesNotCrash) {
    LockingQueue<AVPacket*> queue;
    Muxer muxer;
    Encoder enc;
    ASSERT_TRUE(openPair(muxer, enc, queue));
    AVFrame* frame = makeSyntheticVideoFrame(320, 240, 0);
    EXPECT_TRUE(enc.writeVideoFrame(frame));
    av_frame_free(&frame);
    enc.close();
    muxer.endFile();
}

TEST_F(EncoderTest, WriteMultipleVideoFramesDoesNotCrash) {
    LockingQueue<AVPacket*> queue;
    Muxer muxer;
    Encoder enc;
    ASSERT_TRUE(openPair(muxer, enc, queue));
    for (int i = 0; i < 5; ++i) {
        AVFrame* frame = makeSyntheticVideoFrame(320, 240, static_cast<int64_t>(i) * 3000);
        EXPECT_TRUE(enc.writeVideoFrame(frame));
        av_frame_free(&frame);
    }
    enc.close();
    muxer.endFile();
}

TEST_F(EncoderTest, WriteAudioFrameDoesNotCrash) {
    LockingQueue<AVPacket*> queue;
    Muxer muxer;
    Encoder enc;
    ASSERT_TRUE(openPair(muxer, enc, queue));
    AVFrame* frame = makeSyntheticAudioFrame();
    EXPECT_TRUE(enc.writeAudioFrame(frame));
    av_frame_free(&frame);
    enc.close();
    muxer.endFile();
}

TEST_F(EncoderTest, VideoOnlyOpenSucceeds) {
    LockingQueue<AVPacket*> queue;
    Muxer muxer;
    Encoder enc;
    EXPECT_TRUE(openPair(muxer, enc, queue, 320, 240, 0, 0));
    enc.close();
    muxer.endFile();
}

TEST_F(EncoderTest, OutputFileHasNonZeroSize) {
    LockingQueue<AVPacket*> queue;
    Muxer muxer;
    Encoder enc;
    ASSERT_TRUE(openPair(muxer, enc, queue));
    for (int i = 0; i < 10; ++i) {
        AVFrame* vf = makeSyntheticVideoFrame(320, 240, static_cast<int64_t>(i) * 3000);
        enc.writeVideoFrame(vf);
        av_frame_free(&vf);
        AVFrame* af = makeSyntheticAudioFrame();
        enc.writeAudioFrame(af);
        av_frame_free(&af);
    }
    enc.close();
    muxer.endFile();
    EXPECT_GT(fs::file_size(outPath), static_cast<uintmax_t>(0));
}
