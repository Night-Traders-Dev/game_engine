#ifndef EB_ANDROID

#include "editor/imgui_integration.h"
#include "engine/graphics/renderer.h"
#include "engine/graphics/vulkan_context.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>
#include <cstdio>

namespace eb {

ImGuiIntegration::~ImGuiIntegration() {
    shutdown();
}

static void check_vk(VkResult err) {
    if (err != VK_SUCCESS) {
        std::fprintf(stderr, "[ImGui] Vulkan error: %d\n", (int)err);
    }
}

bool ImGuiIntegration::init(GLFWwindow* window, Renderer& renderer) {
    if (initialized_) return true;

    auto& ctx = renderer.vulkan_context();
    device_ = ctx.device();

    // Create dedicated descriptor pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
    };
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 100;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = pool_sizes;
    if (vkCreateDescriptorPool(device_, &pool_info, nullptr, &imgui_pool_) != VK_SUCCESS) {
        std::fprintf(stderr, "[ImGui] Failed to create descriptor pool\n");
        return false;
    }

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Dark style with custom colors for RPG Maker vibe
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.TabRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(6, 4);
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    // Color scheme: dark blue-gray like RPG Maker
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]           = ImVec4(0.10f, 0.10f, 0.14f, 0.96f);
    colors[ImGuiCol_TitleBg]            = ImVec4(0.08f, 0.08f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.14f, 0.14f, 0.22f, 1.00f);
    colors[ImGuiCol_Tab]                = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);
    colors[ImGuiCol_TabSelected]        = ImVec4(0.20f, 0.22f, 0.32f, 1.00f);
    colors[ImGuiCol_TabHovered]         = ImVec4(0.26f, 0.28f, 0.40f, 0.80f);
    colors[ImGuiCol_Button]             = ImVec4(0.18f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.26f, 0.30f, 0.42f, 1.00f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.14f, 0.16f, 0.24f, 1.00f);
    colors[ImGuiCol_Header]             = ImVec4(0.18f, 0.20f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.26f, 0.28f, 0.40f, 0.80f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.14f, 0.16f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.14f, 0.14f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.22f, 0.22f, 0.30f, 1.00f);
    colors[ImGuiCol_CheckMark]          = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]         = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_Separator]          = ImVec4(0.28f, 0.28f, 0.36f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]        = ImVec4(0.08f, 0.08f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]      = ImVec4(0.22f, 0.22f, 0.30f, 1.00f);

    // Init platform/renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = ctx.instance();
    init_info.PhysicalDevice = ctx.physical_device();
    init_info.Device = ctx.device();
    init_info.QueueFamily = ctx.graphics_family();
    init_info.Queue = ctx.graphics_queue();
    init_info.DescriptorPool = imgui_pool_;
    init_info.MinImageCount = 2;
    init_info.ImageCount = (uint32_t)ctx.swapchain_image_views().size();
    init_info.PipelineInfoMain.RenderPass = renderer.render_pass();
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = check_vk;

    ImGui_ImplVulkan_Init(&init_info);

    initialized_ = true;
    std::printf("[ImGui] Initialized successfully\n");
    return true;
}

void ImGuiIntegration::shutdown() {
    if (!initialized_) return;
    if (device_) {
        vkDeviceWaitIdle(device_);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        if (imgui_pool_) {
            vkDestroyDescriptorPool(device_, imgui_pool_, nullptr);
            imgui_pool_ = VK_NULL_HANDLE;
        }
    }
    initialized_ = false;
}

void ImGuiIntegration::new_frame() {
    if (!initialized_) return;
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiIntegration::render(VkCommandBuffer cmd) {
    if (!initialized_) return;
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

bool ImGuiIntegration::wants_keyboard() const {
    return initialized_ && ImGui::GetIO().WantCaptureKeyboard;
}

bool ImGuiIntegration::wants_mouse() const {
    return initialized_ && ImGui::GetIO().WantCaptureMouse;
}

} // namespace eb

#endif // EB_ANDROID
