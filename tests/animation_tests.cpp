#include "test_checks.h"

#include "runtime/function/animation/animation_system.h"
#include "runtime/function/animation/skeleton.h"
#include "runtime/function/global/global_context.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/config_manager/config_manager.h"

#include <iostream>
#include <limits>

using namespace Piccolo;

namespace
{
    // Two bones expose both local blending and parent-to-child propagation.
    SkeletonData makeSkeleton()
    {
        SkeletonData data;
        data.is_flat              = true;
        data.in_topological_order = true;
        RawBone root;
        root.index                   = 0;
        root.binding_pose.m_position = Vector3(1.f, 0.f, 0.f);
        RawBone child;
        child.index                   = 1;
        child.parent_index            = 0;
        child.binding_pose.m_position = Vector3(0.f, 1.f, 0.f);
        data.bones_map                = {root, child};
        return data;
    }

    AnimationClip makeClip(float offset)
    {
        AnimationChannel channel;
        channel.position_keys = {Vector3(offset, 0.f, 0.f)};
        channel.scaling_keys  = {Vector3::UNIT_SCALE};
        channel.rotation_keys = {Quaternion::IDENTITY};
        AnimationClip clip;
        clip.node_count    = 1;
        clip.total_frame   = 1;
        clip.node_channels = {channel};
        return clip;
    }

    BlendStateWithClipData makeBlend()
    {
        BlendStateWithClipData blend;
        blend.clip_count  = 2;
        blend.blend_clip  = {makeClip(0.f), makeClip(4.f)};
        blend.blend_ratio = {0.f, 0.f};
        AnimSkelMap mapping;
        mapping.convert           = {0};
        blend.blend_anim_skel_map = {mapping, mapping};
        BoneBlendWeight first;
        first.blend_weight = {1.f, 0.f};
        BoneBlendWeight second;
        second.blend_weight = {3.f, 0.f};
        blend.blend_weight  = {first, second};
        return blend;
    }

    void testWeightedPose()
    {
        Skeleton skeleton;
        skeleton.buildSkeleton(makeSkeleton());
        skeleton.applyAnimation(makeBlend());
        requireNear(skeleton.getBones()[0].getPosition().x, 4.f, "Weighted root position includes bind offset");
        requireNear(skeleton.getBones()[1]._getDerivedPosition().x, 4.f, "Child inherits blended root");
        requireNear(skeleton.getBones()[1]._getDerivedPosition().y, 1.f, "Masked child keeps bind pose");
    }

    void testRotationAndScale()
    {
        Skeleton skeleton;
        skeleton.buildSkeleton(makeSkeleton());
        auto blend                            = makeBlend();
        blend.blend_weight[0].blend_weight[0] = 1.f;
        blend.blend_weight[1].blend_weight[0] = 1.f;
        auto& second                          = blend.blend_clip[1].node_channels[0];
        second.scaling_keys[0]                = Vector3(3.f, 3.f, 3.f);
        second.rotation_keys[0]               = Quaternion(Radian(Degree(90.f)), Vector3::UNIT_Z);
        skeleton.applyAnimation(blend);
        const Vector3 direction = skeleton.getBones()[0].getOrientation() * Vector3::UNIT_X;
        requireNear(direction.x, std::sqrt(0.5f), "Rotation blends to 45 degrees");
        requireNear(direction.y, std::sqrt(0.5f), "Rotation takes shortest path");
        requireNear(skeleton.getBones()[0].getScale().x, 2.f, "Scale blends linearly");

        second.rotation_keys[0] = -Quaternion::IDENTITY;
        skeleton.applyAnimation(blend);
        requireNear(std::abs(skeleton.getBones()[0].getOrientation().w), 1.f, "Opposite signs preserve rotation");
    }

    void testIndependentTracks()
    {
        Skeleton skeleton;
        skeleton.buildSkeleton(makeSkeleton());
        auto blend           = makeBlend();
        blend.clip_count     = 1;
        blend.blend_ratio[0] = 0.5f;
        auto& clip           = blend.blend_clip[0];
        clip.total_frame     = 3;
        clip.node_count      = 2;
        clip.node_channels[0].position_keys.clear();
        clip.node_channels[0].rotation_keys.clear();
        AnimationChannel child;
        child.position_keys = {Vector3::ZERO, Vector3(0.f, 2.f, 0.f), Vector3(0.f, 4.f, 0.f)};
        clip.node_channels.push_back(child);
        blend.blend_anim_skel_map[0].convert = {0, 1};
        blend.blend_weight[0].blend_weight   = {1.f, 1.f};
        skeleton.applyAnimation(blend);
        requireNear(skeleton.getBones()[0].getPosition().x, 1.f, "Empty position track uses zero offset");
        requireNear(skeleton.getBones()[1].getPosition().y, 3.f, "Short root track does not clamp child time");
        requireNear(skeleton.getBones()[1].getScale().x, 1.f, "Empty scale track uses unit scale");
        blend.blend_ratio[0] = 1.f;
        skeleton.applyAnimation(blend);
        requireNear(skeleton.getBones()[1].getPosition().y, 5.f, "Final frame is sampled exactly");
    }

    void testZeroWeightAndInvalidMapping()
    {
        Skeleton skeleton;
        skeleton.buildSkeleton(makeSkeleton());
        auto blend                            = makeBlend();
        blend.blend_weight[0].blend_weight[0] = 0.f;
        blend.blend_weight[1].blend_weight[0] = 0.f;
        skeleton.applyAnimation(blend);
        requireNear(skeleton.getBones()[0].getPosition().x, 1.f, "Zero weights preserve bind pose");
        blend.blend_weight[1].blend_weight[0]   = 1.f;
        blend.blend_anim_skel_map[1].convert[0] = std::numeric_limits<int>::max();
        skeleton.applyAnimation(blend);
        requireNear(skeleton.getBones()[0].getPosition().x, 1.f, "Unmapped nodes are skipped");
        skeleton.applyAnimation(BlendStateWithClipData());
        requireNear(skeleton.getBones()[0].getPosition().x, 1.f, "Empty blend preserves bind pose");
    }

    void testSkeletonRebuild()
    {
        Skeleton skeleton;
        skeleton.buildSkeleton(makeSkeleton());
        SkeletonData unsupported;
        skeleton.buildSkeleton(unsupported);
        require(skeleton.getBonesCount() == 0 && skeleton.getBones() == nullptr,
                "Rejected skeleton releases old bones");
        skeleton.applyAnimation(makeBlend());
        skeleton.buildSkeleton(makeSkeleton());
        require(skeleton.getBonesCount() == 2, "Skeleton can rebuild after rejection");
    }

    void testResourceWeights()
    {
        // Load real shipped data without starting the renderer or the world.
        g_runtime_global_context.m_config_manager = std::make_shared<ConfigManager>();
        g_runtime_global_context.m_asset_manager  = std::make_shared<AssetManager>();
        const std::string prefix                  = "asset/objects/character/player/components/animation/data/";
        BlendState        blend;
        blend.clip_count               = 2;
        const std::string clip         = prefix + "W2_CrouchWalk_Aim_F_Loop_IP.animation_clip.json";
        blend.blend_clip_file_path     = {clip, clip};
        blend.blend_anim_skel_map_path = {prefix + "anim.skeleton_map.json", prefix + "anim.skeleton_map.json"};
        blend.blend_mask_file_path     = {prefix + "anim.skeleton_mask.json", prefix + "anim.skeleton_mask.json"};
        blend.blend_weight             = {1.f, 3.f};
        blend.blend_ratio              = {0.f, 0.f};
        auto result                    = AnimationManager::getBlendStateWithClipData(blend);
        requireNear(result.blend_weight[0].blend_weight[1], 0.25f, "First weight is normalized");
        requireNear(result.blend_weight[1].blend_weight[1], 0.75f, "Second weight is normalized");
        blend.blend_weight = {0.f, 0.f};
        result             = AnimationManager::getBlendStateWithClipData(blend);
        requireNear(result.blend_weight[0].blend_weight[1], 0.f, "Zero total weight does not divide by zero");
    }
} // namespace

int main()
{
    try
    {
        testWeightedPose();
        testRotationAndScale();
        testIndependentTracks();
        testZeroWeightAndInvalidMapping();
        testSkeletonRebuild();
        testResourceWeights();
        std::cout << "Passed 6 animation regression cases.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
