#version 330 core

uniform vec3 light_direction;
uniform vec3 camera_position;

uniform sampler2D albedo_texture;
uniform sampler2D normal_texture;
uniform sampler2D reflection_map;

in vec3 position;
in vec3 tangent;
in vec3 normal;
in vec2 texcoord;

layout(location = 0) out vec4 out_color;

const float PI = 3.141592653589793;

void main()
{
    float ambient_light = 0.2;

    vec3 bitangent = cross(tangent, normal);
    mat3 tbn = mat3(tangent, bitangent, normal);
    vec3 real_normal = tbn * (texture(normal_texture, texcoord).xyz * 2.0 - vec3(1.0));

    vec3 camera_direction = normalize(camera_position - position);
    vec3 reflected_ray_direction = 2.0 * real_normal * dot(real_normal, camera_direction) - camera_direction;

    float x = atan(reflected_ray_direction.z, reflected_ray_direction.x) / PI * 0.5 + 0.5;
    float y = -atan(reflected_ray_direction.y, length(reflected_ray_direction.xz)) / PI + 0.5;

    float lightness = ambient_light + max(0.0, dot(normalize(real_normal), light_direction));

    vec3 albedo = texture(albedo_texture, texcoord).rgb;
    vec3 environment_color = texture(reflection_map, vec2(x, y)).rgb;

    out_color = vec4(mix(lightness * albedo, environment_color, 0.5), 1.0);
    albedo = real_normal * 0.5 + vec3(0.5);
    out_color = vec4(lightness * albedo, 1.0);
}
