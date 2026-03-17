#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <algorithm>

namespace eb {

enum class BTStatus { Success, Failure, Running };

struct BTNode {
    virtual ~BTNode() = default;
    virtual BTStatus tick(float dt) = 0;
    virtual void reset() {}
};

// ─── Composites ───

struct BTSequence : BTNode {
    std::vector<std::shared_ptr<BTNode>> children;
    int current_child = 0;

    BTStatus tick(float dt) override {
        while (current_child < static_cast<int>(children.size())) {
            BTStatus status = children[current_child]->tick(dt);
            if (status == BTStatus::Failure) {
                current_child = 0;
                return BTStatus::Failure;
            }
            if (status == BTStatus::Running) {
                return BTStatus::Running;
            }
            ++current_child;
        }
        current_child = 0;
        return BTStatus::Success;
    }

    void reset() override {
        current_child = 0;
        for (auto& child : children) child->reset();
    }
};

struct BTSelector : BTNode {
    std::vector<std::shared_ptr<BTNode>> children;
    int current_child = 0;

    BTStatus tick(float dt) override {
        while (current_child < static_cast<int>(children.size())) {
            BTStatus status = children[current_child]->tick(dt);
            if (status == BTStatus::Success) {
                current_child = 0;
                return BTStatus::Success;
            }
            if (status == BTStatus::Running) {
                return BTStatus::Running;
            }
            ++current_child;
        }
        current_child = 0;
        return BTStatus::Failure;
    }

    void reset() override {
        current_child = 0;
        for (auto& child : children) child->reset();
    }
};

enum class BTParallelPolicy { RequireAll, RequireOne };

struct BTParallel : BTNode {
    std::vector<std::shared_ptr<BTNode>> children;
    BTParallelPolicy success_policy = BTParallelPolicy::RequireAll;
    BTParallelPolicy failure_policy = BTParallelPolicy::RequireOne;

    BTStatus tick(float dt) override {
        int success_count = 0;
        int failure_count = 0;

        for (auto& child : children) {
            BTStatus status = child->tick(dt);
            if (status == BTStatus::Success) ++success_count;
            else if (status == BTStatus::Failure) ++failure_count;
        }

        int total = static_cast<int>(children.size());

        if (failure_policy == BTParallelPolicy::RequireOne && failure_count > 0)
            return BTStatus::Failure;
        if (failure_policy == BTParallelPolicy::RequireAll && failure_count >= total)
            return BTStatus::Failure;

        if (success_policy == BTParallelPolicy::RequireOne && success_count > 0)
            return BTStatus::Success;
        if (success_policy == BTParallelPolicy::RequireAll && success_count >= total)
            return BTStatus::Success;

        return BTStatus::Running;
    }

    void reset() override {
        for (auto& child : children) child->reset();
    }
};

// ─── Decorators ───

struct BTInverter : BTNode {
    std::shared_ptr<BTNode> child;

    BTStatus tick(float dt) override {
        if (!child) return BTStatus::Failure;
        BTStatus status = child->tick(dt);
        if (status == BTStatus::Success) return BTStatus::Failure;
        if (status == BTStatus::Failure) return BTStatus::Success;
        return BTStatus::Running;
    }

    void reset() override {
        if (child) child->reset();
    }
};

struct BTRepeater : BTNode {
    std::shared_ptr<BTNode> child;
    int max_repeats = -1; // -1 = forever
    int current_count = 0;

    BTStatus tick(float dt) override {
        if (!child) return BTStatus::Failure;

        BTStatus status = child->tick(dt);
        if (status == BTStatus::Running) return BTStatus::Running;

        ++current_count;
        if (max_repeats > 0 && current_count >= max_repeats) {
            current_count = 0;
            return status;
        }

        child->reset();
        return BTStatus::Running;
    }

    void reset() override {
        current_count = 0;
        if (child) child->reset();
    }
};

struct BTCooldown : BTNode {
    std::shared_ptr<BTNode> child;
    float cooldown_time = 1.0f;
    float timer = 0.0f;

    BTStatus tick(float dt) override {
        if (!child) return BTStatus::Failure;

        if (timer > 0.0f) {
            timer -= dt;
            return BTStatus::Failure;
        }

        BTStatus status = child->tick(dt);
        if (status != BTStatus::Running) {
            timer = cooldown_time;
        }
        return status;
    }

    void reset() override {
        timer = 0.0f;
        if (child) child->reset();
    }
};

// ─── Leaves ───

struct BTAction : BTNode {
    std::function<BTStatus(float)> action;

    BTStatus tick(float dt) override {
        if (action) return action(dt);
        return BTStatus::Failure;
    }
};

struct BTCondition : BTNode {
    std::function<bool()> predicate;

    BTStatus tick(float dt) override {
        (void)dt;
        if (predicate && predicate()) return BTStatus::Success;
        return BTStatus::Failure;
    }
};

// ─── Behavior Tree ───

struct BehaviorTree {
    std::shared_ptr<BTNode> root;

    BTStatus tick(float dt) {
        if (!root) return BTStatus::Failure;
        return root->tick(dt);
    }

    void reset() {
        if (root) root->reset();
    }
};

} // namespace eb
