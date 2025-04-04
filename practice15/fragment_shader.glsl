#version 330 core

layout(location = 0) out vec4 out_color;

uniform sampler2D sdf_texture;
uniform float sdf_scale;

in vec2 texcoord;

float median(vec3 v) {
    return max(min(v.r, v.g), min(max(v.r, v.g), v.b));
}

void main()
{
    float sdf_value = sdf_scale * (median(texture(sdf_texture, texcoord).rgb) - 0.5);
    float smooth_constant = length(vec2(dFdx(sdf_value), dFdy(sdf_value))) / sqrt(2.0);
    float alpha = smoothstep(-smooth_constant, smooth_constant, sdf_value);
    vec3 color = vec3(0.0);
    if (sdf_value < 0.4) {
        color = vec3(1.0);
    }
    out_color = vec4(color, alpha);
}
