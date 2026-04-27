#include "Logger.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class LoggerTest : public ::testing::Test {
protected:
    fs::path testDir;

    void SetUp() override {
        testDir = fs::temp_directory_path() / "VideoPlayerLoggerTests";
        fs::remove_all(testDir);
        fs::create_directories(testDir);
    }

    void TearDown() override {
        Logger::instance().init(""); // closes the open log file
        fs::remove_all(testDir);
    }

    fs::path logPath(const std::string& name = "test.log") {
        return testDir / name;
    }

    std::string readFile(const fs::path& path) {
        std::ifstream f(path);
        return std::string(std::istreambuf_iterator<char>(f), {});
    }

    int countLogFiles() {
        int n = 0;
        for (const auto& entry : fs::directory_iterator(testDir)) {
            if (entry.path().extension() == ".log") { ++n; }
        }
        return n;
    }
};

TEST_F(LoggerTest, InstanceReturnsSameObject) {
    EXPECT_EQ(&Logger::instance(), &Logger::instance());
}

TEST_F(LoggerTest, InitCreatesLogFile) {
    Logger::instance().init(logPath().string());
    EXPECT_TRUE(fs::exists(logPath()));
}

TEST_F(LoggerTest, InitCreatesParentDirectory) {
    fs::path nested = testDir / "nested" / "logs" / "test.log";
    Logger::instance().init(nested.string());
    EXPECT_TRUE(fs::exists(nested));
}

TEST_F(LoggerTest, DebugMessageWrittenToFile) {
    Logger::instance().init(logPath().string());
    Logger::instance().debug("hello debug");
    EXPECT_NE(readFile(logPath()).find("hello debug"), std::string::npos);
}

TEST_F(LoggerTest, InfoMessageWrittenToFile) {
    Logger::instance().init(logPath().string());
    Logger::instance().info("hello info");
    EXPECT_NE(readFile(logPath()).find("hello info"), std::string::npos);
}

TEST_F(LoggerTest, WarningMessageWrittenToFile) {
    Logger::instance().init(logPath().string());
    Logger::instance().warning("hello warning");
    EXPECT_NE(readFile(logPath()).find("hello warning"), std::string::npos);
}

TEST_F(LoggerTest, ErrorMessageWrittenToFile) {
    Logger::instance().init(logPath().string());
    Logger::instance().error("hello error");
    EXPECT_NE(readFile(logPath()).find("hello error"), std::string::npos);
}

TEST_F(LoggerTest, LevelTagsPresentInOutput) {
    Logger::instance().init(logPath().string());
    Logger::instance().debug("d");
    Logger::instance().info("i");
    Logger::instance().warning("w");
    Logger::instance().error("e");
    std::string content = readFile(logPath());
    EXPECT_NE(content.find("DEBUG"), std::string::npos);
    EXPECT_NE(content.find("INFO "), std::string::npos);
    EXPECT_NE(content.find("WARN "), std::string::npos);
    EXPECT_NE(content.find("ERROR"), std::string::npos);
}

TEST_F(LoggerTest, MinLevelFiltersLowerMessages) {
    Logger::instance().init(logPath().string(), Logger::Level::Warning);
    Logger::instance().trace("filtered");
    Logger::instance().debug("filtered");
    Logger::instance().info("filtered");
    Logger::instance().warning("passed");
    Logger::instance().error("passed");
    std::string content = readFile(logPath());
    EXPECT_EQ(content.find("filtered"), std::string::npos);
    EXPECT_NE(content.find("passed"), std::string::npos);
}

TEST_F(LoggerTest, PruneKeepsExactlyMaxFiles) {
    // Create 12 pre-existing log files with staggered modification times.
    for (int i = 0; i < 12; ++i) {
        fs::path p = testDir / ("old_" + std::to_string(i) + ".log");
        { std::ofstream f(p); }
        auto t = fs::file_time_type::clock::now() - std::chrono::seconds(120 - i);
        fs::last_write_time(p, t);
    }
    // Opening a 13th file with maxFiles=10 should prune 3 oldest.
    Logger::instance().init(logPath("new.log").string(), Logger::Level::Debug, 10);
    EXPECT_EQ(countLogFiles(), 10);
}

TEST_F(LoggerTest, PruneDeletesOldestFiles) {
    // Files 0..11: file 0 is oldest, file 11 is newest.
    for (int i = 0; i < 12; ++i) {
        fs::path p = testDir / ("file_" + std::to_string(i) + ".log");
        { std::ofstream f(p); }
        auto t = fs::file_time_type::clock::now() - std::chrono::seconds(120 - i);
        fs::last_write_time(p, t);
    }
    // maxFiles=10 → the 3 oldest (file_0, file_1, file_2) must be deleted.
    Logger::instance().init(logPath("new.log").string(), Logger::Level::Debug, 10);
    EXPECT_FALSE(fs::exists(testDir / "file_0.log"));
    EXPECT_FALSE(fs::exists(testDir / "file_1.log"));
    EXPECT_FALSE(fs::exists(testDir / "file_2.log"));
    EXPECT_TRUE(fs::exists(testDir / "file_11.log"));
    EXPECT_TRUE(fs::exists(testDir / "new.log"));
}

TEST_F(LoggerTest, TraceMessageWrittenToFile) {
    Logger::instance().init(logPath().string(), Logger::Level::Trace);
    Logger::instance().trace("hello trace");
    EXPECT_NE(readFile(logPath()).find("hello trace"), std::string::npos);
}

TEST_F(LoggerTest, TraceTagPresentInOutput) {
    Logger::instance().init(logPath().string(), Logger::Level::Trace);
    Logger::instance().trace("t");
    EXPECT_NE(readFile(logPath()).find("TRACE"), std::string::npos);
}

TEST_F(LoggerTest, DefaultMinLevelFiltersTrace) {
    Logger::instance().init(logPath().string()); // minLevel defaults to Debug
    Logger::instance().trace("should be filtered");
    EXPECT_EQ(readFile(logPath()).find("should be filtered"), std::string::npos);
}

TEST_F(LoggerTest, MaxFilesZeroDisablesPruning) {
    for (int i = 0; i < 15; ++i) {
        fs::path p = testDir / ("keep_" + std::to_string(i) + ".log");
        std::ofstream f(p);
    }
    Logger::instance().init(logPath("new.log").string(), Logger::Level::Debug, 0);
    EXPECT_EQ(countLogFiles(), 16); // 15 pre-existing + new.log
}
