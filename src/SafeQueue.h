//
// Created by bobi on 7. 05. 26.
//

#pragma once
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template<typename T>
class SafeQueue {
    std::queue<T> q;
    std::mutex m;
    std::condition_variable cv;

public:
    void push(T v) {
        {
            std::lock_guard lock(m);
            q.push(std::move(v));
        }
        cv.notify_one();
    }

    void clear() {
        std::lock_guard lock(m);
        std::queue<T> empty;
        std::swap(q, empty);
    }

    std::optional<T> pop_for(std::chrono::milliseconds timeout) {
        std::unique_lock lock(m);

        if (!cv.wait_for(lock, timeout, [&]{ return !q.empty(); }))
            return std::nullopt;

        T v = std::move(q.front());
        q.pop();
        return v;
    }
};
