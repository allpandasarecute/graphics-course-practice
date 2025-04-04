#version 330 core

uniform vec3 camera_position;

uniform vec3 albedo;

uniform vec3 sun_direction;
uniform vec3 sun_color;

uniform sampler2DShadow shadow_map;
uniform mat4 shadow_projection;

in vec3 position;
in vec3 normal;

layout(location = 0) out vec4 out_color;

vec3 diffuse(vec3 direction) {
    return albedo * max(0.0, dot(normal, direction));
}

vec3 specular(vec3 direction) {
    float power = 64.0;
    vec3 reflected_direction = 2.0 * normal * dot(normal, direction) - direction;
    vec3 view_direction = normalize(camera_position - position);
    return albedo * pow(max(0.0, dot(reflected_direction, view_direction)), power);
}

vec3 phong(vec3 direction) {
    return diffuse(direction) + specular(direction);
}

void main()
{
    float ambient_light = 0.2;
    vec3 color = albedo * ambient_light;

    vec4 ndc = shadow_projection * vec4(position, 1.0);
    ndc = ndc / ndc.w;

    float shadow_sum = 1.0;

    bool in_shadow_texture = (-1.0 <= ndc.x && ndc.x <= 1.0 && -1.0 <= ndc.y && ndc.y <= 1.0 && -1.0 <= ndc.z && ndc.z <= 1.0);
    if (in_shadow_texture) {
        vec2 shadow_texcoord = ndc.xy * 0.5 + 0.5;
        float shadow_depth = ndc.z * 0.5 + 0.5;
        float sum = 0.0;
        float sum_w = 0.0;
        const int N = 7;
        float radius = 3.0;
        for (int x = -N; x <= N; ++x) {
            for (int y = -N; y <= N; ++y) {
                float c = exp(-float(x * x + y * y) / (radius * radius));
                sum += c * texture(shadow_map, vec3(shadow_texcoord.x, shadow_texcoord.y, shadow_depth) + vec3(x, y, 0) / vec3(textureSize(shadow_map, 0), 1));
                sum_w += c;
            }
        }
        shadow_sum = sum / sum_w;
    }

    color += shadow_sum * sun_color * phong(sun_direction);
    out_color = vec4(color, 1.0);
}
