#include "HudTextureHashWhitelist.h"
#include <fstream>
#include <string>
#include <vector>
#include <algorithm> // For std::remove_if, std::isspace
#include <charconv>  // For std::from_chars
#include <cstdint>   // For uint32_t

#include "library.h"
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
HudTextureHashWhitelist::HudTextureHashWhitelist()
    : m_whitelistedHashes() {
    // Hashes are moved into the unordered_set
}

void HudTextureHashWhitelist::populateFromDefaultYamlFile(HudTextureHashWhitelist& ctx) {

    std::string game_id = library::getGameId();
    if (!game_id.empty()) {
        return populateFromYamlFile(ctx, hostfs::getHudConfigurationPath() + game_id + "/hud.yaml");
    } else {
        ERROR_LOG(RENDERER, "No game ID loaded, using default HUD texture whitelist");
        ctx.init(std::vector<u32>{});
    }
}

// Factory method to create an instance from a YAML file
void HudTextureHashWhitelist::populateFromYamlFile(HudTextureHashWhitelist& ctx, const std::string& file_path) {
    NOTICE_LOG(RENDERER, "Loading HUD texture whitelist from: %s", file_path.c_str());

    std::vector<u32> hashes;
    std::ifstream file(file_path);

    if (!file.is_open()) {
        WARN_LOG(RENDERER, "Failed to open HUD texture whitelist file: %s", file_path.c_str()); // Warn log added
        ctx.init(std::vector<u32>{});
        return;
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
                std::string hash_str = trim(trimmed_line.substr(2)); // Remove "- " prefix
                // Remove quotes if present
                if (hash_str.length() >= 2 && hash_str.front() == '"' && hash_str.back() == '"') {
                    hash_str = hash_str.substr(1, hash_str.length() - 2);
                }
                if (!hash_str.empty()) {
                    u32 hash_val;
                    // Convert hex string to u32
                    auto [ptr, ec] = std::from_chars(hash_str.data(), hash_str.data() + hash_str.length(), hash_val, 16);

                    if (ec == std::errc()) {
                        NOTICE_LOG(RENDERER, "Adding texture hash to hud whitelist: 0x%X", hash_val);
                        hashes.push_back(hash_val);
                    } else {
                        WARN_LOG(RENDERER, "Failed to convert hash string '%s' to u32. Error: %d", hash_str.c_str(), static_cast<int>(ec));
                    }
                }
            } else if (!trimmed_line.empty() && trimmed_line.find(':') != std::string::npos) {
                // Another YAML key, so we're out of the textureHashes section
                in_texture_hashes_section = false;
            }
        }
    }
    file.close();

    ctx.init(hashes);
}

bool HudTextureHashWhitelist::isEmpty() const {
    return m_whitelistedHashes.empty();
}

size_t HudTextureHashWhitelist::size() const {
    return m_whitelistedHashes.size();
}

bool HudTextureHashWhitelist::isWhitelisted(u32 hash) const {
    const bool ret = m_whitelistedHashes.count(hash) > 0;
    return ret;
}

std::vector<u32> HudTextureHashWhitelist::getWhitelistedHashes() const {
    std::vector<u32> hashes;
    hashes.reserve(m_whitelistedHashes.size());
    for (const auto& hash : m_whitelistedHashes) {
        hashes.push_back(hash);
    }
    return hashes;
}

void HudTextureHashWhitelist::init(std::vector<u32> hashes) {
    m_whitelistedHashes.reserve(hashes.size());
    for (u32 hash : hashes) {
        m_whitelistedHashes.insert(std::move(hash));
    }
}

}
