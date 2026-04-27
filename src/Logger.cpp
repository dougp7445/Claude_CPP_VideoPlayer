#include "Logger.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>

Logger& Logger::instance() {
    static Logger s;
    return s;
}

void Logger::init(const std::string& filePath, Level minLevel, int maxFiles) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_minLevel = minLevel;
    if (m_file.is_open()) {
        m_file.close();
    }
    if (!filePath.empty()) {
        std::filesystem::path p(filePath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        m_file.open(filePath, std::ios::app);

        if (maxFiles > 0 && p.has_parent_path()) {
            std::vector<std::filesystem::directory_entry> logFiles;
            for (const auto& entry : std::filesystem::directory_iterator(p.parent_path())) {
                if (entry.is_regular_file() && entry.path().extension() == ".log") {
                    logFiles.push_back(entry);
                }
            }
            if (static_cast<int>(logFiles.size()) > maxFiles) {
                std::sort(logFiles.begin(), logFiles.end(),
                    [](const auto& a, const auto& b) {
                        return a.last_write_time() < b.last_write_time();
                    });
                int toDelete = static_cast<int>(logFiles.size()) - maxFiles;
                for (int i = 0; i < toDelete; ++i) {
                    std::filesystem::remove(logFiles[i]);
                }
            }
        }
    }
}

void Logger::trace  (const std::string& msg) { log(Level::Trace,   msg); }
void Logger::debug  (const std::string& msg) { log(Level::Debug,   msg); }
void Logger::info   (const std::string& msg) { log(Level::Info,    msg); }
void Logger::warning(const std::string& msg) { log(Level::Warning, msg); }
void Logger::error  (const std::string& msg) { log(Level::Error,   msg); }

void Logger::log(Level level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (level < m_minLevel) { return; }

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char tsBuf[20];
    std::strftime(tsBuf, sizeof(tsBuf), "%Y-%m-%d %H:%M:%S", &tm);

    std::ostringstream oss;
    oss << '[' << tsBuf << "] [" << levelTag(level) << "] " << msg << '\n';
    const std::string entry = oss.str();

    if (m_file.is_open()) {
        m_file << entry;
        m_file.flush();
    }

    if (level >= Level::Warning) {
        std::cerr << entry;
    } else {
        std::cout << entry;
    }
}

const char* Logger::levelTag(Level level) {
    switch (level) {
        case Level::Trace:   return "TRACE";
        case Level::Debug:   return "DEBUG";
        case Level::Info:    return "INFO ";
        case Level::Warning: return "WARN ";
        case Level::Error:   return "ERROR";
        default:             return "?????";
    }
}
