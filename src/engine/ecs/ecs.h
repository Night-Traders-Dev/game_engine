#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <memory>

namespace eb {

using Entity = uint32_t;
constexpr Entity NULL_ENTITY = 0;

// Type-erased sparse set base
class SparseSetBase {
public:
    virtual ~SparseSetBase() = default;
    virtual void remove(Entity e) = 0;
    virtual bool has(Entity e) const = 0;
};

template<typename T>
class SparseSet : public SparseSetBase {
public:
    T& add(Entity e, T component = {}) {
        if (entity_to_index_.count(e)) return dense_data_[entity_to_index_[e]];
        entity_to_index_[e] = (int)dense_data_.size();
        dense_entities_.push_back(e);
        dense_data_.push_back(std::move(component));
        return dense_data_.back();
    }

    void remove(Entity e) override {
        auto it = entity_to_index_.find(e);
        if (it == entity_to_index_.end()) return;
        int idx = it->second;
        int last = (int)dense_data_.size() - 1;
        if (idx != last) {
            dense_data_[idx] = std::move(dense_data_[last]);
            dense_entities_[idx] = dense_entities_[last];
            entity_to_index_[dense_entities_[idx]] = idx;
        }
        dense_data_.pop_back();
        dense_entities_.pop_back();
        entity_to_index_.erase(e);
    }

    bool has(Entity e) const override { return entity_to_index_.count(e) > 0; }

    T& get(Entity e) { return dense_data_[entity_to_index_.at(e)]; }
    const T& get(Entity e) const { return dense_data_[entity_to_index_.at(e)]; }

    int size() const { return (int)dense_data_.size(); }

    void each(std::function<void(Entity, T&)> fn) {
        for (int i = 0; i < (int)dense_data_.size(); i++)
            fn(dense_entities_[i], dense_data_[i]);
    }

private:
    std::vector<T> dense_data_;
    std::vector<Entity> dense_entities_;
    std::unordered_map<Entity, int> entity_to_index_;
};

class World {
public:
    Entity create() { return next_entity_++; }

    void destroy(Entity e) {
        for (auto& [tid, set] : stores_) set->remove(e);
    }

    template<typename T>
    T& add(Entity e, T component = {}) {
        return get_store<T>().add(e, std::move(component));
    }

    template<typename T>
    void remove(Entity e) { get_store<T>().remove(e); }

    template<typename T>
    bool has(Entity e) { return get_store<T>().has(e); }

    template<typename T>
    T& get(Entity e) { return get_store<T>().get(e); }

    template<typename T>
    void each(std::function<void(Entity, T&)> fn) { get_store<T>().each(fn); }

    template<typename T>
    int count() { return get_store<T>().size(); }

private:
    template<typename T>
    SparseSet<T>& get_store() {
        auto tid = std::type_index(typeid(T));
        auto it = stores_.find(tid);
        if (it == stores_.end()) {
            stores_[tid] = std::make_unique<SparseSet<T>>();
            it = stores_.find(tid);
        }
        return *static_cast<SparseSet<T>*>(it->second.get());
    }

    Entity next_entity_ = 1;
    std::unordered_map<std::type_index, std::unique_ptr<SparseSetBase>> stores_;
};

} // namespace eb
