#version 450 core

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world_pos;
layout(location = 2) in vec2 v_uv;
layout(location = 0) out vec4 out_color;

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

layout(set = 1, binding = 1) uniform sampler2D albedo_texture;
layout(set = 1, binding = 2) uniform sampler2D specular_texture;
layout(set = 1, binding = 3) uniform sampler2D emissive_texture;

struct PointLight {
    vec3 position;
    float intensity;
    vec3 color;
    float _pad;
};

struct SpotLight {
    vec3 position;
    float intensity;
    vec3 direction;
    float inner_angle_cos;
    vec3 color;
    float outer_angle_cos;
};

layout(std430, set = 0, binding = 1) readonly buffer PointLightBuffer {
    PointLight point_lights[];
};

layout(std430, set = 0, binding = 2) readonly buffer SpotLightBuffer {
    SpotLight spot_lights[];
};

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    int effect_type;
    float time;
} pc;

void main() {
    vec3 N = normalize(v_normal);
    vec3 V = normalize(scene.camera_position.xyz - v_world_pos);

    vec2 uv1 = v_uv;
    vec2 uv2 = v_uv;
    vec2 uv3 = v_uv;
    vec3 final_albedo;

    if (pc.effect_type == 1) {
        uv1.x += sin(uv1.y * 15.0 + pc.time * 3.0) * 0.05;
        uv1.y += cos(uv1.x * 15.0 + pc.time * 3.0) * 0.05;

        float drift = pc.time * 0.05;

        uv2.x = v_uv.x + drift + sin(v_uv.y * 15.0 + pc.time * 3.0) * 0.05;
        uv2.y = v_uv.y + cos(v_uv.x * 15.0 + pc.time * 3.0) * 0.05;
        uv3.x = v_uv.x + drift * 0.5 - sin(v_uv.y * 10.0 + pc.time * 2.0) * 0.05;
        uv3.y = v_uv.y - cos(v_uv.x * 10.0 + pc.time * 2.0) * 0.05;

        vec3 tex1 = texture(albedo_texture, uv1).rgb;
        vec3 tex2 = texture(albedo_texture, uv2).rgb;
        vec3 tex3 = texture(albedo_texture, uv3).rgb;
        final_albedo = (tex1 + tex2 + tex3) / 3.0;

        vec3 tint = vec3(
            1.0 + sin(pc.time * 1.5) * 0.2,
            1.0 + sin(pc.time * 1.5 + 2.094) * 0.2,
            1.0 + sin(pc.time * 1.5 + 4.188) * 0.2
        );
        final_albedo *= tint;
    } else {
        final_albedo = texture(albedo_texture, v_uv).rgb;
    }

    vec3 albedo_col = final_albedo * model.albedo.xyz;
    vec3 specular_tex = texture(specular_texture, uv2).rgb;
    vec3 emissive_tex = texture(emissive_texture, uv2).rgb;
    
    vec3 L_dir = normalize(-scene.directional_light_direction.xyz);
    vec3 H_dir = normalize(L_dir + V);
    
    float NdotL_dir = max(dot(N, L_dir), 0.0);
    vec3 diffuse_dir = NdotL_dir * albedo_col;
    float NdotH_dir = max(dot(N, H_dir), 0.0);
    vec3 specular_dir = pow(NdotH_dir, model.alpha) * specular_tex * model.specular.xyz;
    vec3 final_color = scene.ambient_light.xyz * albedo_col
                     + scene.directional_light_color.xyz * (diffuse_dir + specular_dir);

    for (uint i = 0; i < scene.point_light_count; ++i) {
        vec3 L_vec = point_lights[i].position - v_world_pos;
        float distance = length(L_vec);
        vec3 L = L_vec / distance;
        float attenuation = 1.0 / (distance * distance);
        float NdotL = max(dot(N, L), 0.0);
        vec3 diffuse = NdotL * albedo_col;
        vec3 H = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        vec3 specular = pow(NdotH, model.alpha) * specular_tex * model.specular.xyz;
        final_color += (diffuse + specular) * attenuation * point_lights[i].color * point_lights[i].intensity;
    }

    for (uint i = 0; i < scene.spot_light_count; ++i) {
        vec3 L_vec = spot_lights[i].position - v_world_pos;
        float distance = length(L_vec);
        vec3 L = L_vec / distance;

        float attenuation = 1.0 / (distance * distance);

        vec3 spotDir = normalize(spot_lights[i].direction);
        
        float theta = dot(L, -spotDir);
        float epsilon = max(spot_lights[i].inner_angle_cos - spot_lights[i].outer_angle_cos, 0.0001);
        float intensity = clamp((theta - spot_lights[i].outer_angle_cos) / epsilon, 0.0, 1.0);

        if (intensity > 0.0) {
            float NdotL = max(dot(N, L), 0.0);
            vec3 diffuse = NdotL * albedo_col;

            vec3 H = normalize(L + V);
            float NdotH = max(dot(N, H), 0.0);
            vec3 specular = pow(NdotH, model.alpha) * specular_tex * model.specular.xyz;

            final_color += (diffuse + specular) * intensity * attenuation * spot_lights[i].color * spot_lights[i].intensity;
        }
    }

    final_color += emissive_tex;
    out_color = vec4(final_color, 1.0);
}