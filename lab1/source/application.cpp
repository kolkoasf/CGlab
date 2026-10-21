#include "application.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp> 
#include <glm/gtc/matrix_transform.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace application {

namespace {

struct Vertex {
	glm::vec3 position;
	glm::vec3 color;
};

constexpr int sphere_stacks = 9;
constexpr int sphere_slices = 9;

constexpr uint32_t object_count = 2;

struct GlobalUniforms {
	glm::mat4 mvp;
	glm::vec4 ui_color;
};

std::vector<Vertex> sphere_vertices;
std::vector<uint32_t> sphere_indices;

VkBuffer vk_vertex_buffer = VK_NULL_HANDLE;
VmaAllocation vma_vertex_buffer = VK_NULL_HANDLE;

VkBuffer vk_index_buffer = VK_NULL_HANDLE;
VmaAllocation vma_index_buffer = VK_NULL_HANDLE;

VkBuffer vk_uniform_buffers[object_count] = {};
VmaAllocation vma_uniform_buffers[object_count] = {};
GlobalUniforms* vk_uniform_mapped[object_count] = {};

VkDescriptorSetLayout vk_descriptor_set_layout = VK_NULL_HANDLE;
VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
VkDescriptorSet vk_descriptor_sets[object_count] = {};

VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE;
VkPipeline vk_pipeline = VK_NULL_HANDLE;

bool ui_use_perspective = true;
float ui_fov_deg = 60.0f;
float ui_ortho_height = 2.5f;

struct Transform {
	glm::vec3 position;
	glm::vec3 rotation_deg;
	glm::vec3 scale;
};

Transform ui_transform[object_count] = {
	{ { -1.2f, 0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
	{ {  1.2f, 0.0f, 0.0f }, {  0.0f, 45.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
};

glm::vec4 ui_color[object_count] = {
	glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
	glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
};

struct Animation {
	bool playing;
	float speed;
	float radius;
	float height;
	float spin_speed;
};

Animation ui_animation = {
	.playing = true,
	.speed = 1.0f,
	.radius = 1.5f,
	.height = 0.4f,
	.spin_speed = 45.0f,
};

float ui_animation_time = 0.0f;

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

			const glm::vec3 position(
				sin_theta * cos_phi,
				cos_theta,
				sin_theta * sin_phi);

			const glm::vec3 color = position * 0.5f + 0.5f;

			sphere_vertices.push_back({ position, color });
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

bool createSphereBuffers() {
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
			std::cerr << "Failed to create and allocate vertex buffer\n";
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
			std::cerr << "Failed to create and allocate index buffer\n";
			return false;
		}

		std::memcpy(info.pMappedData, sphere_indices.data(), buffer_info.size);
	}

	return true;
}

bool createUniformBuffers() {
	const VmaAllocator allocator = graphics::internal::context.allocator;

	for (uint32_t i = 0; i < object_count; ++i) {
		const VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(GlobalUniforms),
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
		                    &vk_uniform_buffers[i], &vma_uniform_buffers[i], &info) != VK_SUCCESS) {
			std::cerr << "Failed to create and allocate uniform buffer #" << i << "\n";
			return false;
		}

		vk_uniform_mapped[i] = static_cast<GlobalUniforms*>(info.pMappedData);
	}

	return true;
}

bool createDescriptorSetLayout() {
	const VkDescriptorSetLayoutBinding binding = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	const VkDescriptorSetLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &binding,
	};

	if (vkCreateDescriptorSetLayout(graphics::internal::context.device, &layout_info, nullptr,
	                                &vk_descriptor_set_layout) != VK_SUCCESS) {
		std::cerr << "Failed to create descriptor set layout\n";
		return false;
	}

	return true;
}

bool createDescriptorPool() {
	const VkDescriptorPoolSize pool_size = {
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = object_count,
	};

	const VkDescriptorPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = object_count,
		.poolSizeCount = 1,
		.pPoolSizes = &pool_size,
	};

	if (vkCreateDescriptorPool(graphics::internal::context.device, &pool_info, nullptr,
	                           &vk_descriptor_pool) != VK_SUCCESS) {
		std::cerr << "Failed to create descriptor pool\n";
		return false;
	}

	return true;
}

bool allocateDescriptorSets() {
	VkDescriptorSetLayout layouts[object_count];
	for (uint32_t i = 0; i < object_count; ++i) {
		layouts[i] = vk_descriptor_set_layout;
	}

	const VkDescriptorSetAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = vk_descriptor_pool,
		.descriptorSetCount = object_count,
		.pSetLayouts = layouts,
	};

	if (vkAllocateDescriptorSets(graphics::internal::context.device, &alloc_info,
	                             vk_descriptor_sets) != VK_SUCCESS) {
		std::cerr << "Failed to allocate descriptor sets\n";
		return false;
	}

	return true;
}

void writeDescriptorSets() {
	for (uint32_t i = 0; i < object_count; ++i) {
		const VkDescriptorBufferInfo buffer_info = {
			.buffer = vk_uniform_buffers[i],
			.offset = 0,
			.range = sizeof(GlobalUniforms),
		};

		const VkWriteDescriptorSet write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = vk_descriptor_sets[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &buffer_info,
		};

		vkUpdateDescriptorSets(graphics::internal::context.device, 1, &write, 0, nullptr);
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
		std::cerr << "Shader file is empty: " << path << '\n';
		return VK_NULL_HANDLE;
	}

	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(static_cast<std::size_t>(size));
	if (!file.read(buffer.data(), size)) {
		std::cerr << "Failed to read shader file: " << path << '\n';
		return VK_NULL_HANDLE;
	}

	const VkShaderModuleCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = static_cast<std::size_t>(size),
		.pCode = reinterpret_cast<const uint32_t*>(buffer.data()),
	};

	VkShaderModule module = VK_NULL_HANDLE;
	if (vkCreateShaderModule(graphics::internal::context.device, &info, nullptr, &module) != VK_SUCCESS) {
		std::cerr << "Failed to create shader module from: " << path << '\n';
		return VK_NULL_HANDLE;
	}

	return module;
}

bool createPipelineLayout() {
	const VkPipelineLayoutCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &vk_descriptor_set_layout,
	};

	if (vkCreatePipelineLayout(graphics::internal::context.device, &info, nullptr,
	                           &vk_pipeline_layout) != VK_SUCCESS) {
		std::cerr << "Failed to create pipeline layout\n";
		return false;
	}

	return true;
}

bool createGraphicsPipeline() {
	const std::string vert_path = std::string(SHADER_DIR) + "sphere.vert.spv";
	const std::string frag_path = std::string(SHADER_DIR) + "sphere.frag.spv";

	const VkShaderModule vert_module = loadShaderModule(vert_path);
	if (vert_module == VK_NULL_HANDLE) {
		return false;
	}

	const VkShaderModule frag_module = loadShaderModule(frag_path);
	if (frag_module == VK_NULL_HANDLE) {
		vkDestroyShaderModule(graphics::internal::context.device, vert_module, nullptr);
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
			.offset = offsetof(Vertex, color),
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
		.primitiveRestartEnable = VK_FALSE,
	};

	const VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	const VkPipelineRasterizationStateCreateInfo rasterization = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f,
	};

	const VkPipelineMultisampleStateCreateInfo multisample = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
	};

	const VkPipelineDepthStencilStateCreateInfo depth_stencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
	};

	const VkPipelineColorBlendAttachmentState color_attachment = {
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
		                  VK_COLOR_COMPONENT_G_BIT |
		                  VK_COLOR_COMPONENT_B_BIT |
		                  VK_COLOR_COMPONENT_A_BIT,
	};

	const VkPipelineColorBlendStateCreateInfo color_blend = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
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

	const VkResult result = vkCreateGraphicsPipelines(graphics::internal::context.device,
	                                                  VK_NULL_HANDLE, 1, &pipeline_info,
	                                                  nullptr, &vk_pipeline);

	vkDestroyShaderModule(graphics::internal::context.device, frag_module, nullptr);
	vkDestroyShaderModule(graphics::internal::context.device, vert_module, nullptr);

	if (result != VK_SUCCESS) {
		std::cerr << "Failed to create graphics pipeline\n";
		return false;
	}

	return true;
}

} // namespace

bool initialize() {
	generateSphere();

	if (!createSphereBuffers()) {
		std::cerr << "Failed to create sphere buffers\n";
		return false;
	}

		if (!createUniformBuffers()) {
		std::cerr << "Failed to create uniform buffers\n";
		return false;
	}

	if (!createDescriptorSetLayout()) {
		std::cerr << "Failed to create descriptor set layout\n";
		return false;
	}

	if (!createDescriptorPool()) {
		std::cerr << "Failed to create descriptor pool\n";
		return false;
	}

	if (!allocateDescriptorSets()) {
		std::cerr << "Failed to allocate descriptor sets\n";
		return false;
	}

	writeDescriptorSets();

	if (!createPipelineLayout()) {
		std::cerr << "Failed to create pipeline layout\n";
		return false;
	}

	if (!createGraphicsPipeline()) {
		std::cerr << "Failed to create graphics pipeline\n";
		return false;
	}

	return true;
}

void shutdown() {
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);

	if (vk_pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context.device, vk_pipeline, nullptr);
		vk_pipeline = VK_NULL_HANDLE;
	}

	if (vk_pipeline_layout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context.device, vk_pipeline_layout, nullptr);
		vk_pipeline_layout = VK_NULL_HANDLE;
	}

	if (vk_descriptor_pool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context.device, vk_descriptor_pool, nullptr);
		vk_descriptor_pool = VK_NULL_HANDLE;
	}

	if (vk_descriptor_set_layout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context.device, vk_descriptor_set_layout, nullptr);
		vk_descriptor_set_layout = VK_NULL_HANDLE;
	}

	for (uint32_t i = 0; i < object_count; ++i) {
		if (vk_uniform_buffers[i] != VK_NULL_HANDLE) {
			vmaDestroyBuffer(context.allocator, vk_uniform_buffers[i], vma_uniform_buffers[i]);
			vk_uniform_buffers[i] = VK_NULL_HANDLE;
			vma_uniform_buffers[i] = VK_NULL_HANDLE;
			vk_uniform_mapped[i] = nullptr;
		}
	}

	if (vk_vertex_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, vk_vertex_buffer, vma_vertex_buffer);
		vk_vertex_buffer = VK_NULL_HANDLE;
		vma_vertex_buffer = VK_NULL_HANDLE;
	}

	if (vk_index_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, vk_index_buffer, vma_index_buffer);
		vk_index_buffer = VK_NULL_HANDLE;
		vma_index_buffer = VK_NULL_HANDLE;
	}
}

void update([[maybe_unused]] double time) {
	const auto& context = graphics::internal::context;

	const uint32_t width = context.swapchain_extent.width;
	const uint32_t height = context.swapchain_extent.height;
	const float aspect = (height > 0) ? float(width) / float(height) : 1.0f;

	const glm::mat4 view = glm::lookAt(
		glm::vec3(0.0f, 0.0f, 3.0f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f));

	glm::mat4 projection;
	if (ui_use_perspective) {
		projection = glm::perspective(glm::radians(ui_fov_deg), aspect, 0.1f, 100.0f);
	} else {
		const float h = ui_ortho_height;
		const float w = h * aspect;
		projection = glm::ortho(-w, w, -h, h, 0.1f, 100.0f);
	}

	ImGui::Begin("Scene");

	if (ImGui::CollapsingHeader("Projection")) {
		ImGui::Checkbox("Perspective", &ui_use_perspective);
		if (ui_use_perspective) {
			ImGui::SliderFloat("FOV", &ui_fov_deg, 20.0f, 120.0f, "%.0f deg");
		} else {
			ImGui::SliderFloat("Ortho height", &ui_ortho_height, 0.5f, 10.0f, "%.2f");
		}
	}

	if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Playing", &ui_animation.playing);
		ImGui::SliderFloat("Speed", &ui_animation.speed, 0.0f, 5.0f, "%.2f");
		ImGui::SliderFloat("Radius", &ui_animation.radius, 0.2f, 3.0f, "%.2f");
		ImGui::SliderFloat("Height", &ui_animation.height, 0.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Spin", &ui_animation.spin_speed, 0.0f, 360.0f, "%.0f deg/s");

		if (ImGui::Button("Reset")) {
			ui_animation_time = 0.0f;
		}
	}

	if (ui_animation.playing) {
		const float dt = ImGui::GetIO().DeltaTime;
		ui_animation_time += dt * ui_animation.speed;

		const float t = ui_animation_time;
		const float r = ui_animation.radius;
		const float h = ui_animation.height;

		ui_transform[0].position = glm::vec3(
			r * std::cos(t),
			h * std::sin(2.0f * t),
			r * std::sin(t));

		const float spin = ui_animation.spin_speed * t;
		ui_transform[0].rotation_deg = glm::vec3(
			spin * 0.7f,
			spin,
			spin * 0.3f);
	}

	for (uint32_t i = 0; i < object_count; ++i) {
		Transform& t = ui_transform[i];
		const std::string header = "Object " + std::to_string(i);
		if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PushID(int(i));
			ImGui::ColorEdit4("Color", &ui_color[i].x);

			ImGui::DragFloat3("Position", &t.position.x, 0.01f, -10.0f, 10.0f, "%.2f");
			ImGui::DragFloat3("Rotation", &t.rotation_deg.x, 1.0f, -360.0f, 360.0f, "%.0f deg");
			ImGui::DragFloat3("Scale",    &t.scale.x,    0.01f,   0.05f,  5.0f, "%.2f");

			ImGui::PopID();
		}
	}

	ImGui::End();

	for (uint32_t i = 0; i < object_count; ++i) {
		const Transform& t = ui_transform[i];

		glm::mat4 model = glm::translate(glm::mat4(1.0f), t.position);
		model = glm::rotate(model, glm::radians(t.rotation_deg.x), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, glm::radians(t.rotation_deg.y), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(t.rotation_deg.z), glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::scale(model, t.scale);

		vk_uniform_mapped[i]->mvp = projection * view * model;
		vk_uniform_mapped[i]->ui_color = ui_color[i];
	}
}

void render(const graphics::internal::FrameData& fd) {
	const auto& context = graphics::internal::context;

	vkResetCommandBuffer(fd.command_buffer, 0);

	const VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	vkBeginCommandBuffer(fd.command_buffer, &begin_info);

	const VkClearValue clear_values[] = {
		{ .color = { .float32 = { 0.1f, 0.1f, 0.1f, 1.0f } } },
		{ .depthStencil = { 1.0f, 0 } },
	};

	const VkRenderPassBeginInfo render_pass_begin = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = context.render_pass,
		.framebuffer = fd.framebuffer,
		.renderArea = { .extent = context.swapchain_extent },
		.clearValueCount = 2,
		.pClearValues = clear_values,
	};
	vkCmdBeginRenderPass(fd.command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);

	const VkViewport viewport = {
		.x = 0.0f,
		.y = 0.0f,
		.width = float(context.swapchain_extent.width),
		.height = float(context.swapchain_extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	vkCmdSetViewport(fd.command_buffer, 0, 1, &viewport);

	const VkRect2D scissor = {
		.offset = { 0, 0 },
		.extent = context.swapchain_extent,
	};
	vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);

	vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);

	const VkDeviceSize vertex_offset = 0;
	vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vk_vertex_buffer, &vertex_offset);
	vkCmdBindIndexBuffer(fd.command_buffer, vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);

	const uint32_t index_count = static_cast<uint32_t>(sphere_indices.size());

	for (uint32_t i = 0; i < object_count; ++i) {
		vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		                        vk_pipeline_layout, 0, 1, &vk_descriptor_sets[i], 0, nullptr);
		vkCmdDrawIndexed(fd.command_buffer, index_count, 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(fd.command_buffer);

	vkEndCommandBuffer(fd.command_buffer);
}

} // namespace application