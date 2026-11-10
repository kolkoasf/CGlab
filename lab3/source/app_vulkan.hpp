#pragma once

namespace application {

bool createLineBuffer();
bool createSphereBuffers();
bool createUniformBuffers();
bool createDescriptorSetLayouts();
bool createDescriptorPool();
bool allocateDescriptorSets();
void writeDescriptorSets();
bool createPipelineLayout();
bool createGraphicsPipeline();
bool createMarkerPipelines();
void destroyVulkanResources();

} // namespace application