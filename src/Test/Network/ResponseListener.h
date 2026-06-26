#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <optional>

#include "Diary/Log.h"
#include "Test/Utils/SafeQueue.h"
#include "Test/SharedState.h"

using namespace std::chrono_literals;

namespace Test::Network
{

    class ResponseListener
    {
        Log logger;
        static std::thread listen_thread;
        static std::atomic<bool> running;
        static Utils::SafeQueue<std::string> queue;

        // Signals when the response listener is actually bound and accepting, so
        // the first request can't race the listener's startup (dropped response).
        static std::mutex listen_mtx;
        static std::condition_variable listen_cv;
        static bool listening;

        static bool inited;
        static void listen_ingest_udp();
        static void listen_ingest_tcp();

        // SharedState &m_sharedState;

    public:
        ResponseListener();
        ~ResponseListener();

        static void stop();

    private:
    };

}