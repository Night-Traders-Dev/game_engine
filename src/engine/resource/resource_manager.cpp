#include "engine/resource/resource_manager.h"
#include <cstdio>

namespace eb {

ResourceManager::ResourceManager(VulkanContext& ctx) : ctx_(ctx) {
    std::printf("[ResourceManager] Initialized\n");
}

ResourceManager::~ResourceManager() {
    clear();
}

Texture* ResourceManager::load_texture(const std::string& path) {
    auto it = textures_.find(path);
    if (it != textures_.end()) {
        return it->second.get();
    }

    auto tex = std::make_unique<Texture>(ctx_, path);
    Texture* ptr = tex.get();
    textures_[path] = std::move(tex);
    std::printf("[ResourceManager] Loaded texture: %s\n", path.c_str());
    return ptr;
}

Texture* ResourceManager::get_texture(const std::string& path) {
    auto it = textures_.find(path);
    return it != textures_.end() ? it->second.get() : nullptr;
}

void ResourceManager::clear() {
    textures_.clear();
}

} // namespace eb
