#pragma once

#include <string>
#include <unordered_set>
#include <vector>
#include <fstream> // Added for file operations

namespace rend {

class HudTextureHashWhitelist {
public:
    // Constructor to initialize with a list of hashes
    explicit HudTextureHashWhitelist(std::vector<std::string> hashes);

    // Factory method to create an instance from a YAML file
    static HudTextureHashWhitelist createFromYamlFile(const std::string& file_path);
    static HudTextureHashWhitelist createFromDefaultYamlFile();

    // Check if the whitelist is empty
    bool isEmpty() const;

    // Get the number of whitelisted hashes
    size_t size() const;

    // Check if a hash is whitelisted
    bool isWhitelisted(const std::string& hash_str) const;

    // Get all whitelisted hashes (returns a copy)
    std::vector<std::string> getWhitelistedHashes() const;

private:
    // Private default constructor to prevent direct instantiation without hashes
    HudTextureHashWhitelist() = default;

    // Deleted copy constructor and assignment operator for immutability
    HudTextureHashWhitelist(const HudTextureHashWhitelist&) = delete;
    HudTextureHashWhitelist& operator=(const HudTextureHashWhitelist&) = delete;

    const std::unordered_set<std::string> m_whitelistedHashes; // Now const
};

} // namespace rend
