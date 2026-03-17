#pragma once
#include <string>
#include <vector>
#include <cstdio>

namespace eb {

struct ModManifest {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::vector<std::string> scripts;  // .sage files to load
    bool loaded = false;
};

class ModLoader {
public:
    // Scan a directory for mod folders containing mod.json
    void scan_directory(const std::string& mods_dir) {
        mods_dir_ = mods_dir;
        std::printf("[ModLoader] Scanning: %s (TODO: implement directory scan)\n", mods_dir.c_str());
    }

    const std::vector<ModManifest>& mods() const { return mods_; }
    int mod_count() const { return (int)mods_.size(); }

private:
    std::string mods_dir_;
    std::vector<ModManifest> mods_;
};

} // namespace eb
