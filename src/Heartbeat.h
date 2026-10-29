//
// Created by bobi on 6. 05. 26.
//

#pragma once
#include <thread>
#include <atomic>

// #include "support/log/Log.h"

namespace core
{
    class Heartbeat
    {
        std::atomic<bool> running_ingest{false};
        bool inited = false;
        std::thread ingest_thread;

        void start();
        void packet_ingest();

        // Fire an async "describe" at a freshly-added client and fold the reply
        // back into its state when it arrives (off the ingest thread).
        // void requestDescribe(const std::string &ip);

        // Fire async "getfixture value=presets" requests at a freshly-added
        // client's fixtures. The heartbeat-side handling likely differs from the
        // command path, so for now it just prints the raw responses.
        // void requestPresets(const std::string &ip);

    public:
        Heartbeat();
        // Heartbeat(core::SharedState &state);
        ~Heartbeat();

        void stop();
    };
}