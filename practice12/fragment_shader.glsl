#version 330 core

uniform vec3 camera_position;
uniform vec3 light_direction;
uniform vec3 bbox_min;
uniform vec3 bbox_max;

uniform sampler3D cloud_data;

layout(location = 0) out vec4 out_color;

void sort(inout float x, inout float y)
{
    if (x > y)
    {
        float t = x;
        x = y;
        y = t;
    }
}

float vmin(vec3 v)
{
    return min(v.x, min(v.y, v.z));
}

float vmax(vec3 v)
{
    return max(v.x, max(v.y, v.z));
}

vec2 intersect_bbox(vec3 origin, vec3 direction)
{
    vec3 tmin = (bbox_min - origin) / direction;
    vec3 tmax = (bbox_max - origin) / direction;

    sort(tmin.x, tmax.x);
    sort(tmin.y, tmax.y);
    sort(tmin.z, tmax.z);

    return vec2(vmax(tmin), vmin(tmax));
}

float texture_value(vec3 point)
{
    vec3 fixed_point = (point - bbox_min) / (bbox_max - bbox_min);
    return texture(cloud_data, fixed_point).x;
}

const float PI = 3.1415926535;
float absorption = 0.0;

in vec3 position;

void main()
{
    vec3 direction = normalize(position - camera_position);
    vec2 intersect_interval = intersect_bbox(camera_position, direction);
    float tmax = intersect_interval.y;
    float tmin = intersect_interval.x;
    tmin = max(tmin, 0.0);

    //////////////////////////////////////////////////////////////////////

    float optical_depth = (tmax - tmin) * absorption;
    float opacity = 1.0 - exp(-optical_depth);

    //////////////////////////////////////////////////////////////////////

    vec3 point = camera_position + direction * (tmin + tmax) / 2;

    //////////////////////////////////////////////////////////////////////

    optical_depth = 0.0;
    int n = 64;
    float dt = (tmax - tmin) / n;
    for (int i = 0; i < n; i++) {
        float t = tmin + (i + 0.5) * dt;
        point = camera_position + t * direction;
        optical_depth += absorption * texture_value(point) * dt;
    }
    opacity = 1.0 - exp(-optical_depth);

    //////////////////////////////////////////////////////////////////////

    float scattering = 4.0;
    float extinction = absorption + scattering;
    vec3 light_color = vec3(16.0);
    vec3 color = vec3(0.0);

    n = 64;
    int m = 16;

    dt = (tmax - tmin) / n;
    optical_depth = 0.0;

    for (int i = 0; i < n; i++) {
        float t = tmin + (i + 0.5) * dt;
        point = camera_position + t * direction;
        optical_depth += extinction * texture_value(point) * dt;

        vec2 light_tmin_tmax = intersect_bbox(point, light_direction);
        float light_tmin = light_tmin_tmax.x;
        float light_tmax = light_tmin_tmax.y;
        light_tmin = max(light_tmin, 0.0);

        float dl = (light_tmax - light_tmin) / m;

        float light_optical_depth = 0.0;

        for (int j = 0; j < m; j++) {
            float l = light_tmin + (j + 0.5) * dl;
            vec3 l_point = point + l * light_direction;

            light_optical_depth += extinction * texture_value(l_point) * dl;
        }

        color += light_color * exp(-light_optical_depth) * exp(-optical_depth) * dt * texture_value(point) * scattering / 4.0 / PI;
    }

    opacity = 1.0 - exp(-optical_depth);

    //////////////////////////////////////////////////////////////////////

    vec3 vec_scattering = vec3(1.0, 5.8, 9.2);
    vec3 vec_absorption = vec3(absorption);
    vec3 vec_extinction = vec_absorption + vec_scattering;

    vec3 vec_light_color = vec3(16.0);
    color = vec3(0.0);

    vec3 vec_optical_depth = vec3(0.0);

    for (int i = 0; i < n; i++) {
        float t = tmin + (i + 0.5) * dt;
        point = camera_position + t * direction;
        vec_optical_depth += vec_extinction * texture_value(point) * dt;

        vec3 vec_light_optical_depth = vec3(0.0);
        vec2 light_tmin_tmax = intersect_bbox(point, light_direction);
        float light_tmin = light_tmin_tmax.x;
        float light_tmax = light_tmin_tmax.y;
        light_tmin = max(light_tmin, 0.0);

        float dl = (light_tmax - light_tmin) / m;

        for (int j = 0; j < m; j++) {
            float l = light_tmin + (j + 0.5) * dl;
            vec3 l_point = point + l * light_direction;

            vec_light_optical_depth += vec_extinction * texture_value(l_point) * dl;
        }

        color += vec_light_color * exp(-vec_light_optical_depth) * exp(-vec_optical_depth) * dt * texture_value(point) * vec_scattering / 4.0 / PI;
    }
    opacity = 1.0 - exp(-vec_optical_depth.x);

    // out_color = vec4(vec3((tmax - tmin)/4), 1.0);
    // out_color = vec4(0.0, 0.0, 0.5, opacity);
    // out_color = vec4(vec3(texture_value(point)), 1.0);
    out_color = vec4(color, opacity);
}
