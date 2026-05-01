#include "AudioEncoder.h"
#include "Muxer.h"
#include "VideoEncoder.h"
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

    // Open a muxer + video + audio pipeline ready for writing frames.
    // On success muxer.beginFile() has been called and the async write threads
    // are running. Caller must call video.close(), audio.close(), then
    // muxer.endFile() when done.
    bool openAVPair(Muxer& muxer, VideoEncoder& video, AudioEncoder& audio,
                    int w = 320, int h = 240,
                    int sampleRate = 44100, int channels = 2) {
        if (!muxer.open(outPath)) { return false; }
        if (!video.open(w, h, {1, 12800}, 2000, true,
                        muxer.needsGlobalHeader())) { return false; }
        if (!audio.open(sampleRate, channels, 128,
                        muxer.needsGlobalHeader())) { return false; }
        video.setStream(muxer.addStream(video.codecContext()));
        audio.setStream(muxer.addStream(audio.codecContext()));
        return muxer.beginFile() &&
               muxer.startAsync(video.outputQueue(), audio.outputQueue());
    }

    // Open a video-only pipeline.
    bool openVideoPair(Muxer& muxer, VideoEncoder& video,
                       int w = 320, int h = 240) {
        if (!muxer.open(outPath)) { return false; }
        if (!video.open(w, h, {1, 12800}, 2000, true,
                        muxer.needsGlobalHeader())) { return false; }
        video.setStream(muxer.addStream(video.codecContext()));
        return muxer.beginFile() && muxer.startAsync(video.outputQueue());
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
    VideoEncoder video;
    EXPECT_FALSE(video.isOpen());
    AudioEncoder audio;
    EXPECT_FALSE(audio.isOpen());
}

TEST_F(EncoderTest, OpenCreatesOutputFile) {
    Muxer muxer;
    VideoEncoder video;
    AudioEncoder audio;
    ASSERT_TRUE(openAVPair(muxer, video, audio));
    video.close();
    audio.close();
    muxer.endFile();
    EXPECT_TRUE(fs::exists(outPath));
}

TEST_F(EncoderTest, IsOpenAfterOpen) {
    Muxer muxer;
    VideoEncoder video;
    AudioEncoder audio;
    ASSERT_TRUE(openAVPair(muxer, video, audio));
    EXPECT_TRUE(video.isOpen());
    EXPECT_TRUE(audio.isOpen());
    video.close();
    audio.close();
    muxer.endFile();
}

TEST_F(EncoderTest, IsClosedAfterClose) {
    Muxer muxer;
    VideoEncoder video;
    AudioEncoder audio;
    ASSERT_TRUE(openAVPair(muxer, video, audio));
    video.close();
    audio.close();
    muxer.endFile();
    EXPECT_FALSE(video.isOpen());
    EXPECT_FALSE(audio.isOpen());
}

TEST_F(EncoderTest, CloseOnUnopenedReturnsFalse) {
    VideoEncoder video;
    EXPECT_FALSE(video.close());
    AudioEncoder audio;
    EXPECT_FALSE(audio.close());
}

TEST_F(EncoderTest, OpenWithBadPathReturnsFalse) {
    Muxer muxer;
    VideoEncoder video;
    AudioEncoder audio;
    std::string bad = (testDir / "nonexistent" / "out.mp4").string();
    ASSERT_TRUE(muxer.open(bad));
    ASSERT_TRUE(video.open(320, 240, {1, 12800}, 2000, true,
                           muxer.needsGlobalHeader()));
    ASSERT_TRUE(audio.open(44100, 2, 128, muxer.needsGlobalHeader()));
    if (video.codecContext()) {
        video.setStream(muxer.addStream(video.codecContext()));
    }
    if (audio.codecContext()) {
        audio.setStream(muxer.addStream(audio.codecContext()));
    }
    EXPECT_FALSE(muxer.beginFile());
}

TEST_F(EncoderTest, WriteVideoFrameDoesNotCrash) {
    Muxer muxer;
    VideoEncoder video;
    AudioEncoder audio;
    ASSERT_TRUE(openAVPair(muxer, video, audio));
    AVFrame* frame = makeSyntheticVideoFrame(320, 240, 0);
    EXPECT_TRUE(video.writeFrame(frame));
    av_frame_free(&frame);
    video.close();
    audio.close();
    muxer.endFile();
}

TEST_F(EncoderTest, WriteMultipleVideoFramesDoesNotCrash) {
    Muxer muxer;
    VideoEncoder video;
    AudioEncoder audio;
    ASSERT_TRUE(openAVPair(muxer, video, audio));
    for (int i = 0; i < 5; ++i) {
        AVFrame* frame = makeSyntheticVideoFrame(320, 240, static_cast<int64_t>(i) * 3000);
        EXPECT_TRUE(video.writeFrame(frame));
        av_frame_free(&frame);
    }
    video.close();
    audio.close();
    muxer.endFile();
}

TEST_F(EncoderTest, WriteAudioFrameDoesNotCrash) {
    Muxer muxer;
    VideoEncoder video;
    AudioEncoder audio;
    ASSERT_TRUE(openAVPair(muxer, video, audio));
    AVFrame* frame = makeSyntheticAudioFrame();
    EXPECT_TRUE(audio.writeFrame(frame));
    av_frame_free(&frame);
    video.close();
    audio.close();
    muxer.endFile();
}

TEST_F(EncoderTest, VideoOnlyOpenSucceeds) {
    Muxer muxer;
    VideoEncoder video;
    EXPECT_TRUE(openVideoPair(muxer, video));
    video.close();
    muxer.endFile();
}

TEST_F(EncoderTest, OutputFileHasNonZeroSize) {
    Muxer muxer;
    VideoEncoder video;
    AudioEncoder audio;
    ASSERT_TRUE(openAVPair(muxer, video, audio));
    for (int i = 0; i < 10; ++i) {
        AVFrame* vf = makeSyntheticVideoFrame(320, 240, static_cast<int64_t>(i) * 3000);
        video.writeFrame(vf);
        av_frame_free(&vf);
        AVFrame* af = makeSyntheticAudioFrame();
        audio.writeFrame(af);
        av_frame_free(&af);
    }
    video.close();
    audio.close();
    muxer.endFile();
    EXPECT_GT(fs::file_size(outPath), static_cast<uintmax_t>(0));
}
