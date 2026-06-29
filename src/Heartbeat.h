//
// Created by bobi on 6. 05. 26.
//

#pragma once
#include <thread>
#include <atomic>

#include "Diary/Log.h"

namespace Test
{
    class SharedState; // referenced by reference only; full definition in the .cpp

    class Heartbeat
    {
        std::atomic<bool> running_ingest{false};
        bool inited = false;
        std::thread ingest_thread;

        Log logger;
        Test::SharedState &m_sharedState;

        void start();
        void packet_ingest();

        // Fire an async "describe" at a freshly-added client and fold the reply
        // back into its state when it arrives (off the ingest thread).
        void requestDescribe(const std::string &ip);

        // Fire async "getfixture value=presets" requests at a freshly-added
        // client's fixtures. The heartbeat-side handling likely differs from the
        // command path, so for now it just prints the raw responses.
        void requestPresets(const std::string &ip);

    public:
        Heartbeat(Test::SharedState &state);
        ~Heartbeat();

        void stop();
    };
}