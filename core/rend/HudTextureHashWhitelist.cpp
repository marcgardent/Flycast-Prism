#include "HudTextureHashWhitelist.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <string>
#include <vector>
#include <charconv>  // For std::from_chars
#include <cstdint>   // For uint32_t

#include "library.h"
#include "oslib/oslib.h"

namespace rend {

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
    
    try {
        YAML::Node config = YAML::LoadFile(file_path);
        if (config["textureHashes"] && config["textureHashes"].IsSequence()) {
            for (const auto& node : config["textureHashes"]) {
                std::string hash_str = node.as<std::string>();
                
                // Remove hex prefix if present (0x)
                if (hash_str.size() > 2 && hash_str[0] == '0' && (hash_str[1] == 'x' || hash_str[1] == 'X')) {
                    hash_str = hash_str.substr(2);
                }

                if (!hash_str.empty()) {
                    u32 hash_val;
                    // Convert hex string to u32
                    auto [ptr, ec] = std::from_chars(hash_str.data(), hash_str.data() + hash_str.length(), hash_val, 16);

                    if (ec == std::errc()) {
                        hashes.push_back(hash_val);
                    } else {
                        WARN_LOG(RENDERER, "Failed to convert hash string '%s' to u32", hash_str.c_str());
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        WARN_LOG(RENDERER, "Failed to load/parse HUD texture whitelist file: %s (%s)", file_path.c_str(), e.what());
    }

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
