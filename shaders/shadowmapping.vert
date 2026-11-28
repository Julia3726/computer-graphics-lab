#version 450

layout(location = 0) in vec3 in_position;

layout(set = 0, binding = 0) uniform SceneUniforms {
    mat4 shadow_view_projection;
    mat4 spot_shadow_view_projection;
} scene_uniforms;

layout(push_constant) uniform ShadowPushConstants {
    mat4 model;
    float use_spot_shadow;
} shadow;

void main() {
    mat4 light_view_projection =
        shadow.use_spot_shadow > 0.5
            ? scene_uniforms.spot_shadow_view_projection
            : scene_uniforms.shadow_view_projection;

    gl_Position =
        light_view_projection *
        shadow.model *
        vec4(in_position, 1.0);
}