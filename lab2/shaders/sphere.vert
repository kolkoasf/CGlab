#version 450 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

layout(std140, set = 0, binding = 0) uniform SceneUniforms {
    mat4 view_projection;
    vec4 camera_position;
    vec4 directional_light_direction;
    vec4 directional_light_color;
    vec4 ambient_light;
    uint point_light_count;
    uint spot_light_count;
} scene;

layout(std140, set = 1, binding = 0) uniform ModelUniform {
    mat4 model;
    vec4 albedo;
    vec4 specular;
    float alpha;
} model;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_world_pos;

void main() {
    vec4 world_pos = model.model * vec4(in_position, 1.0);
    v_world_pos = world_pos.xyz;

    v_normal = mat3(transpose(inverse(model.model))) * in_normal;

    gl_Position = scene.view_projection * world_pos;
}