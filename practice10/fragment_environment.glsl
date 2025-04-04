#version 330 core

layout(location = 0) out vec4 out_color;

uniform vec3 camera_position;
uniform sampler2D environment_map_texture;

in vec3 position;

const float PI = acos(-1.0);

void main() {
    vec3 camera_direction = normalize(position - camera_position);

    float x = atan(camera_direction.z, camera_direction.x) / PI * 0.5 + 0.5;
    float y = -atan(camera_direction.y, length(camera_direction.xz)) / PI + 0.5;

    vec3 env_color = texture(environment_map_texture, vec2(x, y)).rgb;

    out_color = vec4(env_color, 1.0);
}
