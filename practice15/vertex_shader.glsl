#version 330 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 in_texcoord;

uniform mat4 transform;
out vec2 texcoord;

void main()
{
    gl_Position = transform * vec4(position, 0.0, 1.0);
    texcoord = in_texcoord;
}
