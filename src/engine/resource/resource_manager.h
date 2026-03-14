#pragma once

#include "engine/graphics/texture.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace eb {

class VulkanContext;

class ResourceManager {
public:
    explicit ResourceManager(VulkanContext& ctx);
    ~ResourceManager();

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    Texture* load_texture(const std::string& path);
    Texture* get_texture(const std::string& path);
    void clear();

private:
    VulkanContext& ctx_;
    std::unordered_map<std::string, std::unique_ptr<Texture>> textures_;
};

} // namespace eb
