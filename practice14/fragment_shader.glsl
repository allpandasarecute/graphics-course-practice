#version 330 core

uniform sampler2D albedo;

uniform vec3 light_direction;

layout(location = 0) out vec4 out_color;

in vec3 normal;
in vec2 texcoord;

void main()
{
    vec3 albedo_color = texture(albedo, texcoord).rgb;

    float ambient = 0.4;
    float diffuse = max(0.0, dot(normalize(normal), light_direction));

    out_color = vec4(albedo_color * (ambient + diffuse), 1.0);
}
