#version 450

layout(location = 0) in vec3 vertex_color;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 color;
} global_uniforms;

void main() {
    out_color = vec4(
        vertex_color * global_uniforms.color.rgb,
        1.0
    );
}