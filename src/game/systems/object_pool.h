#pragma once

#include <vector>
#include <functional>
#include <cassert>

namespace eb {

// ─── Object Pool ───

template<typename T>
class ObjectPool {
    std::vector<T> objects;
    std::vector<int> free_list;
    int active_count = 0;

public:
    T& acquire() {
        int idx;
        if (!free_list.empty()) {
            idx = free_list.back();
            free_list.pop_back();
            objects[idx] = T{};
        } else {
            idx = static_cast<int>(objects.size());
            objects.push_back(T{});
        }
        objects[idx].active = true;
        active_count++;
        return objects[idx];
    }

    void release(int index) {
        if (index < 0 || index >= static_cast<int>(objects.size())) return;
        if (!objects[index].active) return;
        objects[index].active = false;
        free_list.push_back(index);
        active_count--;
    }

    void for_each_active(std::function<void(T&, int)> fn) {
        for (int i = 0; i < static_cast<int>(objects.size()); i++) {
            if (objects[i].active) {
                fn(objects[i], i);
            }
        }
    }

    void clear() {
        for (int i = 0; i < static_cast<int>(objects.size()); i++) {
            if (objects[i].active) {
                objects[i].active = false;
                free_list.push_back(i);
            }
        }
        active_count = 0;
    }

    int size() const { return active_count; }
    int capacity() const { return static_cast<int>(objects.size()); }
    T& operator[](int index) { return objects[index]; }
    const T& operator[](int index) const { return objects[index]; }
};

} // namespace eb
