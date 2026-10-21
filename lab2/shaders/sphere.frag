#version 450 core

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world_pos;
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

layout(std430, set = 0, binding = 1) buffer PointLightBuffer {
    PointLight point_lights[];
};

layout(std430, set = 0, binding = 2) buffer SpotLightBuffer {
    SpotLight spot_lights[];
};

void main() {
    vec3 N = normalize(v_normal);
    vec3 V = normalize(scene.camera_position.xyz - v_world_pos);
    
    vec3 L_dir = normalize(-scene.directional_light_direction.xyz);
    vec3 H_dir = normalize(L_dir + V);
    
    float NdotL_dir = max(dot(N, L_dir), 0.0);
    vec3 diffuse_dir = NdotL_dir * model.albedo.xyz;
    
    float NdotH_dir = max(dot(N, H_dir), 0.0);
    vec3 specular_dir = pow(NdotH_dir, model.alpha) * model.specular.xyz;
    
    vec3 final_color = scene.ambient_light.xyz * model.albedo.xyz
                     + scene.directional_light_color.xyz * (diffuse_dir + specular_dir);

    for (uint i = 0; i < scene.point_light_count; ++i) {
        vec3 L_vec = point_lights[i].position - v_world_pos;
        float distance = length(L_vec);
        vec3 L = L_vec / distance; // normalize

        float attenuation = 1.0 / (distance * distance);

        float NdotL = max(dot(N, L), 0.0);
        vec3 diffuse = NdotL * model.albedo.xyz;

        vec3 H = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        vec3 specular = pow(NdotH, model.alpha) * model.specular.xyz;

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
            vec3 diffuse = NdotL * model.albedo.xyz;

            vec3 H = normalize(L + V);
            float NdotH = max(dot(N, H), 0.0);
            vec3 specular = pow(NdotH, model.alpha) * model.specular.xyz;

            final_color += (diffuse + specular) * intensity * attenuation * spot_lights[i].color * spot_lights[i].intensity;
        }
    }

    out_color = vec4(final_color, 1.0);
}