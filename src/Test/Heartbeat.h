//
// Created by bobi on 6. 05. 26.
//

#pragma once
#include <memory>
#include <thread>
#include <atomic>

#include "Client.h"
#include <unordered_map>
#include "Diary/Log.h"
#include "SharedState.h"

namespace Test
{
    class Heartbeat
    {
        std::atomic<bool> running_ingest{false};
        bool inited = false;
        std::thread ingest_thread;

        Log logger;
        Test::SharedState &m_sharedState;
        std::unordered_map<std::string, Client> clientMap;

        void start();
        void packet_ingest();

        // Fire an async "describe" at a freshly-added client and fold the reply
        // back into its state when it arrives (off the ingest thread).
        void requestDescribe(const std::string &ip);

    public:
        Heartbeat(Test::SharedState &state);
        ~Heartbeat();

        void stop();
    };
}