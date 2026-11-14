#include "runtime/function/framework/component/animation/animation_component.h"

#include "runtime/function/animation/animation_system.h"
#include "runtime/function/framework/component/motor/motor_component.h"
#include "runtime/function/framework/object/object.h"

#include <algorithm>
#include <stdexcept>

namespace Piccolo
{
    void AnimationComponent::postLoadResource(std::weak_ptr<GObject> parent_object)
    {
        m_parent_object = parent_object;

        auto skeleton_res = AnimationManager::tryLoadSkeleton(m_animation_res.skeleton_file_path);

        m_skeleton.buildSkeleton(*skeleton_res);
        m_animation_fsm = AnimationFSM();
        m_state_phase   = 0.f;
    }

    void AnimationComponent::tick(float delta_time)
    {
        const auto            parent      = m_parent_object.lock();
        const MotorComponent* motor       = parent ? parent->tryGetComponentConst(MotorComponent) : nullptr;
        const float           speed_ratio = motor ? motor->getSpeedRatio() : 0.f;
        const bool            is_moving   = motor && motor->getIsMoving() && speed_ratio > 0.01f;
        const bool            is_jumping  = motor && motor->isJumping();
        updateAnimation(delta_time, speed_ratio, is_moving, is_jumping);
    }

    namespace
    {
        // A one-dimensional blend space activates the two samples around the current speed.
        void updateSpeedWeights(AnimationStateRes& state, float speed_ratio)
        {
            const auto& samples = state.speed_ratios;
            if (samples.empty())
            {
                return;
            }
            if (samples.size() != static_cast<size_t>(state.blend_state.clip_count))
            {
                throw std::invalid_argument("Animation speed samples must match clip_count.");
            }
            for (size_t index = 1; index < samples.size(); ++index)
            {
                if (samples[index] <= samples[index - 1])
                {
                    throw std::invalid_argument("Animation speed samples must increase strictly.");
                }
            }
            auto& weights = state.blend_state.blend_weight;
            weights.assign(samples.size(), 0.f);
            const auto upper = std::upper_bound(samples.begin(), samples.end(), speed_ratio);
            if (upper == samples.begin())
            {
                weights.front() = 1.f;
            }
            else if (upper == samples.end())
            {
                weights.back() = 1.f;
            }
            else
            {
                const size_t upper_index = static_cast<size_t>(upper - samples.begin());
                const size_t lower_index = upper_index - 1;
                const float  ratio =
                    (speed_ratio - samples[lower_index]) / (samples[upper_index] - samples[lower_index]);
                weights[lower_index] = 1.f - ratio;
                weights[upper_index] = ratio;
            }
        }
    } // namespace

    void AnimationComponent::updateAnimation(float delta_time, float speed_ratio, bool is_moving, bool is_jumping)
    {
        BlendState* active_blend = &m_animation_res.blend_state;
        if (m_animation_res.use_locomotion)
        {
            if (m_animation_fsm.update(is_moving, is_jumping, m_state_phase >= 1.f))
            {
                m_state_phase = 0.f;
            }
            auto&      states = m_animation_res.locomotion_states;
            const auto state  = std::find_if(states.begin(), states.end(), [this](const AnimationStateRes& resource) {
                return resource.name == m_animation_fsm.getStateName();
            });
            if (state == states.end())
            {
                throw std::invalid_argument(std::string("Missing animation state: ") + m_animation_fsm.getStateName());
            }
            updateSpeedWeights(*state, speed_ratio);
            active_blend = &state->blend_state;

            // Shared phase keeps blended walk and run footfalls synchronized.
            float duration     = 0.f;
            float total_weight = 0.f;
            for (int index = 0; index < active_blend->clip_count; ++index)
            {
                const float weight = std::max(active_blend->blend_weight.at(index), 0.f);
                duration += active_blend->blend_clip_file_length.at(index) * weight;
                total_weight += weight;
            }
            if (duration <= 0.f || total_weight <= 0.f)
            {
                throw std::invalid_argument("Locomotion animation needs a positive duration and weight.");
            }
            m_state_phase += std::max(delta_time, 0.f) * total_weight / duration;
            if (m_animation_fsm.isLooping())
            {
                m_state_phase -= std::floor(m_state_phase);
            }
            else
            {
                m_state_phase = std::min(m_state_phase, 1.f);
            }
            active_blend->blend_ratio.assign(active_blend->clip_count, m_state_phase);
        }
        else
        {
            // Legacy resources advance each clip independently at its own duration.
            for (int index = 0; index < active_blend->clip_count; ++index)
            {
                float&      phase    = active_blend->blend_ratio.at(index);
                const float duration = active_blend->blend_clip_file_length.at(index);
                if (duration > 0.f)
                {
                    phase += std::max(delta_time, 0.f) / duration;
                    phase -= std::floor(phase);
                }
            }
        }

        m_skeleton.applyAnimation(AnimationManager::getBlendStateWithClipData(*active_blend));
        m_animation_res.animation_result = m_skeleton.outputAnimationResult();
    }

    const char* AnimationComponent::getAnimationStateName() const { return m_animation_fsm.getStateName(); }

    const AnimationResult& AnimationComponent::getResult() const { return m_animation_res.animation_result; }

    const Skeleton& AnimationComponent::getSkeleton() const { return m_skeleton; }
} // namespace Piccolo
