#pragma once

#include "engine/platform/input.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace eb {

class Platform {
public:
    virtual ~Platform() = default;

    virtual void poll_events() = 0;
    virtual bool should_close() const = 0;

    virtual VkSurfaceKHR create_surface(VkInstance instance) const = 0;
    virtual std::vector<const char*> get_required_extensions() const = 0;

    virtual int get_width() const = 0;
    virtual int get_height() const = 0;
    virtual bool was_resized() = 0;

    virtual const InputState& input() const = 0;
};

} // namespace eb
