//
// Created by bobi on 7. 05. 26.
//

#pragma once
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <unordered_map>
#include <string>

namespace Test::Utils
{

    class SafeQueue
    {
        std::unordered_map<std::string, std::queue<std::string>> q;
        std::mutex m;
        std::condition_variable cv;

    public:
        void push(std::string ip, std::string resp)
        {
            {
                std::lock_guard lock(m);
                q[ip].push(std::move(resp));
            }
            cv.notify_all();
        }

        void clear(const std::string &ip)
        {
            std::lock_guard lock(m);
            std::queue<std::string> empty;
            std::swap(q[ip], empty);
        }

        std::optional<std::string> pop_for(std::string ip, std::chrono::milliseconds timeout)
        {
            std::unique_lock lock(m);

            if (!cv.wait_for(lock, timeout, [&]
                             { return !q[ip].empty(); }))
                return std::nullopt;

            std::string v = std::move(q[ip].front());
            q[ip].pop();
            return v;
        }
    };

}