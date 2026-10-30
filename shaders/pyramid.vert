#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

layout(location = 0) out vec3 world_position;
layout(location = 1) out vec3 normal;

layout(set = 0, binding = 0) uniform SceneUniforms {
    mat4 view;
    mat4 projection;
    vec4 light_direction;
    vec4 light_color_intensity;
    vec4 camera_position;
} scene_uniforms;

layout(set = 1, binding = 0) uniform ModelUniforms {
    mat4 model;
    vec4 albedo;
    vec4 specular_color;
    float specular_coefficient;
} model_uniforms;

layout(push_constant) uniform VisualizationPushConstants {
    mat4 model;
    vec4 color;
    float enabled;
} visualization;

void main() {
    vec4 position;

    if (visualization.enabled > 0.5) {
        position = visualization.model * vec4(in_position, 1.0);
        world_position = position.xyz;
        normal = in_normal;
    } else {
        position = model_uniforms.model * vec4(in_position, 1.0);
        world_position = position.xyz;
        normal = normalize(mat3(model_uniforms.model) * in_normal);
    }

    gl_Position =
        scene_uniforms.projection *
        scene_uniforms.view *
        position;
}