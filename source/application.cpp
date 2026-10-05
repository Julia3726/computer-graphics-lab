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
	float x;
	float y;
	float z;
	float r;
	float g;
	float b;
};

struct GlobalUniforms {
    glm::mat4 model;		
    glm::mat4 view;			
    glm::mat4 projection;	
	glm::vec4 color;
};

const Vertex vertices[] = {
    {-0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f},
    { 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f},
    { 0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f},
    {-0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.0f},
    { 0.0f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f},
};
 
const uint32_t indices[] = {
	0, 1, 4,
	1, 2, 4,
	2, 3, 4,
	3, 0, 4,

	0, 1, 2,
	0, 2, 3,
};


VkBuffer vertex_buffer = VK_NULL_HANDLE; 
VmaAllocation vertex_buffer_allocation = VK_NULL_HANDLE; 

VkBuffer index_buffer = VK_NULL_HANDLE; 
VmaAllocation index_buffer_allocation = VK_NULL_HANDLE; 

VkBuffer uniform_buffer = VK_NULL_HANDLE; 
VmaAllocation uniform_buffer_allocation = VK_NULL_HANDLE; 
GlobalUniforms* uniform_buffer_memory = nullptr; 

VkBuffer uniform_buffer_2 = VK_NULL_HANDLE;
VmaAllocation uniform_buffer_2_allocation = VK_NULL_HANDLE;
GlobalUniforms* uniform_buffer_2_memory = nullptr;

VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE; 

VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
VkDescriptorSet descriptor_set = VK_NULL_HANDLE; 
VkDescriptorSet descriptor_set_2 = VK_NULL_HANDLE;

VkPipelineLayout pipeline_layout = VK_NULL_HANDLE; // Описывает интерфейс конвейера с внешними ресурсами 
VkPipeline graphics_pipeline = VK_NULL_HANDLE; 

VkShaderModule vertex_shader_module = VK_NULL_HANDLE;
VkShaderModule fragment_shader_module = VK_NULL_HANDLE;

bool use_perspective = true;

glm::vec3 pyramid_position = glm::vec3(0.0f);
glm::vec3 animated_position = glm::vec3(0.0f);

glm::vec3 pyramid_rotation = glm::vec3(0.0f);
glm::vec3 animated_rotation = glm::vec3(0.0f);

glm::vec3 pyramid_scale = glm::vec3(1.0f);
glm::vec3 pyramid_color = glm::vec3(0.8f, 0.3f, 0.2f);

bool animation_playing = true;
double animation_time = 0.0;

float animation_speed = 0.5f;
float trajectory_radius_x = 1.3f;
float trajectory_radius_z = 1.3f;
float trajectory_height = 0.5f;
float rotation_speed = 1.0f;

glm::vec3 second_pyramid_position = glm::vec3(1.5f, 0.0f, -1.0f);
glm::vec3 second_pyramid_color = glm::vec3(0.2f, 0.4f, 1.0f);

float second_pyramid_rotation = 0.0f;
float second_pyramid_phase = glm::pi<float>();

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

VkVertexInputAttributeDescription vertex_color_attribute = {
    .location = 1,
    .binding = 0,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = sizeof(float) * 3,
};

const VkVertexInputAttributeDescription vertex_attributes[] = {
	vertex_position_attribute,
	vertex_color_attribute,
};

namespace application {

bool initialize() {
	auto& context = graphics::internal::context;

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

	const VkBufferCreateInfo uniform_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(GlobalUniforms),
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo uniform_buffer_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &uniform_buffer_info, &uniform_buffer_allocation_info,
			&uniform_buffer, &uniform_buffer_allocation, nullptr) != VK_SUCCESS) {
		std::cerr << "Failed to create uniform buffer\n";
		return false;
	}

	if (vmaCreateBuffer(context.allocator, &uniform_buffer_info, &uniform_buffer_allocation_info,
			&uniform_buffer_2, &uniform_buffer_2_allocation, nullptr) != VK_SUCCESS) {
		std::cerr << "Failed to create second uniform buffer\n";
		return false;
	}

	// Получаем указатель на память uniform buffer, чтобы CPU мог записывать туда матрицы
	void* uniform_data = nullptr;

	if (vmaMapMemory(context.allocator, uniform_buffer_allocation, &uniform_data) != VK_SUCCESS) {
		std::cerr << "Failed to map uniform buffer memory\n";
		return false;
	}

	uniform_buffer_memory = static_cast<GlobalUniforms*>(uniform_data);

	void* uniform_data_2 = nullptr;

	if (vmaMapMemory(context.allocator, uniform_buffer_2_allocation, &uniform_data_2) != VK_SUCCESS) {
		std::cerr << "Failed to map second uniform buffer memory\n";
		return false;
	}

	uniform_buffer_2_memory = static_cast<GlobalUniforms*>(uniform_data_2);

	// Начальное положение и ориентация пирамиды
	uniform_buffer_memory->model = glm::mat4(1.0f);

	// Матрица камеры
	uniform_buffer_memory->view = glm::lookAt(
		glm::vec3(2.0f, 2.0f, 2.0f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	uniform_buffer_2_memory->model = glm::translate(
		glm::mat4(1.0f),
		glm::vec3(1.5f, 0.0f, 0.0f)
	);

	uniform_buffer_2_memory->view = uniform_buffer_memory->view;

	// Описываем ресурс, который будет доступен шейдеру
	const VkDescriptorSetLayoutBinding uniform_buffer_binding = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,				// Дополнительные sampler'ы здесь не нужны

	};

	const VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &uniform_buffer_binding,
	};

	if (vkCreateDescriptorSetLayout(context.device, &descriptor_set_layout_info,
			nullptr, &descriptor_set_layout) != VK_SUCCESS) {
		std::cerr << "Failed to create descriptor set layout\n";
		return false;
	}

	const VkDescriptorPoolSize descriptor_pool_size = {
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 2,
	};

	const VkDescriptorPoolCreateInfo descriptor_pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 2,
		.poolSizeCount = 1,						
		.pPoolSizes = &descriptor_pool_size,

	};

	if (vkCreateDescriptorPool(context.device, &descriptor_pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) {
		std::cerr << "Failed to create descriptor pool\n";
		return false;
	}

	const VkDescriptorSetLayout descriptor_set_layouts[] = {
		descriptor_set_layout,
		descriptor_set_layout,
	};

	// Описываем, из какого layout нужно создать descriptor set
	const VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptor_pool,
		.descriptorSetCount = 2,
		.pSetLayouts = descriptor_set_layouts,
	};

	VkDescriptorSet descriptor_sets[] = {
		VK_NULL_HANDLE,
		VK_NULL_HANDLE,
	};

	// Выделяем descriptor set из descriptor pool
	if (vkAllocateDescriptorSets(context.device, &descriptor_set_allocate_info, descriptor_sets) != VK_SUCCESS) {
		std::cerr << "Failed to allocate descriptor sets\n";
		return false;
	}

	descriptor_set = descriptor_sets[0];
	descriptor_set_2 = descriptor_sets[1];

	// Описываем, какой конкретно Vulkan Buffer будет находиться в нашем descriptor set
	const VkDescriptorBufferInfo uniform_buffer_descriptor = {
		.buffer = uniform_buffer,
		.offset = 0,
		.range = sizeof(GlobalUniforms),
	};

	const VkDescriptorBufferInfo uniform_buffer_descriptor_2 = {
		.buffer = uniform_buffer_2,
		.offset = 0,
		.range = sizeof(GlobalUniforms),
	};

	// Описываем изменение descriptor set
	const VkWriteDescriptorSet descriptor_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descriptor_set,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &uniform_buffer_descriptor,
	};

	const VkWriteDescriptorSet descriptor_write_2 = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descriptor_set_2,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &uniform_buffer_descriptor_2,
	};

	const VkWriteDescriptorSet descriptor_writes[] = {
		descriptor_write,
		descriptor_write_2,
	};

	// Записываем информацию о Uniform Buffer в descriptor set.
	vkUpdateDescriptorSets(context.device, 2, descriptor_writes, 0, nullptr);

	const VkPipelineLayoutCreateInfo pipeline_layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptor_set_layout,	
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
	return true;
}

void shutdown() {
	auto& context = graphics::internal::context;

	vkQueueWaitIdle(context.graphics_queue);

	vkDestroyPipeline(context.device, graphics_pipeline, nullptr);
	vkDestroyPipelineLayout(context.device, pipeline_layout, nullptr);
	vkDestroyDescriptorPool(context.device, descriptor_pool, nullptr);
	vkDestroyDescriptorSetLayout(context.device, descriptor_set_layout, nullptr);

	vkDestroyShaderModule(context.device, vertex_shader_module, nullptr);
	vkDestroyShaderModule(context.device, fragment_shader_module, nullptr);

	vmaUnmapMemory(context.allocator, uniform_buffer_allocation);
	vmaUnmapMemory(context.allocator, uniform_buffer_2_allocation);

	vmaDestroyBuffer(context.allocator, uniform_buffer, uniform_buffer_allocation);
	vmaDestroyBuffer(context.allocator, vertex_buffer, vertex_buffer_allocation);
	vmaDestroyBuffer(context.allocator, uniform_buffer_2, uniform_buffer_2_allocation);

	vmaDestroyBuffer(context.allocator, index_buffer, index_buffer_allocation);
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

	uniform_buffer_memory->model = model;

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

	uniform_buffer_2_memory->model = second_model;

	float aspect_ratio =
		static_cast<float>(graphics::internal::context.swapchain_extent.width) /
		static_cast<float>(graphics::internal::context.swapchain_extent.height);

	if (use_perspective) {
		uniform_buffer_memory->projection = glm::perspectiveRH_ZO(
			glm::radians(60.0f),
			aspect_ratio,
			0.1f,
			10.0f
		);
	} else {
		uniform_buffer_memory->projection = glm::orthoRH_ZO(
			-3.0f * aspect_ratio,
			3.0f * aspect_ratio,
			-3.0f,
			3.0f,
			0.1f,
			10.0f
		);
	}

	uniform_buffer_memory->projection[1][1] *= -1.0f;

	uniform_buffer_2_memory->view = uniform_buffer_memory->view;
	uniform_buffer_2_memory->projection = uniform_buffer_memory->projection;

	ImGui::Begin("Pyramid");

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

	if (ImGui::Button("Perspective")) {
		use_perspective = true;
	}

	if (ImGui::Button("Orthographic")) {
		use_perspective = false;
	}

	ImGui::Separator();

	ImGui::SliderFloat3(
		"Position",
		&pyramid_position.x,
		-3.0f,
		3.0f
	);

	ImGui::SliderFloat3(
		"Rotation",
		&pyramid_rotation.x,
		-3.14f,
		3.14f
	);

	ImGui::SliderFloat3(
		"Scale",
		&pyramid_scale.x,
		0.1f,
		3.0f
	);

	ImGui::ColorEdit3(
		"Color",
		&pyramid_color.x
	);

	ImGui::End();

	uniform_buffer_memory->color = glm::vec4(
		pyramid_color,
		1.0f
	);

	uniform_buffer_2_memory->color = glm::vec4(
		second_pyramid_color,
		1.0f
	);
}

void render(const graphics::internal::FrameData& fd) {
    VkCommandBuffer command_buffer = fd.command_buffer;				

    vkResetCommandBuffer(command_buffer, 0);

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
		&descriptor_set, 0, nullptr);

	VkDeviceSize vertex_offset = 0;
	vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, &vertex_offset);

	vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(sizeof(indices) / sizeof(indices[0])),
		1, 0, 0, 0);

	vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(sizeof(indices) / sizeof(indices[0])),
   		1, 0, 0, 0);

	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline_layout,
		0,
		1,
		&descriptor_set_2, 0, nullptr);

	vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(sizeof(indices) / sizeof(indices[0])),
		1, 0, 0, 0);

	vkCmdEndRenderPass(command_buffer);

    vkEndCommandBuffer(command_buffer);
}

} // namespace application