#ifndef EXPORTVIDEO_H
#define EXPORTVIDEO_H

#include "EncoderSettings.h"
#include <atomic>
#include <string>
#include <thread>

class ExportVideo {
public:
    ~ExportVideo();

    // Begin encoding inputPath to outputPath on a background thread.
    // Call wait() before destroying.
    void start(const std::string& inputPath,
               const std::string& outputPath,
               const EncoderSettings& settings);

    void  cancel();
    float progress()  const { return m_progress.load(); }
    bool  isDone()    const { return m_done.load(); }
    bool  cancelled() const { return m_cancel.load(); }
    void  wait();

private:
    void run(const std::string& inputPath,
             const std::string& outputPath,
             const EncoderSettings& settings);

    std::thread        m_thread;
    std::atomic<bool>  m_cancel{false};
    std::atomic<bool>  m_done{false};
    std::atomic<float> m_progress{0.0f};
};

#endif // EXPORTVIDEO_H
