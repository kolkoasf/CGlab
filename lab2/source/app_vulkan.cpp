#include "app_vulkan.hpp"
#include "app_shared.hpp"
#include "graphics_internal.hpp"

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <glm/gtc/constants.hpp>

namespace application {

namespace {

void generateSphere() {
	sphere_vertices.clear();
	sphere_indices.clear();
	sphere_vertices.reserve(std::size_t(sphere_stacks + 1) * std::size_t(sphere_slices + 1));
	sphere_indices.reserve(std::size_t(sphere_stacks) * std::size_t(sphere_slices) * 6);

	const float pi = glm::pi<float>();
	const float two_pi = glm::two_pi<float>();
	for (int i = 0; i <= sphere_stacks; ++i) {
		const float theta = pi * float(i) / float(sphere_stacks);
		const float sin_theta = std::sin(theta);
		const float cos_theta = std::cos(theta);
		for (int j = 0; j <= sphere_slices; ++j) {
			const float phi = two_pi * float(j) / float(sphere_slices);
			const float sin_phi = std::sin(phi);
			const float cos_phi = std::cos(phi);

			const glm::vec3 position(sin_theta * cos_phi, cos_theta, sin_theta * sin_phi);
			const glm::vec3 normal = glm::normalize(position);
			sphere_vertices.push_back({ position, normal });
		}
	}

	const uint32_t row_stride = uint32_t(sphere_slices + 1);
	for (int i = 0; i < sphere_stacks; ++i) {
		for (int j = 0; j < sphere_slices; ++j) {
			const uint32_t a = uint32_t(i) * row_stride + uint32_t(j);
			const uint32_t b = a + row_stride;
			const uint32_t c = a + 1;
			const uint32_t d = b + 1;

			sphere_indices.push_back(a);
			sphere_indices.push_back(b);
			sphere_indices.push_back(c);
			sphere_indices.push_back(c);
			sphere_indices.push_back(b);
			sphere_indices.push_back(d);
		}
	}
}

VkShaderModule loadShaderModule(const std::string& path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) {
		std::cerr << "Failed to open shader file: " << path << '\n';
		return VK_NULL_HANDLE;
	}

	const std::streamsize size = file.tellg();
	if (size <= 0) {
		return VK_NULL_HANDLE;
	}

	file.seekg(0, std::ios::beg);
	std::vector<char> buffer(static_cast<std::size_t>(size));
	if (!file.read(buffer.data(), size)) {
		return VK_NULL_HANDLE;
	}

	const VkShaderModuleCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = static_cast<std::size_t>(size),
		.pCode = reinterpret_cast<const uint32_t*>(buffer.data()),
	};

	VkShaderModule module = VK_NULL_HANDLE;
	VkResult result = vkCreateShaderModule(graphics::internal::context.device, &info, nullptr, &module);
	if (result != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}
	return module;
}

} // namespace

bool createLineBuffer() {
	const VmaAllocator allocator = graphics::internal::context.allocator;
	const VkDeviceSize buffer_size = sizeof(LineVertex) * MAX_SPOT_LIGHTS * 8;

	const VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = buffer_size,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo alloc_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		         VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	VmaAllocationInfo info{};
	if (vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
	                    &vk_line_buffer, &vma_line_buffer, &info) != VK_SUCCESS) {
		std::cerr << "Failed to create line buffer\n";
		return false;
	}
	vk_line_mapped = info.pMappedData;
	return true;
}

bool createSphereBuffers() {
	generateSphere();
	const VmaAllocator allocator = graphics::internal::context.allocator;

	{
		const VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(Vertex) * sphere_vertices.size(),
			.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		};
		const VmaAllocationCreateInfo alloc_info = {
			.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
			         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
		};
		VmaAllocationInfo info{};
		if (vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
		                    &vk_vertex_buffer, &vma_vertex_buffer, &info) != VK_SUCCESS) {
			return false;
		}
		std::memcpy(info.pMappedData, sphere_vertices.data(), buffer_info.size);
	}

	{
		const VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(uint32_t) * sphere_indices.size(),
			.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		};
		const VmaAllocationCreateInfo alloc_info = {
			.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
			         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
		};
		VmaAllocationInfo info{};
		if (vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
		                    &vk_index_buffer, &vma_index_buffer, &info) != VK_SUCCESS) {
			return false;
		}
		std::memcpy(info.pMappedData, sphere_indices.data(), buffer_info.size);
	}
	return true;
}

bool createUniformBuffers() {
	const VmaAllocator allocator = graphics::internal::context.allocator;

	{
		const VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(SceneUniforms),
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		};
		const VmaAllocationCreateInfo alloc_info = {
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
			         VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
		};
		VmaAllocationInfo info{};
		if (vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
		                    &vk_scene_uniform_buffer, &vma_scene_uniform_buffer, &info) != VK_SUCCESS) {
			return false;
		}
		vk_scene_uniform_mapped = static_cast<SceneUniforms*>(info.pMappedData);
	}

	for (uint32_t i = 0; i < object_count; ++i) {
		const VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(ModelUniform),
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		};
		const VmaAllocationCreateInfo alloc_info = {
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
			         VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
		};
		VmaAllocationInfo info{};
		if (vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
		                    &vk_model_uniform_buffers[i], &vma_model_uniform_buffers[i], &info) != VK_SUCCESS) {
			return false;
		}
		vk_model_uniform_mapped[i] = static_cast<ModelUniform*>(info.pMappedData);
	}

	{
		const VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(PointLight) * MAX_POINT_LIGHTS,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		};
		const VmaAllocationCreateInfo alloc_info = {
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
			         VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
		};
		VmaAllocationInfo info{};
		if (vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
		                    &vk_point_light_buffer, &vma_point_light_buffer, &info) != VK_SUCCESS) {
			return false;
		}
		vk_point_light_mapped = info.pMappedData;
	}

	{
		const VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(SpotLight) * MAX_SPOT_LIGHTS,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		};
		const VmaAllocationCreateInfo alloc_info = {
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
			         VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
		};
		VmaAllocationInfo info{};
		if (vmaCreateBuffer(allocator, &buffer_info, &alloc_info,
		                    &vk_spot_light_buffer, &vma_spot_light_buffer, &info) != VK_SUCCESS) {
			return false;
		}
		vk_spot_light_mapped = info.pMappedData;
	}
	return true;
}

bool createDescriptorSetLayouts() {
	{
		const VkDescriptorSetLayoutBinding bindings[] = {
			{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			},
			{
				.binding = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			},
			{
				.binding = 2,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			},
		};
		const VkDescriptorSetLayoutCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 3,
			.pBindings = bindings,
		};
		VkResult result = vkCreateDescriptorSetLayout(
			graphics::internal::context.device, &info, nullptr, &vk_scene_descriptor_set_layout);
		if (result != VK_SUCCESS) {
			return false;
		}
	}

	{
		const VkDescriptorSetLayoutBinding binding = {
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		};
		const VkDescriptorSetLayoutCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 1,
			.pBindings = &binding,
		};
		VkResult result = vkCreateDescriptorSetLayout(
			graphics::internal::context.device, &info, nullptr, &vk_model_descriptor_set_layout);
		if (result != VK_SUCCESS) {
			return false;
		}
	}
	return true;
}

bool createDescriptorPool() {
	const VkDescriptorPoolSize pool_sizes[] = {
		{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1 + object_count,
		},
		{
			.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 2,
		},
	};
	const VkDescriptorPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1 + object_count,
		.poolSizeCount = 2,
		.pPoolSizes = pool_sizes,
	};
	VkResult result = vkCreateDescriptorPool(
		graphics::internal::context.device, &pool_info, nullptr, &vk_descriptor_pool);
	return result == VK_SUCCESS;
}

bool allocateDescriptorSets() {
	const VkDescriptorSetAllocateInfo scene_alloc = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = vk_descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &vk_scene_descriptor_set_layout,
	};
	VkResult result = vkAllocateDescriptorSets(
		graphics::internal::context.device, &scene_alloc, &vk_scene_descriptor_set);
	if (result != VK_SUCCESS) {
		return false;
	}

	const VkDescriptorSetLayout model_layouts[object_count] = {
		vk_model_descriptor_set_layout,
		vk_model_descriptor_set_layout,
	};
	const VkDescriptorSetAllocateInfo model_alloc = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = vk_descriptor_pool,
		.descriptorSetCount = object_count,
		.pSetLayouts = model_layouts,
	};
	result = vkAllocateDescriptorSets(
		graphics::internal::context.device, &model_alloc, vk_model_descriptor_sets);
	return result == VK_SUCCESS;
}

void writeDescriptorSets() {
	const VkDescriptorBufferInfo scene_ubo = {
		.buffer = vk_scene_uniform_buffer,
		.offset = 0,
		.range = sizeof(SceneUniforms),
	};
	const VkDescriptorBufferInfo point_buf = {
		.buffer = vk_point_light_buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE,
	};
	const VkDescriptorBufferInfo spot_buf = {
		.buffer = vk_spot_light_buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE,
	};

	VkWriteDescriptorSet scene_writes[3] = {
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = vk_scene_descriptor_set,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &scene_ubo,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = vk_scene_descriptor_set,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &point_buf,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = vk_scene_descriptor_set,
			.dstBinding = 2,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &spot_buf,
		},
	};
	vkUpdateDescriptorSets(graphics::internal::context.device, 3, scene_writes, 0, nullptr);

	for (uint32_t i = 0; i < object_count; ++i) {
		const VkDescriptorBufferInfo model_ubo = {
			.buffer = vk_model_uniform_buffers[i],
			.offset = 0,
			.range = sizeof(ModelUniform),
		};
		const VkWriteDescriptorSet write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = vk_model_descriptor_sets[i],
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &model_ubo,
		};
		vkUpdateDescriptorSets(graphics::internal::context.device, 1, &write, 0, nullptr);
	}
}

bool createPipelineLayout() {
	const VkDescriptorSetLayout set_layouts[] = {
		vk_scene_descriptor_set_layout,
		vk_model_descriptor_set_layout,
	};
	const VkPushConstantRange push_constant = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof(glm::mat4) + sizeof(glm::vec4),
	};
	const VkPipelineLayoutCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 2,
		.pSetLayouts = set_layouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &push_constant,
	};
	VkResult result = vkCreatePipelineLayout(
		graphics::internal::context.device, &info, nullptr, &vk_pipeline_layout);
	return result == VK_SUCCESS;
}

bool createGraphicsPipeline() {
	const VkShaderModule vert_module = loadShaderModule(std::string(SHADER_DIR) + "sphere.vert.spv");
	const VkShaderModule frag_module = loadShaderModule(std::string(SHADER_DIR) + "sphere.frag.spv");
	if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
		return false;
	}

	const VkPipelineShaderStageCreateInfo stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vert_module,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = frag_module,
			.pName = "main",
		},
	};

	const VkVertexInputBindingDescription vertex_binding = {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
	const VkVertexInputAttributeDescription vertex_attributes[] = {
		{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, position),
		},
		{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, normal),
		},
	};
	const VkPipelineVertexInputStateCreateInfo vertex_input = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertex_binding,
		.vertexAttributeDescriptionCount = 2,
		.pVertexAttributeDescriptions = vertex_attributes,
	};

	const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};
	const VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};
	const VkPipelineRasterizationStateCreateInfo rasterization = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f,
	};
	const VkPipelineMultisampleStateCreateInfo multisample = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};
	const VkPipelineDepthStencilStateCreateInfo depth_stencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
	};
	const VkPipelineColorBlendAttachmentState color_attachment = {
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
		                  VK_COLOR_COMPONENT_G_BIT |
		                  VK_COLOR_COMPONENT_B_BIT |
		                  VK_COLOR_COMPONENT_A_BIT,
	};
	const VkPipelineColorBlendStateCreateInfo color_blend = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_attachment,
	};
	const VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	const VkPipelineDynamicStateCreateInfo dynamic_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamic_states,
	};

	const VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = stages,
		.pVertexInputState = &vertex_input,
		.pInputAssemblyState = &input_assembly,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterization,
		.pMultisampleState = &multisample,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blend,
		.pDynamicState = &dynamic_state,
		.layout = vk_pipeline_layout,
		.renderPass = graphics::internal::context.render_pass,
		.subpass = 0,
	};

	VkResult result = vkCreateGraphicsPipelines(
		graphics::internal::context.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &vk_pipeline);
	vkDestroyShaderModule(graphics::internal::context.device, frag_module, nullptr);
	vkDestroyShaderModule(graphics::internal::context.device, vert_module, nullptr);
	return result == VK_SUCCESS;
}

bool createMarkerPipelines() {
	const VkShaderModule vert_module = loadShaderModule(std::string(SHADER_DIR) + "marker.vert.spv");
	const VkShaderModule frag_module = loadShaderModule(std::string(SHADER_DIR) + "marker.frag.spv");
	if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
		return false;
	}

	const VkPipelineShaderStageCreateInfo stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vert_module,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = frag_module,
			.pName = "main",
		},
	};

	const VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};
	const VkPipelineRasterizationStateCreateInfo rasterization = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f,
	};
	const VkPipelineMultisampleStateCreateInfo multisample = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};
	const VkPipelineDepthStencilStateCreateInfo depth_stencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
	};
	const VkPipelineColorBlendAttachmentState color_attachment = {
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
		                  VK_COLOR_COMPONENT_G_BIT |
		                  VK_COLOR_COMPONENT_B_BIT |
		                  VK_COLOR_COMPONENT_A_BIT,
	};
	const VkPipelineColorBlendStateCreateInfo color_blend = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_attachment,
	};
	const VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	const VkPipelineDynamicStateCreateInfo dynamic_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamic_states,
	};

	{
		const VkVertexInputBindingDescription binding = {
			.binding = 0,
			.stride = sizeof(Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		};
		const VkVertexInputAttributeDescription attrs[] = {
			{
				.location = 0,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = offsetof(Vertex, position),
			},
			{
				.location = 1,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = offsetof(Vertex, normal),
			},
		};
		const VkPipelineVertexInputStateCreateInfo vertex_input = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &binding,
			.vertexAttributeDescriptionCount = 2,
			.pVertexAttributeDescriptions = attrs,
		};
		const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		};

		const VkGraphicsPipelineCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = 2,
			.pStages = stages,
			.pVertexInputState = &vertex_input,
			.pInputAssemblyState = &input_assembly,
			.pViewportState = &viewport_state,
			.pRasterizationState = &rasterization,
			.pMultisampleState = &multisample,
			.pDepthStencilState = &depth_stencil,
			.pColorBlendState = &color_blend,
			.pDynamicState = &dynamic_state,
			.layout = vk_pipeline_layout,
			.renderPass = graphics::internal::context.render_pass,
			.subpass = 0,
		};
		vkCreateGraphicsPipelines(
			graphics::internal::context.device, VK_NULL_HANDLE, 1, &info, nullptr, &vk_marker_sphere_pipeline);
	}

	{
		const VkVertexInputBindingDescription binding = {
			.binding = 0,
			.stride = sizeof(LineVertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		};
		const VkVertexInputAttributeDescription attrs[] = {
			{
				.location = 0,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = 0,
			},
		};
		const VkPipelineVertexInputStateCreateInfo vertex_input = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &binding,
			.vertexAttributeDescriptionCount = 1,
			.pVertexAttributeDescriptions = attrs,
		};
		const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
		};

		const VkGraphicsPipelineCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = 2,
			.pStages = stages,
			.pVertexInputState = &vertex_input,
			.pInputAssemblyState = &input_assembly,
			.pViewportState = &viewport_state,
			.pRasterizationState = &rasterization,
			.pMultisampleState = &multisample,
			.pDepthStencilState = &depth_stencil,
			.pColorBlendState = &color_blend,
			.pDynamicState = &dynamic_state,
			.layout = vk_pipeline_layout,
			.renderPass = graphics::internal::context.render_pass,
			.subpass = 0,
		};
		vkCreateGraphicsPipelines(
			graphics::internal::context.device, VK_NULL_HANDLE, 1, &info, nullptr, &vk_marker_line_pipeline);
	}

	vkDestroyShaderModule(graphics::internal::context.device, frag_module, nullptr);
	vkDestroyShaderModule(graphics::internal::context.device, vert_module, nullptr);
	return true;
}

void destroyVulkanResources() {
	auto& context = graphics::internal::context;

	if (vk_pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context.device, vk_pipeline, nullptr);
	}
	if (vk_marker_sphere_pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context.device, vk_marker_sphere_pipeline, nullptr);
	}
	if (vk_marker_line_pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context.device, vk_marker_line_pipeline, nullptr);
	}
	if (vk_pipeline_layout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context.device, vk_pipeline_layout, nullptr);
	}

	if (vk_scene_descriptor_set_layout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context.device, vk_scene_descriptor_set_layout, nullptr);
	}
	if (vk_model_descriptor_set_layout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context.device, vk_model_descriptor_set_layout, nullptr);
	}

    if (vk_descriptor_pool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context.device, vk_descriptor_pool, nullptr);
	}

	if (vk_scene_uniform_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, vk_scene_uniform_buffer, vma_scene_uniform_buffer);
	}
	for (uint32_t i = 0; i < object_count; ++i) {
		if (vk_model_uniform_buffers[i] != VK_NULL_HANDLE) {
			vmaDestroyBuffer(context.allocator, vk_model_uniform_buffers[i], vma_model_uniform_buffers[i]);
		}
	}
	if (vk_point_light_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, vk_point_light_buffer, vma_point_light_buffer);
	}
	if (vk_spot_light_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, vk_spot_light_buffer, vma_spot_light_buffer);
	}
	if (vk_vertex_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, vk_vertex_buffer, vma_vertex_buffer);
	}
	if (vk_index_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, vk_index_buffer, vma_index_buffer);
	}
	if (vk_line_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, vk_line_buffer, vma_line_buffer);
	}
}

} // namespace application