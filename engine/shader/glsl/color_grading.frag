#version 310 es

#extension GL_GOOGLE_include_directive : enable

#include "constants.h"

layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInput in_color;
layout(set = 0, binding = 1) uniform sampler2D color_grading_lut_texture_sampler;
layout(location = 0) out highp vec4 out_color;

// The LUT stores N blue slices horizontally in an N*N by N texture.
// Tone mapping supplies sRGB values. The LUT contains the graded sRGB values.
void main()
{
    highp ivec2 lut_size = textureSize(color_grading_lut_texture_sampler, 0);
    highp float slice_size = float(lut_size.y);
    highp vec4 color = subpassLoad(in_color).rgba;
    highp vec3 grid_position = clamp(color.rgb, 0.0, 1.0) * (slice_size - 1.0);

    highp float lower_slice = floor(grid_position.b);
    highp float upper_slice = min(lower_slice + 1.0, slice_size - 1.0);
    highp float slice_weight = grid_position.b - lower_slice;

    // Texel centers keep red/green filtering inside each blue slice at the edges.
    highp vec2 lower_uv = vec2(lower_slice * slice_size + grid_position.r + 0.5, grid_position.g + 0.5);
    highp vec2 upper_uv = vec2(upper_slice * slice_size + grid_position.r + 0.5, grid_position.g + 0.5);
    lower_uv /= vec2(lut_size);
    upper_uv /= vec2(lut_size);

    // Hardware filters red and green. This interpolation supplies the blue axis.
    highp vec3 lower_color = texture(color_grading_lut_texture_sampler, lower_uv).rgb;
    highp vec3 upper_color = texture(color_grading_lut_texture_sampler, upper_uv).rgb;
    out_color = vec4(mix(lower_color, upper_color, slice_weight), color.a);
}
