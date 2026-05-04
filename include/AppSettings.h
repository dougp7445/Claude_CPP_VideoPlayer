#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <string>
#include <vector>

class AppSettings {
public:
    explicit AppSettings(const std::string& filePath);

    void addRecentFile(const std::string& path);
    void setLastExportDir(const std::string& dir);

    const std::vector<std::string>& recentFiles()  const { return m_recentFiles; }
    const std::string&              lastExportDir() const { return m_lastExportDir; }

private:
    void load();
    void save() const;

    std::string              m_filePath;
    std::vector<std::string> m_recentFiles;
    std::string              m_lastExportDir;

    static constexpr int MAX_ENTRIES = 10;
};

#endif // APP_SETTINGS_H
