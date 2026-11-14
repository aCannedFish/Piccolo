#include "test_checks.h"

#include "runtime/core/log/log_system.h"
#include "runtime/function/controller/character_controller.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/physics/physics_scene.h"

#include <iostream>

using namespace Piccolo;

namespace
{
    // Release the async logger before spdlog's process-wide thread pool is destroyed.
    struct TestLog
    {
        TestLog()
        {
            g_runtime_global_context.m_logger_system = std::make_shared<LogSystem>();
        }

        ~TestLog()
        {
            g_runtime_global_context.m_logger_system.reset();
        }
    };

    // Use real Jolt bodies to exercise query normals, distances, and contact response together.
    void addBox(PhysicsScene& scene, const Vector3& position, const Vector3& half_extents)
    {
        RigidBodyComponentRes resource;
        resource.m_inverse_mass = 0.f;
        resource.m_actor_type   = 0;
        resource.m_shapes.emplace_back();
        auto& shape                                                  = resource.m_shapes.back();
        shape.m_type                                                 = RigidBodyShapeType::box;
        shape.m_geometry                                             = PICCOLO_REFLECTION_NEW(Box);
        static_cast<Box*>(shape.m_geometry.getPtr())->m_half_extents = half_extents;
        const uint32_t body =
            scene.createRigidBody(Transform(position, Quaternion::IDENTITY, Vector3::UNIT_SCALE), resource);
        require(body != s_invalid_rigidbody_id, "Fixture body was created");
    }

    void testPlatformLanding(PhysicsScene& scene)
    {
        CharacterController controller {Capsule()};
        const Vector3       position = controller.move(scene, Vector3(-4.f, 0.f, 4.f), Vector3(0.f, 0.f, -5.f));
        requireNear(position.z, 1.005f, "Character lands on platform top", 0.015f);
        require(controller.isGrounded(), "Platform reports ground support");
        const Vector3 jump = controller.move(scene, position, Vector3(0.f, 0.f, 0.3f));
        requireNear(jump.z, position.z + 0.3f, "Character can jump away from a support surface");
        require(!controller.isGrounded(), "Jump clears grounded state");
    }

    void testWallAndSliding(PhysicsScene& scene)
    {
        CharacterController controller {Capsule()};
        const Vector3       blocked = controller.move(scene, Vector3(0.f, 0.f, 0.01f), Vector3(20.f, 0.f, 0.f));
        require(blocked.x > 1.15f && blocked.x < 1.21f, "Sweep blocks large displacements before the wall");
        const Vector3 slide = controller.move(scene, Vector3(0.f, 0.f, 0.01f), Vector3(3.f, 2.f, 0.f));
        require(slide.x < 1.21f, "Slide stays outside the wall");
        requireNear(slide.y, 2.f, "Slide retains tangential movement", 0.02f);
    }

    void testFallingAlongWall(PhysicsScene& scene)
    {
        CharacterController controller {Capsule()};
        const Vector3       position = controller.move(scene, Vector3(1.19f, 0.f, 3.f), Vector3(1.f, 0.f, -1.f));
        requireNear(position.z, 2.f, "Wall contact does not cancel downward motion", 0.01f);
        require(!controller.isGrounded(), "Wall cannot count as ground");
    }

    void testLeavingPlatform(PhysicsScene& scene)
    {
        CharacterController controller {Capsule()};
        const Vector3       position = controller.move(scene, Vector3(-4.f, 0.f, 1.005f), Vector3(0.f, 3.f, 0.f));
        requireNear(position.y, 3.f, "Character moves beyond the platform edge");
        require(!controller.isGrounded(), "Ground probe uses the final horizontal position");
    }

    void testCeiling(PhysicsScene& scene)
    {
        CharacterController controller {Capsule()};
        const Vector3       position = controller.move(scene, Vector3(0.f, 5.f, 0.01f), Vector3(0.f, 0.f, 2.f));
        requireNear(position.z, 0.795f, "Capsule top stops below ceiling", 0.015f);
        require(controller.hitCeiling(), "Ceiling contact reaches the motor");
        require(!controller.isGrounded(), "Ceiling does not count as ground");
    }

    void testSweepContract(PhysicsScene& scene)
    {
        RigidBodyShape shape;
        shape.m_geometry                                             = PICCOLO_REFLECTION_NEW(Box);
        static_cast<Box*>(shape.m_geometry.getPtr())->m_half_extents = Vector3(0.25f, 0.25f, 0.25f);
        shape.m_type                                                 = RigidBodyShapeType::box;
        std::vector<PhysicsHitInfo> hits;
        const Transform             transform(Vector3(0.f, -5.f, 3.f), Quaternion::IDENTITY, Vector3(2.f, 2.f, 2.f));
        require(scene.sweep(shape, transform.getMatrix(), Vector3::NEGATIVE_UNIT_Z, 5.f, hits), "Sweep finds floor");
        requireNear(hits.front().hit_distance, 2.5f, "Query scale is applied once", 0.01f);
        requireNear(hits.front().hit_normal.z, 1.f, "Floor normal points upward");
        require(!scene.sweep(shape, transform.getMatrix(), Vector3::UNIT_Z, 1.f, hits), "Upward sweep misses");
        require(hits.empty(), "Miss clears previous results");
        hits.emplace_back();
        require(!scene.sweep(shape, transform.getMatrix(), Vector3::ZERO, 1.f, hits), "Zero direction has no hit");
        require(hits.empty(), "Empty sweep clears previous results");
    }
} // namespace

int main()
{
    try
    {
        TestLog log;
        PhysicsScene scene(Vector3(0.f, 0.f, -9.8f));
        addBox(scene, Vector3(0.f, 0.f, -0.5f), Vector3(20.f, 20.f, 0.5f));
        addBox(scene, Vector3(2.f, 0.f, 3.f), Vector3(0.5f, 10.f, 3.f));
        addBox(scene, Vector3(-4.f, 0.f, 0.5f), Vector3(1.f, 1.f, 0.5f));
        addBox(scene, Vector3(0.f, 5.f, 3.f), Vector3(1.f, 1.f, 0.2f));
        testPlatformLanding(scene);
        testWallAndSliding(scene);
        testFallingAlongWall(scene);
        testLeavingPlatform(scene);
        testCeiling(scene);
        testSweepContract(scene);
        std::cout << "Passed 6 controller regression cases.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
