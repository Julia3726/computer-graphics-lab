#version 450

layout(location = 0) in vec3 world_position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 shadow_position;
layout(location = 4) in vec4 spot_shadow_position;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform SceneUniforms {
    mat4 shadow_view_projection;
    mat4 spot_shadow_view_projection;

    mat4 view;
    mat4 projection;

    vec4 light_direction;
    vec4 light_color_intensity;

    vec4 camera_position;
} scene_uniforms;

struct PointLight {
    vec4 position;
    vec4 color_intensity;
};

struct SpotLight {
    vec4 position;
    vec4 direction;
    vec4 color_intensity;
    vec4 angles;
};

layout(set = 0, binding = 1, std430) readonly buffer PointLights {
    PointLight point_lights[];
};

layout(set = 0, binding = 2, std430) readonly buffer SpotLights {
    SpotLight spot_lights[];
};

layout(set = 0, binding = 3) uniform sampler2DShadow shadow_map;
layout(set = 0, binding = 4) uniform sampler2DShadow spot_shadow_map;

layout(set = 1, binding = 0) uniform ModelUniforms {
    mat4 model;
    vec4 albedo;
    vec4 specular_color;
    float specular_coefficient;
} model_uniforms;

layout(set = 1, binding = 1) uniform sampler2D albedo_texture;
layout(set = 1, binding = 2) uniform sampler2D specular_texture;
layout(set = 1, binding = 3) uniform sampler2D emissive_texture;

layout(push_constant) uniform VisualizationPushConstants {
    mat4 model;
    vec4 color;
    float enabled;
    float sampling_effect_enabled;
    float time;
} visualization;

vec3 sample_albedo_texture(vec2 texture_uv) {
    if (visualization.sampling_effect_enabled < 0.5) {
        return texture(albedo_texture, texture_uv).rgb;
    }

    ivec2 texture_dimensions = textureSize(albedo_texture, 0);
    vec2 texel_size = 1.0 / vec2(texture_dimensions);

    vec3 color_sum = vec3(0.0);
    const float blur_radius = 3.0;

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(float(x), float(y)) *
                          texel_size *
                          blur_radius;

            color_sum += texture(
                albedo_texture,
                texture_uv + offset
            ).rgb;
        }
    }

    vec3 blurred_color = color_sum / 9.0;

    vec3 phase = vec3(0.0, 2.094, 4.188);
    vec3 color_modulation =
        0.75 + 0.25 * sin(visualization.time + phase);

    return blurred_color * color_modulation;
}

float calculate_directional_diffuse(vec3 surface_normal) {
    vec3 light_direction =
        normalize(-scene_uniforms.light_direction.xyz);

    return max(
        dot(normalize(surface_normal), light_direction),
        0.0
    );
}

float calculate_shadow_visibility(vec4 light_space_position) {
    vec3 projected_position =
        light_space_position.xyz / light_space_position.w;

    vec2 shadow_uv =
        projected_position.xy * 0.5 + vec2(0.5);

    if (projected_position.z < 0.0 || projected_position.z > 1.0) {
        return 1.0;
    }

    return texture(
        shadow_map,
        vec3(shadow_uv, projected_position.z)
    );
}

float calculate_spot_shadow_visibility(vec4 light_space_position) {
    if (light_space_position.w <= 0.0) {
        return 1.0;
    }

    vec3 projected_position =
        light_space_position.xyz / light_space_position.w;

    vec2 shadow_uv =
        projected_position.xy * 0.5 + vec2(0.5);

    if (projected_position.z < 0.0 || projected_position.z > 1.0) {
        return 1.0;
    }

    return texture(
        spot_shadow_map,
        vec3(shadow_uv, projected_position.z)
    );
}

vec3 calculate_lighting(
    vec3 albedo_color,
    vec3 material_specular_color,
    float specular_coefficient,
    float specular_mask,
    vec3 emissive_color
) {
    vec3 n = normalize(normal);

    vec3 l = normalize(-scene_uniforms.light_direction.xyz);
    vec3 v = normalize(scene_uniforms.camera_position.xyz - world_position);
    vec3 h = normalize(l + v);

    float ambient = 0.1;
    float shadow_visibility =
        calculate_shadow_visibility(shadow_position);
    float diffuse = calculate_directional_diffuse(n);
    float specular = pow(
        max(dot(n, h), 0.0),
        specular_coefficient
    );

    vec3 directional_diffuse =
        albedo_color *
        scene_uniforms.light_color_intensity.rgb *
        scene_uniforms.light_color_intensity.a *
        diffuse *
        shadow_visibility;

    vec3 directional_specular =
        material_specular_color *
        scene_uniforms.light_color_intensity.rgb *
        scene_uniforms.light_color_intensity.a *
        specular *
        specular_mask *
        shadow_visibility;

    vec3 point_light_color = vec3(0.0);

    for (int i = 0; i < point_lights.length(); ++i) {
        vec3 to_light =
            point_lights[i].position.xyz - world_position;

        float distance_to_light = length(to_light);
        vec3 light_direction = normalize(to_light);

        float point_diffuse = max(dot(n, light_direction), 0.0);
        vec3 halfway = normalize(light_direction + v);
        float point_specular = pow(
            max(dot(n, halfway), 0.0),
            specular_coefficient
        );

        float attenuation =
            1.0 / max(distance_to_light * distance_to_light, 0.01);

        point_light_color +=
            albedo_color *
            point_lights[i].color_intensity.rgb *
            point_lights[i].color_intensity.a *
            point_diffuse *
            attenuation;

        point_light_color +=
            material_specular_color *
            point_lights[i].color_intensity.rgb *
            point_lights[i].color_intensity.a *
            point_specular *
            attenuation *
            specular_mask;
    }

    vec3 spot_light_color = vec3(0.0);

    for (int i = 0; i < spot_lights.length(); ++i) {
        vec3 to_light =
            spot_lights[i].position.xyz - world_position;

        float distance_to_light = length(to_light);
        vec3 light_direction = normalize(to_light);
        vec3 spot_direction = normalize(spot_lights[i].direction.xyz);

        float spot_cosine = dot(spot_direction, -light_direction);
        float inner_cosine = cos(spot_lights[i].angles.x);
        float outer_cosine = cos(spot_lights[i].angles.y);

        float cone = smoothstep(
            outer_cosine,
            inner_cosine,
            spot_cosine
        );

        float spot_shadow_visibility = 1.0;

        if (i == 0) {
            spot_shadow_visibility =
                calculate_spot_shadow_visibility(spot_shadow_position);
        }

        float spot_diffuse = max(dot(n, light_direction), 0.0);
        vec3 halfway = normalize(light_direction + v);
        float spot_specular = pow(
            max(dot(n, halfway), 0.0),
            specular_coefficient
        );

        float attenuation =
            1.0 / max(distance_to_light * distance_to_light, 0.01);

        spot_light_color +=
            albedo_color *
            spot_lights[i].color_intensity.rgb *
            spot_lights[i].color_intensity.a *
            spot_diffuse *
            attenuation *
            cone *
            spot_shadow_visibility;

        spot_light_color +=
            material_specular_color *
            spot_lights[i].color_intensity.rgb *
            spot_lights[i].color_intensity.a *
            spot_specular *
            attenuation *
            specular_mask *
            cone *
            spot_shadow_visibility;
    }

    return
        albedo_color * ambient +
        directional_diffuse +
        directional_specular +
        point_light_color +
        spot_light_color +
        emissive_color;
}

void main() {
    vec3 albedo_color;
    vec3 material_specular_color;
    float specular_coefficient;
    float specular_mask;
    vec3 emissive_color;
    float alpha;

    if (visualization.enabled > 0.5) {
        // У кубов материал задан цветом из Push Constants
        albedo_color = visualization.color.rgb;
        material_specular_color = vec3(0.0);
        specular_coefficient = 32.0;
        specular_mask = 0.0;
        emissive_color = vec3(0.0);
        alpha = visualization.color.a;
    } else {
        // У пирамид цвет материала берётся из текстуры
        albedo_color =
            sample_albedo_texture(uv) *
            model_uniforms.albedo.rgb;

        material_specular_color = model_uniforms.specular_color.rgb;
        specular_coefficient = model_uniforms.specular_coefficient;
        specular_mask = texture(specular_texture, uv).r;
        emissive_color = texture(emissive_texture, uv).rgb;
        alpha = 1.0;
    }

    vec3 color = calculate_lighting(
        albedo_color,
        material_specular_color,
        specular_coefficient,
        specular_mask,
        emissive_color
    );

    out_color = vec4(color, alpha);
}