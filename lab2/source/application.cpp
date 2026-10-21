#include "application.hpp"
#include "app_shared.hpp"
#include "app_vulkan.hpp"
#include "app_ui.hpp"

#include <cmath>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace application {

bool initialize() {
	if (!createLineBuffer()) return false;
	if (!createSphereBuffers()) return false;
	if (!createUniformBuffers()) return false;
	if (!createDescriptorSetLayouts()) return false;
	if (!createDescriptorPool()) return false;
	if (!allocateDescriptorSets()) return false;

	writeDescriptorSets();

	if (!createPipelineLayout()) return false;
	if (!createGraphicsPipeline()) return false;
	if (!createMarkerPipelines()) return false;

	return true;
}

void shutdown() {
	destroyVulkanResources();
}

void update([[maybe_unused]] double time) {
	const auto& context = graphics::internal::context;

	const uint32_t width = context.swapchain_extent.width;
	const uint32_t height = context.swapchain_extent.height;
	const float aspect = (height > 0) ? float(width) / float(height) : 1.0f;

	glm::vec3 front;
	front.x = std::cos(glm::radians(ui_camera_yaw)) * std::cos(glm::radians(ui_camera_pitch));
	front.y = std::sin(glm::radians(ui_camera_pitch));
	front.z = std::sin(glm::radians(ui_camera_yaw)) * std::cos(glm::radians(ui_camera_pitch));
	front = glm::normalize(front);

	const glm::mat4 view = glm::lookAt(
		ui_camera_position,
		ui_camera_position + front,
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	glm::mat4 projection;
	if (ui_use_perspective) {
		projection = glm::perspective(glm::radians(ui_fov_deg), aspect, 0.1f, 100.0f);
	} else {
		const float h = ui_ortho_height;
		const float w = h * aspect;
		projection = glm::ortho(-w, w, -h, h, 0.1f, 100.0f);
	}

	line_vertices.clear();
	for (const auto& spot : spot_lights) {
		glm::vec3 dir = glm::normalize(spot.direction);
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
		if (glm::abs(glm::dot(dir, up)) > 0.99f) {
			up = glm::vec3(1.0f, 0.0f, 0.0f);
		}

		glm::vec3 right = glm::normalize(glm::cross(dir, up));
		up = glm::normalize(glm::cross(right, dir));

		float length = 2.0f;
		float angle = glm::acos(glm::clamp(spot.outer_angle_cos, -1.0f, 1.0f));
		float radius = length * glm::tan(angle);

		glm::vec3 tip = spot.position;
		glm::vec3 center = tip + dir * length;

		glm::vec3 p1 = center + right * radius;
		glm::vec3 p2 = center - right * radius;
		glm::vec3 p3 = center + up * radius;
		glm::vec3 p4 = center - up * radius;

		line_vertices.push_back({tip}); line_vertices.push_back({p1});
		line_vertices.push_back({tip}); line_vertices.push_back({p2});
		line_vertices.push_back({tip}); line_vertices.push_back({p3});
		line_vertices.push_back({tip}); line_vertices.push_back({p4});
	}

	if (!line_vertices.empty()) {
		std::memcpy(vk_line_mapped, line_vertices.data(), sizeof(LineVertex) * line_vertices.size());
	}

	vk_scene_uniform_mapped->view_projection = projection * view;
	vk_scene_uniform_mapped->camera_position = glm::vec4(ui_camera_position, 1.0f);
	vk_scene_uniform_mapped->directional_light_direction = 
		glm::vec4(ui_directional_light_direction, 0.0f);
	vk_scene_uniform_mapped->directional_light_color = 
		glm::vec4(ui_directional_light_color, 1.0f);
	vk_scene_uniform_mapped->ambient_light = glm::vec4(ui_ambient_light, 1.0f);
	vk_scene_uniform_mapped->point_light_count = 
		static_cast<uint32_t>(point_lights.size());
	vk_scene_uniform_mapped->spot_light_count = 
		static_cast<uint32_t>(spot_lights.size());

	if (!point_lights.empty()) {
		std::memcpy(
			vk_point_light_mapped, 
			point_lights.data(), 
			sizeof(PointLight) * point_lights.size()
		);
	}
	if (!spot_lights.empty()) {
		std::memcpy(
			vk_spot_light_mapped, 
			spot_lights.data(), 
			sizeof(SpotLight) * spot_lights.size()
		);
	}

	for (uint32_t i = 0; i < object_count; ++i) {
		const Transform& t = ui_transform[i];

		glm::mat4 model = glm::translate(glm::mat4(1.0f), t.position);
		model = glm::rotate(
			model, glm::radians(t.rotation_deg.x), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(
			model, glm::radians(t.rotation_deg.y), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(
			model, glm::radians(t.rotation_deg.z), glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::scale(model, t.scale);

		vk_model_uniform_mapped[i]->model = model;
		vk_model_uniform_mapped[i]->albedo = ui_color[i];
		vk_model_uniform_mapped[i]->specular = glm::vec4(ui_specular[i], 0.0f);
		vk_model_uniform_mapped[i]->alpha = ui_alpha[i];
	}

	drawUI();
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
		.extent = context.swapchain_extent 
	};
	vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);

	vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);

	const VkDeviceSize vertex_offset = 0;
	vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vk_vertex_buffer, &vertex_offset);
	vkCmdBindIndexBuffer(fd.command_buffer, vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);

	const uint32_t index_count = static_cast<uint32_t>(sphere_indices.size());

	vkCmdBindDescriptorSets(
		fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk_pipeline_layout, 0, 1, &vk_scene_descriptor_set, 0, nullptr
	);

	for (uint32_t i = 0; i < object_count; ++i) {
		vkCmdBindDescriptorSets(
			fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk_pipeline_layout, 1, 1, &vk_model_descriptor_sets[i], 0, nullptr
		);
		vkCmdDrawIndexed(fd.command_buffer, index_count, 1, 0, 0, 0);
	}

	glm::mat4 marker_scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.15f));

	if (ui_show_point_lights && !point_lights.empty()) {
		vkCmdBindPipeline(
			fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_marker_sphere_pipeline);
		vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vk_vertex_buffer, &vertex_offset);
		vkCmdBindIndexBuffer(fd.command_buffer, vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);

		for (const auto& light : point_lights) {
			PushData pc = { 
				glm::translate(glm::mat4(1.0f), light.position) * marker_scale, 
				glm::vec4(light.color, 1.0f) 
			};
			vkCmdPushConstants(
				fd.command_buffer, vk_pipeline_layout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0, sizeof(pc), &pc
			);
			vkCmdDrawIndexed(fd.command_buffer, index_count, 1, 0, 0, 0);
		}
	}

	if (ui_show_spot_lights) {
		if (!spot_lights.empty()) {
			vkCmdBindPipeline(
				fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_marker_sphere_pipeline);
			vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vk_vertex_buffer, &vertex_offset);
			vkCmdBindIndexBuffer(fd.command_buffer, vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);

			for (const auto& light : spot_lights) {
				PushData pc = { 
					glm::translate(glm::mat4(1.0f), light.position) * marker_scale, 
					glm::vec4(light.color, 1.0f) 
				};
				vkCmdPushConstants(
					fd.command_buffer, vk_pipeline_layout,
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0, sizeof(pc), &pc
				);
				vkCmdDrawIndexed(fd.command_buffer, index_count, 1, 0, 0, 0);
			}
		}

		if (!line_vertices.empty()) {
			vkCmdBindPipeline(
				fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_marker_line_pipeline);
			vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vk_line_buffer, &vertex_offset);

			PushData pc = { 
				glm::mat4(1.0f), 
				glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) 
			};
			vkCmdPushConstants(
				fd.command_buffer, vk_pipeline_layout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0, sizeof(pc), &pc
			);
			vkCmdDraw(
				fd.command_buffer, static_cast<uint32_t>(line_vertices.size()), 1, 0, 0);
		}
	}

	vkCmdEndRenderPass(fd.command_buffer);
	vkEndCommandBuffer(fd.command_buffer);
}

} // namespace application