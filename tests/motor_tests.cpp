#include "test_checks.h"

#include "_generated/serializer/all_serializer.h"
#include "runtime/core/meta/reflection/reflection_register.h"
#include "runtime/function/framework/component/motor/motor_component.h"

#include <iostream>

using namespace Piccolo;

namespace
{
    // Register the generated metadata used by the editor's recursive property controls.
    struct TestReflection
    {
        TestReflection() { Reflection::TypeMetaRegister::metaRegister(); }

        ~TestReflection() { Reflection::TypeMetaRegister::metaUnregister(); }
    };

    void testJumpModes()
    {
        MotorComponentRes resource;
        resource.m_jump_height = 2.f;
        requireNear(
            MotorComponent::calculateJumpInitialSpeed(resource, 9.8f), std::sqrt(39.2f), "Height mode uses gravity");
        resource.m_use_jump_initial_speed = true;
        resource.m_jump_initial_speed     = 6.f;
        requireNear(MotorComponent::calculateJumpInitialSpeed(resource, 9.8f), 6.f, "Speed mode uses the edited speed");
        requireNear(
            MotorComponent::calculateJumpInitialSpeed(resource, 3.f), 6.f, "Speed mode is independent of gravity");
        resource.m_jump_initial_speed = -2.f;
        requireNear(MotorComponent::calculateJumpInitialSpeed(resource, 9.8f), 0.f, "Negative speed disables takeoff");
        resource.m_use_jump_initial_speed = false;
        resource.m_jump_height            = -1.f;
        requireNear(MotorComponent::calculateJumpInitialSpeed(resource, 9.8f), 0.f, "Negative height disables takeoff");
    }

    void testReflectionAndSerialization()
    {
        MotorComponentRes resource;
        resource.m_controller_config = PICCOLO_REFLECTION_NEW(PhysicsControllerConfig);
        auto              metadata = Reflection::TypeMeta::newMetaFromName("MotorComponentRes");
        require(metadata.isValid(), "Motor resource metadata exists");
        auto speed_field = metadata.getFieldByName("m_jump_initial_speed");
        auto mode_field  = metadata.getFieldByName("m_use_jump_initial_speed");
        require(std::string(speed_field.getFieldTypeName()) == "float", "Speed uses the editor float control");
        require(std::string(mode_field.getFieldTypeName()) == "bool", "Mode uses the editor checkbox");
        float speed     = 7.f;
        bool  use_speed = true;
        speed_field.set(&resource, &speed);
        mode_field.set(&resource, &use_speed);
        requireNear(
            MotorComponent::calculateJumpInitialSpeed(resource, 9.8f), 7.f, "Reflected edit affects launch speed");

        const Json saved = Serializer::write(resource);
        requireNear(static_cast<float>(saved["jump_initial_speed"].number_value()), 7.f, "Save includes edited speed");
        MotorComponentRes restored;
        Serializer::read(saved, restored);
        require(restored.m_use_jump_initial_speed, "Load restores the selected mode");
        requireNear(restored.m_jump_initial_speed, 7.f, "Load restores the edited speed");
        PICCOLO_REFLECTION_DELETE(restored.m_controller_config);
        PICCOLO_REFLECTION_DELETE(resource.m_controller_config);
    }

    void testLegacyResource()
    {
        MotorComponentRes resource;
        const Json        old_resource = Json::object {{"jump_height", 1.5}};
        Serializer::read(old_resource, resource);
        require(!resource.m_use_jump_initial_speed, "Old assets keep height mode");
        requireNear(MotorComponent::calculateJumpInitialSpeed(resource, 9.8f),
                    std::sqrt(29.4f),
                    "Old jump height is preserved");
    }
} // namespace

int main()
{
    try
    {
        TestReflection reflection;
        testJumpModes();
        testReflectionAndSerialization();
        testLegacyResource();
        std::cout << "Passed 3 motor regression cases.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
