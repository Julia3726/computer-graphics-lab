#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 vertex_color;

// Uniform Buffer находится в descriptor set 0
layout(set = 0, binding = 0) uniform GlobalUniforms {
    mat4 model;
    mat4 view;
    mat4 projection;
} global_uniforms;

void main() {
    // Преобразуем координаты вершины: 
    gl_Position =
        global_uniforms.projection *
        global_uniforms.view *
        global_uniforms.model *
        vec4(in_position, 1.0);

    vertex_color = in_color;
}