#version 330 core

uniform vec3 camera_position;

uniform vec3 albedo;
uniform vec3 ambient_light;

uniform vec3 sun_direction;
uniform vec3 sun_color;

uniform vec3 point_light_position;
uniform vec3 point_light_color;
uniform vec3 point_light_attenuation;

uniform float glossiness;
uniform float roughness;

uniform float alpha;

in vec3 position;
in vec3 normal;

layout(location = 0) out vec4 out_color;

vec3 diffuse(vec3 direction) {
    return albedo * max(0.0, dot(normal, direction));
}

vec3 specular(vec3 direction) {
    vec3 view_direction = normalize(camera_position - position);
    vec3 reflected = 2.0 * normal * dot(normal, normalize(direction)) - normalize(direction);
    float power = 1.0 / (roughness * roughness) - 1.0;
    return glossiness * albedo * pow(max(0.0, dot(reflected, view_direction)), power);
}

void main()
{
    vec3 ambient = albedo * ambient_light;

    vec3 inner_sun_color = (diffuse(sun_direction) + specular(sun_direction)) * sun_color;

    float point_light_distance = distance(position, point_light_position);
    vec3 point_light_direction = normalize(point_light_position - position);
    vec3 point_color = (diffuse(point_light_direction) + specular(point_light_direction)) * point_light_color / (point_light_attenuation.x + point_light_attenuation.y * point_light_distance + point_light_attenuation.z * point_light_distance * point_light_distance);

    vec3 color = ambient + inner_sun_color + point_color;
    out_color = vec4(color, alpha);
}
