#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace eb {

class VulkanContext;

struct PipelineConfig {
    std::string vert_shader_path;
    std::string frag_shader_path;
    std::vector<VkVertexInputBindingDescription> binding_descriptions;
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    bool alpha_blending = true;
};

class Pipeline {
public:
    Pipeline(VulkanContext& ctx, const PipelineConfig& config);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    VkPipeline handle() const { return pipeline_; }

private:
    VulkanContext& ctx_;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace eb
