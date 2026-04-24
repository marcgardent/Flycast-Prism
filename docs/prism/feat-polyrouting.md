# Moteur de Routage avec Flexibilité des Hashes

Le système accepte désormais les hashes hexadécimaux avec ou sans le préfixe `0x`, facilitant la configuration pour l'utilisateur final.

---

### 1. Amélioration de l'Expérience Utilisateur
Le parser détecte automatiquement la présence du préfixe. Les formats suivants sont tous valides :
* `texHash: "0x12345678"`
* `texHash: "12345678"`
* `texHash: "ABCDEF01"` (insensible à la casse via le format hexadécimal)

---

### 2. Exemple de Configuration YAML Simplifiée

```yaml
routing:
  - name: "Chronomètre"
    match: { texHash: "12345678" } # Pas besoin de 0x ici
    pass: ["Hud"]

  - name: "Ombre Buggée"
    match: { texHash: "0x99887766" } # Toujours supporté pour la compatibilité
    policies: ["depth"]
```

---

### 3. Implémentation C++ (Parsing Flexible)

```cpp:PolyExclusionManager.h
#include <vector>
#include <string>
#include <cmath>
#include <charconv> 
#include <string_view>
#include <yaml-cpp/yaml.h>

typedef uint32_t u32;

struct PolyMatch {
    float x = -1e9f, y = -1e9f, z = -1e9f;
    int polyCount = -1;
    u32 texHash = 0;

    bool matches(float px, float py, float pz, int count, u32 hash) const {
        if (polyCount != -1 && polyCount != count) return false;
        if (texHash != 0 && texHash != hash) return false;

        if (x != -1e9f) {
            if (std::abs(x - px) > 0.01f) return false;
            if (std::abs(y - py) > 0.01f) return false;
            if (std::abs(z - pz) > 0.01f) return false;
        }
        return true;
    }
};

struct RoutingRule {
    std::string name;
    PolyMatch criteria;
    bool toHud = false;
    bool ignoreDepth = false;
    bool triggerGlobalHud = false;
};

class PolyExclusionManager {
public:
    bool isGlobalHudActive = false;
    std::vector<RoutingRule> rules;

    void NewFrame() { isGlobalHudActive = false; }

    void LoadConfig(const std::string& filename) {
        YAML::Node config = YAML::LoadFile(filename);
        if (!config["routing"]) return;

        for (auto node : config["routing"]) {
            RoutingRule rule;
            rule.name = node["name"].as<std::string>("Sans nom");
            
            auto m = node["match"];
            if (m["pos"]) {
                rule.criteria.x = m["pos"][0].as<float>();
                rule.criteria.y = m["pos"][1].as<float>();
                rule.criteria.z = m["pos"][2].as<float>();
            }
            if (m["polyCount"]) {
                rule.criteria.polyCount = m["polyCount"].as<int>();
            }
            
            // Parsing flexible du Hash
            if (m["texHash"]) {
                std::string hash_str = m["texHash"].as<std::string>();
                std::string_view sv = hash_str;

                // On nettoie le préfixe si l'utilisateur l'a mis
                if (sv.starts_with("0x") || sv.starts_with("0X")) {
                    sv.remove_prefix(2);
                }

                u32 val = 0;
                auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val, 16);
                if (ec == std::errc()) {
                    rule.criteria.texHash = val;
                }
            }

            // Traitement des Pass/Policies
            if (node["pass"]) {
                for (auto p : node["pass"]) if (p.as<std::string>() == "Hud") rule.toHud = true;
            }
            if (node["policy"] && node["policy"].as<std::string>() == "StartHudPass")
                rule.triggerGlobalHud = true;
            if (node["policies"]) {
                for (auto p : node["policies"]) if (p.as<std::string>() == "depth") rule.ignoreDepth = true;
            }

            rules.push_back(rule);
        }
    }

    struct Action { bool discard = false; bool forceHud = false; };

    Action GetAction(float x, float y, float z, int count, u32 hash) {
        if (isGlobalHudActive) return { false, true };

        for (auto& rule : rules) {
            if (rule.criteria.matches(x, y, z, count, hash)) {
                if (rule.triggerGlobalHud) isGlobalHudActive = true;
                return { rule.ignoreDepth, rule.toHud || rule.triggerGlobalHud };
            }
        }
        return { false, false };
    }
};
```
