#version 330 core

uniform vec3 ambient;

uniform vec3 light_direction;
uniform vec3 light_color;

uniform mat4 transform;

uniform sampler2D shadow_map;

in vec3 position;
in vec3 normal;

layout(location = 0) out vec4 out_color;

float calc_factor(vec2 data, float sh_z) {
    float mu = data.r;
    float sigma = data.g - mu * mu;
    float z = sh_z - 0.005;
    float factor = (z < mu) ? 1.0 : sigma / (sigma + (z - mu) * (z - mu));

    float delta = 0.125;
    if (factor < delta) {
        factor = 0;
    } else {
        factor = (factor - delta) / (1 - delta);
    }

    return factor;
}

void main()
{
    vec4 shadow_pos = transform * vec4(position, 1.0);
    shadow_pos /= shadow_pos.w;
    shadow_pos = shadow_pos * 0.5 + vec4(0.5);

    bool in_shadow_texture = (shadow_pos.x > 0.0) && (shadow_pos.x < 1.0) && (shadow_pos.y > 0.0) && (shadow_pos.y < 1.0) && (shadow_pos.z > 0.0) && (shadow_pos.z < 1.0);
    float shadow_factor = 1.0;

    vec2 sum = vec2(0.0);
    float sum_w = 0.0;
    const int N = 7;
    float radius = 10.0;

    for (int x = -N; x <= N; ++x) {
        for (int y = -N; y <= N; ++y) {
            float c = exp(-float(x * x + y * y) / (radius * radius));
            sum += c * texture(shadow_map, shadow_pos.xy + vec2(x, y) / vec2(textureSize(shadow_map, 0))).xy;
            sum_w += c;
        }
    }

    vec2 data = sum / sum_w;
    if (in_shadow_texture) {
        shadow_factor = calc_factor(data, shadow_pos.z);
    }

    vec3 albedo = vec3(1.0, 1.0, 1.0);

    vec3 light = ambient;
    light += light_color * max(0.0, dot(normal, light_direction)) * shadow_factor;
    vec3 color = albedo * light;

    out_color = vec4(color, 1.0);
}
