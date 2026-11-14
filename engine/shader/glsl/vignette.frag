#version 310 es

layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInput in_color;

// Matches VignettePushConstants: viewport origin/size followed by strength and radii.
layout(push_constant) uniform VignetteConstants
{
    highp vec4 viewport;
    highp vec4 settings;
} constants;

layout(location = 0) out highp vec4 out_color;

void main()
{
    highp vec4 color = subpassLoad(in_color);

    // Measure distance in units of half the viewport height. This keeps circles round
    // when the game viewport changes size or occupies only part of the editor window.
    highp vec2 position = (gl_FragCoord.xy - constants.viewport.xy) / constants.viewport.zw;
    position = position * 2.0 - 1.0;
    position.x *= constants.viewport.z / constants.viewport.w;

    highp float edge_weight = smoothstep(constants.settings.y, constants.settings.z, length(position));
    highp float attenuation = 1.0 - constants.settings.x * edge_weight;

    // UI is composited in a later subpass. Scene alpha remains unchanged.
    out_color = vec4(color.rgb * attenuation, color.a);
}
