#pragma once

#include "runtime/function/render/render_pass.h"
#include "runtime/resource/res_type/data/vignette_settings.h"

namespace Piccolo
{
    // Borrows the main camera render pass and its scene-color input attachment.
    struct VignettePassInitInfo : RenderPassInitInfo
    {
        RHIRenderPass*   render_pass;
        RHIImageView*    input_attachment;
        VignetteSettings settings;
    };

    // Copies the scene through a radial attenuation pass before the UI is composited.
    class VignettePass : public RenderPass
    {
    public:
        void initialize(const RenderPassInitInfo* init_info) override final;
        void draw() override final;
        void setSettings(const VignetteSettings& settings);
        void updateAfterFramebufferRecreate(RHIImageView* input_attachment);

    private:
        // Two vec4 blocks match the GLSL push-constant layout without a per-frame buffer.
        struct VignettePushConstants
        {
            float viewport[4];
            float settings[4];
        };

        void setupDescriptorSetLayout();
        void setupPipelines();
        void setupDescriptorSet();

        VignetteSettings m_settings;
    };
} // namespace Piccolo
