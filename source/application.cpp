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
#include "lodepng.h"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
	glm::vec2 uv;
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
	glm::vec2 uv;
};

struct ModelUniforms {
    glm::mat4 model;
    glm::vec4 albedo;				// основной цвет материала
    glm::vec4 specular_color;		// цвет блика
    float specular_coefficient;	// насколько блик концентрированный
};

struct LoadedPng {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<unsigned char> pixels;
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
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{ 0.0f,  0.5f,  0.0f}, {0.0f, 0.0f, 0.0f}, {0.5f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},

    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{ 0.0f,  0.5f,  0.0f}, {0.0f, 0.0f, 0.0f}, {0.5f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},

    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{ 0.0f,  0.5f,  0.0f}, {0.0f, 0.0f, 0.0f}, {0.5f, 0.0f}},
    {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},

    {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{ 0.0f,  0.5f,  0.0f}, {0.0f, 0.0f, 0.0f}, {0.5f, 0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
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

VkImage albedo_texture_image = VK_NULL_HANDLE;
VmaAllocation albedo_texture_allocation = VK_NULL_HANDLE;
VkImageView albedo_texture_view = VK_NULL_HANDLE;
VkSampler albedo_texture_sampler = VK_NULL_HANDLE;

VkImage second_albedo_texture_image = VK_NULL_HANDLE;
VmaAllocation second_albedo_texture_allocation = VK_NULL_HANDLE;
VkImageView second_albedo_texture_view = VK_NULL_HANDLE;

VkImage black_fallback_texture_image = VK_NULL_HANDLE;
VmaAllocation black_fallback_texture_allocation = VK_NULL_HANDLE;
VkImageView black_fallback_texture_view = VK_NULL_HANDLE;

VkImage specular_texture_image = VK_NULL_HANDLE;
VmaAllocation specular_texture_allocation = VK_NULL_HANDLE;
VkImageView specular_texture_view = VK_NULL_HANDLE;

VkImage emissive_texture_image = VK_NULL_HANDLE;
VmaAllocation emissive_texture_allocation = VK_NULL_HANDLE;
VkImageView emissive_texture_view = VK_NULL_HANDLE;

VkBuffer texture_staging_buffer = VK_NULL_HANDLE;
VmaAllocation texture_staging_allocation = VK_NULL_HANDLE;

SceneUniforms* scene_uniform_buffer_memory = nullptr;

VkBuffer scene_uniform_buffer = VK_NULL_HANDLE;
VmaAllocation scene_uniform_buffer_allocation = VK_NULL_HANDLE;

VkBuffer point_lights_buffer;
VmaAllocation point_lights_buffer_allocation;
PointLight* point_lights_buffer_memory;

VkBuffer spot_light_buffer;
VmaAllocation spot_light_buffer_allocation;
SpotLight* spot_light_buffer_memory;

VkDeviceSize model_uniform_stride = 0;

VkBuffer dynamic_model_uniform_buffer = VK_NULL_HANDLE;
VmaAllocation dynamic_model_uniform_buffer_allocation = VK_NULL_HANDLE;
unsigned char* dynamic_model_uniform_buffer_memory = nullptr;

ModelUniforms& get_dynamic_model_uniform(uint32_t model_index) {
    return *reinterpret_cast<ModelUniforms*>(
        dynamic_model_uniform_buffer_memory +
        model_uniform_stride * model_index
    );
}


bool sampling_effect_enabled = false; 

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

glm::vec3 pyramid_albedo = glm::vec3(0.85f, 0.78f, 0.64f);
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

glm::vec3 second_pyramid_albedo = glm::vec3(0.45f, 0.28f, 0.16f);
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

int pyramid_texture_index = 0;
int second_pyramid_texture_index = 1;

std::vector<SpotLight> spot_lights = {
    {
        glm::vec4(1.3f, 1.0f, -0.5f, 1.0f),
		glm::vec4(-0.6f, -0.8f, 0.0f, 0.0f),
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
        glm::vec4(2.0f, 0.0f, -0.5f, 1.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 5.0f),
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
            glm::vec3(0.0f),
			glm::vec2(0.0f)
        };

        light_line_vertex_buffer_memory[vertex + 1] = {
            position + direction * length,
            glm::vec3(0.0f),
			glm::vec2(0.0f)
        };

        light_line_vertex_buffer_memory[vertex + 2] = {
            position,
            glm::vec3(0.0f),
			glm::vec2(0.0f)
        };

        light_line_vertex_buffer_memory[vertex + 3] = {
            position + left_direction * length,
            glm::vec3(0.0f),
			glm::vec2(0.0f)
        };

        light_line_vertex_buffer_memory[vertex + 4] = {
            position,
            glm::vec3(0.0f),
			glm::vec2(0.0f)
        };

        light_line_vertex_buffer_memory[vertex + 5] = {
            position + right_direction * length,
            glm::vec3(0.0f),
			glm::vec2(0.0f)
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

bool load_png_rgba(const std::string& filename, LoadedPng& image) {
    unsigned error = lodepng::decode(
        image.pixels,
        image.width,
        image.height,
        filename
    );

    if (error != 0) {
        std::cerr
            << "Failed to load PNG: "
            << filename
            << ": "
            << lodepng_error_text(error)
            << '\n';

        return false;
    }

    return true;
}

LoadedPng make_specular_map(const LoadedPng& source) {
    LoadedPng specular_map;
    specular_map.width = source.width;
    specular_map.height = source.height;
    specular_map.pixels.resize(source.pixels.size());

    for (size_t i = 0; i < source.pixels.size(); i += 4) {
        // Усредняем RGB состовляющие - простая оценка яркости
		const unsigned int brightness =
            (source.pixels[i] +
             source.pixels[i + 1] +
             source.pixels[i + 2]) / 3;

		// где будет блик
        const unsigned char mask =
            brightness > 128 ? 255 : 0;


        specular_map.pixels[i] = mask;
        specular_map.pixels[i + 1] = mask;
        specular_map.pixels[i + 2] = mask;
        specular_map.pixels[i + 3] = 255;	// полная непрозрачность
    }

    return specular_map;
}

LoadedPng make_emissive_map(const LoadedPng& source) {
    LoadedPng emissive_map;
    emissive_map.width = source.width;
    emissive_map.height = source.height;
    emissive_map.pixels.resize(source.pixels.size());

    for (size_t i = 0; i < source.pixels.size(); i += 4) {
        const unsigned int brightness =
            (source.pixels[i] +
             source.pixels[i + 1] +
             source.pixels[i + 2]) / 3;

        const bool is_glowing = brightness > 220;

        emissive_map.pixels[i] =
            is_glowing ? 255 : 0;
        emissive_map.pixels[i + 1] =
            is_glowing ? 90 : 0;
        emissive_map.pixels[i + 2] =
            is_glowing ? 20 : 0;
        emissive_map.pixels[i + 3] = 255;
    }

    return emissive_map;
}

bool upload_texture_pixels(const LoadedPng& image, VkImage destination_image) {
    auto& context = graphics::internal::context;

    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkFence upload_fence = VK_NULL_HANDLE;

    auto cleanup_commands = [&]() {
        if (upload_fence != VK_NULL_HANDLE) {
            vkDestroyFence(context.device, upload_fence, nullptr);
        }

        if (command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(context.device, command_pool, nullptr);
        }
    };

    const VkCommandPoolCreateInfo command_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = context.graphics_queue_index,
    };

    if (vkCreateCommandPool(
            context.device,
            &command_pool_info,
            nullptr,
            &command_pool
        ) != VK_SUCCESS) {
        std::cerr << "Failed to create texture command pool\n";
        return false;
    }

    const VkCommandBufferAllocateInfo command_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    if (vkAllocateCommandBuffers(
            context.device,
            &command_buffer_info,
            &command_buffer
        ) != VK_SUCCESS) {
        std::cerr << "Failed to allocate texture command buffer\n";
        cleanup_commands();
        return false;
    }

    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        std::cerr << "Failed to begin texture command buffer\n";
        cleanup_commands();
        return false;
    }

    VkImageMemoryBarrier to_transfer = {};
    to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_transfer.srcAccessMask = 0;
    to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.image = destination_image;
    to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_transfer.subresourceRange.baseMipLevel = 0;
    to_transfer.subresourceRange.levelCount = 1;
    to_transfer.subresourceRange.baseArrayLayer = 0;
    to_transfer.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &to_transfer
    );

    VkBufferImageCopy copy_region = {};
    copy_region.bufferOffset = 0;
    copy_region.bufferRowLength = 0;
    copy_region.bufferImageHeight = 0;
    copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.mipLevel = 0;
    copy_region.imageSubresource.baseArrayLayer = 0;
    copy_region.imageSubresource.layerCount = 1;
    copy_region.imageOffset = {0, 0, 0};
    copy_region.imageExtent = {
        image.width,
        image.height,
        1
    };

    vkCmdCopyBufferToImage(
        command_buffer,
        texture_staging_buffer,
        destination_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copy_region
    );

    VkImageMemoryBarrier to_shader_read = {};
    to_shader_read.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_shader_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_shader_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    to_shader_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_shader_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader_read.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader_read.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader_read.image = destination_image;
    to_shader_read.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_shader_read.subresourceRange.baseMipLevel = 0;
    to_shader_read.subresourceRange.levelCount = 1;
    to_shader_read.subresourceRange.baseArrayLayer = 0;
    to_shader_read.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &to_shader_read
    );

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to end texture command buffer\n";
        cleanup_commands();
        return false;
    }

    const VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };

    if (vkCreateFence(
            context.device,
            &fence_info,
            nullptr,
            &upload_fence
        ) != VK_SUCCESS) {
        std::cerr << "Failed to create texture upload fence\n";
        cleanup_commands();
        return false;
    }

    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };

    if (vkQueueSubmit(
            context.graphics_queue,
            1,
            &submit_info,
            upload_fence
        ) != VK_SUCCESS) {
        std::cerr << "Failed to submit texture upload commands\n";
        cleanup_commands();
        return false;
    }

    if (vkWaitForFences(
            context.device,
            1,
            &upload_fence,
            VK_TRUE,
            UINT64_MAX
        ) != VK_SUCCESS) {
        std::cerr << "Failed to wait for texture upload\n";
        cleanup_commands();
        return false;
    }

    cleanup_commands();

    vmaDestroyBuffer(
        context.allocator,
        texture_staging_buffer,
        texture_staging_allocation
    );

    texture_staging_buffer = VK_NULL_HANDLE;
    texture_staging_allocation = VK_NULL_HANDLE;

    return true;
}

// Копируем байты из одного уже созданного буфера в другой
bool copy_buffer(
    VkBuffer source_buffer,
    VkBuffer destination_buffer,
    VkDeviceSize size
) {
    auto& context = graphics::internal::context;

    VkCommandPool command_pool = VK_NULL_HANDLE;

    const VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = context.graphics_queue_index,
    };

    if (vkCreateCommandPool(
            context.device,
            &pool_info,
            nullptr,
            &command_pool
        ) != VK_SUCCESS) {
        std::cerr << "Failed to create buffer copy command pool\n";
        return false;
    }

    const VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;

    if (vkAllocateCommandBuffers(
            context.device,
            &allocate_info,
            &command_buffer
        ) != VK_SUCCESS) {
        std::cerr << "Failed to allocate buffer copy command buffer\n";
        vkDestroyCommandPool(context.device, command_pool, nullptr);
        return false;
    }

    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        std::cerr << "Failed to begin buffer copy command buffer\n";
        vkDestroyCommandPool(context.device, command_pool, nullptr);
        return false;
    }

    const VkBufferCopy copy_region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size,
    };

    vkCmdCopyBuffer(
        command_buffer,
        source_buffer,
        destination_buffer,
        1,
        &copy_region
    );

	// Копирование в буфер завершило запись. Можно читать как атрибуты вершин или индексы
	const VkBufferMemoryBarrier buffer_barrier = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask =
			VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
			VK_ACCESS_INDEX_READ_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer = destination_buffer,
		.offset = 0,
		.size = size,
	};

	vkCmdPipelineBarrier(
		command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		0,
		0, nullptr,
		1, &buffer_barrier,
		0, nullptr
	);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to end buffer copy command buffer\n";
        vkDestroyCommandPool(context.device, command_pool, nullptr);
        return false;
    }

	// Отправка команды видеокарте
    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };

	// Какую команду выполнить для очереди 
    if (vkQueueSubmit(
            context.graphics_queue,
            1,
            &submit_info,
            VK_NULL_HANDLE
        ) != VK_SUCCESS) {
        std::cerr << "Failed to submit buffer copy commands\n";
        vkDestroyCommandPool(context.device, command_pool, nullptr);
        return false;
    }

    if (vkQueueWaitIdle(context.graphics_queue) != VK_SUCCESS) {
        std::cerr << "Failed to wait for buffer copy\n";
        vkDestroyCommandPool(context.device, command_pool, nullptr);
        return false;
    }

    vkDestroyCommandPool(context.device, command_pool, nullptr);
    return true;
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

VkVertexInputAttributeDescription vertex_uv_attribute = {
    .location = 2,
    .binding = 0,
    .format = VK_FORMAT_R32G32_SFLOAT,
    .offset = offsetof(Vertex, uv),
};

const VkVertexInputAttributeDescription vertex_attributes[] = {
    vertex_position_attribute,
    vertex_normal_attribute,
	vertex_uv_attribute,
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

VkVertexInputAttributeDescription light_line_vertex_uv_attribute = {
    .location = 2,
    .binding = 0,
    .format = VK_FORMAT_R32G32_SFLOAT,
    .offset = offsetof(LightLineVertex, uv),
};

const VkVertexInputAttributeDescription light_line_vertex_attributes[] = {
    light_line_vertex_position_attribute,
    light_line_vertex_normal_attribute,
	light_line_vertex_uv_attribute,
};

namespace application {

bool initialize() {
	auto& context = graphics::internal::context;

	VkPhysicalDeviceProperties device_properties{};
	vkGetPhysicalDeviceProperties(
		context.physical_device,
		&device_properties
	);

	const VkDeviceSize model_uniform_size = sizeof(ModelUniforms);
	const VkDeviceSize alignment =
		device_properties.limits.minUniformBufferOffsetAlignment;

	model_uniform_stride =
		((model_uniform_size + alignment - 1) / alignment) * alignment;

	calculate_vertex_normals(vertices);

	LoadedPng check_image;

	if (!load_png_rgba("textures/check.png", check_image)) {
		return false;
	}

	std::cout
		<< "PNG loaded: "
		<< check_image.width << " x " << check_image.height
		<< ", bytes: " << check_image.pixels.size()
		<< '\n';

	LoadedPng second_image;

	if (!load_png_rgba("textures/second.png", second_image)) {
		return false;
	}

	std::cout
		<< "Second PNG loaded: "
		<< second_image.width << " x " << second_image.height
		<< ", bytes: " << second_image.pixels.size()
		<< '\n';

	LoadedPng specular_image = make_specular_map(second_image);
	LoadedPng emissive_image = make_emissive_map(second_image);

	LoadedPng black_fallback_image;
		black_fallback_image.width = 1;
		black_fallback_image.height = 1;
		black_fallback_image.pixels = {
			0,   // красный
			0,   // зелёный
			0,   // синий
			255  // альфа
		};

	const VkImageCreateInfo texture_image_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.extent = {
			check_image.width,
			check_image.height,
			1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage =
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	const VmaAllocationCreateInfo texture_allocation_info = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
	};

	if (vmaCreateImage(
			context.allocator,
			&texture_image_info,
			&texture_allocation_info,
			&albedo_texture_image,
			&albedo_texture_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create texture image\n";
		return false;
	}

	const VkImageCreateInfo second_texture_image_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.extent = {
			second_image.width,
			second_image.height,
			1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage =
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	if (vmaCreateImage(
			context.allocator,
			&second_texture_image_info,
			&texture_allocation_info,
			&second_albedo_texture_image,
			&second_albedo_texture_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create second texture image\n";
		return false;
	}

	const VkImageCreateInfo black_fallback_image_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.extent = {
			black_fallback_image.width,
			black_fallback_image.height,
			1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage =
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	if (vmaCreateImage(
			context.allocator,
			&black_fallback_image_info,
			&texture_allocation_info,
			&black_fallback_texture_image,
			&black_fallback_texture_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create black fallback texture image\n";
		return false;
	}

	const VkImageCreateInfo specular_image_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.extent = {
			specular_image.width,
			specular_image.height,
			1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage =
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	if (vmaCreateImage(
			context.allocator,
			&specular_image_info,
			&texture_allocation_info,
			&specular_texture_image,
			&specular_texture_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create specular texture image\n";
		return false;
	}

	const VkImageCreateInfo emissive_image_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.extent = {
			emissive_image.width,
			emissive_image.height,
			1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage =
			VK_IMAGE_USAGE_TRANSFER_DST_BIT |
			VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	if (vmaCreateImage(
			context.allocator,
			&emissive_image_info,
			&texture_allocation_info,
			&emissive_texture_image,
			&emissive_texture_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create emissive texture image\n";
		return false;
	}

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

	const VkDeviceSize texture_data_size =
		static_cast<VkDeviceSize>(check_image.pixels.size());

	const VkBufferCreateInfo staging_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = texture_data_size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo staging_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&staging_buffer_info,
			&staging_allocation_info,
			&texture_staging_buffer,
			&texture_staging_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create texture staging buffer\n";
		return false;
	}

	void* texture_staging_memory = nullptr;

	if (vmaMapMemory(
			context.allocator,
			texture_staging_allocation,
			&texture_staging_memory
		) != VK_SUCCESS) {
		std::cerr << "Failed to map texture staging buffer\n";

		vmaDestroyBuffer(
			context.allocator,
			texture_staging_buffer,
			texture_staging_allocation
		);

		texture_staging_buffer = VK_NULL_HANDLE;
		texture_staging_allocation = VK_NULL_HANDLE;

		return false;
	}

	std::memcpy(
		texture_staging_memory,
		check_image.pixels.data(),
		check_image.pixels.size()
	);

	vmaFlushAllocation(
		context.allocator,
		texture_staging_allocation,
		0,
		texture_data_size
	);

	vmaUnmapMemory(
		context.allocator,
		texture_staging_allocation
	);

	if (!upload_texture_pixels(check_image, albedo_texture_image)) {
		return false;
	}

	const VkDeviceSize second_texture_data_size =
		static_cast<VkDeviceSize>(second_image.pixels.size());

	const VkBufferCreateInfo second_staging_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = second_texture_data_size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&second_staging_buffer_info,
			&staging_allocation_info,
			&texture_staging_buffer,
			&texture_staging_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create second texture staging buffer\n";
		return false;
	}

	void* second_texture_staging_memory = nullptr;

	if (vmaMapMemory(
			context.allocator,
			texture_staging_allocation,
			&second_texture_staging_memory
		) != VK_SUCCESS) {
		std::cerr << "Failed to map second texture staging buffer\n";
		return false;
	}

	std::memcpy(
		second_texture_staging_memory,
		second_image.pixels.data(),
		second_texture_data_size
	);

	vmaFlushAllocation(
		context.allocator,
		texture_staging_allocation,
		0,
		second_texture_data_size
	);

	vmaUnmapMemory(context.allocator, texture_staging_allocation);

	if (!upload_texture_pixels(second_image, second_albedo_texture_image)) {
		return false;
	}

	const VkDeviceSize black_fallback_data_size =
		static_cast<VkDeviceSize>(black_fallback_image.pixels.size());

	const VkBufferCreateInfo black_fallback_staging_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = black_fallback_data_size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&black_fallback_staging_info,
			&staging_allocation_info,
			&texture_staging_buffer,
			&texture_staging_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create black fallback staging buffer\n";
		return false;
	}

	void* black_fallback_staging_memory = nullptr;

	if (vmaMapMemory(
			context.allocator,
			texture_staging_allocation,
			&black_fallback_staging_memory
		) != VK_SUCCESS) {
		std::cerr << "Failed to map black fallback staging buffer\n";
		return false;
	}

	std::memcpy(
		black_fallback_staging_memory,
		black_fallback_image.pixels.data(),
		black_fallback_data_size
	);

	vmaFlushAllocation(
		context.allocator,
		texture_staging_allocation,
		0,
		black_fallback_data_size
	);

	vmaUnmapMemory(context.allocator, texture_staging_allocation);

	if (!upload_texture_pixels(
			black_fallback_image,
			black_fallback_texture_image
		)) {
		return false;
	}

	// Загружаем пиксели через staging
	const VkDeviceSize specular_data_size =
		static_cast<VkDeviceSize>(specular_image.pixels.size());

	const VkBufferCreateInfo specular_staging_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = specular_data_size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&specular_staging_info,
			&staging_allocation_info,
			&texture_staging_buffer,
			&texture_staging_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create specular staging buffer\n";
		return false;
	}

	void* specular_staging_memory = nullptr;

	if (vmaMapMemory(
			context.allocator,
			texture_staging_allocation,
			&specular_staging_memory
		) != VK_SUCCESS) {
		std::cerr << "Failed to map specular staging buffer\n";
		return false;
	}

	std::memcpy(
		specular_staging_memory,
		specular_image.pixels.data(),
		specular_data_size
	);

	vmaFlushAllocation(
		context.allocator,
		texture_staging_allocation,
		0,
		specular_data_size
	);

	vmaUnmapMemory(context.allocator, texture_staging_allocation);

	if (!upload_texture_pixels(specular_image, specular_texture_image)) {
		return false;
	}

	// Загружаем пиксели через staging
	const VkDeviceSize emissive_data_size =
		static_cast<VkDeviceSize>(emissive_image.pixels.size());

	const VkBufferCreateInfo emissive_staging_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = emissive_data_size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&emissive_staging_info,
			&staging_allocation_info,
			&texture_staging_buffer,
			&texture_staging_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create emissive staging buffer\n";
		return false;
	}

	void* emissive_staging_memory = nullptr;

	if (vmaMapMemory(
			context.allocator,
			texture_staging_allocation,
			&emissive_staging_memory
		) != VK_SUCCESS) {
		std::cerr << "Failed to map emissive staging buffer\n";
		return false;
	}

	std::memcpy(
		emissive_staging_memory,
		emissive_image.pixels.data(),
		emissive_data_size
	);

	vmaFlushAllocation(
		context.allocator,
		texture_staging_allocation,
		0,
		emissive_data_size
	);

	vmaUnmapMemory(context.allocator, texture_staging_allocation);

	if (!upload_texture_pixels(emissive_image, emissive_texture_image)) {
		return false;
	}

	const VkImageViewCreateInfo texture_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = albedo_texture_image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	if (vkCreateImageView(
			context.device,
			&texture_view_info,
			nullptr,
			&albedo_texture_view
		) != VK_SUCCESS) {
		std::cerr << "Failed to create texture image view\n";
		return false;
	}

	const VkImageViewCreateInfo second_texture_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = second_albedo_texture_image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	if (vkCreateImageView(
			context.device,
			&second_texture_view_info,
			nullptr,
			&second_albedo_texture_view
		) != VK_SUCCESS) {
		std::cerr << "Failed to create second texture image view\n";
		return false;
	}

	const VkImageViewCreateInfo black_fallback_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = black_fallback_texture_image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	if (vkCreateImageView(
			context.device,
			&black_fallback_view_info,
			nullptr,
			&black_fallback_texture_view
		) != VK_SUCCESS) {
		std::cerr << "Failed to create black fallback image view\n";
		return false;
	}

	const VkImageViewCreateInfo specular_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = specular_texture_image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	if (vkCreateImageView(
			context.device,
			&specular_view_info,
			nullptr,
			&specular_texture_view
		) != VK_SUCCESS) {
		std::cerr << "Failed to create specular texture image view\n";
		return false;
	}

		const VkImageViewCreateInfo emissive_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = emissive_texture_image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	if (vkCreateImageView(
			context.device,
			&emissive_view_info,
			nullptr,
			&emissive_texture_view
		) != VK_SUCCESS) {
		std::cerr << "Failed to create specular texture image view\n";
		return false;
	}

	const VkSamplerCreateInfo texture_sampler_info = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.anisotropyEnable = VK_FALSE,
		.maxAnisotropy = 1.0f,
		.compareEnable = VK_FALSE,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE,
	};

	if (vkCreateSampler(
			context.device,
			&texture_sampler_info,
			nullptr,
			&albedo_texture_sampler
		) != VK_SUCCESS) {
		std::cerr << "Failed to create texture sampler\n";
		return false;
	}

	// Связываем описание стадии с соответствующим shader module
	vertex_shader_stage.module = vertex_shader_module;
	fragment_shader_stage.module = fragment_shader_module;

	const VkDeviceSize vertex_buffer_size = sizeof(vertices);

	const VkBufferCreateInfo vertex_staging_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = vertex_buffer_size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,			// Источник данных для копирования
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo vertex_staging_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	VkBuffer vertex_staging_buffer = VK_NULL_HANDLE;
	VmaAllocation vertex_staging_allocation = VK_NULL_HANDLE;

	if (vmaCreateBuffer(
			context.allocator,
			&vertex_staging_buffer_info,
			&vertex_staging_allocation_info,
			&vertex_staging_buffer,
			&vertex_staging_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create vertex staging buffer\n";
		return false;
	}

	void* vertex_staging_memory = nullptr;

	if (vmaMapMemory(
			context.allocator,
			vertex_staging_allocation,
			&vertex_staging_memory
		) != VK_SUCCESS) {
		std::cerr << "Failed to map vertex staging buffer\n";
		vmaDestroyBuffer(
			context.allocator,
			vertex_staging_buffer,
			vertex_staging_allocation
		);
		return false;
	}

	std::memcpy(vertex_staging_memory, vertices, vertex_buffer_size);

	vmaFlushAllocation(
		context.allocator,
		vertex_staging_allocation,
		0,
		vertex_buffer_size
	);

	vmaUnmapMemory(context.allocator, vertex_staging_allocation);

	const VkBufferCreateInfo vertex_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = vertex_buffer_size,
		.usage =
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo vertex_buffer_allocation_info = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&vertex_buffer_info,
			&vertex_buffer_allocation_info,
			&vertex_buffer,
			&vertex_buffer_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create GPU vertex buffer\n";
		vmaDestroyBuffer(
			context.allocator,
			vertex_staging_buffer,
			vertex_staging_allocation
		);
		return false;
	}

	if (!copy_buffer(
			vertex_staging_buffer,
			vertex_buffer,
			vertex_buffer_size
		)) {
		std::cerr << "Failed to copy vertices to GPU buffer\n";

		vmaDestroyBuffer(
			context.allocator,
			vertex_staging_buffer,
			vertex_staging_allocation
		);

		vmaDestroyBuffer(
			context.allocator,
			vertex_buffer,
			vertex_buffer_allocation
		);

		vertex_buffer = VK_NULL_HANDLE;
		vertex_buffer_allocation = VK_NULL_HANDLE;

		return false;
	}

	vmaDestroyBuffer(
		context.allocator,
		vertex_staging_buffer,
		vertex_staging_allocation
	);

	const VkDeviceSize index_buffer_size = sizeof(indices);

	const VkBufferCreateInfo index_staging_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = index_buffer_size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo index_staging_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	VkBuffer index_staging_buffer = VK_NULL_HANDLE;
	VmaAllocation index_staging_allocation = VK_NULL_HANDLE;

	if (vmaCreateBuffer(
			context.allocator,
			&index_staging_buffer_info,
			&index_staging_allocation_info,
			&index_staging_buffer,
			&index_staging_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create index staging buffer\n";
		return false;
	}

	void* index_staging_memory = nullptr;

	if (vmaMapMemory(
			context.allocator,
			index_staging_allocation,
			&index_staging_memory
		) != VK_SUCCESS) {
		std::cerr << "Failed to map index staging buffer\n";
		vmaDestroyBuffer(
			context.allocator,
			index_staging_buffer,
			index_staging_allocation
		);
		return false;
	}

	std::memcpy(index_staging_memory, indices, index_buffer_size);

	vmaFlushAllocation(
		context.allocator,
		index_staging_allocation,
		0,
		index_buffer_size
	);

	vmaUnmapMemory(context.allocator, index_staging_allocation);

	const VkBufferCreateInfo index_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = index_buffer_size,
		.usage =
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo index_buffer_allocation_info = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&index_buffer_info,
			&index_buffer_allocation_info,
			&index_buffer,
			&index_buffer_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create GPU index buffer\n";
		vmaDestroyBuffer(
			context.allocator,
			index_staging_buffer,
			index_staging_allocation
		);
		return false;
	}

	if (!copy_buffer(
			index_staging_buffer,
			index_buffer,
			index_buffer_size
		)) {
		std::cerr << "Failed to copy indices to GPU buffer\n";

		vmaDestroyBuffer(
			context.allocator,
			index_staging_buffer,
			index_staging_allocation
		);

		vmaDestroyBuffer(
			context.allocator,
			index_buffer,
			index_buffer_allocation
		);

		index_buffer = VK_NULL_HANDLE;
		index_buffer_allocation = VK_NULL_HANDLE;

		return false;
	}

	vmaDestroyBuffer(
		context.allocator,
		index_staging_buffer,
		index_staging_allocation
	);


	const VmaAllocationCreateInfo dynamic_model_uniform_buffer_allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	const VkBufferCreateInfo dynamic_model_uniform_buffer_create_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = model_uniform_stride * 2,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	if (vmaCreateBuffer(
			context.allocator,
			&dynamic_model_uniform_buffer_create_info,
			&dynamic_model_uniform_buffer_allocation_info,
			&dynamic_model_uniform_buffer,
			&dynamic_model_uniform_buffer_allocation,
			nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create dynamic model uniform buffer\n";
		return false;
	}

	void* dynamic_model_uniform_data = nullptr;

	if (vmaMapMemory(
			context.allocator,
			dynamic_model_uniform_buffer_allocation,
			&dynamic_model_uniform_data
		) != VK_SUCCESS) {
		std::cerr << "Failed to map dynamic model uniform buffer memory\n";
		return false;
	}

	dynamic_model_uniform_buffer_memory =
		static_cast<unsigned char*>(dynamic_model_uniform_data);

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

	if (vmaCreateBuffer(context.allocator, &scene_uniform_buffer_create_info,
			&dynamic_model_uniform_buffer_allocation_info, &scene_uniform_buffer,
		    &scene_uniform_buffer_allocation, nullptr
		) != VK_SUCCESS) {
		std::cerr << "Failed to create scene uniform buffer\n";
		return false;
	}

	void* scene_uniform_data = nullptr;

	if (vmaMapMemory(context.allocator, scene_uniform_buffer_allocation,
			&scene_uniform_data) != VK_SUCCESS) {
		std::cerr << "Failed to map scene uniform buffer memory\n";
		return false;
	}

	scene_uniform_buffer_memory = static_cast<SceneUniforms*>(scene_uniform_data);

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
	get_dynamic_model_uniform(0).model = glm::mat4(1.0f);

	get_dynamic_model_uniform(1).model = glm::translate(
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
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	const VkDescriptorSetLayoutBinding model_texture_binding = {
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	const VkDescriptorSetLayoutBinding model_specular_binding = {
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	const VkDescriptorSetLayoutBinding model_emissive_binding = {
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	const VkDescriptorSetLayoutBinding model_bindings[] = {
		model_uniform_binding,
		model_texture_binding,
		model_specular_binding,
		model_emissive_binding,
	};

	const VkDescriptorSetLayoutCreateInfo model_descriptor_set_layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 4,
		.pBindings = model_bindings,
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
			.descriptorCount = 1,
		},
		{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.descriptorCount = 2,
		},
		{
			.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 2,
		},
		{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 6,
		},
	};

	const VkDescriptorPoolCreateInfo descriptor_pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 3,
		.poolSizeCount = 4,
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
		.buffer = dynamic_model_uniform_buffer,
		.offset = 0,
		.range = sizeof(ModelUniforms),
	};

	VkDescriptorBufferInfo model_uniform_buffer_info_2 = {
		.buffer = dynamic_model_uniform_buffer,
		.offset = 0,
		.range = sizeof(ModelUniforms),
	};

	VkDescriptorImageInfo albedo_texture_info = {
		.sampler = albedo_texture_sampler,
		.imageView = albedo_texture_view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkDescriptorImageInfo second_albedo_texture_info = {
		.sampler = albedo_texture_sampler,
		.imageView = second_albedo_texture_view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkDescriptorImageInfo black_fallback_texture_info = {
		.sampler = albedo_texture_sampler,
		.imageView = black_fallback_texture_view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkDescriptorImageInfo specular_texture_info = {
		.sampler = albedo_texture_sampler,
		.imageView = specular_texture_view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkDescriptorImageInfo emissive_texture_info = {
		.sampler = albedo_texture_sampler,
		.imageView = emissive_texture_view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		.pBufferInfo = &model_uniform_buffer_info,
	};

	VkWriteDescriptorSet model_descriptor_write_2 = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = model_descriptor_set_2,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		.pBufferInfo = &model_uniform_buffer_info_2,
	};

	VkWriteDescriptorSet model_texture_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = model_descriptor_set,
		.dstBinding = 1,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &albedo_texture_info,
	};

	VkWriteDescriptorSet model_texture_write_2 = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = model_descriptor_set_2,
		.dstBinding = 1,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &second_albedo_texture_info,
	};

	VkWriteDescriptorSet specular_texture_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = model_descriptor_set,
		.dstBinding = 2,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &black_fallback_texture_info,
	};

	VkWriteDescriptorSet specular_texture_write_2 = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = model_descriptor_set_2,
		.dstBinding = 2,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &specular_texture_info,
	};

	VkWriteDescriptorSet emissive_texture_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = model_descriptor_set,
		.dstBinding = 3,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &black_fallback_texture_info,
	};

	VkWriteDescriptorSet emissive_texture_write_2 = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = model_descriptor_set_2,
		.dstBinding = 3,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &emissive_texture_info,
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
		model_texture_write,
		model_texture_write_2,
		specular_texture_write,
		specular_texture_write_2,
		emissive_texture_write,
		emissive_texture_write_2,
	};

	// Записываем информацию в descriptor set
	vkUpdateDescriptorSets(context.device, 11, descriptor_writes, 0, nullptr);

	const VkDescriptorSetLayout pipeline_set_layouts[] = {
		scene_descriptor_set_layout,
		model_descriptor_set_layout,
	};

	const VkPushConstantRange visualization_push_constant_range = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof(glm::mat4) + sizeof(glm::vec4) + 3 * sizeof(float),
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
		.vertexAttributeDescriptionCount = 3,
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
		.vertexAttributeDescriptionCount = 3,
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
	vmaDestroyBuffer(context.allocator, scene_uniform_buffer, scene_uniform_buffer_allocation);
	
	vmaDestroyBuffer(context.allocator, vertex_buffer, vertex_buffer_allocation);
	vmaDestroyBuffer(context.allocator, index_buffer, index_buffer_allocation);

	if (dynamic_model_uniform_buffer_memory != nullptr) {
		vmaUnmapMemory(context.allocator, dynamic_model_uniform_buffer_allocation);
		dynamic_model_uniform_buffer_memory = nullptr;
	}

	if (dynamic_model_uniform_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, dynamic_model_uniform_buffer, dynamic_model_uniform_buffer_allocation);
		dynamic_model_uniform_buffer = VK_NULL_HANDLE;
		dynamic_model_uniform_buffer_allocation = VK_NULL_HANDLE;
	}

	vmaDestroyBuffer(context.allocator, light_visualization_vertex_buffer, light_visualization_vertex_buffer_allocation);
	vmaDestroyBuffer(context.allocator, light_visualization_index_buffer, light_visualization_index_buffer_allocation);

	vmaUnmapMemory(context.allocator, light_line_vertex_buffer_allocation);
	vmaDestroyBuffer(context.allocator, light_line_vertex_buffer, light_line_vertex_buffer_allocation);

	vmaUnmapMemory(context.allocator, point_lights_buffer_allocation);
	vmaDestroyBuffer(context.allocator, point_lights_buffer, point_lights_buffer_allocation);

	vmaUnmapMemory(context.allocator, spot_light_buffer_allocation);
	vmaDestroyBuffer(context.allocator, spot_light_buffer, spot_light_buffer_allocation);

	if (albedo_texture_sampler != VK_NULL_HANDLE) {
		vkDestroySampler(context.device, albedo_texture_sampler, nullptr);
		albedo_texture_sampler = VK_NULL_HANDLE;
	}

	if (albedo_texture_view != VK_NULL_HANDLE) {
		vkDestroyImageView(context.device, albedo_texture_view, nullptr);
		albedo_texture_view = VK_NULL_HANDLE;
	}

	if (second_albedo_texture_view != VK_NULL_HANDLE) {
		vkDestroyImageView(
			context.device,
			second_albedo_texture_view,
			nullptr
		);
		second_albedo_texture_view = VK_NULL_HANDLE;
	}

	if (black_fallback_texture_view != VK_NULL_HANDLE) {
		vkDestroyImageView(
			context.device,
			black_fallback_texture_view,
			nullptr
		);
		black_fallback_texture_view = VK_NULL_HANDLE;
	}

	if (specular_texture_view != VK_NULL_HANDLE) {
		vkDestroyImageView(
			context.device,
			specular_texture_view,
			nullptr
		);
		specular_texture_view = VK_NULL_HANDLE;
	}

	if (emissive_texture_view != VK_NULL_HANDLE) {
		vkDestroyImageView(
			context.device,
			emissive_texture_view,
			nullptr
		);
		emissive_texture_view = VK_NULL_HANDLE;
	}

	if (albedo_texture_image != VK_NULL_HANDLE) {
		vmaDestroyImage(
			graphics::internal::context.allocator,
			albedo_texture_image,
			albedo_texture_allocation
		);
	}

	if (second_albedo_texture_image != VK_NULL_HANDLE) {
		vmaDestroyImage(
			context.allocator,
			second_albedo_texture_image,
			second_albedo_texture_allocation
		);
		second_albedo_texture_image = VK_NULL_HANDLE;
		second_albedo_texture_allocation = VK_NULL_HANDLE;
	}

	if (black_fallback_texture_image != VK_NULL_HANDLE) {
		vmaDestroyImage(
			context.allocator,
			black_fallback_texture_image,
			black_fallback_texture_allocation
		);
		black_fallback_texture_image = VK_NULL_HANDLE;
		black_fallback_texture_allocation = VK_NULL_HANDLE;
	}

	if (specular_texture_image != VK_NULL_HANDLE) {
		vmaDestroyImage(
			context.allocator,
			specular_texture_image,
			specular_texture_allocation
		);
		specular_texture_image = VK_NULL_HANDLE;
		specular_texture_allocation = VK_NULL_HANDLE;
	}

	if (emissive_texture_image != VK_NULL_HANDLE) {
		vmaDestroyImage(
			context.allocator,
			emissive_texture_image,
			emissive_texture_allocation
		);
		emissive_texture_image = VK_NULL_HANDLE;
		emissive_texture_allocation = VK_NULL_HANDLE;
	}

	if (texture_staging_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(
			graphics::internal::context.allocator,
			texture_staging_buffer,
			texture_staging_allocation
		);

		texture_staging_buffer = VK_NULL_HANDLE;
		texture_staging_allocation = VK_NULL_HANDLE;
	}
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

	get_dynamic_model_uniform(0).model = model;

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

	get_dynamic_model_uniform(1).model = second_model;

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

	if (ImGui::CollapsingHeader(
			"Texture Effect",
			ImGuiTreeNodeFlags_DefaultOpen
		)) {
		ImGui::Checkbox(
			"Enable Sampling Effect",
			&sampling_effect_enabled
		);
	}

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
		const char* texture_options[] = {
			"check.png",
			"second.png",
		};

		ImGui::Combo(
			"First Pyramid Texture",
			&pyramid_texture_index,
			texture_options,
			2
		);

		ImGui::Combo(
			"Second Pyramid Texture",
			&second_pyramid_texture_index,
			texture_options,
			2
		);

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
				glm::vec4(2.0f, 0.0f, -0.5f, 1.0f),
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
				glm::vec4(1.3f, 1.0f, -0.5f, 1.0f),
				glm::vec4(-0.6f, -0.8f, 0.0f, 0.0f),
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

	get_dynamic_model_uniform(0).albedo = glm::vec4(
		pyramid_albedo,
		1.0f
	);

	get_dynamic_model_uniform(0).specular_color = glm::vec4(
		pyramid_specular_color,
		1.0f
	);

	get_dynamic_model_uniform(0).specular_coefficient =
    	pyramid_specular_coefficient;

	get_dynamic_model_uniform(1).albedo = glm::vec4(
		second_pyramid_albedo,
		1.0f
	);

	get_dynamic_model_uniform(1).specular_color = glm::vec4(
		second_pyramid_specular_color,
		1.0f
	);

	get_dynamic_model_uniform(1).specular_coefficient =
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
		&scene_descriptor_set, 0, nullptr
	);

	const uint32_t first_model_offset = 0;	
	const VkDescriptorSet material_descriptor_sets[] = {
		model_descriptor_set,
		model_descriptor_set_2,
	};

	vkCmdBindDescriptorSets(
		command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline_layout,
		1,
		1,
		&material_descriptor_sets[pyramid_texture_index],
		1,
		&first_model_offset
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
		float sampling_effect_enabled;
		float time;
	};

	VisualizationPushConstants visualization_push_constants = {
		glm::mat4(1.0f),
		glm::vec4(1.0f),
		0.0f,
		0.0f,
		0.0f,
	};

	visualization_push_constants.sampling_effect_enabled =
		sampling_effect_enabled ? 1.0f : 0.0f;

	visualization_push_constants.time =
		static_cast<float>(animation_time);

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

	const uint32_t second_model_offset =
    	static_cast<uint32_t>(model_uniform_stride);

	vkCmdBindDescriptorSets(
		command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline_layout,
		1,
		1,
		&material_descriptor_sets[second_pyramid_texture_index],
		1,
		&second_model_offset
	);

	visualization_push_constants.sampling_effect_enabled = 0.0f;

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