#pragma once

#include "runtime/function/animation/animation_fsm.h"
#include "runtime/function/animation/skeleton.h"
#include "runtime/function/framework/component/component.h"
#include "runtime/resource/res_type/components/animation.h"

namespace Piccolo
{
    // Advances clip playback and publishes the skeleton matrices used by rendering.
    REFLECTION_TYPE(AnimationComponent)
    CLASS(AnimationComponent : public Component, WhiteListFields)
    {
        REFLECTION_BODY(AnimationComponent)

    public:
        AnimationComponent() = default;

        void postLoadResource(std::weak_ptr<GObject> parent_object) override;

        void tick(float delta_time) override;

        // Explicit motor signals let gameplay and preview tools use the same playback path.
        void updateAnimation(float delta_time, float speed_ratio, bool is_moving, bool is_jumping);
        const char* getAnimationStateName() const;

        const AnimationResult& getResult() const;

        const Skeleton& getSkeleton() const;

    protected:
        META(Enable)
        AnimationComponentRes m_animation_res;

        Skeleton m_skeleton;
        AnimationFSM m_animation_fsm;
        float m_state_phase {0.f};
    };
} // namespace Piccolo
