#version 330 core

uniform mat4 viewmodel;
uniform mat4 projection;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texcoord;

out vec3 normal;
out vec2 texcoord;

void main()
{
    gl_Position = projection * viewmodel * vec4(in_position, 1.0);
    normal = mat3(viewmodel) * in_normal;
    texcoord = in_texcoord;
}
