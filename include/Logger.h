#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <mutex>
#include <string>

class Logger {
public:
    enum class Level { Debug, Info, Warning, Error };

    static Logger& instance();

    // Open a log file and set the minimum level to emit.
    // Must be called before any log calls if file output is desired.
    void init(const std::string& filePath, Level minLevel = Level::Debug, int maxFiles = 10);

    void debug  (const std::string& msg);
    void info   (const std::string& msg);
    void warning(const std::string& msg);
    void error  (const std::string& msg);

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(Level level, const std::string& msg);
    static const char* levelTag(Level level);

    Level         m_minLevel = Level::Debug;
    std::ofstream m_file;
    std::mutex    m_mutex;
};

#endif // LOGGER_H
