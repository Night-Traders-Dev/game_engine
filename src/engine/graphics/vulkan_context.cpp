#include "engine/graphics/vulkan_context.h"
#include "engine/platform/platform.h"

#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <set>
#include <algorithm>

#ifdef __ANDROID__
#include <android/log.h>
#define VLOG_TAG "TWEngine-Vulkan"
#define VLOGI(...) __android_log_print(ANDROID_LOG_INFO, VLOG_TAG, __VA_ARGS__)
#define VLOGE(...) __android_log_print(ANDROID_LOG_ERROR, VLOG_TAG, __VA_ARGS__)
#else
#define VLOGI(...) std::printf(__VA_ARGS__)
#define VLOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace eb {

static const std::vector<const char*> validation_layers = {
    "VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*user_data*/) {
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::fprintf(stderr, "[Vulkan] %s\n", data->pMessage);
    }
    return VK_FALSE;
}

VulkanContext::VulkanContext(Platform& platform, bool vsync) : vsync_(vsync) {
    VLOGI("Creating VulkanContext...\n");
    create_instance(platform);
    VLOGI("Instance created\n");
    setup_debug_messenger();
    create_surface(platform);
    VLOGI("Surface created\n");
    pick_physical_device();
    VLOGI("Physical device selected\n");
    create_logical_device();
    VLOGI("Logical device created\n");
    create_command_pool();
    VLOGI("Command pool created\n");
    create_swapchain(platform.get_width(), platform.get_height());
    VLOGI("VulkanContext initialized\n");
}

VulkanContext::~VulkanContext() {
    if (device_) {
        vkDeviceWaitIdle(device_);
        cleanup_swapchain();
        vkDestroyCommandPool(device_, command_pool_, nullptr);
        vkDestroyDevice(device_, nullptr);
    }
    destroy_debug_messenger();
    if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (instance_) vkDestroyInstance(instance_, nullptr);
}

void VulkanContext::wait_idle() {
    vkDeviceWaitIdle(device_);
}

void VulkanContext::create_instance(Platform& platform) {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Twilight Game Engine";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "Twilight Engine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    // Check if validation layers are actually available
    bool use_validation = enable_validation_;
    if (use_validation) {
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> available(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available.data());

        bool found = false;
        for (const auto& layer : available) {
            if (std::strcmp(layer.layerName, validation_layers[0]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            VLOGI("Validation layer not available, skipping\n");
            use_validation = false;
        }
    }

    auto extensions = platform.get_required_extensions();

    // Check if debug utils extension is available before requesting it
    if (use_validation) {
        uint32_t ext_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
        std::vector<VkExtensionProperties> available_exts(ext_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, available_exts.data());

        bool debug_ext_found = false;
        for (const auto& ext : available_exts) {
            if (std::strcmp(ext.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
                debug_ext_found = true;
                break;
            }
        }
        if (debug_ext_found) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        } else {
            VLOGI("Debug utils extension not available, skipping validation\n");
            use_validation = false;
        }
    }

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    if (use_validation) {
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();
    }

    VLOGI("Creating Vulkan instance with %u extensions, %u layers\n",
          create_info.enabledExtensionCount, create_info.enabledLayerCount);

    VkResult inst_result = vkCreateInstance(&create_info, nullptr, &instance_);
    if (inst_result != VK_SUCCESS) {
        VLOGE("vkCreateInstance failed with error %d\n", static_cast<int>(inst_result));
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    validation_active_ = use_validation;
}

void VulkanContext::setup_debug_messenger() {
    if (!validation_active_) return;

    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debug_callback;

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
    if (func) {
        func(instance_, &info, nullptr, &debug_messenger_);
    }
}

void VulkanContext::destroy_debug_messenger() {
    if (!validation_active_ || debug_messenger_ == VK_NULL_HANDLE) return;
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
    if (func) {
        func(instance_, debug_messenger_, nullptr);
    }
}

void VulkanContext::create_surface(Platform& platform) {
    surface_ = platform.create_surface(instance_);
}

void VulkanContext::pick_physical_device() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
        throw std::runtime_error("No Vulkan-capable GPU found");
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    for (auto& dev : devices) {
        // Check queue families
        uint32_t qcount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, nullptr);
        std::vector<VkQueueFamilyProperties> families(qcount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, families.data());

        bool found_graphics = false, found_present = false;
        for (uint32_t i = 0; i < qcount; i++) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics_family_ = i;
                found_graphics = true;
            }
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &present_support);
            if (present_support) {
                present_family_ = i;
                found_present = true;
            }
            if (found_graphics && found_present) break;
        }

        // Check extension support
        uint32_t ext_count = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, nullptr);
        std::vector<VkExtensionProperties> available_exts(ext_count);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &ext_count, available_exts.data());

        std::set<std::string> required(device_extensions.begin(), device_extensions.end());
        for (auto& ext : available_exts) {
            required.erase(ext.extensionName);
        }

        if (found_graphics && found_present && required.empty()) {
            auto support = query_swapchain_support(dev);
            if (!support.formats.empty() && !support.present_modes.empty()) {
                physical_device_ = dev;
                VkPhysicalDeviceProperties props;
                vkGetPhysicalDeviceProperties(dev, &props);
                VLOGI("Using GPU: %s (Vulkan %u.%u.%u)\n", props.deviceName,
                      VK_VERSION_MAJOR(props.apiVersion),
                      VK_VERSION_MINOR(props.apiVersion),
                      VK_VERSION_PATCH(props.apiVersion));
                return;
            }
        }
    }

    throw std::runtime_error("Failed to find a suitable GPU");
}

void VulkanContext::create_logical_device() {
    std::set<uint32_t> unique_families = {graphics_family_, present_family_};

    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    float priority = 1.0f;
    for (uint32_t family : unique_families) {
        VkDeviceQueueCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueFamilyIndex = family;
        info.queueCount = 1;
        info.pQueuePriorities = &priority;
        queue_infos.push_back(info);
    }

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
    create_info.pQueueCreateInfos = queue_infos.data();
    create_info.pEnabledFeatures = &features;
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();

    if (vkCreateDevice(physical_device_, &create_info, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, present_family_, 0, &present_queue_);
}

void VulkanContext::create_command_pool() {
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = graphics_family_;

    if (vkCreateCommandPool(device_, &info, nullptr, &command_pool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

SwapchainSupport VulkanContext::query_swapchain_support(VkPhysicalDevice device) {
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &support.capabilities);

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, nullptr);
    if (format_count) {
        support.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, support.formats.data());
    }

    uint32_t mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &mode_count, nullptr);
    if (mode_count) {
        support.present_modes.resize(mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &mode_count, support.present_modes.data());
    }

    return support;
}

void VulkanContext::create_swapchain(int width, int height) {
    auto support = query_swapchain_support(physical_device_);

    // Choose format (prefer SRGB)
    VkSurfaceFormatKHR surface_format = support.formats[0];
    for (auto& fmt : support.formats) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = fmt;
            break;
        }
    }

    // Choose present mode
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // Always available (vsync)
    if (!vsync_) {
        for (auto& mode : support.present_modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                present_mode = mode;
                break;
            }
        }
    }

    // Choose extent
    VkExtent2D extent;
    if (support.capabilities.currentExtent.width != UINT32_MAX) {
        extent = support.capabilities.currentExtent;
    } else {
        extent.width = std::clamp(static_cast<uint32_t>(width),
                                  support.capabilities.minImageExtent.width,
                                  support.capabilities.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(height),
                                   support.capabilities.minImageExtent.height,
                                   support.capabilities.maxImageExtent.height);
    }
    // On Android in landscape, the surface may report portrait-ordered dimensions
    // if preTransform includes rotation. Since we use IDENTITY, ensure width > height.
#ifdef __ANDROID__
    if (extent.height > extent.width) {
        std::swap(extent.width, extent.height);
        VLOGI("Swapped extent to landscape: %ux%u", extent.width, extent.height);
    }
#endif

    uint32_t image_count = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && image_count > support.capabilities.maxImageCount) {
        image_count = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface_;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t family_indices[] = {graphics_family_, present_family_};
    if (graphics_family_ != present_family_) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    // On Android, the surface may have a rotation transform (portrait device in landscape).
    // Use IDENTITY so we render in the orientation we want — the compositor handles rotation.
    if (support.capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        create_info.preTransform = support.capabilities.currentTransform;
    }

    // Pick a supported composite alpha mode (Android often lacks OPAQUE)
    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR preferred[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
    };
    for (auto mode : preferred) {
        if (support.capabilities.supportedCompositeAlpha & mode) {
            composite_alpha = mode;
            break;
        }
    }
    create_info.compositeAlpha = composite_alpha;

    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VLOGI("Creating swapchain %ux%u, %u images, compositeAlpha=%u\n",
          extent.width, extent.height, image_count, static_cast<uint32_t>(composite_alpha));

    VkResult sc_result = vkCreateSwapchainKHR(device_, &create_info, nullptr, &swapchain_);
    if (sc_result != VK_SUCCESS) {
        VLOGE("vkCreateSwapchainKHR failed with error %d\n", static_cast<int>(sc_result));
        throw std::runtime_error("Failed to create swapchain");
    }

    swapchain_format_ = surface_format.format;
    swapchain_extent_ = extent;

    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr);
    swapchain_images_.resize(image_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swapchain_images_.data());

    swapchain_image_views_.resize(image_count);
    for (size_t i = 0; i < image_count; i++) {
        swapchain_image_views_[i] = create_image_view(swapchain_images_[i], swapchain_format_);
    }

    VLOGI("Swapchain created (%ux%u, %u images)\n", extent.width, extent.height, image_count);
}

void VulkanContext::cleanup_swapchain() {
    for (auto view : swapchain_image_views_) {
        vkDestroyImageView(device_, view, nullptr);
    }
    swapchain_image_views_.clear();
    if (swapchain_) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::recreate_swapchain(int width, int height) {
    vkDeviceWaitIdle(device_);
    cleanup_swapchain();
    create_swapchain(width, height);
}

// ── Buffer helpers ──

uint32_t VulkanContext::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

VkBuffer VulkanContext::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                      VkMemoryPropertyFlags properties, VkDeviceMemory& memory) {
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(device_, &info, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(device_, buffer, &mem_req);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_req.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device_, buffer, memory, 0);
    return buffer;
}

void VulkanContext::copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    auto cmd = begin_single_command();
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    end_single_command(cmd);
}

// ── Image helpers ──

VkImage VulkanContext::create_image(uint32_t width, uint32_t height, VkFormat format,
                                    VkImageTiling tiling, VkImageUsageFlags usage,
                                    VkMemoryPropertyFlags properties, VkDeviceMemory& memory) {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.format = format;
    info.tiling = tiling;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.samples = VK_SAMPLE_COUNT_1_BIT;

    VkImage image;
    if (vkCreateImage(device_, &info, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image");
    }

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(device_, image, &mem_req);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_req.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory");
    }

    vkBindImageMemory(device_, image, memory, 0);
    return image;
}

VkImageView VulkanContext::create_image_view(VkImage image, VkFormat format,
                                              VkImageAspectFlags aspect) {
    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = format;
    info.subresourceRange.aspectMask = aspect;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;

    VkImageView view;
    if (vkCreateImageView(device_, &info, nullptr, &view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view");
    }
    return view;
}

void VulkanContext::transition_image_layout(VkImage image, VkFormat /*format*/,
                                             VkImageLayout old_layout, VkImageLayout new_layout) {
    auto cmd = begin_single_command();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags src_stage, dst_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported layout transition");
    }

    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    end_single_command(cmd);
}

void VulkanContext::copy_buffer_to_image(VkBuffer buffer, VkImage image,
                                          uint32_t width, uint32_t height) {
    auto cmd = begin_single_command();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    end_single_command(cmd);
}

// ── Command helpers ──

VkCommandBuffer VulkanContext::begin_single_command() {
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = command_pool_;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device_, &info, &cmd);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    return cmd;
}

void VulkanContext::end_single_command(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    vkQueueSubmit(graphics_queue_, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_);

    vkFreeCommandBuffers(device_, command_pool_, 1, &cmd);
}

VkShaderModule VulkanContext::create_shader_module(const std::vector<char>& code) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module;
    if (vkCreateShaderModule(device_, &info, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
    return module;
}

} // namespace eb
