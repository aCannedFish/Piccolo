#include "test_checks.h"

#include "runtime/function/animation/animation_system.h"
#include "runtime/function/animation/skeleton.h"
#include "runtime/function/framework/component/animation/animation_component.h"
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

    void testStateTransitions()
    {
        AnimationFSM state;
        require(!state.update(false, false, true), "Idle remains idle after its loop");
        state.update(true, false, false);
        require(std::string(state.getStateName()) == "walk_start", "Movement begins with walk start");
        state.update(true, true, true);
        require(std::string(state.getStateName()) == "jump_start", "Jump has priority over walk completion");
        state.update(true, false, false);
        require(std::string(state.getStateName()) == "jump_end", "Early landing interrupts jump start");
        state.update(false, true, false);
        require(std::string(state.getStateName()) == "jump_start", "A new jump interrupts landing");
        state.update(false, true, true);
        require(std::string(state.getStateName()) == "jump_loop", "Completed takeoff enters flight loop");
        state.update(false, false, false);
        state.update(true, false, true);
        require(std::string(state.getStateName()) == "walk_run", "Moving landing resumes locomotion");
        state.update(false, false, false);
        require(std::string(state.getStateName()) == "walk_stop", "Stopping plays walk stop");
        state.update(true, false, false);
        require(std::string(state.getStateName()) == "walk_start", "Movement interrupts walk stop");
        state.update(false, false, false);
        state.update(false, false, true);
        require(std::string(state.getStateName()) == "idle", "Completed stop returns to idle");
    }

    // Exposes protected resource data for assertions without changing runtime visibility.
    class PlaybackFixture : public AnimationComponent
    {
    public:
        void load(const AnimationComponentRes& resource)
        {
            m_animation_res = resource;
            postLoadResource({});
        }

        const AnimationComponentRes& resource() const { return m_animation_res; }
    };

    AnimationComponentRes loadPlayerAnimation()
    {
        std::ifstream     input("asset/objects/character/player/player.object.json");
        std::stringstream buffer;
        buffer << input.rdbuf();
        std::string error;
        const Json  player = Json::parse(buffer.str(), error);
        require(error.empty(), "Player JSON parses");
        for (const Json& component : player["components"].array_items())
        {
            if (component["$typeName"].string_value() == "AnimationComponent")
            {
                AnimationComponentRes resource;
                Serializer::read(component["$context"]["animation_res"], resource);
                return resource;
            }
        }
        throw std::runtime_error("Player has no animation component.");
    }

    void testLocomotionPlayback()
    {
        PlaybackFixture animation;
        animation.load(loadPlayerAnimation());
        animation.updateAnimation(0.1f, 0.f, false, false);
        require(animation.getResult().node.size() == 58, "Idle produces all shipped skeleton matrices");
        animation.updateAnimation(2.f, 1.f, true, false);
        require(std::string(animation.getAnimationStateName()) == "walk_start", "One-shot holds its last frame");
        animation.updateAnimation(0.f, 1.5f, true, false);
        require(std::string(animation.getAnimationStateName()) == "walk_run", "Walk start completes");
        animation.updateAnimation(0.1f, 1.5f, true, false);
        for (const auto& state : animation.resource().locomotion_states)
        {
            if (state.name == "walk_run")
            {
                requireNear(state.blend_state.blend_weight[0], 0.5f, "Midpoint walk weight");
                requireNear(state.blend_state.blend_weight[1], 0.5f, "Midpoint run weight");
                requireNear(
                    state.blend_state.blend_ratio[0], state.blend_state.blend_ratio[1], "Walk and run share phase");
            }
        }
        animation.updateAnimation(0.7f, 1.5f, true, true);
        animation.updateAnimation(0.f, 1.5f, true, true);
        require(std::string(animation.getAnimationStateName()) == "jump_loop", "Takeoff finishes in the air");
        animation.updateAnimation(4.f, 1.5f, true, true);
        require(std::string(animation.getAnimationStateName()) == "jump_loop", "Flight animation loops");
        animation.updateAnimation(1.f, 0.f, false, false);
        require(std::string(animation.getAnimationStateName()) == "jump_end", "Landing plays the end clip");
        animation.updateAnimation(0.f, 0.f, false, false);
        require(std::string(animation.getAnimationStateName()) == "idle", "Landing completes into idle");
    }

    void testLegacyPlayback()
    {
        auto resource           = loadPlayerAnimation();
        resource.use_locomotion = false;
        PlaybackFixture animation;
        animation.load(resource);
        animation.updateAnimation(resource.blend_state.blend_clip_file_length[0] * 1.25f, 0.f, false, false);
        requireNear(animation.resource().blend_state.blend_ratio[0], 0.25f, "Legacy clip loops independently");
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
        testStateTransitions();
        testLocomotionPlayback();
        testLegacyPlayback();
        std::cout << "Passed 9 animation regression cases.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
