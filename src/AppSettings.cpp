#include "AppSettings.h"
#include "JsonConstants.h"
#include <algorithm>
#include <fstream>

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

static std::string jsonUnescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[++i]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

// Reads a JSON string value starting just after the opening quote.
// Advances i past the closing quote and returns the unescaped value.
static std::string readJsonString(const std::string& content, size_t& i) {
    std::string raw;
    while (i < content.size()) {
        char c = content[i++];
        if (c == '\\' && i < content.size()) { raw += '\\'; raw += content[i++]; }
        else if (c == '"') { break; }
        else { raw += c; }
    }
    return jsonUnescape(raw);
}

AppSettings::AppSettings(const std::string& filePath) : m_filePath(filePath) {
    load();
}

void AppSettings::addRecentFile(const std::string& path) {
    m_recentFiles.erase(std::remove(m_recentFiles.begin(), m_recentFiles.end(), path),
                        m_recentFiles.end());
    m_recentFiles.insert(m_recentFiles.begin(), path);
    if (static_cast<int>(m_recentFiles.size()) > MAX_ENTRIES) {
        m_recentFiles.resize(MAX_ENTRIES);
    }
    save();
}

void AppSettings::setLastExportDir(const std::string& dir) {
    m_lastExportDir = dir;
    save();
}

void AppSettings::load() {
    std::ifstream f(m_filePath);
    if (!f) { return; }

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    size_t recentKey = content.find(std::string("\"") + KEY_RECENT_FILES + "\"");
    if (recentKey != std::string::npos) {
        size_t arrayStart = content.find('[', recentKey);
        size_t arrayEnd   = content.find(']', arrayStart != std::string::npos ? arrayStart : 0);
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            size_t pos = arrayStart + 1;
            while (pos < arrayEnd) {
                size_t strOpen = content.find('"', pos);
                if (strOpen == std::string::npos || strOpen >= arrayEnd) { break; }
                size_t i = strOpen + 1;
                std::string val = readJsonString(content, i);
                if (!val.empty()) { m_recentFiles.push_back(val); }
                pos = i;
            }
            if (static_cast<int>(m_recentFiles.size()) > MAX_ENTRIES) {
                m_recentFiles.resize(MAX_ENTRIES);
            }
        }
    }

    size_t exportKey = content.find(std::string("\"") + KEY_LAST_EXPORT_DIR + "\"");
    if (exportKey != std::string::npos) {
        size_t colon    = content.find(':', exportKey);
        size_t strOpen  = colon != std::string::npos ? content.find('"', colon) : std::string::npos;
        if (strOpen != std::string::npos) {
            size_t i = strOpen + 1;
            m_lastExportDir = readJsonString(content, i);
        }
    }
}

void AppSettings::save() const {
    std::ofstream f(m_filePath);
    f << "{\n";
    f << "    \"" << KEY_RECENT_FILES << "\": [\n";
    for (size_t i = 0; i < m_recentFiles.size(); ++i) {
        f << "        \"" << jsonEscape(m_recentFiles[i]) << '"';
        if (i + 1 < m_recentFiles.size()) { f << ','; }
        f << '\n';
    }
    f << "    ],\n";
    f << "    \"" << KEY_LAST_EXPORT_DIR << "\": \"" << jsonEscape(m_lastExportDir) << "\"\n";
    f << "}\n";
}
