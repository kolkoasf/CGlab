#pragma once
#include <string>
#include "app_shared.hpp"

namespace application {

bool beginOneShotCommands(VkDevice device, uint32_t queue_family,
                          VkCommandPool& out_pool, VkCommandBuffer& out_cb);
void submitOneShot(VkDevice device, VkQueue queue,
                   VkCommandPool pool, VkCommandBuffer cb);

bool createStagingBuffer();
bool createImageAndView(uint32_t width, uint32_t height, VkDeviceSize image_offset, Texture& out_texture);
bool loadTextureFromFile(const std::string& path, Texture& out_texture);
bool createSolidColorTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a, Texture& out_texture);
bool flushTextureUploads();
void destroyTextures();

} // namespace application