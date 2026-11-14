#include "runtime/function/controller/character_controller.h"

#include "runtime/core/base/macro.h"

#include "runtime/function/framework/component/motor/motor_component.h"
#include "runtime/function/framework/world/world_manager.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/physics/physics_scene.h"

#include <algorithm>
#include <cmath>

namespace Piccolo
{
    CharacterController::CharacterController(const Capsule& capsule) : m_capsule(capsule)
    {
        m_rigidbody_shape                                    = RigidBodyShape();
        m_rigidbody_shape.m_geometry                         = PICCOLO_REFLECTION_NEW(Capsule);
        *static_cast<Capsule*>(m_rigidbody_shape.m_geometry) = m_capsule;

        m_rigidbody_shape.m_type = RigidBodyShapeType::capsule;

        Quaternion orientation;
        orientation.fromAngleAxis(Radian(Degree(90.f)), Vector3::UNIT_X);

        m_rigidbody_shape.m_local_transform =
            Transform(Vector3(0, 0, capsule.m_half_height + capsule.m_radius), orientation, Vector3::UNIT_SCALE);
    }

    Vector3 CharacterController::move(const Vector3& current_position, const Vector3& displacement)
    {
        std::shared_ptr<PhysicsScene> physics_scene =
            g_runtime_global_context.m_world_manager->getCurrentActivePhysicsScene().lock();
        ASSERT(physics_scene);

        return move(*physics_scene, current_position, displacement);
    }

    namespace
    {
        constexpr float kContactOffset        = 0.005f;
        constexpr float kGroundProbeDistance  = 0.05f;
        constexpr float kMinimumGroundNormalZ = 0.7071068f;

        // Ignore tangent contacts so a floor cannot block horizontal travel or a wall block falling.
        bool findBlockingHit(PhysicsScene&         scene,
                             const RigidBodyShape& shape,
                             const Vector3&        position,
                             const Vector3&        direction,
                             float                 distance,
                             PhysicsHitInfo&       result)
        {
            std::vector<PhysicsHitInfo> hits;
            const Transform             transform(position, Quaternion::IDENTITY, Vector3::UNIT_SCALE);
            scene.sweep(shape, transform.getMatrix(), direction, distance, hits);
            for (const PhysicsHitInfo& hit : hits)
            {
                if (direction.dotProduct(hit.hit_normal) < -0.0001f)
                {
                    result = hit;
                    return true;
                }
            }
            return false;
        }
    } // namespace

    Vector3 CharacterController::move(PhysicsScene& scene, const Vector3& current_position, const Vector3& displacement)
    {
        Vector3 position = current_position;
        m_is_grounded    = false;
        m_hit_ceiling    = false;

        // Resolve height first. A wall contact in the next pass cannot cancel this movement.
        if (std::abs(displacement.z) > 0.00001f)
        {
            const Vector3  direction = displacement.z > 0.f ? Vector3::UNIT_Z : Vector3::NEGATIVE_UNIT_Z;
            const float    distance  = std::abs(displacement.z);
            PhysicsHitInfo hit;
            if (findBlockingHit(scene, m_rigidbody_shape, position, direction, distance + kContactOffset, hit))
            {
                const float travel = std::clamp(hit.hit_distance - kContactOffset, 0.f, distance);
                position += direction * travel;
                m_is_grounded = displacement.z < 0.f && hit.hit_normal.z >= kMinimumGroundNormalZ;
                m_hit_ceiling = displacement.z > 0.f;
            }
            else
            {
                position.z += displacement.z;
            }
        }

        // Project the unconsumed displacement onto the wall. Recast at corners.
        Vector3 remaining(displacement.x, displacement.y, 0.f);
        for (int iteration = 0; iteration < 3; ++iteration)
        {
            const float distance = remaining.length();
            if (distance < 0.00001f)
            {
                break;
            }
            const Vector3  direction = remaining / distance;
            PhysicsHitInfo hit;
            if (!findBlockingHit(scene, m_rigidbody_shape, position, direction, distance + kContactOffset, hit))
            {
                position += remaining;
                break;
            }
            const float travel = std::clamp(hit.hit_distance - kContactOffset, 0.f, distance);
            position += direction * travel;
            remaining -= direction * travel;
            remaining -= hit.hit_normal * std::min(remaining.dotProduct(hit.hit_normal), 0.f);
        }

        // Probe at the final horizontal position so walking off a platform starts a fall.
        if (displacement.z <= 0.f)
        {
            PhysicsHitInfo ground;
            m_is_grounded = findBlockingHit(scene,
                                            m_rigidbody_shape,
                                            position,
                                            Vector3::NEGATIVE_UNIT_Z,
                                            kGroundProbeDistance + kContactOffset,
                                            ground) &&
                            ground.hit_normal.z >= kMinimumGroundNormalZ;
            if (m_is_grounded)
            {
                position.z -= std::max(ground.hit_distance - kContactOffset, 0.f);
            }
        }
        return position;
    }

    bool CharacterController::isGrounded() const { return m_is_grounded; }

    bool CharacterController::hitCeiling() const { return m_hit_ceiling; }
} // namespace Piccolo
