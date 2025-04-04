#version 330 core

in vec2 texcoord;

uniform sampler2D render_result;
uniform int mode;
uniform float time;

layout(location = 0) out vec4 out_color;

void main()
{
    vec2 inner_texcoord = texcoord;
    if (mode == 2) {
        inner_texcoord += vec2(sin(inner_texcoord.y * 50.0 + time) * 0.01, 0.0);
    }
    vec3 color = vec3(texture(render_result, inner_texcoord));
    if (mode == 1) {
        color = floor(color * 4.0) / 3.0;
    }
    if (mode == 3) {
        vec4 sum = vec4(0.0);
        float sum_w = 0.0;
        const int N = 7;
        float radius = 5.0;

        for (int x = -N; x <= N; ++x) {
            for (int y = -N; y <= N; ++y) {
                float c = exp(-float(x * x + y * y) / (radius * radius));
                sum += c * texture(render_result, inner_texcoord + vec2(x, y) / vec2(textureSize(render_result, 0)));
                sum_w += c;
            }
        }
        out_color = sum / sum_w;
    } else {
        out_color = vec4(color, 1.0);
    }
}
