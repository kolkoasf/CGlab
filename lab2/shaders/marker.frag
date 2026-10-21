#version 450 core

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
} pc;

void main() {
    out_color = pc.color;
}