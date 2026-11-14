#include "test_checks.h"

#include "runtime/engine.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/window_system.h"

#include <array>
#include <iostream>

using namespace Piccolo;

namespace
{
    // Run both main-camera paths through effect changes and actual swapchain recreation.
    void runScenarios(PiccoloEngine& engine)
    {
        auto&                                     context = g_runtime_global_context;
        const std::array<RENDER_PIPELINE_TYPE, 2> pipelines {RENDER_PIPELINE_TYPE::DEFERRED_PIPELINE,
                                                             RENDER_PIPELINE_TYPE::FORWARD_PIPELINE};
        const std::array<std::array<int, 2>, 3>   sizes {{{960, 640}, {720, 720}, {1100, 600}}};
        VignetteSettings                          settings;
        settings.m_strength = 0.65f;

        for (const auto pipeline : pipelines)
        {
            context.m_render_system->setRenderPipelineType(pipeline);
            for (const bool enabled : {false, true})
            {
                settings.m_enabled = enabled;
                context.m_render_system->setVignetteSettings(settings);
                for (const auto& size : sizes)
                {
                    glfwSetWindowSize(context.m_window_system->getWindow(), size[0], size[1]);
                    context.m_window_system->pollEvents();
                    int width  = 0;
                    int height = 0;
                    glfwGetFramebufferSize(context.m_window_system->getWindow(), &width, &height);
                    require(width > 0 && height > 0, "Resized framebuffer has a valid extent");

                    // Also exercise an editor-like offset viewport within each framebuffer.
                    context.m_render_system->updateEngineContentViewport(
                        24.f, 16.f, static_cast<float>(width) - 48.f, static_cast<float>(height) - 32.f);
                    for (int frame = 0; frame < 6; ++frame)
                    {
                        require(engine.tickOneFrame(1.f / 60.f), "Post-process window remains open");
                    }
                }
            }
        }
        std::cout << "Passed 12 post-process combinations across pipelines, enable states, and window sizes.\n";
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: PiccoloPostProcessSmoke /absolute/path/to/PiccoloEditor.ini\n";
        return 1;
    }
    PiccoloEngine engine;
    bool          started = false;
    try
    {
        // Keep the scene still while exercising rendering paths and framebuffer recreation.
        g_is_editor_mode = true;
        engine.startEngine(argv[1]);
        started = true;
        runScenarios(engine);
        engine.shutdownEngine();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        if (started)
        {
            engine.shutdownEngine();
        }
        return 1;
    }
}
