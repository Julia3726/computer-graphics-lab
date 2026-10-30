#include "application.hpp"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

#include <imgui.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct SceneUniforms {
    glm::mat4 view;
    glm::mat4 projection;

    glm::vec4 light_direction;			// направление света
    glm::vec4 light_color_intensity;	// цвет

	glm::vec4 camera_position;			
};

struct PointLight {
    glm::vec4 position;
    glm::vec4 color_intensity;
};

struct SpotLight {
    glm::vec4 position;
    glm::vec4 direction;
    glm::vec4 color_intensity;
    glm::vec4 angles;
};

struct LightLineVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct ModelUniforms {
    glm::mat4 model;
    glm::vec4 albedo;				// основной цвет материала
    glm::vec4 specular_color;		// цвет блика
    float specular_coefficient;	// насколько блик концентрированный
};

glm::vec3 calculate_normal(
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& c
) {
    return glm::normalize(
		glm::cross(
			b - a,
			c - a
		)
	);
}

Vertex vertices[] = {
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},
    {{ 0.0f,  0.5f,  0.0f}, {0.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},

    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},
    {{ 0.0f,  0.5f,  0.0f}, {0.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}},

    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}},
    {{ 0.0f,  0.5f,  0.0f}, {0.0f, 0.0f, 0.0f}},
    {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}},

    {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}},
    {{ 0.0f,  0.5f,  0.0f}, {0.0f, 0.0f, 0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},

    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}},

    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}},
    {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}},
};
 
const uint32_t indices[] = {
    0, 1, 2,
    3, 4, 5,
    6, 7, 8,
    9, 10, 11,
    12, 13, 14,
    15, 16, 17,
};

Vertex light_visualization_vertices[] = {
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},

    {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}},
    {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}},
};

const uint32_t light_visualization_indices[] = {
    0, 1, 2,
    2, 3, 0,

    4, 5, 6,
    6, 7, 4,

    0, 4, 7,
    7, 3, 0,

    1, 5, 6,
    6, 2, 1,

    3, 2, 6,
    6, 7, 3,

    0, 1, 5,
    5, 4, 0,
};

void calculate_vertex_normals(Vertex* vertices) {
    for (int i = 0; i < 18; i += 3) {
        glm::vec3 normal = calculate_normal(
            vertices[i].position,
            vertices[i + 1].position,
            vertices[i + 2].position
        );

        vertices[i].normal = normal;
        vertices[i + 1].normal = normal;
        vertices[i + 2].normal = normal;
    }
}

VkBuffer vertex_buffer = VK_NULL_HANDLE; 
VmaAllocation vertex_buffer_allocation = VK_NULL_HANDLE; 

VkBuffer index_buffer = VK_NULL_HANDLE; 
VmaAllocation index_buffer_allocation = VK_NULL_HANDLE; 

SceneUniforms* scene_uniform_buffer_memory = nullptr;
ModelUniforms* model_uniform_buffer_memory = nullptr;
ModelUniforms* model_uniform_buffer_2_memory = nullptr;

VkBuffer scene_uniform_buffer = VK_NULL_HANDLE;
VmaAllocation scene_uniform_buffer_allocation = VK_NULL_HANDLE;

VkBuffer point_lights_buffer;
VmaAllocation point_lights_buffer_allocation;
PointLight* point_lights_buffer_memory;

VkBuffer spot_light_buffer;
VmaAllocation spot_light_buffer_allocation;
SpotLight* spot_light_buffer_memory;

VkBuffer model_uniform_buffer = VK_NULL_HANDLE;
VmaAllocation model_uniform_buffer_allocation = VK_NULL_HANDLE;

VkBuffer model_uniform_buffer_2 = VK_NULL_HANDLE;
VmaAllocation model_uniform_buffer_2_allocation = VK_NULL_HANDLE;

VkBuffer light_visualization_vertex_buffer;
VmaAllocation light_visualization_vertex_buffer_allocation;

VkBuffer light_visualization_index_buffer;
VmaAllocation light_visualization_index_buffer_allocation;

VkBuffer light_line_vertex_buffer;
VmaAllocation light_line_vertex_buffer_allocation;
LightLineVertex* light_line_vertex_buffer_memory;

VkDescriptorSetLayout scene_descriptor_set_layout = VK_NULL_HANDLE;
VkDescriptorSetLayout model_descriptor_set_layout = VK_NULL_HANDLE;

VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

VkDescriptorSet scene_descriptor_set = VK_NULL_HANDLE;
VkDescriptorSet model_descriptor_set = VK_NULL_HANDLE;
VkDescriptorSet model_descriptor_set_2 = VK_NULL_HANDLE;

VkPipelineLayout pipeline_layout = VK_NULL_HANDLE; // Описывает интерфейс конвейера с внешними ресурсами 
VkPipeline graphics_pipeline = VK_NULL_HANDLE; 

VkPipeline light_line_pipeline = VK_NULL_HANDLE;

VkShaderModule vertex_shader_module = VK_NULL_HANDLE;
VkShaderModule fragment_shader_module = VK_NULL_HANDLE;

bool use_perspective = true;
bool show_light_visualization = true;

glm::vec3 pyramid_position = glm::vec3(0.0f);
glm::vec3 animated_position = glm::vec3(0.0f);

glm::vec3 pyramid_rotation = glm::vec3(0.0f);
glm::vec3 animated_rotation = glm::vec3(0.0f);
glm::vec3 pyramid_scale = glm::vec3(1.0f);

glm::vec3 pyramid_albedo = glm::vec3(0.8f, 0.3f, 0.2f);
glm::vec3 pyramid_specular_color = glm::vec3(1.0f, 1.0f, 1.0f);
float pyramid_specular_coefficient = 32.0f;

bool animation_playing = true;
double animation_time = 0.0;

float animation_speed = 0.5f;
float trajectory_radius_x = 1.3f;
float trajectory_radius_z = 1.3f;
float trajectory_height = 0.5f;
float rotation_speed = 1.0f;

glm::vec3 camera_position = glm::vec3(0.0f, 0.0f, 2.0f);
glm::vec3 camera_rotation = glm::vec3(0.0f, 0.0f, 0.0f);
float camera_fov = 60.0f;									// угол обзора

glm::vec3 second_pyramid_position = glm::vec3(1.5f, 0.0f, -1.0f);

glm::vec3 second_pyramid_albedo = glm::vec3(0.2f, 0.4f, 0.8f);
glm::vec3 second_pyramid_specular_color = glm::vec3(1.0f, 1.0f, 1.0f);
float second_pyramid_specular_coefficient = 8.0f;

float second_pyramid_rotation = 0.0f;
float second_pyramid_phase = glm::pi<float>();

glm::vec3 light_direction = glm::vec3(-1.0f, -1.0f, -1.0f);		// по диагонали вниз и в сторону
glm::vec3 light_color = glm::vec3(1.0f, 1.0f, 1.0f);			// белый свет
float light_intensity = 1.0f;

glm::vec3 point_light_position = glm::vec3(2.0f, 2.0f, 2.0f);
glm::vec3 point_light_color = glm::vec3(1.0f, 1.0f, 1.0f);
float point_light_intensity = 5.0f;

std::vector<SpotLight> spot_lights = {
    {
        glm::vec4(0.0f, 3.0f, 2.0f, 1.0f),
        glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 5.0f),
        glm::vec4(
            glm::radians(15.0f),
            glm::radians(25.0f),
            0.0f,
            0.0f
        )
    }
};

std::vector<PointLight> point_lights = {
    {
        glm::vec4(2.0f, 2.0f, 2.0f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 5.0f)
    }
};

void update_light_line_vertices() {
    const float length = 2.0f;

    for (size_t i = 0; i < spot_lights.size(); ++i) {
        glm::vec3 position =
            glm::vec3(spot_lights[i].position);

        glm::vec3 direction =
            glm::normalize(
                glm::vec3(spot_lights[i].direction)
            );

        float outer_angle =
            spot_lights[i].angles.y;

        glm::vec3 up(0.0f, 1.0f, 0.0f);

        if (glm::abs(glm::dot(direction, up)) > 0.99f) {
            up = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        glm::vec3 right =
            glm::normalize(
                glm::cross(direction, up)
            );

        glm::vec3 left_direction =
            glm::normalize(
                direction * glm::cos(outer_angle) +
                right * glm::sin(outer_angle)
            );

        glm::vec3 right_direction =
            glm::normalize(
                direction * glm::cos(outer_angle) -
                right * glm::sin(outer_angle)
            );

        size_t vertex = i * 6;

        light_line_vertex_buffer_memory[vertex + 0] = {
            position,
            glm::vec3(0.0f)
        };

        light_line_vertex_buffer_memory[vertex + 1] = {
            position + direction * length,
            glm::vec3(0.0f)
        };

        light_line_vertex_buffer_memory[vertex + 2] = {
            position,
            glm::vec3(0.0f)
        };

        light_line_vertex_buffer_memory[vertex + 3] = {
            position + left_direction * length,
            glm::vec3(0.0f)
        };

        light_line_vertex_buffer_memory[vertex + 4] = {
            position,
            glm::vec3(0.0f)
        };

        light_line_vertex_buffer_memory[vertex + 5] = {
            position + right_direction * length,
            glm::vec3(0.0f)
        };
    }
}

VkPipelineShaderStageCreateInfo vertex_shader_stage = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	.stage = VK_SHADER_STAGE_VERTEX_BIT,
	.module = VK_NULL_HANDLE,
	.pName = "main",
};

VkPipelineShaderStageCreateInfo fragment_shader_stage = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
	.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
	.module = VK_NULL_HANDLE,
	.pName = "main",
};

// Чтение скомпилированного SPIR-V файла с диска
std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		std::cerr << "Failed to open shader file: " << filename << '\n';
		return {};
	}

	const size_t file_size = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(file_size);

	file.seekg(0);
	file.read(buffer.data(), file_size);
	file.close();

	return buffer;
}


VkShaderModule createShaderModule(
	const std::vector<char>& code,
	VkDevice device
) {
	VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.size(),
		.pCode = reinterpret_cast<const uint32_t*>(code.data()),
	};

	VkShaderModule shader_module = VK_NULL_HANDLE;

	if (vkCreateShaderModule(
			device,
			&create_info,
			nullptr,
			&shader_module) != VK_SUCCESS) {

		std::cerr << "Failed to create shader module\n";
		return VK_NULL_HANDLE;
	}

	return shader_module;
}

// Описание того, что Vulkan должен читать vertex buffer
VkVertexInputBindingDescription vertex_binding = {
	.binding = 0,						 			
	.stride = sizeof(Vertex),							
	.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,		
};

VkVertexInputAttributeDescription vertex_position_attribute = {
	.location = 0, 									
	.binding = 0,
	.format = VK_FORMAT_R32G32B32_SFLOAT,
	.offset = 0,
};

VkVertexInputAttributeDescription vertex_normal_attribute = {
    .location = 1,
    .binding = 0,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = offsetof(Vertex, normal),
};

const VkVertexInputAttributeDescription vertex_attributes[] = {
    vertex_position_attribute,
    vertex_normal_attribute,
};

VkVertexInputBindingDescription light_line_vertex_binding = {
    .binding = 0,
    .stride = sizeof(LightLineVertex),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
};

VkVertexInputAttributeDescription light_line_vertex_position_attribute = {
    .location = 0,
    .binding = 0,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = offsetof(LightLineVertex, position),
};

VkVertexInputAttributeDescription light_line_vertex_normal_attribute = {
    .location = 1,
    .binding = 0,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = offsetof(LightLineVertex, normal),
};

const VkVertexInputAttributeDescription light_line_vertex_attributes[] = {
    light_line_vertex_position_attribute,
    light_line_vertex_normal_attribute,
};

namespace application {

bool initialize() {
	auto& context = graphics::internal::context;

	calculate_vertex_normals(vertices);

	const auto vertex_shader_code = readFile("shaders/pyramid.vert.spv");
	const auto fragment_shader_code = readFile("shaders/pyramid.frag.spv");

	if (vertex_shader_code.empty() || fragment_shader_code.empty()) {
		return false;
	}

	vertex_shader_module = createShaderModule(vertex_shader_code, context.device);
	fragment_shader_module = createShaderModule(fragment_shader_code, context.device);

	if (vertex_shader_module == VK_NULL_HANDLE ||
		fragment_shader_module == VK_NULL_HANDLE) {
		return false;
	}

	// Связываем описание стадии с соответствующим shader module
	vertex_shader_stage.module = vertex_shader_module;
	fragment_shader_stage.module = fragment_shader_module;

	const VkBufferCreateInfo vertex_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(vertices),
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo vertex_buffer_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &vertex_buffer_info,
			&vertex_buffer_allocation_info, &vertex_buffer,
			&vertex_buffer_allocation, nullptr) != VK_SUCCESS) {
		std::cerr << "Failed to create vertex buffer\n";
		return false;
	}

	void* vertex_data = nullptr;

	if (vmaMapMemory(context.allocator, vertex_buffer_allocation,
			&vertex_data) != VK_SUCCESS) {
		std::cerr << "Failed to map vertex buffer memory\n";
		return false;
	}

	std::memcpy(vertex_data, vertices, sizeof(vertices));

	vmaUnmapMemory(context.allocator, vertex_buffer_allocation);

	const VkBufferCreateInfo index_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(indices),
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo index_buffer_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &index_buffer_info,
			&index_buffer_allocation_info, &index_buffer,
			&index_buffer_allocation, nullptr) != VK_SUCCESS) {
		std::cerr << "Failed to create index buffer\n";
		return false;
	}

	void* index_data = nullptr;

	if (vmaMapMemory(context.allocator, index_buffer_allocation,
			&index_data) != VK_SUCCESS) {
		std::cerr << "Failed to map index buffer memory\n";
		return false;
	}

	std::memcpy(index_data, indices, sizeof(indices));

	vmaUnmapMemory(context.allocator, index_buffer_allocation);

	const VkBufferCreateInfo light_visualization_vertex_buffer_create_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(light_visualization_vertices),
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo light_visualization_vertex_buffer_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&light_visualization_vertex_buffer_create_info,
			&light_visualization_vertex_buffer_allocation_info,
			&light_visualization_vertex_buffer,
			&light_visualization_vertex_buffer_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create light visualization vertex buffer\n";
		return false;
	}

	void* light_visualization_vertex_data = nullptr;

	if (vmaMapMemory(
			context.allocator,
			light_visualization_vertex_buffer_allocation,
			&light_visualization_vertex_data
		) != VK_SUCCESS) {
		std::cerr << "Failed to map light visualization vertex buffer memory\n";
		return false;
	}

	std::memcpy(
		light_visualization_vertex_data,
		light_visualization_vertices,
		sizeof(light_visualization_vertices)
	);

	vmaUnmapMemory(
		context.allocator,
		light_visualization_vertex_buffer_allocation
	);

	const VkBufferCreateInfo light_line_vertex_buffer_create_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(LightLineVertex) * 6 * spot_lights.size(),
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo light_line_vertex_buffer_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&light_line_vertex_buffer_create_info,
			&light_line_vertex_buffer_allocation_info,
			&light_line_vertex_buffer,
			&light_line_vertex_buffer_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create light line vertex buffer\n";
		return false;
	}

	void* light_line_vertex_data = nullptr;

	if (vmaMapMemory(
			context.allocator,
			light_line_vertex_buffer_allocation,
			&light_line_vertex_data
		) != VK_SUCCESS) {
		std::cerr << "Failed to map light line vertex buffer memory\n";
		return false;
	}

	light_line_vertex_buffer_memory = static_cast<LightLineVertex*>(light_line_vertex_data);

	const VkBufferCreateInfo light_visualization_index_buffer_create_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(light_visualization_indices),
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo light_visualization_index_buffer_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&light_visualization_index_buffer_create_info,
			&light_visualization_index_buffer_allocation_info,
			&light_visualization_index_buffer,
			&light_visualization_index_buffer_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create light visualization index buffer\n";
		return false;
	}

	void* light_visualization_index_data = nullptr;

	if (vmaMapMemory(
			context.allocator,
			light_visualization_index_buffer_allocation,
			&light_visualization_index_data
		) != VK_SUCCESS) {
		std::cerr << "Failed to map light visualization index buffer memory\n";
		return false;
	}

	std::memcpy(
		light_visualization_index_data,
		light_visualization_indices,
		sizeof(light_visualization_indices)
	);

	vmaUnmapMemory(
		context.allocator,
		light_visualization_index_buffer_allocation
	);

	const VkBufferCreateInfo scene_uniform_buffer_create_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(SceneUniforms),
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo uniform_buffer_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &scene_uniform_buffer_create_info,
			&uniform_buffer_allocation_info, &scene_uniform_buffer,
		    &scene_uniform_buffer_allocation, nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create scene uniform buffer\n";
		return false;
	}

	const VkBufferCreateInfo model_uniform_buffer_create_info  = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(ModelUniforms),
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	if (vmaCreateBuffer(context.allocator, &model_uniform_buffer_create_info , &uniform_buffer_allocation_info,
			&model_uniform_buffer, &model_uniform_buffer_allocation, nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create model uniform buffer\n";
		return false;
	}

	if (vmaCreateBuffer(context.allocator, &model_uniform_buffer_create_info ,
			&uniform_buffer_allocation_info, &model_uniform_buffer_2,
			&model_uniform_buffer_2_allocation, nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create second model uniform buffer\n";
		return false;
	}

	void* scene_uniform_data = nullptr;

	if (vmaMapMemory(context.allocator, scene_uniform_buffer_allocation,
			&scene_uniform_data) != VK_SUCCESS) {
		std::cerr << "Failed to map scene uniform buffer memory\n";
		return false;
	}

	scene_uniform_buffer_memory = static_cast<SceneUniforms*>(scene_uniform_data);

	void* model_uniform_data = nullptr;

	if (vmaMapMemory(context.allocator, model_uniform_buffer_allocation,
			&model_uniform_data) != VK_SUCCESS) {
		std::cerr << "Failed to map model uniform buffer memory\n";
		return false;
	}

	model_uniform_buffer_memory = static_cast<ModelUniforms*>(model_uniform_data);

	void* model_uniform_data_2 = nullptr;

	if (vmaMapMemory(context.allocator, model_uniform_buffer_2_allocation,
			&model_uniform_data_2) != VK_SUCCESS) {
		std::cerr << "Failed to map second model uniform buffer memory\n";
		return false;
	}

	model_uniform_buffer_2_memory = static_cast<ModelUniforms*>(model_uniform_data_2);

	const VkBufferCreateInfo point_lights_buffer_create_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(PointLight) * point_lights.size(),
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo storage_buffer_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&point_lights_buffer_create_info,
			&storage_buffer_allocation_info,
			&point_lights_buffer,
			&point_lights_buffer_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create point lights buffer\n";
		return false;
	}

	void* point_lights_data = nullptr;

	if (vmaMapMemory(
			context.allocator,
			point_lights_buffer_allocation,
			&point_lights_data
		) != VK_SUCCESS) {
		std::cerr << "Failed to map point lights buffer memory\n";
		return false;
	}

	point_lights_buffer_memory = static_cast<PointLight*>(point_lights_data);

	std::memcpy(
		point_lights_buffer_memory,
		point_lights.data(),
		sizeof(PointLight) * point_lights.size()
	);

	const VkBufferCreateInfo spot_light_buffer_create_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(SpotLight) * spot_lights.size(),
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo spot_light_buffer_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&spot_light_buffer_create_info,
			&spot_light_buffer_allocation_info,
			&spot_light_buffer,
			&spot_light_buffer_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create spot light buffer\n";
		return false;
	}

	void* spot_light_data = nullptr;

	if (vmaMapMemory(
			context.allocator,
			spot_light_buffer_allocation,
			&spot_light_data
		) != VK_SUCCESS) {
		std::cerr << "Failed to map spot light buffer memory\n";
		return false;
	}

	spot_light_buffer_memory = static_cast<SpotLight*>(spot_light_data);

	std::memcpy(
		spot_light_buffer_memory,
		spot_lights.data(),
		sizeof(SpotLight)
	);

	// Начальное положение и ориентация пирамиды
	model_uniform_buffer_memory->model = glm::mat4(1.0f);

	model_uniform_buffer_2_memory->model = glm::translate(
		glm::mat4(1.0f),
		glm::vec3(1.5f, 0.0f, 0.0f)
	);

	// Описываем ресурс, который будет доступен шейдеру
	const VkDescriptorSetLayoutBinding scene_uniform_binding = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	const VkDescriptorSetLayoutBinding point_lights_binding = {
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	const VkDescriptorSetLayoutBinding spot_light_binding = {
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	const VkDescriptorSetLayoutBinding scene_bindings[] = {
		scene_uniform_binding,
		point_lights_binding,
		spot_light_binding,
	};

	const VkDescriptorSetLayoutCreateInfo scene_descriptor_set_layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 3,
		.pBindings = scene_bindings,
	};

	if (vkCreateDescriptorSetLayout(
			context.device,
			&scene_descriptor_set_layout_info,
			nullptr,
			&scene_descriptor_set_layout
		) != VK_SUCCESS) {
		std::cerr << "Failed to create scene descriptor set layout\n";
		return false;
	}

	const VkDescriptorSetLayoutBinding model_uniform_binding = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	const VkDescriptorSetLayoutCreateInfo model_descriptor_set_layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &model_uniform_binding,
	};

	if (vkCreateDescriptorSetLayout(
			context.device,
			&model_descriptor_set_layout_info,
			nullptr,
			&model_descriptor_set_layout
		) != VK_SUCCESS) {
		std::cerr << "Failed to create model descriptor set layout\n";
		return false;
	}

	const VkDescriptorPoolSize descriptor_pool_sizes[] = {
		{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 3,
		},
		{
			.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 2,
		},
	};

	const VkDescriptorPoolCreateInfo descriptor_pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 3,
		.poolSizeCount = 2,
		.pPoolSizes = descriptor_pool_sizes,
	};

	if (vkCreateDescriptorPool(context.device, &descriptor_pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) {
		std::cerr << "Failed to create descriptor pool\n";
		return false;
	}

	const VkDescriptorSetLayout scene_layouts[] = {
		scene_descriptor_set_layout,
	};

	const VkDescriptorSetAllocateInfo scene_descriptor_set_allocate_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = scene_layouts,
	};

	if (vkAllocateDescriptorSets(
			context.device,
			&scene_descriptor_set_allocate_info,
			&scene_descriptor_set
		) != VK_SUCCESS) {
		std::cerr << "Failed to allocate scene descriptor set\n";
		return false;
	}

	const VkDescriptorSetLayout model_layouts[] = {
		model_descriptor_set_layout,
		model_descriptor_set_layout,
	};

	const VkDescriptorSetAllocateInfo model_descriptor_set_allocate_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptor_pool,
		.descriptorSetCount = 2,
		.pSetLayouts = model_layouts,
	};

	VkDescriptorSet model_descriptor_sets[] = {
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
	};

	if (vkAllocateDescriptorSets(
			context.device,
			&model_descriptor_set_allocate_info,
			model_descriptor_sets
		) != VK_SUCCESS) {
		std::cerr << "Failed to allocate model descriptor sets\n";
		return false;
	}

	model_descriptor_set = model_descriptor_sets[0];
	model_descriptor_set_2 = model_descriptor_sets[1];

	// Описываем, какой конкретно Vulkan Buffer будет находиться в нашем descriptor set
	VkDescriptorBufferInfo scene_uniform_buffer_info = {
		.buffer = scene_uniform_buffer,
		.offset = 0,
		.range = sizeof(SceneUniforms),
	};

	VkDescriptorBufferInfo point_lights_buffer_info = {
		.buffer = point_lights_buffer,
		.offset = 0,
		.range = sizeof(PointLight) * point_lights.size(),
	};

	VkDescriptorBufferInfo spot_light_buffer_info = {
		.buffer = spot_light_buffer,
		.offset = 0,
		.range = sizeof(SpotLight) * spot_lights.size(),
	};

	VkDescriptorBufferInfo model_uniform_buffer_info = {
		.buffer = model_uniform_buffer,
		.offset = 0,
		.range = sizeof(ModelUniforms),
	};

	VkDescriptorBufferInfo model_uniform_buffer_info_2 = {
		.buffer = model_uniform_buffer_2,
		.offset = 0,
		.range = sizeof(ModelUniforms),
	};

	// Описываем изменение descriptor set
	VkWriteDescriptorSet scene_descriptor_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = scene_descriptor_set,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &scene_uniform_buffer_info,
	};

	VkWriteDescriptorSet model_descriptor_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = model_descriptor_set,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &model_uniform_buffer_info,
	};

	VkWriteDescriptorSet model_descriptor_write_2 = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = model_descriptor_set_2,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &model_uniform_buffer_info_2,
	};

	VkWriteDescriptorSet point_lights_descriptor_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = scene_descriptor_set,
		.dstBinding = 1,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pBufferInfo = &point_lights_buffer_info,
	};

	VkWriteDescriptorSet spot_light_descriptor_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = scene_descriptor_set,
		.dstBinding = 2,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pBufferInfo = &spot_light_buffer_info,
	};

	const VkWriteDescriptorSet descriptor_writes[] = {
		scene_descriptor_write,
		model_descriptor_write,
		model_descriptor_write_2,
		point_lights_descriptor_write,
		spot_light_descriptor_write,
	};

	// Записываем информацию в descriptor set
	vkUpdateDescriptorSets(context.device, 5, descriptor_writes, 0, nullptr);

	const VkDescriptorSetLayout pipeline_set_layouts[] = {
		scene_descriptor_set_layout,
		model_descriptor_set_layout,
	};

	const VkPushConstantRange visualization_push_constant_range = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof(glm::mat4) + sizeof(glm::vec4) + sizeof(float),
	};

	const VkPipelineLayoutCreateInfo pipeline_layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 2,
		.pSetLayouts = pipeline_set_layouts,
		.pushConstantRangeCount = 1,
    	.pPushConstantRanges = &visualization_push_constant_range,
	};

	if (vkCreatePipelineLayout(context.device, &pipeline_layout_info,
			nullptr, &pipeline_layout) != VK_SUCCESS) {
		std::cerr << "Failed to create pipeline layout\n";
		return false;
	}

	// Графический конвейер использует 2 shader-стадии
	const VkPipelineShaderStageCreateInfo shader_stages[] = {
		vertex_shader_stage,
		fragment_shader_stage,
	};

	// Определяем, как Vulkan собирает примитивы из вершин/индексов
	const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,						// треугольник
		.primitiveRestartEnable = VK_FALSE,										// специальный индекс для перезапуска примитива не используется
	};

	// Передаем конвейеру описание структуры наших вершин
	const VkPipelineVertexInputStateCreateInfo vertex_input = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertex_binding,
		.vertexAttributeDescriptionCount = 2,
		.pVertexAttributeDescriptions = vertex_attributes,
	};

	const VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	const VkPipelineRasterizationStateCreateInfo rasterizer = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,					// Не ограничиваем глубину специальным образом
		.rasterizerDiscardEnable = VK_FALSE,			// Не отбрасываем всю геометрию до растеризации
		.polygonMode = VK_POLYGON_MODE_FILL, 			
		.cullMode = VK_CULL_MODE_NONE,					// Пока не отбрасываем задние/передние грани
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,	
		.depthBiasEnable = VK_FALSE,					// Смещение глубины не используем
		.lineWidth = 1.0f,								// Толщина линии, используется при рисовании линий
	};

	const VkPipelineMultisampleStateCreateInfo multisampling = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,						
	};

	const VkPipelineDepthStencilStateCreateInfo depth_stencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,				
		.depthWriteEnable = VK_TRUE,			
		.depthCompareOp = VK_COMPARE_OP_LESS, 	
		.depthBoundsTestEnable = VK_FALSE,		// Проверку диапазона глубины не используем
		.stencilTestEnable = VK_FALSE,			
	};

	// Настройки записи результата фрагментного шейдера в color attacment
	const VkPipelineColorBlendAttachmentState color_blend_attachment = {
		.blendEnable = VK_FALSE, 						// смешивание цветов
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
		                   VK_COLOR_COMPONENT_G_BIT |
		                   VK_COLOR_COMPONENT_B_BIT |
		                   VK_COLOR_COMPONENT_A_BIT,
	};

	const VkPipelineColorBlendStateCreateInfo color_blending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,					
		.attachmentCount = 1,
		.pAttachments = &color_blend_attachment,
	};

	const VkDynamicState dynamic_states[] = {
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_SCISSOR,
	};

	const VkPipelineDynamicStateCreateInfo dynamic_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamic_states,
	};

	// Объединяем все настройки в одно описание графического конвейера
	const VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,

		.stageCount = 2,
		.pStages = shader_stages,

		.pVertexInputState = &vertex_input,			
		.pInputAssemblyState = &input_assembly,		
		.pViewportState = &viewport_state,			
		.pRasterizationState = &rasterizer,			// превращает треугольники в фрагменты (пиксели)
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,		
		.pColorBlendState = &color_blending,		// как записывать цвета фрагментного шейдера
		.pDynamicState = &dynamic_state,			

		.layout = pipeline_layout,					// интерфейс конвейера с внешними ресурсами шейдеров
		.renderPass = context.render_pass,			
		.subpass = 0,								
	};

	if (vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1,
			&pipeline_info, nullptr, &graphics_pipeline) != VK_SUCCESS) {
		std::cerr << "Failed to create graphics pipeline\n";
		return false;
	}

	const VkPipelineInputAssemblyStateCreateInfo light_line_input_assembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	const VkPipelineVertexInputStateCreateInfo light_line_vertex_input = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &light_line_vertex_binding,
		.vertexAttributeDescriptionCount = 2,
		.pVertexAttributeDescriptions = light_line_vertex_attributes,
	};

	const VkGraphicsPipelineCreateInfo light_line_pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,

		.stageCount = 2,
		.pStages = shader_stages,

		.pVertexInputState = &light_line_vertex_input,
		.pInputAssemblyState = &light_line_input_assembly,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blending,
		.pDynamicState = &dynamic_state,

		.layout = pipeline_layout,
		.renderPass = context.render_pass,
		.subpass = 0,
	};

	if (vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1,
			&light_line_pipeline_info, nullptr, &light_line_pipeline) != VK_SUCCESS) {
		std::cerr << "Failed to create light line graphics pipeline\n";
		return false;
	}

	return true;
}

void recreate_point_lights_buffer() {
	auto& context = graphics::internal::context;

	vkDeviceWaitIdle(context.device);

    if (point_lights_buffer != VK_NULL_HANDLE) {
        vmaUnmapMemory(
            context.allocator,
            point_lights_buffer_allocation
        );

        vmaDestroyBuffer(
            context.allocator,
            point_lights_buffer,
            point_lights_buffer_allocation
        );
    }

    const VkBufferCreateInfo buffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(PointLight) * point_lights.size(),
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    const VmaAllocationCreateInfo allocation_info = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    if (vmaCreateBuffer(
            context.allocator,
            &buffer_create_info,
            &allocation_info,
            &point_lights_buffer,
            &point_lights_buffer_allocation,
            nullptr
        ) != VK_SUCCESS) {
        throw std::runtime_error("Failed to recreate point lights buffer");
    }

    void* data = nullptr;

    if (vmaMapMemory(
            context.allocator,
            point_lights_buffer_allocation,
            &data
        ) != VK_SUCCESS) {
        throw std::runtime_error("Failed to map point lights buffer memory");
    }

    point_lights_buffer_memory =
        static_cast<PointLight*>(data);

    std::memcpy(
        point_lights_buffer_memory,
        point_lights.data(),
        sizeof(PointLight) * point_lights.size()
    );
}

void recreate_spot_lights_buffer() {
	auto& context = graphics::internal::context;

	vkQueueWaitIdle(context.graphics_queue);

	vmaUnmapMemory(
		context.allocator,
		spot_light_buffer_allocation
	);

	vmaDestroyBuffer(
		context.allocator,
		spot_light_buffer,
		spot_light_buffer_allocation
	);

	const VkBufferCreateInfo spot_light_buffer_create_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(SpotLight) * spot_lights.size(),
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo spot_light_buffer_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&spot_light_buffer_create_info,
			&spot_light_buffer_allocation_info,
			&spot_light_buffer,
			&spot_light_buffer_allocation,
			nullptr
		) != VK_SUCCESS) {
		throw std::runtime_error("Failed to recreate spot light buffer");
	}

	void* spot_light_data = nullptr;

	if (vmaMapMemory(
			context.allocator,
			spot_light_buffer_allocation,
			&spot_light_data
		) != VK_SUCCESS) {
		throw std::runtime_error("Failed to map spot light buffer memory");
	}

	spot_light_buffer_memory =
		static_cast<SpotLight*>(spot_light_data);

	std::memcpy(
		spot_light_buffer_memory,
		spot_lights.data(),
		sizeof(SpotLight) * spot_lights.size()
	);

	VkDescriptorBufferInfo spot_light_buffer_info = {
		.buffer = spot_light_buffer,
		.offset = 0,
		.range = sizeof(SpotLight) * spot_lights.size(),
	};

	VkWriteDescriptorSet spot_light_descriptor_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = scene_descriptor_set,
		.dstBinding = 2,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pBufferInfo = &spot_light_buffer_info,
	};

	vkUpdateDescriptorSets(
		context.device,
		1,
		&spot_light_descriptor_write,
		0,
		nullptr
	);
}

void recreate_light_line_vertex_buffer() {
    auto& context = graphics::internal::context;

    vkQueueWaitIdle(context.graphics_queue);

    vmaUnmapMemory(
        context.allocator,
        light_line_vertex_buffer_allocation
    );

    vmaDestroyBuffer(
        context.allocator,
        light_line_vertex_buffer,
        light_line_vertex_buffer_allocation
    );

    const VkBufferCreateInfo buffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(LightLineVertex) * 6 * spot_lights.size(),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    const VmaAllocationCreateInfo allocation_info = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    if (vmaCreateBuffer(
            context.allocator,
            &buffer_create_info,
            &allocation_info,
            &light_line_vertex_buffer,
            &light_line_vertex_buffer_allocation,
            nullptr
        ) != VK_SUCCESS) {
        throw std::runtime_error(
            "Failed to recreate light line vertex buffer"
        );
    }

    void* data = nullptr;

    if (vmaMapMemory(
            context.allocator,
            light_line_vertex_buffer_allocation,
            &data
        ) != VK_SUCCESS) {
        throw std::runtime_error(
            "Failed to map light line vertex buffer memory"
        );
    }

    light_line_vertex_buffer_memory =
        static_cast<LightLineVertex*>(data);

    update_light_line_vertices();
}

void update_point_lights_descriptor() {
	auto& context = graphics::internal::context;

    VkDescriptorBufferInfo point_lights_buffer_info = {
        .buffer = point_lights_buffer,
        .offset = 0,
        .range = sizeof(PointLight) * point_lights.size(),
    };

    VkWriteDescriptorSet point_lights_descriptor_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = scene_descriptor_set,
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &point_lights_buffer_info,
    };

    vkUpdateDescriptorSets(
        context.device,
        1,
        &point_lights_descriptor_write,
        0,
        nullptr
    );
}

void shutdown() {
	auto& context = graphics::internal::context;

	vkQueueWaitIdle(context.graphics_queue);

	vkDestroyPipeline(context.device, graphics_pipeline, nullptr);
	vkDestroyPipeline(context.device, light_line_pipeline, nullptr);

	vkDestroyPipelineLayout(context.device, pipeline_layout, nullptr);
	vkDestroyDescriptorPool(context.device, descriptor_pool, nullptr);

	vkDestroyDescriptorSetLayout(context.device,scene_descriptor_set_layout,nullptr);
	vkDestroyDescriptorSetLayout(context.device, model_descriptor_set_layout, nullptr);

	vkDestroyShaderModule(context.device, vertex_shader_module, nullptr);
	vkDestroyShaderModule(context.device, fragment_shader_module, nullptr);

	vmaUnmapMemory(context.allocator, scene_uniform_buffer_allocation);
	vmaUnmapMemory(context.allocator, model_uniform_buffer_allocation);
	vmaUnmapMemory(context.allocator, model_uniform_buffer_2_allocation);

	vmaDestroyBuffer(context.allocator, scene_uniform_buffer, scene_uniform_buffer_allocation);
	vmaDestroyBuffer(context.allocator, model_uniform_buffer, model_uniform_buffer_allocation);
	vmaDestroyBuffer(context.allocator, model_uniform_buffer_2, model_uniform_buffer_2_allocation);
	
	vmaDestroyBuffer(context.allocator, vertex_buffer, vertex_buffer_allocation);
	vmaDestroyBuffer(context.allocator, index_buffer, index_buffer_allocation);

	vmaDestroyBuffer(context.allocator, light_visualization_vertex_buffer, light_visualization_vertex_buffer_allocation);
	vmaDestroyBuffer(context.allocator, light_visualization_index_buffer, light_visualization_index_buffer_allocation);

	vmaUnmapMemory(context.allocator, light_line_vertex_buffer_allocation);
	vmaDestroyBuffer(context.allocator, light_line_vertex_buffer, light_line_vertex_buffer_allocation);

	vmaUnmapMemory(context.allocator, point_lights_buffer_allocation);
	vmaDestroyBuffer(context.allocator, point_lights_buffer, point_lights_buffer_allocation);

	vmaUnmapMemory(context.allocator, spot_light_buffer_allocation);
	vmaDestroyBuffer(context.allocator, spot_light_buffer, spot_light_buffer_allocation);
}

void update(double time) {
	static double previous_time = time;

	double delta_time = time - previous_time;
	previous_time = time;

	if (animation_playing) {
		animation_time += delta_time * animation_speed;
		second_pyramid_rotation += static_cast<float>(delta_time);

		animated_position.x =
			trajectory_radius_x * std::cos(animation_time);

		animated_position.y =
			trajectory_height * std::sin(2.0 * animation_time);

		animated_position.z =
			trajectory_radius_z * std::sin(animation_time);

		animated_rotation.y =
			animation_time * rotation_speed;
	}

	glm::mat4 view = glm::mat4(1.0f);

	view = glm::rotate(
		view,
		-camera_rotation.x,
		glm::vec3(1.0f, 0.0f, 0.0f)
	);

	view = glm::rotate(
		view,
		-camera_rotation.y,
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	view = glm::rotate(
		view,
		-camera_rotation.z,
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	view = glm::translate(
		view,
		-camera_position
	);

	scene_uniform_buffer_memory->view = view;

	scene_uniform_buffer_memory->light_direction =
		glm::vec4(
			glm::normalize(light_direction),
			0.0f
		);

	scene_uniform_buffer_memory->light_color_intensity =
		glm::vec4(
			light_color,
			light_intensity
		);
	
    glm::mat4 model = glm::mat4(1.0f);

	model = glm::translate(
		model,
		pyramid_position + animated_position
	);

	model = glm::rotate(
		model,
		pyramid_rotation.x,
		glm::vec3(1.0f, 0.0f, 0.0f)
	);

	model = glm::rotate(
		model,
		pyramid_rotation.y + animated_rotation.y,
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	model = glm::rotate(
		model,
		pyramid_rotation.z,
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	model = glm::scale(
		model,
		pyramid_scale
	);

	model_uniform_buffer_memory->model = model;

	glm::vec3 second_animated_position;

	second_animated_position.x =
		trajectory_radius_x *
		std::cos(animation_time + second_pyramid_phase);

	second_animated_position.y =
		trajectory_height *
		std::sin(2.0 * (animation_time + second_pyramid_phase));

	second_animated_position.z =
		trajectory_radius_z *
		std::sin(animation_time + second_pyramid_phase);

	glm::mat4 second_model = glm::mat4(1.0f);

	second_model = glm::translate(
		second_model,
		second_pyramid_position + second_animated_position
	);

	second_model = glm::rotate(
		second_model,
		second_pyramid_rotation,
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	model_uniform_buffer_2_memory->model = second_model;

	float aspect_ratio =
		static_cast<float>(graphics::internal::context.swapchain_extent.width) /
		static_cast<float>(graphics::internal::context.swapchain_extent.height);

	if (use_perspective) {
		scene_uniform_buffer_memory->projection = glm::perspectiveRH_ZO(
			glm::radians(camera_fov),
			aspect_ratio,
			0.1f,
			10.0f
		);
	} else {
		scene_uniform_buffer_memory->projection = glm::orthoRH_ZO(
			-3.0f * aspect_ratio,
			3.0f * aspect_ratio,
			-3.0f,
			3.0f,
			0.1f,
			10.0f
		);
	}

	scene_uniform_buffer_memory->projection[1][1] *= -1.0f;

	ImGui::Begin("Pyramid");

	if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (animation_playing) {
			if (ImGui::Button("Pause")) {
				animation_playing = false;
			}
		} else {
			if (ImGui::Button("Play")) {
				animation_playing = true;
			}
		}

		ImGui::SliderFloat(
			"Animation Speed",
			&animation_speed,
			0.1f,
			2.0f
		);

		ImGui::SliderFloat(
			"Trajectory Radius X",
			&trajectory_radius_x,
			0.5f,
			2.0f
		);

		ImGui::SliderFloat(
			"Trajectory Radius Z",
			&trajectory_radius_z,
			0.5f,
			2.0f
		);

		ImGui::SliderFloat(
			"Trajectory Height",
			&trajectory_height,
			0.0f,
			1.0f
		);
	}

	if (ImGui::CollapsingHeader("Projection", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::Button("Perspective")) {
			use_perspective = true;
		}

		ImGui::SameLine();

		if (ImGui::Button("Orthographic")) {
			use_perspective = false;
		}
	}

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3(
			"Object Position",
			&pyramid_position.x,
			0.01f
		);

		ImGui::DragFloat3(
			"Object Rotation",
			&pyramid_rotation.x,
			0.01f
		);

		ImGui::DragFloat3(
			"Object Scale",
			&pyramid_scale.x,
			0.01f,
			0.1f,
			3.0f
		);
	}

	if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::ColorEdit3(
			"Albedo",
			&pyramid_albedo.x
		);

		ImGui::ColorEdit3(
			"Specular Color",
			&pyramid_specular_color.x
		);

		ImGui::SliderFloat(
			"Specular Coefficient",
			&pyramid_specular_coefficient,
			1.0f,
			256.0f
		);
	}

	 if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3(
			"Light Direction",
			&light_direction.x,
			0.01f
		);

		ImGui::ColorEdit3(
			"Light Color",
			&light_color.x
		);

		ImGui::SliderFloat(
			"Light Intensity",
			&light_intensity,
			0.0f,
			5.0f
		);
	}

	if (ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::Button("Add Point Light")) {
			point_lights.push_back({
				glm::vec4(0.0f, 2.0f, 2.0f, 1.0f),
				glm::vec4(1.0f, 1.0f, 1.0f, 5.0f)
			});

			recreate_point_lights_buffer();
			update_point_lights_descriptor();
		}

		ImGui::SameLine();

		if (ImGui::Button("Delete Point Light") && point_lights.size() > 1) {
			point_lights.pop_back();

			recreate_point_lights_buffer();
			update_point_lights_descriptor();
		}

		for (size_t i = 0; i < point_lights.size(); ++i) {
			ImGui::PushID("PointLight");
			ImGui::PushID(static_cast<int>(i));

			ImGui::Text("Point Light %zu", i);

			ImGui::DragFloat3(
				"Position",
				&point_lights[i].position.x,
				0.01f
			);

			ImGui::ColorEdit3(
				"Color",
				&point_lights[i].color_intensity.x
			);

			ImGui::SliderFloat(
				"Intensity",
				&point_lights[i].color_intensity.w,
				0.0f,
				20.0f
			);

			ImGui::Separator();

			ImGui::PopID();
			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader("Spot Lights")) {
		if (ImGui::Button("Add Spot Light")) {
			spot_lights.push_back({
				glm::vec4(0.0f, 3.0f, 2.0f, 1.0f),
				glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),
				glm::vec4(1.0f, 1.0f, 1.0f, 5.0f),
				glm::vec4(
					glm::radians(15.0f),
					glm::radians(25.0f),
					0.0f,
					0.0f
				)
			});

			recreate_spot_lights_buffer();
			recreate_light_line_vertex_buffer();
		}

		if (ImGui::Button("Delete Spot Light") && spot_lights.size() > 1) {
			spot_lights.pop_back();
			recreate_spot_lights_buffer();
			recreate_light_line_vertex_buffer();
		}

		for (size_t i = 0; i < spot_lights.size(); ++i) {
			ImGui::PushID("SpotLight");
			ImGui::PushID(static_cast<int>(i));

			ImGui::Text("Spot Light %zu", i);

			ImGui::DragFloat3(
				"Position",
				&spot_lights[i].position.x,
				0.05f
			);

			ImGui::DragFloat3(
				"Direction",
				&spot_lights[i].direction.x,
				0.05f
			);

			spot_lights[i].direction =
				glm::vec4(
					glm::normalize(
						glm::vec3(spot_lights[i].direction)
					),
					0.0f
				);

			ImGui::ColorEdit3(
				"Color",
				&spot_lights[i].color_intensity.x
			);

			ImGui::DragFloat(
				"Intensity",
				&spot_lights[i].color_intensity.w,
				0.1f,
				0.0f,
				100.0f
			);

			float inner_angle = glm::degrees(spot_lights[i].angles.x);
			float outer_angle = glm::degrees(spot_lights[i].angles.y);

			ImGui::DragFloat(
				"Inner Angle",
				&inner_angle,
				0.5f,
				0.0f,
				90.0f
			);

			ImGui::DragFloat(
				"Outer Angle",
				&outer_angle,
				0.5f,
				0.0f,
				90.0f
			);

			if (inner_angle > outer_angle) {
				outer_angle = inner_angle;
			}

			spot_lights[i].angles.x = glm::radians(inner_angle);
			spot_lights[i].angles.y = glm::radians(outer_angle);

			ImGui::PopID();
			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader("Light Visualization", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox(
			"Show Light Sources",
			&show_light_visualization
		);
	}

	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3(
			"Camera Position",
			&camera_position.x,
			0.01f
		);

		ImGui::DragFloat3(
			"Camera Rotation",
			&camera_rotation.x,
			0.01f
		);

		ImGui::SliderFloat(
			"Camera FOV",
			&camera_fov,
			20.0f,
			120.0f
		);
	}

	ImGui::End();

	std::memcpy(
		point_lights_buffer_memory,
		point_lights.data(),
		sizeof(PointLight) * point_lights.size()
	);

	std::memcpy(
		spot_light_buffer_memory,
		spot_lights.data(),
		sizeof(SpotLight) * spot_lights.size()
	);

	model_uniform_buffer_memory->albedo = glm::vec4(
		pyramid_albedo,
		1.0f
	);

	model_uniform_buffer_memory->specular_color = glm::vec4(
		pyramid_specular_color,
		1.0f
	);

	model_uniform_buffer_memory->specular_coefficient =
    	pyramid_specular_coefficient;

	model_uniform_buffer_2_memory->albedo = glm::vec4(
		second_pyramid_albedo,
		1.0f
	);

	model_uniform_buffer_2_memory->specular_color = glm::vec4(
		second_pyramid_specular_color,
		1.0f
	);

	model_uniform_buffer_2_memory->specular_coefficient =
   		second_pyramid_specular_coefficient;

	scene_uniform_buffer_memory->camera_position =
		glm::vec4(
			camera_position,
			1.0f
		);
}

void render(const graphics::internal::FrameData& fd) {
    VkCommandBuffer command_buffer = fd.command_buffer;	

    vkResetCommandBuffer(command_buffer, 0);

	update_light_line_vertices();

    const VkCommandBufferBeginInfo command_buffer_begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,		// command buffer будет выполнен один раз
    };

    vkBeginCommandBuffer(command_buffer, &command_buffer_begin);

    VkClearValue clear_values[2] = {};

    clear_values[0].color = {
        { 0.1f, 0.1f, 0.1f, 1.0f }
    };

    clear_values[1].depthStencil = {
        1.0f, 0
    };

    const VkRenderPassBeginInfo render_pass_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = graphics::internal::context.render_pass,			
        .framebuffer = fd.framebuffer,									
        .renderArea = {													
            .offset = { 0, 0 },											
            .extent = graphics::internal::context.swapchain_extent		
        },
        .clearValueCount = 2,											
        .pClearValues = clear_values,									
    };

    vkCmdBeginRenderPass(command_buffer, &render_pass_begin,
        VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		graphics_pipeline);

	const VkViewport viewport = {
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(
			graphics::internal::context.swapchain_extent.width
		),
		.height = static_cast<float>(
			graphics::internal::context.swapchain_extent.height
		),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	vkCmdSetViewport(command_buffer, 0, 1, &viewport);

	const VkRect2D scissor = {
		.offset = { 0, 0 },
		.extent = graphics::internal::context.swapchain_extent,
	};

	vkCmdSetScissor(command_buffer, 0, 1, &scissor);

	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline_layout,
		0,                  
		1,                  
		&scene_descriptor_set, 0, nullptr);

	vkCmdBindDescriptorSets(
		command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline_layout,
		1,
		1,
		&model_descriptor_set,
		0,
		nullptr
	);

	VkDeviceSize vertex_offset = 0;

	vkCmdBindVertexBuffers(
		command_buffer,
		0,
		1,
		&vertex_buffer,
		&vertex_offset
	);

	vkCmdBindIndexBuffer(
		command_buffer,
		index_buffer,
		0,
		VK_INDEX_TYPE_UINT32
	);

	struct VisualizationPushConstants {
		glm::mat4 model;
		glm::vec4 color;
		float enabled;
	};

	VisualizationPushConstants visualization_push_constants = {
		glm::mat4(1.0f),
		glm::vec4(1.0f),
		0.0f
	};

	vkCmdPushConstants(
		command_buffer,
		pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(VisualizationPushConstants),
		&visualization_push_constants
	);

	vkCmdDrawIndexed(
		command_buffer,
		static_cast<uint32_t>(sizeof(indices) / sizeof(indices[0])),
		1,
		0,
		0,
		0
	);

	vkCmdBindDescriptorSets(
		command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline_layout,
		1,
		1,
		&model_descriptor_set_2,
		0,
		nullptr
	);

	vkCmdPushConstants(
		command_buffer,
		pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(VisualizationPushConstants),
		&visualization_push_constants
	);

	vkCmdDrawIndexed(
		command_buffer,
		static_cast<uint32_t>(sizeof(indices) / sizeof(indices[0])),
		1,
		0,
		0,
		0
	);

	if (show_light_visualization) {
		VkBuffer light_vertex_buffers[] = {
			light_visualization_vertex_buffer
		};

		VkDeviceSize light_vertex_offsets[] = {
			0
		};

		vkCmdBindVertexBuffers(
			command_buffer,
			0,
			1,
			light_vertex_buffers,
			light_vertex_offsets
		);

		vkCmdBindIndexBuffer(
			command_buffer,
			light_visualization_index_buffer,
			0,
			VK_INDEX_TYPE_UINT32
		);

		for (size_t i = 0; i < point_lights.size(); ++i) {
			VisualizationPushConstants light_push_constants = {
				glm::translate(
					glm::mat4(1.0f),
					glm::vec3(point_lights[i].position)
				) *
				glm::scale(
					glm::mat4(1.0f),
					glm::vec3(0.15f)
				),
				glm::vec4(
					point_lights[i].color_intensity.r,
					point_lights[i].color_intensity.g,
					point_lights[i].color_intensity.b,
					1.0f
				),
				1.0f
			};

			vkCmdPushConstants(
				command_buffer,
				pipeline_layout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(VisualizationPushConstants),
				&light_push_constants
			);

			vkCmdDrawIndexed(
				command_buffer,
				sizeof(light_visualization_indices) / sizeof(light_visualization_indices[0]),
				1,
				0,
				0,
				0
			);
		}

		for (size_t i = 0; i < spot_lights.size(); ++i) {
			VisualizationPushConstants spot_light_push_constants = {
				glm::translate(
					glm::mat4(1.0f),
					glm::vec3(spot_lights[i].position)
				) *
				glm::scale(
					glm::mat4(1.0f),
					glm::vec3(0.15f)
				),
				glm::vec4(
					spot_lights[i].color_intensity.r,
					spot_lights[i].color_intensity.g,
					spot_lights[i].color_intensity.b,
					1.0f
				),
				1.0f
			};

			vkCmdPushConstants(
				command_buffer,
				pipeline_layout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(VisualizationPushConstants),
				&spot_light_push_constants
			);

			vkCmdDrawIndexed(
				command_buffer,
				sizeof(light_visualization_indices) / sizeof(light_visualization_indices[0]),
				1,
				0,
				0,
				0
			);
		}

		vkCmdBindPipeline(
			command_buffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			light_line_pipeline
		);

		VkDeviceSize light_line_offset = 0;

		vkCmdBindVertexBuffers(
			command_buffer,
			0,
			1,
			&light_line_vertex_buffer,
			&light_line_offset
		);

		VisualizationPushConstants line_push_constants = {
			glm::mat4(1.0f),
			glm::vec4(1.0f),
			1.0f
		};

		vkCmdPushConstants(
			command_buffer,
			pipeline_layout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			sizeof(VisualizationPushConstants),
			&line_push_constants
		);

		vkCmdDraw(
			command_buffer,
			static_cast<uint32_t>(6 * spot_lights.size()),
			1,
			0,
			0
		);
	}

	vkCmdEndRenderPass(command_buffer);

    vkEndCommandBuffer(command_buffer);
}

} // namespace application