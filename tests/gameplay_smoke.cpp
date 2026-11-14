#include "test_checks.h"

#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/engine.h"
#include "runtime/function/character/character.h"
#include "runtime/function/framework/component/animation/animation_component.h"
#include "runtime/function/framework/component/motor/motor_component.h"
#include "runtime/function/framework/component/transform/transform_component.h"
#include "runtime/function/framework/level/level.h"
#include "runtime/function/framework/object/object.h"
#include "runtime/function/framework/world/world_manager.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/input/input_system.h"
#include "runtime/function/physics/physics_scene.h"
#include "runtime/function/render/window_system.h"

#include <algorithm>
#include <iostream>
#include <set>

using namespace Piccolo;

namespace
{
    constexpr float kFrameTime = 1.f / 60.f;

    // Exercise production input, motor, collision, animation, and rendering in the default world.
    void runScenario(PiccoloEngine& engine)
    {
        auto& context = g_runtime_global_context;
        context.m_window_system->setFocusMode(true);
        for (int frame = 0; frame < 10; ++frame)
        {
            require(engine.tickOneFrame(kFrameTime), "Window remains open during world loading");
        }
        const auto level = context.m_world_manager->getCurrentActiveLevel().lock();
        require(level != nullptr, "Default level loaded");
        const auto character = level->getCurrentActiveCharacter().lock();
        require(character != nullptr, "Default player exists");
        const auto player    = level->getGObjectByID(character->getObjectID()).lock();
        auto*      transform = player->tryGetComponent(TransformComponent);
        auto*      animation = player->tryGetComponent(AnimationComponent);
        auto*      motor     = player->tryGetComponent(MotorComponent);
        require(transform && animation && motor, "Player has transform, animation, and motor components");

        auto  motor_type    = Reflection::TypeMeta::newMetaFromName("MotorComponent");
        void* resource      = motor_type.getFieldByName("m_motor_res").get(motor);
        auto  resource_type = Reflection::TypeMeta::newMetaFromName("MotorComponentRes");
        auto  speed_field   = resource_type.getFieldByName("m_jump_initial_speed");
        bool  speed_mode    = true;
        resource_type.getFieldByName("m_use_jump_initial_speed").set(resource, &speed_mode);

        std::set<std::string> visited_states;
        auto                  tick = [&]() {
            require(engine.tickOneFrame(kFrameTime), "Window remains open during gameplay");
            visited_states.insert(animation->getAnimationStateName());
            require(animation->getResult().node.size() == 58, "Player keeps a complete skinning palette");
        };

        auto jump = [&](float launch_speed) {
            speed_field.set(resource, &launch_speed);
            const float ground_height = transform->getPosition().z;
            float       peak          = ground_height;
            context.m_input_system->onKey(GLFW_KEY_SPACE, 0, GLFW_PRESS, 0);
            tick();
            context.m_input_system->onKey(GLFW_KEY_SPACE, 0, GLFW_RELEASE, 0);
            for (int frame = 0; frame < 150; ++frame)
            {
                tick();
                peak = std::max(peak, transform->getPosition().z);
            }
            require(!motor->isJumping(), "Player lands after releasing jump");
            requireNear(transform->getPosition().z, ground_height, "Player returns to its support surface", 0.03f);
            std::cout << "Jump speed: " << launch_speed << ", ground: " << ground_height << ", peak: " << peak << "\n";
            return peak - ground_height;
        };

        const float first_height  = jump(4.5f);
        const float second_height = jump(6.f);
        const float gravity       = level->getPhysicsScene().lock()->getGravity().length();
        // The discrete motor integration has at most one frame of launch-speed height error.
        requireNear(first_height,
                    4.5f * 4.5f / (2.f * gravity),
                    "Default jump matches the level gravity",
                    4.5f * kFrameTime + 0.01f);
        requireNear(second_height,
                    6.f * 6.f / (2.f * gravity),
                    "Edited jump matches the level gravity",
                    6.f * kFrameTime + 0.01f);
        require(second_height > first_height, "Reflected speed edit increases the next jump height");
        for (const char* state : {"idle", "jump_start", "jump_loop", "jump_end"})
        {
            require(visited_states.count(state) > 0, std::string("Gameplay visits ") + state);
        }

        const Vector3 start = transform->getPosition();
        context.m_input_system->onKey(GLFW_KEY_S, 0, GLFW_PRESS, 0);
        context.m_input_system->onKey(GLFW_KEY_LEFT_SHIFT, 0, GLFW_PRESS, 0);
        for (int frame = 0; frame < 120; ++frame)
        {
            tick();
        }
        context.m_input_system->onKey(GLFW_KEY_S, 0, GLFW_RELEASE, 0);
        context.m_input_system->onKey(GLFW_KEY_LEFT_SHIFT, 0, GLFW_RELEASE, 0);
        require((transform->getPosition() - start).length() > 0.1f, "Input moves the default player");
        require(visited_states.count("walk_start") > 0, "Movement plays its start animation");
        require(visited_states.count("walk_run") > 0, "Sustained movement reaches walk/run blending");
        std::cout << "Gameplay passed. Jump heights: " << first_height << ", " << second_height << " metres.\n";
        context.m_window_system->setFocusMode(false);
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: PiccoloGameplaySmoke /absolute/path/to/PiccoloEditor.ini\n";
        return 1;
    }
    PiccoloEngine engine;
    bool          started = false;
    try
    {
        g_is_editor_mode = false;
        engine.startEngine(argv[1]);
        started = true;
        runScenario(engine);
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
