#include "engine/resource/resource_manager.h"
#include <cstdio>

#ifdef __ANDROID__
#include <android/log.h>
#define RMLOGI(...) __android_log_print(ANDROID_LOG_INFO, "TWEngine-Resources", __VA_ARGS__)
#define RMLOGE(...) __android_log_print(ANDROID_LOG_ERROR, "TWEngine-Resources", __VA_ARGS__)
#else
#define RMLOGI(...) std::printf(__VA_ARGS__)
#define RMLOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace eb {

ResourceManager::ResourceManager(VulkanContext& ctx) : ctx_(ctx) {
    RMLOGI("ResourceManager initialized\n");
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
    RMLOGI("Loaded texture: %s (%ux%u)\n", path.c_str(), ptr->width(), ptr->height());
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
