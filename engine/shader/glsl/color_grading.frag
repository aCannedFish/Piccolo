#version 310 es

#extension GL_GOOGLE_include_directive : enable

#include "constants.h"

layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInput in_color;

layout(set = 0, binding = 1) uniform sampler2D color_grading_lut_texture_sampler;

layout(location = 0) out highp vec4 out_color;

void main()
{
    highp ivec2 lut_size = textureSize(color_grading_lut_texture_sampler, 0);
    highp float n = float(lut_size.y);
    highp vec4 color = subpassLoad(in_color).rgba;
    highp vec3 p = clamp(color.rgb, 0.0, 1.0) * (n - 1.0);
    
    highp float b0 = floor(p.b);
    highp float b1 = min(b0 + 1.0, n - 1.0);
    highp float weight = p.b - b0;

    highp vec2 uv0 = vec2(b0 * n + p.r + 0.5, p.g + 0.5);
    highp vec2 uv1 = vec2(b1 * n + p.r + 0.5, p.g + 0.5);
    uv0 /= vec2(lut_size);
    uv1 /= vec2(lut_size);

    highp vec3 c0 = texture(color_grading_lut_texture_sampler, uv0).rgb;
    highp vec3 c1 = texture(color_grading_lut_texture_sampler, uv1).rgb;
    out_color = vec4(mix(c0, c1, weight), color.a);
}
