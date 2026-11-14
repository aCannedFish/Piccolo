#pragma once

#include "runtime/core/math/vector3.h"
#include "runtime/resource/res_type/components/rigid_body.h"
#include "runtime/resource/res_type/data/basic_shape.h"

namespace Piccolo
{
    class PhysicsScene;
    enum SweepPass
    {
        SWEEP_PASS_UP,
        SWEEP_PASS_SIDE,
        SWEEP_PASS_DOWN,
        SWEEP_PASS_SENSOR
    };

    // Reports collision-resolved movement and support contacts to the motor.
    class Controller
    {
    public:
        virtual ~Controller() = default;

        virtual Vector3 move(const Vector3& current_position, const Vector3& displacement) = 0;
        virtual bool    isGrounded() const                                                 = 0;
        virtual bool    hitCeiling() const                                                 = 0;
    };

    // Sweeps a vertical capsule in separate vertical and horizontal passes.
    class CharacterController : public Controller
    {
    public:
        CharacterController(const Capsule& capsule);
        ~CharacterController() = default;

        Vector3 move(const Vector3& current_position, const Vector3& displacement) override;

        // The explicit scene overload also supports simulation without a loaded world.
        Vector3 move(PhysicsScene& scene, const Vector3& current_position, const Vector3& displacement);
        bool    isGrounded() const override;
        bool    hitCeiling() const override;

    private:
        Capsule        m_capsule;
        RigidBodyShape m_rigidbody_shape;
        bool           m_is_grounded {false};
        bool           m_hit_ceiling {false};
    };
} // namespace Piccolo
