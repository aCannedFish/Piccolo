#include "test_checks.h"

#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/interface/vulkan/vulkan_rhi.h"
#include "runtime/function/render/passes/vignette_pass.h"
#include "runtime/function/render/window_system.h"

#include <array>
#include <cstring>
#include <iostream>

using namespace Piccolo;

namespace
{
    // Render the production pass against a known color, then read its GPU output.
    // The earlier subpasses preserve that color at the index used by MainCameraPass.
    class OffscreenVignette
    {
    public:
        explicit OffscreenVignette(std::shared_ptr<VulkanRHI> rhi) : m_rhi(std::move(rhi))
        {
            createRenderPass();
            createImages(160, 120);
            RenderPassCommonInfo common;
            common.rhi             = m_rhi;
            common.render_resource = std::make_shared<RenderResource>();
            m_pass.setCommonInfo(common);
            VignettePassInitInfo init;
            init.render_pass      = m_render_pass;
            init.input_attachment = m_views[0];
            m_pass.initialize(&init);
        }

        ~OffscreenVignette()
        {
            m_rhi->queueWaitIdle(m_rhi->getGraphicsQueue());
            const auto& pipeline = m_pass.m_render_pipelines.front();
            vkDestroyPipeline(m_rhi->m_device, static_cast<VulkanPipeline*>(pipeline.pipeline)->getResource(), nullptr);
            vkDestroyPipelineLayout(
                m_rhi->m_device, static_cast<VulkanPipelineLayout*>(pipeline.layout)->getResource(), nullptr);
            vkDestroyDescriptorSetLayout(
                m_rhi->m_device,
                static_cast<VulkanDescriptorSetLayout*>(m_pass.m_descriptor_infos[0].layout)->getResource(),
                nullptr);
            delete static_cast<VulkanPipeline*>(pipeline.pipeline);
            delete static_cast<VulkanPipelineLayout*>(pipeline.layout);
            delete static_cast<VulkanDescriptorSetLayout*>(m_pass.m_descriptor_infos[0].layout);
            delete static_cast<VulkanDescriptorSet*>(m_pass.m_descriptor_infos[0].descriptor_set);
            destroyImages();
            vkDestroyRenderPass(m_rhi->m_device, static_cast<VulkanRenderPass*>(m_render_pass)->getResource(), nullptr);
            delete static_cast<VulkanRenderPass*>(m_render_pass);
        }

        void recreate(uint32_t width, uint32_t height)
        {
            m_rhi->queueWaitIdle(m_rhi->getGraphicsQueue());
            destroyImages();
            createImages(width, height);
            m_pass.updateAfterFramebufferRecreate(m_views[0]);
        }

        std::vector<uint8_t> render(const VignetteSettings& settings, const RHIViewport& viewport)
        {
            m_pass.setSettings(settings);
            m_rhi->m_viewport                  = viewport;
            m_rhi->m_scissor                   = {{0, 0}, {m_width, m_height}};
            RHICommandBuffer* command          = m_rhi->beginSingleTimeCommands();
            RHICommandBuffer* previous_command = m_rhi->m_current_command_buffer;
            m_rhi->m_current_command_buffer    = command;

            RHIClearValue clear_values[2] {};
            clear_values[0].color = {{0.8f, 0.4f, 0.2f, 0.6f}};
            RHIRenderPassBeginInfo begin {};
            begin.sType             = RHI_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            begin.renderPass        = m_render_pass;
            begin.framebuffer       = m_framebuffer;
            begin.renderArea.extent = {m_width, m_height};
            begin.clearValueCount   = 2;
            begin.pClearValues      = clear_values;
            m_rhi->cmdBeginRenderPassPFN(command, &begin, RHI_SUBPASS_CONTENTS_INLINE);
            for (int subpass = 0; subpass < _main_camera_subpass_vignette; ++subpass)
            {
                m_rhi->cmdNextSubpassPFN(command, RHI_SUBPASS_CONTENTS_INLINE);
            }
            m_pass.draw();
            m_rhi->cmdEndRenderPassPFN(command);

            RHIBufferImageCopy copy {};
            copy.imageSubresource.aspectMask = RHI_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.layerCount = 1;
            copy.imageExtent                 = {m_width, m_height, 1};
            m_rhi->cmdCopyImageToBuffer(
                command, m_images[1], RHI_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_readback, 1, &copy);

            RHIBufferMemoryBarrier host_barrier {};
            host_barrier.sType               = RHI_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            host_barrier.srcAccessMask       = RHI_ACCESS_TRANSFER_WRITE_BIT;
            host_barrier.dstAccessMask       = RHI_ACCESS_HOST_READ_BIT;
            host_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            host_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            host_barrier.buffer              = m_readback;
            host_barrier.size                = m_width * m_height * 4;
            m_rhi->cmdPipelineBarrier(command,
                                      RHI_PIPELINE_STAGE_TRANSFER_BIT,
                                      RHI_PIPELINE_STAGE_HOST_BIT,
                                      0,
                                      0,
                                      nullptr,
                                      1,
                                      &host_barrier,
                                      0,
                                      nullptr);
            m_rhi->endSingleTimeCommands(command);
            m_rhi->m_current_command_buffer = previous_command;

            std::vector<uint8_t> pixels(m_width * m_height * 4);
            void*                mapped = nullptr;
            require(m_rhi->mapMemory(m_readback_memory, 0, pixels.size(), 0, &mapped), "Readback memory maps");
            std::memcpy(pixels.data(), mapped, pixels.size());
            m_rhi->unmapMemory(m_readback_memory);
            return pixels;
        }

    private:
        void createRenderPass()
        {
            RHIAttachmentDescription attachments[2] {};
            for (auto& attachment : attachments)
            {
                attachment.format         = RHI_FORMAT_R8G8B8A8_UNORM;
                attachment.samples        = RHI_SAMPLE_COUNT_1_BIT;
                attachment.loadOp         = RHI_ATTACHMENT_LOAD_OP_CLEAR;
                attachment.storeOp        = RHI_ATTACHMENT_STORE_OP_STORE;
                attachment.stencilLoadOp  = RHI_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.stencilStoreOp = RHI_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment.initialLayout  = RHI_IMAGE_LAYOUT_UNDEFINED;
            }
            attachments[0].finalLayout = RHI_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            attachments[1].finalLayout = RHI_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            const RHIAttachmentReference source_write {0, RHI_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            const RHIAttachmentReference source_read {0, RHI_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            const RHIAttachmentReference destination {1, RHI_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            const uint32_t               preserved = 0;
            std::array<RHISubpassDescription, _main_camera_subpass_vignette + 1> subpasses {};
            for (auto& subpass : subpasses)
            {
                subpass.pipelineBindPoint = RHI_PIPELINE_BIND_POINT_GRAPHICS;
            }
            subpasses[0].colorAttachmentCount = 1;
            subpasses[0].pColorAttachments    = &source_write;
            for (int index = 1; index < _main_camera_subpass_vignette; ++index)
            {
                subpasses[index].preserveAttachmentCount = 1;
                subpasses[index].pPreserveAttachments    = &preserved;
            }
            subpasses.back().inputAttachmentCount = 1;
            subpasses.back().pInputAttachments    = &source_read;
            subpasses.back().colorAttachmentCount = 1;
            subpasses.back().pColorAttachments    = &destination;

            RHISubpassDependency dependencies[2] {};
            dependencies[0].srcSubpass      = 0;
            dependencies[0].dstSubpass      = _main_camera_subpass_vignette;
            dependencies[0].srcStageMask    = RHI_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[0].dstStageMask    = RHI_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[0].srcAccessMask   = RHI_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[0].dstAccessMask   = RHI_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            dependencies[0].dependencyFlags = RHI_DEPENDENCY_BY_REGION_BIT;
            dependencies[1].srcSubpass      = _main_camera_subpass_vignette;
            dependencies[1].dstSubpass      = RHI_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask    = RHI_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[1].dstStageMask    = RHI_PIPELINE_STAGE_TRANSFER_BIT;
            dependencies[1].srcAccessMask   = RHI_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[1].dstAccessMask   = RHI_ACCESS_TRANSFER_READ_BIT;

            RHIRenderPassCreateInfo info {};
            info.sType           = RHI_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            info.attachmentCount = 2;
            info.pAttachments    = attachments;
            info.subpassCount    = static_cast<uint32_t>(subpasses.size());
            info.pSubpasses      = subpasses.data();
            info.dependencyCount = 2;
            info.pDependencies   = dependencies;
            require(m_rhi->createRenderPass(&info, m_render_pass), "Offscreen render pass is created");
        }

        void createImages(uint32_t width, uint32_t height)
        {
            m_width  = width;
            m_height = height;
            for (size_t index = 0; index < m_images.size(); ++index)
            {
                m_rhi->createImage(width,
                                   height,
                                   RHI_FORMAT_R8G8B8A8_UNORM,
                                   RHI_IMAGE_TILING_OPTIMAL,
                                   RHI_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | RHI_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                                       RHI_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                   RHI_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   m_images[index],
                                   m_memory[index],
                                   0,
                                   1,
                                   1);
                m_rhi->createImageView(m_images[index],
                                       RHI_FORMAT_R8G8B8A8_UNORM,
                                       RHI_IMAGE_ASPECT_COLOR_BIT,
                                       RHI_IMAGE_VIEW_TYPE_2D,
                                       1,
                                       1,
                                       m_views[index]);
            }
            RHIFramebufferCreateInfo framebuffer {};
            framebuffer.sType           = RHI_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer.renderPass      = m_render_pass;
            framebuffer.attachmentCount = 2;
            framebuffer.pAttachments    = m_views.data();
            framebuffer.width           = width;
            framebuffer.height          = height;
            framebuffer.layers          = 1;
            require(m_rhi->createFramebuffer(&framebuffer, m_framebuffer), "Offscreen framebuffer is created");
            m_rhi->createBuffer(width * height * 4,
                                RHI_BUFFER_USAGE_TRANSFER_DST_BIT,
                                RHI_MEMORY_PROPERTY_HOST_VISIBLE_BIT | RHI_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                m_readback,
                                m_readback_memory);
        }

        void destroyImages()
        {
            m_rhi->destroyFramebuffer(m_framebuffer);
            delete static_cast<VulkanFramebuffer*>(m_framebuffer);
            for (size_t index = 0; index < m_images.size(); ++index)
            {
                m_rhi->destroyImageView(m_views[index]);
                m_rhi->destroyImage(m_images[index]);
                delete static_cast<VulkanImageView*>(m_views[index]);
                delete static_cast<VulkanImage*>(m_images[index]);
                m_rhi->freeMemory(m_memory[index]);
            }
            m_rhi->destroyBuffer(m_readback);
            m_rhi->freeMemory(m_readback_memory);
        }

        std::shared_ptr<VulkanRHI>      m_rhi;
        VignettePass                    m_pass;
        RHIRenderPass*                  m_render_pass {nullptr};
        RHIFramebuffer*                 m_framebuffer {nullptr};
        std::array<RHIImage*, 2>        m_images {};
        std::array<RHIImageView*, 2>    m_views {};
        std::array<RHIDeviceMemory*, 2> m_memory {};
        RHIBuffer*                      m_readback {nullptr};
        RHIDeviceMemory*                m_readback_memory {nullptr};
        uint32_t                        m_width {0};
        uint32_t                        m_height {0};
    };

    void verifyViewport(OffscreenVignette& fixture, uint32_t image_width, const RHIViewport& viewport)
    {
        VignetteSettings settings;
        settings.m_enabled  = false;
        const auto disabled = fixture.render(settings, viewport);
        settings.m_enabled  = true;
        settings.m_strength = 0.f;
        require(fixture.render(settings, viewport) == disabled, "Zero strength exactly matches disabled output");
        settings.m_strength     = 0.6f;
        settings.m_inner_radius = 0.25f;
        settings.m_outer_radius = 1.25f;
        const auto enabled      = fixture.render(settings, viewport);

        const int center_x = static_cast<int>(viewport.x + viewport.width / 2.f);
        const int center_y = static_cast<int>(viewport.y + viewport.height / 2.f);
        auto      value    = [&](const std::vector<uint8_t>& pixels, int x, int y, int channel) {
            return static_cast<float>(pixels[(y * image_width + x) * 4 + channel]);
        };
        requireNear(value(enabled, center_x, center_y, 0), 204.f, "Center retains its red value", 1.f);
        requireNear(value(enabled, center_x, center_y, 1), 102.f, "Center retains its green value", 1.f);
        requireNear(value(enabled, center_x, center_y, 3), 153.f, "Center retains alpha", 1.f);
        const int corner_x = static_cast<int>(viewport.x);
        const int corner_y = static_cast<int>(viewport.y);
        requireNear(value(disabled, corner_x, corner_y, 0), 204.f, "Disabled pass copies edge color", 1.f);
        requireNear(
            value(enabled, corner_x, corner_y, 0), 204.f * 0.4f, "Outer edge applies the configured strength", 1.f);
        requireNear(value(enabled, corner_x, corner_y, 3), 153.f, "Darkened edge retains alpha", 1.f);
        requireNear(value(enabled, center_x + 28, center_y, 0),
                    value(enabled, center_x, center_y + 28, 0),
                    "Equal pixel distances have equal attenuation",
                    1.f);
        requireNear(value(enabled, center_x - 28, center_y, 0),
                    value(enabled, center_x + 28, center_y, 0),
                    "Attenuation is symmetric about the viewport center",
                    1.f);
        requireNear(value(enabled, 0, 0, 0), 0.f, "Pixels outside the offset viewport remain untouched");
    }
} // namespace

int main()
{
    try
    {
        g_runtime_global_context.m_logger_system = std::make_shared<LogSystem>();
        {
            auto window = std::make_shared<WindowSystem>();
            window->initialize({256, 192, "Piccolo Vignette GPU Test", false});
            auto        rhi = std::make_shared<VulkanRHI>();
            RHIInitInfo init;
            init.window_system = window;
            rhi->initialize(init);
            {
                OffscreenVignette fixture(rhi);
                verifyViewport(fixture, 160, {16.f, 12.f, 127.f, 95.f, 0.f, 1.f});
                fixture.recreate(240, 128);
                verifyViewport(fixture, 240, {16.f, 8.f, 201.f, 101.f, 0.f, 1.f});
            }
            rhi->clear();
        }
        g_runtime_global_context.m_logger_system.reset();
        std::cout << "Passed GPU pixel checks before and after framebuffer recreation.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        g_runtime_global_context.m_logger_system.reset();
        return 1;
    }
}
