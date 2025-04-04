#version 330 core

layout(location = 0) out vec4 out_color;

uniform sampler2D particle_texture;
uniform sampler1D color_texture;

in vec2 texcoord;

void main()
{
    out_color = vec4(texture(color_texture, texture(particle_texture, texcoord).r).rgb, texture(particle_texture, texcoord).r);
}
