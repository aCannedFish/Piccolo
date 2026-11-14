#include "runtime/function/animation/skeleton.h"

#include "runtime/core/math/math.h"

#include <algorithm>
#include <cmath>

namespace Piccolo
{
    Skeleton::~Skeleton() { delete[] m_bones; }

    void Skeleton::resetSkeleton()
    {
        for (size_t i = 0; i < m_bone_count; i++)
            m_bones[i].resetToInitialPose();
    }

    void Skeleton::buildSkeleton(const SkeletonData& skeleton_definition)
    {
        m_is_flat = skeleton_definition.is_flat;
        delete[] m_bones;
        m_bones = nullptr;
        m_bone_count = 0;
        if (!m_is_flat || !skeleton_definition.in_topological_order)
        {
            return;
        }
        m_bone_count = skeleton_definition.bones_map.size();
        m_bones      = new Bone[m_bone_count];
        for (size_t i = 0; i < m_bone_count; i++)
        {
            const RawBone bone_definition = skeleton_definition.bones_map[i];
            Bone* parent_bone = nullptr;
            if (bone_definition.parent_index >= 0 && bone_definition.parent_index < static_cast<int>(i))
            {
                parent_bone = &m_bones[bone_definition.parent_index];
            }
            m_bones[i].initialize(std::make_shared<RawBone>(bone_definition), parent_bone);
        }
    }

    namespace
    {
        // Each track clamps its frame independently. Empty tracks supply an identity offset.
        template<typename Value, typename Interpolator>
        Value sampleTrack(const std::vector<Value>& keys,
                          float frame,
                          const Value& fallback,
                          Interpolator interpolate)
        {
            if (keys.empty())
            {
                return fallback;
            }
            const float track_frame = std::min(frame, static_cast<float>(keys.size() - 1));
            const size_t lower = static_cast<size_t>(std::floor(track_frame));
            const size_t upper = std::min(lower + 1, keys.size() - 1);
            return interpolate(keys[lower], keys[upper], track_frame - static_cast<float>(lower));
        }
    } // namespace

    void Skeleton::applyAnimation(const BlendStateWithClipData& blend_state)
    {
        if (!m_bones)
        {
            return;
        }
        resetSkeleton();

        std::vector<Vector3> positions(m_bone_count, Vector3::ZERO);
        std::vector<Vector3> scales(m_bone_count, Vector3::ZERO);
        std::vector<Quaternion> rotations(m_bone_count, Quaternion(0.f, 0.f, 0.f, 0.f));
        std::vector<Quaternion> reference_rotations(m_bone_count, Quaternion::IDENTITY);
        std::vector<float> total_weights(m_bone_count, 0.f);

        const size_t clip_count = std::min({static_cast<size_t>(std::max(blend_state.clip_count, 0)),
                                           blend_state.blend_clip.size(),
                                           blend_state.blend_ratio.size(),
                                           blend_state.blend_anim_skel_map.size(),
                                           blend_state.blend_weight.size()});
        for (size_t clip_index = 0; clip_index < clip_count; ++clip_index)
        {
            const AnimationClip& clip = blend_state.blend_clip[clip_index];
            if (clip.total_frame <= 0)
            {
                continue;
            }
            const float phase = std::clamp(blend_state.blend_ratio[clip_index], 0.f, 1.f);
            const float frame = phase * static_cast<float>(clip.total_frame - 1);
            const auto& mapping = blend_state.blend_anim_skel_map[clip_index].convert;
            const auto& weights = blend_state.blend_weight[clip_index].blend_weight;
            const size_t channel_count = std::min({static_cast<size_t>(std::max(clip.node_count, 0)),
                                                  clip.node_channels.size(), mapping.size()});
            for (size_t channel_index = 0; channel_index < channel_count; ++channel_index)
            {
                const size_t bone_index = mapping[channel_index];
                if (bone_index >= static_cast<size_t>(m_bone_count) || bone_index >= weights.size())
                {
                    continue;
                }
                const float weight = weights[bone_index];
                if (weight <= 0.f)
                {
                    continue;
                }

                const AnimationChannel& channel = clip.node_channels[channel_index];
                const Vector3 position = sampleTrack(channel.position_keys, frame, Vector3::ZERO, Vector3::lerp);
                const Vector3 scale = sampleTrack(channel.scaling_keys, frame, Vector3::UNIT_SCALE, Vector3::lerp);
                Quaternion rotation = sampleTrack(
                    channel.rotation_keys, frame, Quaternion::IDENTITY,
                    [](const Quaternion& from, const Quaternion& to, float ratio)
                    {
                        return Quaternion::nLerp(ratio, from, to, true);
                    });

                // Equivalent quaternions q and -q must share a hemisphere before summation.
                if (total_weights[bone_index] == 0.f)
                {
                    reference_rotations[bone_index] = rotation;
                }
                if (reference_rotations[bone_index].dot(rotation) < 0.f)
                {
                    rotation = -rotation;
                }
                positions[bone_index] += position * weight;
                scales[bone_index] += scale * weight;
                rotations[bone_index] = rotations[bone_index] + rotation * weight;
                total_weights[bone_index] += weight;
            }
        }

        // Apply each blended offset once, then propagate transforms in parent-first order.
        // Bones with no contributing animation retain their bind pose.
        for (size_t bone_index = 0; bone_index < static_cast<size_t>(m_bone_count); ++bone_index)
        {
            const float weight = total_weights[bone_index];
            if (weight > 0.f)
            {
                Quaternion rotation = rotations[bone_index];
                rotation.normalise();
                m_bones[bone_index].rotate(rotation);
                m_bones[bone_index].scale(scales[bone_index] / weight);
                m_bones[bone_index].translate(positions[bone_index] / weight);
            }
            m_bones[bone_index].update();
        }
    }

    AnimationResult Skeleton::outputAnimationResult()
    {
        AnimationResult animation_result;
        for (size_t i = 0; i < m_bone_count; i++)
        {
            std::shared_ptr<AnimationResultElement> animation_result_element =
                std::make_shared<AnimationResultElement>();
            Bone* bone                      = &m_bones[i];
            animation_result_element->index = bone->getID() + 1;
            Vector3 temp_translation        = bone->_getDerivedPosition();

            // TODO: the unit of the joint matrices is wrong
            Vector3 temp_scale = bone->_getDerivedScale();

            Quaternion temp_rotation = bone->_getDerivedOrientation();

            // auto scale = bone->_getDerivedTScale();
            // scale.x = 1.f / scale.x;
            // scale.y = 1.f / scale.y;
            // scale.z = 1.f / scale.z;

            // auto revTmat = getMatrix(
            //	-bone->_getDerivedTPosition(),
            //	scale,
            //	conjugate( bone->_getDerivedTOrientation())
            //);
            auto objMat =
                Transform(bone->_getDerivedPosition(), bone->_getDerivedOrientation(), bone->_getDerivedScale())
                    .getMatrix();

            auto resMat = objMat * bone->_getInverseTpose();

            animation_result_element->transform = resMat.toMatrix4x4_();

            animation_result.node.push_back(*animation_result_element);
        }
        return animation_result;
    }

    const Bone* Skeleton::getBones() const
    {
        return m_bones;
    }

    int32_t Skeleton::getBonesCount() const
    {
        return m_bone_count;
    }
} // namespace Piccolo