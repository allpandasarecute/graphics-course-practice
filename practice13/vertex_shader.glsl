#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform mat4x3 bones[100];

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texcoord;
layout(location = 3) in ivec4 in_joints;
layout(location = 4) in vec4 in_weights;

out vec3 normal;
out vec2 texcoord;
out vec4 weights;

void main()
{
    mat4x3 average = mat4x3(0);
    for (int i = 0; i < 4; i++) {
        average += in_weights[i] * bones[in_joints[i]];
    }

    gl_Position = projection * view * model * mat4(average) * vec4(in_position, 1.0);
    normal = mat3(model) * mat3(average) * in_normal;
    texcoord = in_texcoord;
    weights = in_weights;
}
