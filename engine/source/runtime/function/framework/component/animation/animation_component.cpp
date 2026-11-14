#include "runtime/function/framework/component/animation/animation_component.h"

#include "runtime/function/animation/animation_system.h"
#include "runtime/function/framework/object/object.h"

namespace Piccolo
{
    void AnimationComponent::postLoadResource(std::weak_ptr<GObject> parent_object)
    {
        m_parent_object = parent_object;

        auto skeleton_res = AnimationManager::tryLoadSkeleton(m_animation_res.skeleton_file_path);

        m_skeleton.buildSkeleton(*skeleton_res);
    }

    void AnimationComponent::tick(float delta_time)
    {
        // Every active clip advances at its own duration before pose sampling.
        BlendState& blend_state = m_animation_res.blend_state;
        for (int clip_index = 0; clip_index < blend_state.clip_count; ++clip_index)
        {
            float& phase = blend_state.blend_ratio.at(clip_index);
            const float duration = blend_state.blend_clip_file_length.at(clip_index);
            if (duration > 0.f)
            {
                phase += delta_time / duration;
                phase -= std::floor(phase);
            }
        }

        m_skeleton.applyAnimation(AnimationManager::getBlendStateWithClipData(m_animation_res.blend_state));
        m_animation_res.animation_result = m_skeleton.outputAnimationResult();
    }

    const AnimationResult& AnimationComponent::getResult() const { return m_animation_res.animation_result; }

    const Skeleton& AnimationComponent::getSkeleton() const { return m_skeleton; }
} // namespace Piccolo
