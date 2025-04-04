#version 330 core

in vec3 normal;

in vec2 texcoord;

layout(location = 0) out vec4 out_color;

uniform sampler2D sampler;
uniform float time;

void main()
{
    float lightness = 0.5 + 0.5 * dot(normalize(normal), normalize(vec3(1.0, 2.0, 3.0)));
    vec3 albedo = vec3(texture(sampler, vec2(texcoord.x + log(time), texcoord.y - sin(time))));
    out_color = vec4(lightness * albedo, 1.0);
}
