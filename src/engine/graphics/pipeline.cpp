#include "engine/graphics/pipeline.h"
#include "engine/graphics/vulkan_context.h"
#include "engine/resource/file_io.h"

#include <fstream>
#include <stdexcept>
#include <cstdio>

#ifdef __ANDROID__
#include <android/log.h>
#define PLOGI(...) __android_log_print(ANDROID_LOG_INFO, "TWEngine-Pipeline", __VA_ARGS__)
#define PLOGE(...) __android_log_print(ANDROID_LOG_ERROR, "TWEngine-Pipeline", __VA_ARGS__)
#else
#define PLOGI(...) std::printf(__VA_ARGS__)
#define PLOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace eb {

static std::vector<char> read_shader_file(const std::string& path) {
    PLOGI("Loading shader: %s\n", path.c_str());
    auto data = FileIO::read_file_chars(path);
    if (data.empty()) {
        PLOGE("Failed to load shader: %s\n", path.c_str());
        throw std::runtime_error("Failed to open shader file: " + path);
    }
    PLOGI("Shader loaded: %s (%zu bytes)\n", path.c_str(), data.size());
    return data;
}

Pipeline::Pipeline(VulkanContext& ctx, const PipelineConfig& config) : ctx_(ctx) {
    auto vert_code = read_shader_file(config.vert_shader_path);
    auto frag_code = read_shader_file(config.frag_shader_path);

    VkShaderModule vert_module = ctx.create_shader_module(vert_code);
    VkShaderModule frag_module = ctx.create_shader_module(frag_code);

    VkPipelineShaderStageCreateInfo vert_stage{};
    vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.module = vert_module;
    vert_stage.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage{};
    frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.module = frag_module;
    frag_stage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vert_stage, frag_stage};

    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = static_cast<uint32_t>(config.binding_descriptions.size());
    vertex_input.pVertexBindingDescriptions = config.binding_descriptions.data();
    vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(config.attribute_descriptions.size());
    vertex_input.pVertexAttributeDescriptions = config.attribute_descriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Dynamic viewport and scissor
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE; // 2D sprites need no culling
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling (off)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending
    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (config.alpha_blending) {
        blend_attachment.blendEnable = VK_TRUE;
        blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    } else {
        blend_attachment.blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo color_blend{};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.logicOpEnable = VK_FALSE;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &blend_attachment;

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = config.pipeline_layout;
    pipeline_info.renderPass = config.render_pass;
    pipeline_info.subpass = 0;

    if (vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1,
                                  &pipeline_info, nullptr, &pipeline_) != VK_SUCCESS) {
        vkDestroyShaderModule(ctx.device(), vert_module, nullptr);
        vkDestroyShaderModule(ctx.device(), frag_module, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(ctx.device(), vert_module, nullptr);
    vkDestroyShaderModule(ctx.device(), frag_module, nullptr);
}

Pipeline::~Pipeline() {
    if (pipeline_) {
        vkDestroyPipeline(ctx_.device(), pipeline_, nullptr);
    }
}

} // namespace eb
