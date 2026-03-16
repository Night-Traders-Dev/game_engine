#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "engine/core/types.h"

namespace eb {

// ─── Persistent flags (key-value store, survives save/load) ───

struct GameFlags {
    std::unordered_map<std::string, std::string> data;

    void set(const std::string& key, const std::string& value) { data[key] = value; }
    std::string get(const std::string& key, const std::string& def = "") const {
        auto it = data.find(key);
        return it != data.end() ? it->second : def;
    }
    bool has(const std::string& key) const { return data.find(key) != data.end(); }
    void remove(const std::string& key) { data.erase(key); }
};

// ─── Save data structure ───

struct InventorySaveItem {
    std::string id, name;
    int quantity;
    std::string type;       // "consumable", "weapon", "key"
    std::string description;
    int heal_hp, damage;
    int buy_price, sell_price;
    std::string element, sage_func;
};

struct PartySave {
    std::string name;
    int hp, hp_max, atk;
};

struct SaveData {
    // Version
    int version = 1;
    // Player
    float player_x = 0, player_y = 0;
    int player_dir = 0;
    int player_hp = 100, player_hp_max = 100;
    int player_atk = 10, player_def = 5;
    int player_level = 1, player_xp = 0;
    // Stats (S.P.E.C.I.A.L style)
    int stat_vitality = 5, stat_arcana = 5, stat_agility = 5;
    int stat_tactics = 5, stat_spirit = 5, stat_strength = 5;
    // Party
    std::vector<PartySave> party;
    // Inventory
    std::vector<InventorySaveItem> items;
    int gold = 0;
    // Equipment
    std::string equip_weapon, equip_armor, equip_accessory, equip_shield;
    // World
    std::string current_map;
    float game_hour = 8.0f;
    int day_count = 1;
    // Flags
    GameFlags flags;
    // Meta
    float playtime_seconds = 0;
    std::string timestamp;
};

// ─── Save/Load I/O ───

namespace SaveSystem {

inline std::string saves_dir() {
    return "saves";
}

inline std::string slot_path(int slot) {
    return saves_dir() + "/slot_" + std::to_string(slot) + ".json";
}

inline bool has_save(int slot) {
    return std::filesystem::exists(slot_path(slot));
}

inline bool delete_save(int slot) {
    if (!has_save(slot)) return false;
    return std::filesystem::remove(slot_path(slot));
}

// Escape a string for JSON output
inline std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

inline bool save(int slot, const SaveData& data) {
    std::filesystem::create_directories(saves_dir());

    std::ofstream f(slot_path(slot));
    if (!f.is_open()) return false;

    f << "{\n";
    f << "  \"version\": " << data.version << ",\n";
    f << "  \"player_x\": " << data.player_x << ", \"player_y\": " << data.player_y << ",\n";
    f << "  \"player_dir\": " << data.player_dir << ",\n";
    f << "  \"player_hp\": " << data.player_hp << ", \"player_hp_max\": " << data.player_hp_max << ",\n";
    f << "  \"player_atk\": " << data.player_atk << ", \"player_def\": " << data.player_def << ",\n";
    f << "  \"player_level\": " << data.player_level << ", \"player_xp\": " << data.player_xp << ",\n";
    f << "  \"stats\": [" << data.stat_vitality << "," << data.stat_arcana << ","
      << data.stat_agility << "," << data.stat_tactics << ","
      << data.stat_spirit << "," << data.stat_strength << "],\n";
    f << "  \"gold\": " << data.gold << ",\n";
    f << "  \"current_map\": \"" << json_escape(data.current_map) << "\",\n";
    f << "  \"game_hour\": " << data.game_hour << ",\n";
    f << "  \"day_count\": " << data.day_count << ",\n";
    f << "  \"playtime\": " << data.playtime_seconds << ",\n";
    f << "  \"timestamp\": \"" << json_escape(data.timestamp) << "\",\n";

    // Equipment
    f << "  \"equipment\": [\"" << json_escape(data.equip_weapon) << "\",\""
      << json_escape(data.equip_armor) << "\",\""
      << json_escape(data.equip_accessory) << "\",\""
      << json_escape(data.equip_shield) << "\"],\n";

    // Items
    f << "  \"items\": [\n";
    for (size_t i = 0; i < data.items.size(); i++) {
        auto& it = data.items[i];
        f << "    {\"id\":\"" << json_escape(it.id) << "\",\"name\":\"" << json_escape(it.name)
          << "\",\"qty\":" << it.quantity << ",\"type\":\"" << it.type << "\"}";
        if (i + 1 < data.items.size()) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    // Party
    f << "  \"party\": [\n";
    for (size_t i = 0; i < data.party.size(); i++) {
        auto& p = data.party[i];
        f << "    {\"name\":\"" << json_escape(p.name) << "\",\"hp\":" << p.hp
          << ",\"hp_max\":" << p.hp_max << ",\"atk\":" << p.atk << "}";
        if (i + 1 < data.party.size()) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    // Flags
    f << "  \"flags\": {\n";
    size_t fi = 0;
    for (auto& [k, v] : data.flags.data) {
        f << "    \"" << json_escape(k) << "\": \"" << json_escape(v) << "\"";
        if (++fi < data.flags.data.size()) f << ",";
        f << "\n";
    }
    f << "  }\n";

    f << "}\n";
    f.close();

    std::printf("[Save] Saved to slot %d: %s\n", slot, slot_path(slot).c_str());
    return true;
}

// Minimal JSON parser for loading save data
inline bool load(int slot, SaveData& data) {
    std::ifstream f(slot_path(slot));
    if (!f.is_open()) return false;

    std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    // Simple field extraction helpers
    auto find_num = [&](const std::string& key) -> double {
        auto pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return 0;
        pos = json.find(':', pos);
        if (pos == std::string::npos) return 0;
        pos++;
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n')) pos++;
        return std::stod(json.substr(pos));
    };

    auto find_str = [&](const std::string& key) -> std::string {
        auto pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";
        pos = json.find('"', pos + 1);
        if (pos == std::string::npos) return "";
        pos++;
        auto end = json.find('"', pos);
        return (end != std::string::npos) ? json.substr(pos, end - pos) : "";
    };

    data.version = (int)find_num("version");
    data.player_x = (float)find_num("player_x");
    data.player_y = (float)find_num("player_y");
    data.player_dir = (int)find_num("player_dir");
    data.player_hp = (int)find_num("player_hp");
    data.player_hp_max = (int)find_num("player_hp_max");
    data.player_atk = (int)find_num("player_atk");
    data.player_def = (int)find_num("player_def");
    data.player_level = (int)find_num("player_level");
    data.player_xp = (int)find_num("player_xp");
    data.gold = (int)find_num("gold");
    data.current_map = find_str("current_map");
    data.game_hour = (float)find_num("game_hour");
    data.day_count = (int)find_num("day_count");
    data.playtime_seconds = (float)find_num("playtime");
    data.timestamp = find_str("timestamp");

    // Equipment
    data.equip_weapon = find_str("equip_weapon");
    data.equip_armor = find_str("equip_armor");

    // Note: Full item/party/flag deserialization would need a more robust parser.
    // For now the basics (player state + map + gold + playtime) are loaded.

    std::printf("[Save] Loaded slot %d: map=%s, level=%d, playtime=%.0fs\n",
                slot, data.current_map.c_str(), data.player_level, data.playtime_seconds);
    return true;
}

} // namespace SaveSystem
} // namespace eb
