#ifndef RECENT_FILES_H
#define RECENT_FILES_H

#include <string>
#include <vector>

class RecentFiles {
public:
    explicit RecentFiles(const std::string& filePath);

    // Prepend path, remove any existing duplicate, trim to MAX_ENTRIES, then save.
    void add(const std::string& path);

    const std::vector<std::string>& entries() const { return m_entries; }

private:
    void load();
    void save() const;

    std::string              m_filePath;
    std::vector<std::string> m_entries;

    static constexpr int MAX_ENTRIES = 10;
};

#endif // RECENT_FILES_H
