#version 330 core
layout(location = 0) out vec4 out_color;

uniform sampler2D render_result;

in vec2 texcoord;

void main()
{
    out_color = vec4(texture(render_result, texcoord).r);
}
