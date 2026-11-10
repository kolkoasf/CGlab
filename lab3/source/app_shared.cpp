#include "app_shared.hpp"

namespace application {

std::vector<PointLight> point_lights = {
	{ { 2.0f, 2.0f, 2.0f}, 1.0f, {1.0f, 0.0f, 0.0f}, 0.0f }
};
std::vector<SpotLight> spot_lights = {
	{ { -2.0f, 2.0f, 2.0f}, 1.0f, {0.0f, -1.0f, -1.0f}, 0.9f, {0.0f, 1.0f, 0.0f}, 0.8f }
};

bool ui_show_point_lights = true;
bool ui_show_spot_lights = true;

std::vector<Vertex> sphere_vertices;
std::vector<uint32_t> sphere_indices;
std::vector<Texture> g_textures;
std::vector<LineVertex> line_vertices;

VkBuffer vk_vertex_buffer = VK_NULL_HANDLE;
VmaAllocation vma_vertex_buffer = VK_NULL_HANDLE;
VkBuffer vk_index_buffer = VK_NULL_HANDLE;
VmaAllocation vma_index_buffer = VK_NULL_HANDLE;

VkBuffer vk_scene_uniform_buffer = VK_NULL_HANDLE;
VmaAllocation vma_scene_uniform_buffer = VK_NULL_HANDLE;
SceneUniforms* vk_scene_uniform_mapped = nullptr;

VkBuffer vk_model_uniform_buffer = VK_NULL_HANDLE;
VmaAllocation vma_model_uniform_buffer = VK_NULL_HANDLE;
uint8_t* vk_model_uniform_mapped = nullptr;
VkDeviceSize vk_model_uniform_sizeof = 0;

VkBuffer vk_point_light_buffer = VK_NULL_HANDLE;
VmaAllocation vma_point_light_buffer = VK_NULL_HANDLE;
void* vk_point_light_mapped = nullptr;

VkBuffer vk_spot_light_buffer = VK_NULL_HANDLE;
VmaAllocation vma_spot_light_buffer = VK_NULL_HANDLE;
void* vk_spot_light_mapped = nullptr;

VkBuffer vk_staging_buffer = VK_NULL_HANDLE;
VmaAllocation vma_staging_buffer = VK_NULL_HANDLE;
uint8_t* vk_staging_mapped = nullptr;

VkBuffer vk_line_buffer = VK_NULL_HANDLE;
VmaAllocation vma_line_buffer = VK_NULL_HANDLE;
void* vk_line_mapped = nullptr;

VkDeviceSize vk_staging_offset = 0;
VkDeviceSize vk_vertex_staging_offset = 0;
VkDeviceSize vk_index_staging_offset = 0;
VkDeviceSize vk_vertex_size = 0;
VkDeviceSize vk_index_size = 0;

VkDescriptorSetLayout vk_scene_descriptor_set_layout = VK_NULL_HANDLE;
VkDescriptorSetLayout vk_model_descriptor_set_layout = VK_NULL_HANDLE;
VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
VkDescriptorSet vk_scene_descriptor_set = VK_NULL_HANDLE;

VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE;
VkPipeline vk_pipeline = VK_NULL_HANDLE;
VkPipeline vk_marker_sphere_pipeline = VK_NULL_HANDLE;
VkPipeline vk_marker_line_pipeline = VK_NULL_HANDLE;

Material ui_model_materials[object_count] = {
	{ 0, 2, 2 },
	{ 0, 1, 1 }
};
std::vector<std::string> ui_texture_names = {
	"texture.png",
	"black_fallback",
	"specular-texture.png",
	"perline-noise.png"
};
VkDescriptorSet vk_material_descriptor_sets[MAX_MATERIALS] = {};

bool ui_nontrivial_sampling[object_count] = { false, true };
float ui_current_time = 0.0f;

bool ui_use_perspective = true;
float ui_fov_deg = 60.0f;
float ui_ortho_height = 2.5f;

glm::vec3 ui_camera_position = glm::vec3(0.0f, 0.0f, 3.0f);
float ui_camera_yaw = -90.0f;
float ui_camera_pitch = 0.0f;

Transform ui_transform[object_count] = {
	{ { -1.2f, 0.0f, 0.0f }, {  0.0f,  0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
	{ {  1.2f, 0.0f, 0.0f }, {  0.0f, 45.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
};

glm::vec3 ui_directional_light_direction = glm::vec3(-0.5f, -1.0f, -0.3f);
glm::vec3 ui_directional_light_color = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3 ui_ambient_light = glm::vec3(0.1f, 0.1f, 0.1f);

glm::vec3 ui_specular[object_count] = { glm::vec3(1.0f), glm::vec3(1.0f) };
float ui_alpha[object_count] = { 32.0f, 32.0f };
glm::vec4 ui_color[object_count] = { glm::vec4(1.0f), glm::vec4(1.0f) };

Animation ui_animation = {
	.playing = true,
	.speed = 1.0f,
	.radius = 1.5f,
	.height = 0.4f,
	.spin_speed = 45.0f,
};
float ui_animation_time = 0.0f;

} // namespace application