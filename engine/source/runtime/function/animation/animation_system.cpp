#include "runtime/function/animation/animation_system.h"

#include "resource/res_type/data/skeleton_mask.h"

#include "runtime/function/animation/animation_loader.h"
#include "runtime/function/animation/skeleton.h"

#include <algorithm>
#include <stdexcept>

namespace Piccolo
{
    std::map<std::string, std::shared_ptr<SkeletonData>>  AnimationManager::m_skeleton_definition_cache;
    std::map<std::string, std::shared_ptr<AnimationClip>> AnimationManager::m_animation_data_cache;
    std::map<std::string, std::shared_ptr<AnimSkelMap>>   AnimationManager::m_animation_skeleton_map_cache;
    std::map<std::string, std::shared_ptr<BoneBlendMask>> AnimationManager::m_skeleton_mask_cache;

    std::shared_ptr<SkeletonData> AnimationManager::tryLoadSkeleton(std::string file_path)
    {
        std::shared_ptr<SkeletonData> res;
        AnimationLoader               loader;
        auto                          found = m_skeleton_definition_cache.find(file_path);
        if (found == m_skeleton_definition_cache.end())
        {
            res = loader.loadSkeletonData(file_path);
            m_skeleton_definition_cache.emplace(file_path, res);
        }
        else
        {
            res = found->second;
        }
        return res;
    }

    std::shared_ptr<AnimationClip> AnimationManager::tryLoadAnimation(std::string file_path)
    {
        std::shared_ptr<AnimationClip> res;
        AnimationLoader                loader;
        auto                           found = m_animation_data_cache.find(file_path);
        if (found == m_animation_data_cache.end())
        {
            res = loader.loadAnimationClipData(file_path);
            m_animation_data_cache.emplace(file_path, res);
        }
        else
        {
            res = found->second;
        }
        return res;
    }

    std::shared_ptr<AnimSkelMap> AnimationManager::tryLoadAnimationSkeletonMap(std::string file_path)
    {
        std::shared_ptr<AnimSkelMap> res;
        AnimationLoader              loader;
        auto                         found = m_animation_skeleton_map_cache.find(file_path);
        if (found == m_animation_skeleton_map_cache.end())
        {
            res = loader.loadAnimSkelMap(file_path);
            m_animation_skeleton_map_cache.emplace(file_path, res);
        }
        else
        {
            res = found->second;
        }
        return res;
    }

    std::shared_ptr<BoneBlendMask> AnimationManager::tryLoadSkeletonMask(std::string file_path)
    {
        std::shared_ptr<BoneBlendMask> res;
        AnimationLoader                loader;
        auto                           found = m_skeleton_mask_cache.find(file_path);
        if (found == m_skeleton_mask_cache.end())
        {
            res = loader.loadSkeletonMask(file_path);
            m_skeleton_mask_cache.emplace(file_path, res);
        }
        else
        {
            res = found->second;
        }
        return res;
    }

    BlendStateWithClipData AnimationManager::getBlendStateWithClipData(const BlendState& blend_state)
    {

        BlendStateWithClipData result;
        if (blend_state.clip_count <= 0)
        {
            return result;
        }
        const size_t clip_count = static_cast<size_t>(blend_state.clip_count);
        if (blend_state.blend_clip_file_path.size() < clip_count ||
            blend_state.blend_anim_skel_map_path.size() < clip_count ||
            blend_state.blend_mask_file_path.size() < clip_count ||
            blend_state.blend_weight.size() < clip_count ||
            blend_state.blend_ratio.size() < clip_count)
        {
            throw std::invalid_argument("Animation blend arrays must contain clip_count entries.");
        }

        result.clip_count = blend_state.clip_count;
        std::vector<std::shared_ptr<BoneBlendMask>> masks;
        for (size_t clip_index = 0; clip_index < clip_count; ++clip_index)
        {
            result.blend_clip.push_back(*tryLoadAnimation(blend_state.blend_clip_file_path[clip_index]));
            result.blend_anim_skel_map.push_back(
                *tryLoadAnimationSkeletonMap(blend_state.blend_anim_skel_map_path[clip_index]));
            result.blend_ratio.push_back(blend_state.blend_ratio[clip_index]);
            masks.push_back(tryLoadSkeletonMask(blend_state.blend_mask_file_path[clip_index]));
        }

        // Masks refer to skeleton assets. Load that resource through the skeleton cache.
        const auto skeleton = tryLoadSkeleton(masks.front()->skeleton_file_path);
        const size_t bone_count = skeleton->bones_map.size();
        result.blend_weight.resize(clip_count);
        for (size_t clip_index = 0; clip_index < clip_count; ++clip_index)
        {
            if (masks[clip_index]->skeleton_file_path != masks.front()->skeleton_file_path ||
                masks[clip_index]->enabled.size() < bone_count)
            {
                throw std::invalid_argument("Animation masks must reference the same skeleton and cover all bones.");
            }
            result.blend_weight[clip_index].blend_weight.resize(bone_count, 0.f);
        }

        // Normalize per bone after applying masks. Zero total weight preserves the bind pose.
        for (size_t bone_index = 0; bone_index < bone_count; ++bone_index)
        {
            float total_weight = 0.f;
            for (size_t clip_index = 0; clip_index < clip_count; ++clip_index)
            {
                if (masks[clip_index]->enabled[bone_index])
                {
                    const float weight = std::max(blend_state.blend_weight[clip_index], 0.f);
                    result.blend_weight[clip_index].blend_weight[bone_index] = weight;
                    total_weight += weight;
                }
            }
            if (total_weight > 0.f)
            {
                for (size_t clip_index = 0; clip_index < clip_count; ++clip_index)
                {
                    result.blend_weight[clip_index].blend_weight[bone_index] /= total_weight;
                }
            }
        }
        return result;
    }
} // namespace Piccolo
