#include "HudTextureHashWhitelist.h"
#include <fstream>
#include <string>
#include <vector>
#include <algorithm> // For std::remove_if, std::isspace
#include "oslib/oslib.h"

namespace rend {

// Helper function to trim whitespace from a string
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(first, (last - first + 1));
}

// Constructor to initialize with a list of hashes
HudTextureHashWhitelist::HudTextureHashWhitelist(std::vector<std::string> hashes)
    : m_whitelistedHashes(std::make_move_iterator(hashes.begin()), std::make_move_iterator(hashes.end())) {
    // Hashes are moved into the unordered_set
}

HudTextureHashWhitelist HudTextureHashWhitelist::createFromDefaultYamlFile() {
    return HudTextureHashWhitelist::createFromYamlFile(hostfs::getHudConfigurationPath());
}

// Factory method to create an instance from a YAML file
HudTextureHashWhitelist HudTextureHashWhitelist::createFromYamlFile(const std::string& file_path) {
    INFO_LOG(RENDERER, "Loading HUD texture whitelist from: %s", file_path.c_str());

    std::vector<std::string> hashes;
    std::ifstream file(file_path);

    if (!file.is_open()) {
        WARN_LOG(RENDERER, "Failed to open HUD texture whitelist file: %s", file_path.c_str()); // Warn log added
        return HudTextureHashWhitelist(std::vector<std::string>{});
    }

    std::string line;
    bool in_texture_hashes_section = false;

    while (std::getline(file, line)) {
        std::string trimmed_line = trim(line);

        if (trimmed_line == "textureHashes:") {
            in_texture_hashes_section = true;
            continue;
        }

        if (in_texture_hashes_section) {
            if (trimmed_line.rfind("- ", 0) == 0) { // Starts with "- "
                std::string hash = trim(trimmed_line.substr(2)); // Remove "- " prefix
                // Remove quotes if present
                if (hash.length() >= 2 && hash.front() == '"' && hash.back() == '"') {
                    hash = hash.substr(1, hash.length() - 2);
                }
                if (!hash.empty()) {
                    hashes.push_back(hash);
                }
            } else if (!trimmed_line.empty() && trimmed_line.find(':') != std::string::npos) {
                // Another YAML key, so we're out of the textureHashes section
                in_texture_hashes_section = false;
            }
        }
    }
    file.close();

    return HudTextureHashWhitelist(std::move(hashes));
}

bool HudTextureHashWhitelist::isEmpty() const {
    return m_whitelistedHashes.empty();
}

size_t HudTextureHashWhitelist::size() const {
    return m_whitelistedHashes.size();
}

bool HudTextureHashWhitelist::isWhitelisted(const std::string& hash_str) const {
    return m_whitelistedHashes.count(hash_str) > 0;
}

std::vector<std::string> HudTextureHashWhitelist::getWhitelistedHashes() const {
    std::vector<std::string> hashes;
    hashes.reserve(m_whitelistedHashes.size());
    for (const auto& hash : m_whitelistedHashes) {
        hashes.push_back(hash);
    }
    return hashes;
}

} // namespace rend
