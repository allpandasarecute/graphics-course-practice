#version 330 core
layout(location = 0) in vec3 in_position;

uniform mat4 model;
uniform mat4 shadow_projection;

void main()
{
    gl_Position = shadow_projection * model * vec4(in_position, 1.0);
}
