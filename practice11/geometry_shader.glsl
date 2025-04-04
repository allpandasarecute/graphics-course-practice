#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 camera_position;

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

out vec2 texcoord;

in float size[];
in float angle[];

void main()
{
    vec3 center = gl_in[0].gl_Position.xyz;
    float sz = size[0];

    vec3 vertices[4] = vec3[4](vec3(-sz, -sz, 0), vec3(-sz, sz, 0), vec3(sz, -sz, 0), vec3(sz, sz, 0));

    for (int i = 0; i < 4; i++) {
        vec3 vertice_z = normalize(center - camera_position);
        vec3 vertice_x = normalize(cross(vertice_z, vec3(0.0, 1.0, 0.0)));
        vec3 vertice_y = cross(vertice_x, vertice_z);

        vertice_x = vertice_x * cos(angle[0]) + vertice_y * sin(angle[0]);
        vertice_y = cross(vertice_x, vertice_z);

        vec3 vertice_position = vertice_x * vertices[i].x + vertice_y * vertices[i].y;

        gl_Position = projection * view * model * vec4(center + vertice_position, 1.0);

        texcoord = (vertices[i].xy * 0.5 + vec2(sz, sz) * 0.5) / sz;

        EmitVertex();
    }
    EndPrimitive();
}
