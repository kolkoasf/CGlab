#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace application {

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
};

constexpr int sphere_stacks = 9;
constexpr int sphere_slices = 9;
constexpr uint32_t object_count = 2;
constexpr uint32_t MAX_POINT_LIGHTS = 32;
constexpr uint32_t MAX_SPOT_LIGHTS = 32;

struct SceneUniforms {
	glm::mat4 view_projection;
	glm::vec4 camera_position;
	glm::vec4 directional_light_direction;
	glm::vec4 directional_light_color;
	glm::vec4 ambient_light;
	uint32_t point_light_count;
	uint32_t spot_light_count;
	uint32_t _padding[2];
};

struct ModelUniform {
	glm::mat4 model;
	glm::vec4 albedo;
	glm::vec4 specular;
	float alpha;
	float _padding[3];
};

struct PointLight {
	glm::vec3 position;
	float intensity;
	glm::vec3 color;
	float _padding;
};

struct SpotLight {
	glm::vec3 position;
	float intensity;
	glm::vec3 direction;
	float inner_angle_cos;
	glm::vec3 color;
	float outer_angle_cos;
};

struct LineVertex {
	glm::vec3 position;
};

struct Transform {
	glm::vec3 position;
	glm::vec3 rotation_deg;
	glm::vec3 scale;
};

struct Animation {
	bool playing;
	float speed;
	float radius;
	float height;
	float spin_speed;
};

struct PushData {
	glm::mat4 model;
	glm::vec4 color;
};

// Global state
extern std::vector<PointLight> point_lights;
extern std::vector<SpotLight> spot_lights;
extern bool ui_show_point_lights;
extern bool ui_show_spot_lights;

extern std::vector<Vertex> sphere_vertices;
extern std::vector<uint32_t> sphere_indices;
extern std::vector<LineVertex> line_vertices;

extern VkBuffer vk_vertex_buffer;
extern VmaAllocation vma_vertex_buffer;
extern VkBuffer vk_index_buffer;
extern VmaAllocation vma_index_buffer;

extern VkBuffer vk_scene_uniform_buffer;
extern VmaAllocation vma_scene_uniform_buffer;
extern SceneUniforms* vk_scene_uniform_mapped;

extern VkBuffer vk_model_uniform_buffers[object_count];
extern VmaAllocation vma_model_uniform_buffers[object_count];
extern ModelUniform* vk_model_uniform_mapped[object_count];

extern VkBuffer vk_point_light_buffer;
extern VmaAllocation vma_point_light_buffer;
extern void* vk_point_light_mapped;

extern VkBuffer vk_spot_light_buffer;
extern VmaAllocation vma_spot_light_buffer;
extern void* vk_spot_light_mapped;

extern VkBuffer vk_line_buffer;
extern VmaAllocation vma_line_buffer;
extern void* vk_line_mapped;

extern VkDescriptorSetLayout vk_scene_descriptor_set_layout;
extern VkDescriptorSetLayout vk_model_descriptor_set_layout;
extern VkDescriptorPool vk_descriptor_pool;
extern VkDescriptorSet vk_scene_descriptor_set;
extern VkDescriptorSet vk_model_descriptor_sets[object_count];

extern VkPipelineLayout vk_pipeline_layout;
extern VkPipeline vk_pipeline;
extern VkPipeline vk_marker_sphere_pipeline;
extern VkPipeline vk_marker_line_pipeline;

extern bool ui_use_perspective;
extern float ui_fov_deg;
extern float ui_ortho_height;
extern glm::vec3 ui_camera_position;
extern float ui_camera_yaw;
extern float ui_camera_pitch;

extern Transform ui_transform[object_count];
extern glm::vec3 ui_directional_light_direction;
extern glm::vec3 ui_directional_light_color;
extern glm::vec3 ui_ambient_light;
extern glm::vec3 ui_specular[object_count];
extern float ui_alpha[object_count];
extern glm::vec4 ui_color[object_count];

extern Animation ui_animation;
extern float ui_animation_time;

} // namespace application