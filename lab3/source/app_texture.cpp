#include "app_texture.hpp"
#include "graphics_internal.hpp"

#include <lodepng.h>

#include <cstring>
#include <iostream>
#include <vector>

namespace application {

bool beginOneShotCommands(VkDevice device, uint32_t queue_family,
                          VkCommandPool& out_pool, VkCommandBuffer& out_cb) {
    const VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = queue_family,
    };
    if (vkCreateCommandPool(device, &pool_info, nullptr, &out_pool) != VK_SUCCESS) {
        std::cerr << "Failed to create one-shot command pool\n";
        return false;
    }

    const VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = out_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    if (vkAllocateCommandBuffers(device, &alloc_info, &out_cb) != VK_SUCCESS) {
        std::cerr << "Failed to allocate one-shot command buffer\n";
        return false;
    }

    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    return vkBeginCommandBuffer(out_cb, &begin_info) == VK_SUCCESS;
}

void submitOneShot(VkDevice device, VkQueue queue,
                   VkCommandPool pool, VkCommandBuffer cb) {
    vkEndCommandBuffer(cb);

    const VkFenceCreateInfo fence_info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(device, &fence_info, nullptr, &fence);

    const VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cb,
    };
    vkQueueSubmit(queue, 1, &submit, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(device, fence, nullptr);
    vkDestroyCommandPool(device, pool, nullptr);
}

bool createStagingBuffer() {
    const VmaAllocator allocator = graphics::internal::context.allocator;

    const VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = STAGING_BUFFER_SIZE,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    const VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VmaAllocationInfo info{};
    if (vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
                        &vk_staging_buffer, &vma_staging_buffer, &info) != VK_SUCCESS) {
        std::cerr << "Failed to create staging buffer\n";
        return false;
    }

    vk_staging_mapped = static_cast<uint8_t*>(info.pMappedData);
    vk_staging_offset = 0;
    return true;
}

bool createImageAndView(uint32_t width, uint32_t height, VkDeviceSize image_offset, Texture& out_texture) {
    const auto& context = graphics::internal::context;

    const VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = { width, height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    const VmaAllocationCreateInfo alloc_info = { .usage = VMA_MEMORY_USAGE_GPU_ONLY };
    if (vmaCreateImage(context.allocator, &image_info, &alloc_info,
                       &out_texture.image, &out_texture.allocation, nullptr) != VK_SUCCESS) {
        return false;
    }

    const VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = out_texture.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0, .levelCount = 1,
            .baseArrayLayer = 0, .layerCount = 1,
        },
    };
    if (vkCreateImageView(context.device, &view_info, nullptr, &out_texture.view) != VK_SUCCESS) {
        return false;
    }

    const VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    };
    if (vkCreateSampler(context.device, &sampler_info, nullptr, &out_texture.sampler) != VK_SUCCESS) {
        return false;
    }

    out_texture.staging_offset = image_offset;
    out_texture.width = width;
    out_texture.height = height;
    return true;
}

bool loadTextureFromFile(const std::string& path, Texture& out_texture) {
    unsigned int width = 0, height = 0;
    unsigned char* pixels = nullptr;
    const unsigned error = lodepng_decode32_file(&pixels, &width, &height, path.c_str());
    if (error != 0) {
        std::cerr << "Failed to decode PNG '" << path << "': "
                  << lodepng_error_text(error) << '\n';
        return false;
    }

    const VkDeviceSize image_size = VkDeviceSize(width) * VkDeviceSize(height) * 4;
    const VkDeviceSize image_offset = vk_staging_offset;
    std::memcpy(vk_staging_mapped + vk_staging_offset, pixels, image_size);
    vk_staging_offset += image_size;
    free(pixels);

    return createImageAndView(width, height, image_offset, out_texture);
}

bool createSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a, Texture& out_texture) {
    constexpr uint32_t W = 1, H = 1;
    const VkDeviceSize image_size = 4;

    const VkDeviceSize image_offset = vk_staging_offset;
    const uint8_t pixels[4] = { r, g, b, a };
    std::memcpy(vk_staging_mapped + vk_staging_offset, pixels, image_size);
    vk_staging_offset += image_size;

    return createImageAndView(W, H, image_offset, out_texture);
}

bool flushTextureUploads() {
    if (g_textures.empty()) {
        return true;
    }

    const auto& context = graphics::internal::context;

    VkCommandPool cmd_pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (!beginOneShotCommands(context.device, context.graphics_queue_index, cmd_pool, cmd)) {
        return false;
    }

    for (const Texture& tex : g_textures) {
        const VkImageMemoryBarrier to_dst = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = tex.image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0, .levelCount = 1,
                .baseArrayLayer = 0, .layerCount = 1,
            },
        };
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &to_dst);

        const VkBufferImageCopy region = {
            .bufferOffset = tex.staging_offset,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1,
            },
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { tex.width, tex.height, 1 },
        };
        vkCmdCopyBufferToImage(cmd, vk_staging_buffer, tex.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        const VkImageMemoryBarrier to_read = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = tex.image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0, .levelCount = 1,
                .baseArrayLayer = 0, .layerCount = 1,
            },
        };
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &to_read);
    }

    submitOneShot(context.device, context.graphics_queue, cmd_pool, cmd);
    return true;
}

void destroyTextures() {
    const auto& context = graphics::internal::context;
    for (Texture& tex : g_textures) {
        if (tex.sampler != VK_NULL_HANDLE) vkDestroySampler(context.device, tex.sampler, nullptr);
        if (tex.view != VK_NULL_HANDLE) vkDestroyImageView(context.device, tex.view, nullptr);
        if (tex.image != VK_NULL_HANDLE) vmaDestroyImage(context.allocator, tex.image, tex.allocation);
    }
    g_textures.clear();

    if (vk_staging_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(context.allocator, vk_staging_buffer, vma_staging_buffer);
        vk_staging_buffer = VK_NULL_HANDLE;
        vma_staging_buffer = VK_NULL_HANDLE;
        vk_staging_mapped = nullptr;
        vk_staging_offset = 0;
    }
}

} // namespace application