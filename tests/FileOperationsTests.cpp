#include <gtest/gtest.h>
#include <filesystem>
#include "FileOperations.h"

// ── executableDir() ───────────────────────────────────────────────────────────

TEST(ExecutableDirTest, ReturnsNonEmptyString) {
    EXPECT_FALSE(executableDir().empty());
}

TEST(ExecutableDirTest, EndsWithPathSeparator) {
    std::string dir = executableDir();
    ASSERT_FALSE(dir.empty());
    char last = dir.back();
    EXPECT_TRUE(last == '/' || last == '\\');
}

TEST(ExecutableDirTest, IsAbsolutePath) {
    std::string dir = executableDir();
    ASSERT_FALSE(dir.empty());
    // Windows: "C:\...", Unix: "/..."
    bool isAbsolute = (dir.size() >= 2 && dir[1] == ':') || dir[0] == '/';
    EXPECT_TRUE(isAbsolute);
}

TEST(ExecutableDirTest, DirectoryExistsOnDisk) {
    std::string dir = executableDir();
    ASSERT_FALSE(dir.empty());
    EXPECT_TRUE(std::filesystem::is_directory(dir));
}

TEST(ExecutableDirTest, IsIdempotent) {
    EXPECT_EQ(executableDir(), executableDir());
}

// ── openFileDialog() ──────────────────────────────────────────────────────────
// openFileDialog() opens a native OS modal dialog that blocks until the user
// makes a selection. It cannot be driven in a headless test runner and is
// therefore excluded from automated tests.
