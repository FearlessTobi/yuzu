// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <vector>
#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"

namespace Vulkan {

UniqueShaderModule BuildShader(const VKDevice& device, std::size_t code_size, const u8* code_data) {
    // Avoid undefined behavior by copying to a staging allocation
    ASSERT(code_size % sizeof(u32) == 0);
    const auto data = std::make_unique<u32[]>(code_size / sizeof(u32));
    std::memcpy(data.get(), code_data, code_size);

    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    const vk::ShaderModuleCreateInfo shader_ci({}, code_size, data.get());
    vk::ShaderModule shader_module;
    if (dev.createShaderModule(&shader_ci, nullptr, &shader_module, dld) != vk::Result::eSuccess) {
        UNREACHABLE_MSG("Shader module failed to build!");
    }

    return UniqueShaderModule(shader_module, vk::ObjectDestroy(dev, nullptr, dld));
}

} // namespace Vulkan
