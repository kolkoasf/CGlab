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

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
} pc;

void main() {
    gl_Position = scene.view_projection * pc.model * vec4(in_position, 1.0);
}