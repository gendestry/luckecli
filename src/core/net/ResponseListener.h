#pragma once
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "support/log/Log.h"

using namespace std::chrono_literals;

namespace core
{
    class SharedState; // referenced by reference only; full definition in the .cpp
}

namespace core::net
{

    class ResponseListener
    {
        Log logger;
        std::thread listen_thread;
        std::atomic<bool> running{true};

        // Signals when the response listener is actually bound and accepting, so
        // the first request can't race the listener's startup (dropped response).
        std::mutex listen_mtx;
        std::condition_variable listen_cv;
        bool listening = false;

        bool inited = false;
        void listen_ingest_udp();
        void listen_ingest_tcp();

        core::SharedState &m_sharedState;

    public:
        ResponseListener(core::SharedState &state);
        ~ResponseListener();

        void stop();

    private:
    };

}