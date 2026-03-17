#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include "engine/core/types.h"

namespace eb {

struct Bone {
    std::string name;
    int parent = -1;
    Vec2 local_pos = {0, 0};
    float local_rotation = 0;
    float local_scale = 1;
    Vec2 world_pos = {0, 0};
    float world_rotation = 0;
    float world_scale = 1;
    std::string sprite_region;
    int sprite_atlas_idx = -1;
};

struct Skeleton {
    std::vector<Bone> bones;

    void update_world_transforms() {
        for (size_t i = 0; i < bones.size(); ++i) {
            Bone& bone = bones[i];
            if (bone.parent < 0) {
                bone.world_pos = bone.local_pos;
                bone.world_rotation = bone.local_rotation;
                bone.world_scale = bone.local_scale;
            } else {
                const Bone& parent = bones[bone.parent];
                float pr = parent.world_rotation;
                float ps = parent.world_scale;
                float cos_r = std::cos(pr);
                float sin_r = std::sin(pr);
                // Transform local position by parent world transform
                bone.world_pos.x = parent.world_pos.x +
                    ps * (cos_r * bone.local_pos.x - sin_r * bone.local_pos.y);
                bone.world_pos.y = parent.world_pos.y +
                    ps * (sin_r * bone.local_pos.x + cos_r * bone.local_pos.y);
                bone.world_rotation = parent.world_rotation + bone.local_rotation;
                bone.world_scale = parent.world_scale * bone.local_scale;
            }
        }
    }

    int find_bone(const std::string& name) {
        for (size_t i = 0; i < bones.size(); ++i) {
            if (bones[i].name == name) return static_cast<int>(i);
        }
        return -1;
    }

    int add_bone(const std::string& name, int parent_idx,
                 Vec2 local_pos, float local_rot) {
        Bone b;
        b.name = name;
        b.parent = parent_idx;
        b.local_pos = local_pos;
        b.local_rotation = local_rot;
        b.local_scale = 1.0f;
        int idx = static_cast<int>(bones.size());
        bones.push_back(b);
        return idx;
    }
};

struct BoneKeyframe {
    float time;
    Vec2 position;
    float rotation;
    float scale;
};

struct BoneTrack {
    std::string bone_name;
    std::vector<BoneKeyframe> keyframes;
};

struct SkeletonAnimation {
    std::string name;
    std::vector<BoneTrack> tracks;
    float duration;
    bool loop = true;
};

struct SkeletonAnimPlayer {
    Skeleton skeleton;
    std::unordered_map<std::string, SkeletonAnimation> animations;
    std::string current;
    float timer = 0;
    bool playing = false;

    void play(const std::string& name) {
        if (animations.find(name) == animations.end()) return;
        current = name;
        timer = 0;
        playing = true;
    }

    void stop() {
        playing = false;
        timer = 0;
    }

    void update(float dt) {
        if (!playing || current.empty()) return;

        auto it = animations.find(current);
        if (it == animations.end()) return;

        SkeletonAnimation& anim = it->second;
        timer += dt;

        if (anim.loop) {
            if (anim.duration > 0) {
                while (timer >= anim.duration) timer -= anim.duration;
            }
        } else {
            if (timer >= anim.duration) {
                timer = anim.duration;
                playing = false;
            }
        }

        // Apply each track to its bone
        for (auto& track : anim.tracks) {
            int bone_idx = skeleton.find_bone(track.bone_name);
            if (bone_idx < 0) continue;
            if (track.keyframes.empty()) continue;

            Bone& bone = skeleton.bones[bone_idx];

            // Find the two keyframes to interpolate between
            const BoneKeyframe* kf_a = &track.keyframes[0];
            const BoneKeyframe* kf_b = &track.keyframes[0];

            for (size_t i = 0; i < track.keyframes.size() - 1; ++i) {
                if (timer >= track.keyframes[i].time &&
                    timer <= track.keyframes[i + 1].time) {
                    kf_a = &track.keyframes[i];
                    kf_b = &track.keyframes[i + 1];
                    break;
                }
            }

            // If past the last keyframe, use the last one
            if (timer >= track.keyframes.back().time) {
                kf_a = &track.keyframes.back();
                kf_b = kf_a;
            }

            // Lerp
            float segment_dur = kf_b->time - kf_a->time;
            float t = 0.0f;
            if (segment_dur > 0.0001f) {
                t = std::clamp((timer - kf_a->time) / segment_dur, 0.0f, 1.0f);
            }

            bone.local_pos.x = kf_a->position.x + (kf_b->position.x - kf_a->position.x) * t;
            bone.local_pos.y = kf_a->position.y + (kf_b->position.y - kf_a->position.y) * t;
            bone.local_rotation = kf_a->rotation + (kf_b->rotation - kf_a->rotation) * t;
            bone.local_scale = kf_a->scale + (kf_b->scale - kf_a->scale) * t;
        }

        skeleton.update_world_transforms();
    }
};

} // namespace eb
