#version 330 core
const vec2 VERTICES[6] = vec2[6](
        vec2(-1.0, -1.0),
        vec2(1.0, -1.0),
        vec2(-1.0, 1.0),
        vec2(-1.0, 1.0),
        vec2(1.0, -1.0),
        vec2(1.0, 1.0)
    );

uniform mat4 view_projection_inverse;

out vec3 position;

void main() {
    gl_Position = vec4(VERTICES[gl_VertexID], 0.0, 1.0);
    vec4 clip_space = view_projection_inverse * gl_Position;
    position = clip_space.xyz / clip_space.w;
}
