#pragma once

#include <string>
#include <unordered_set>
#include <vector>
#include <cstdint> // For uint32_t, assuming u32 is uint32_t

namespace rend {

// Assuming u32 is uint32_t
using u32 = uint32_t;

class HudTextureHashWhitelist {
public:
    // Constructor to initialize with a list of hashes
    explicit HudTextureHashWhitelist();

    // Factory method to create an instance from a YAML file
    static void populateFromYamlFile(HudTextureHashWhitelist& ctx,const std::string& file_path);
    static void populateFromDefaultYamlFile(HudTextureHashWhitelist &ctx);

    // Check if the whitelist is empty
    bool isEmpty() const;

    // Get the number of whitelisted hashes
    size_t size() const;

    // Check if a hash is whitelisted
    bool isWhitelisted(u32 hash) const;

    // Get all whitelisted hashes (returns a copy)
    std::vector<u32> getWhitelistedHashes() const;

    //init when game loaded
    void init(std::vector<u32> hashes);

private:
    std::unordered_set<u32> m_whitelistedHashes;
};

} // namespace rend
