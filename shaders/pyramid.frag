#version 450

layout(location = 0) in vec3 world_position;
layout(location = 1) in vec3 normal;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform SceneUniforms {
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
    if (visualization.enabled > 0.5) {
        out_color = visualization.color;
        return;
    }

    vec3 n = normalize(normal);

    vec3 l = normalize(-scene_uniforms.light_direction.xyz);
    vec3 v = normalize(scene_uniforms.camera_position.xyz - world_position);
    vec3 h = normalize(l + v);

    float diffuse = max(dot(n, l), 0.0);

    float ambient = 0.1;

    float specular = pow(
        max(dot(n, h), 0.0),
        model_uniforms.specular_coefficient
    );

    vec3 diffuse_color =
        model_uniforms.albedo.rgb *
        scene_uniforms.light_color_intensity.rgb *
        scene_uniforms.light_color_intensity.a *
        diffuse;

    vec3 specular_color =
        model_uniforms.specular_color.rgb *
        scene_uniforms.light_color_intensity.rgb *
        scene_uniforms.light_color_intensity.a *
        specular;

    vec3 point_light_color = vec3(0.0);

    for (int i = 0; i < point_lights.length(); ++i) {
        vec3 point_light_vector =
            point_lights[i].position.xyz - world_position;

        float point_light_distance =
            length(point_light_vector);

        vec3 point_light_direction =
            normalize(point_light_vector);

        float point_diffuse =
            max(dot(n, point_light_direction), 0.0);

        vec3 point_halfway =
            normalize(point_light_direction + v);

        float point_specular =
            pow(
                max(dot(n, point_halfway), 0.0),
                model_uniforms.specular_coefficient
            );

        float point_attenuation =
            1.0 /
            (point_light_distance * point_light_distance);

        vec3 point_diffuse_color =
            model_uniforms.albedo.rgb *
            point_lights[i].color_intensity.rgb *
            point_lights[i].color_intensity.a *
            point_diffuse *
            point_attenuation;

        vec3 point_specular_color =
            model_uniforms.specular_color.rgb *
            point_lights[i].color_intensity.rgb *
            point_lights[i].color_intensity.a *
            point_specular *
            point_attenuation;

        point_light_color +=
            point_diffuse_color +
            point_specular_color;
    }

    vec3 spot_light_color = vec3(0.0);

    for (int i = 0; i < spot_lights.length(); ++i) {
        vec3 spot_light_vector =
            spot_lights[i].position.xyz - world_position;

        float spot_light_distance =
            length(spot_light_vector);

        vec3 spot_light_direction =
            normalize(spot_light_vector);

        vec3 spot_direction =
            normalize(spot_lights[i].direction.xyz);

        float spot_cosine =
            dot(spot_direction, -spot_light_direction); 

        float inner_angle =
            spot_lights[i].angles.x;

        float outer_angle =
            spot_lights[i].angles.y;

        float inner_cosine =
            cos(inner_angle);

        float outer_cosine =
            cos(outer_angle);

        float spot_factor =
            smoothstep(
                outer_cosine,
                inner_cosine,
                spot_cosine
            );

        float spot_diffuse =
            max(
                dot(n, spot_light_direction),
                0.0
            );

        vec3 spot_halfway =
            normalize(
                spot_light_direction + v
            );

        float spot_specular =
            pow(
                max(dot(n, spot_halfway), 0.0),
                model_uniforms.specular_coefficient
            );

        float spot_attenuation =
            1.0 /
            (spot_light_distance * spot_light_distance);

        vec3 spot_diffuse_color =
            model_uniforms.albedo.rgb *
            spot_lights[i].color_intensity.rgb *
            spot_lights[i].color_intensity.a *
            spot_diffuse *
            spot_attenuation;

        vec3 spot_specular_color =
            model_uniforms.specular_color.rgb *
            spot_lights[i].color_intensity.rgb *
            spot_lights[i].color_intensity.a *
            spot_specular *
            spot_attenuation;

        spot_light_color +=
            (spot_diffuse_color +
            spot_specular_color) *
            spot_factor;
    }
    vec3 color =
        model_uniforms.albedo.rgb * ambient +
        diffuse_color +
        specular_color +
        point_light_color +
        spot_light_color;

    out_color = vec4(color, 1.0);
}