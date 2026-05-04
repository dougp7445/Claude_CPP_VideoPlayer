#ifndef MUXER_H
#define MUXER_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <mutex>
#include <string>
#include <thread>
#include "LockingQueue.h"

class Muxer {
public:
    Muxer();
    ~Muxer();

    // Allocate an output format context inferred from the file extension.
    bool open(const std::string& outputPath);

    // True when the output format requires a global codec header
    // (AVFMT_GLOBALHEADER). Codecs must set AV_CODEC_FLAG_GLOBAL_HEADER
    // before being opened when this returns true.
    bool needsGlobalHeader() const;

    // Register a fully-opened codec context as a new output stream.
    // Copies codec parameters and time base to the stream. Returns the
    // created AVStream, or nullptr on failure.
    AVStream* addStream(AVCodecContext* codecCtx);

    // Open the output file and write the container header.
    // Call after all addStream() calls and before any writePacket() calls.
    bool beginFile();

    // Start a background thread that drains queue and writes each packet.
    // Call after beginFile() and before the encoder begins producing packets.
    bool startAsync(LockingQueue<AVPacket*>& queue);

    // Two-queue variant: drains videoQueue and audioQueue on separate threads,
    // serializing writes with an internal mutex.
    bool startAsync(LockingQueue<AVPacket*>& videoQueue, LockingQueue<AVPacket*>& audioQueue);

    // Write one encoded packet using interleaved muxing.
    bool writePacket(AVPacket* packet);

    // Write the container trailer and close the output file.
    bool endFile();

    // Release all resources without finalizing (safe to call on error paths).
    void close();

private:
    AVFormatContext*         m_fmtCtx    = nullptr;
    std::string              m_outputPath;
    std::thread              m_writeThread;
    std::thread              m_writeThread2;
    std::mutex               m_writeMutex;
    LockingQueue<AVPacket*>* m_videoPacketQueue  = nullptr;
    LockingQueue<AVPacket*>* m_audioPacketQueue = nullptr;
};

#endif // MUXER_H
