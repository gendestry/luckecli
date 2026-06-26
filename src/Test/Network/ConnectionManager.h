#pragma once
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include "Connection.h"

namespace Test::Network
{

    class ConnectionManager
    {
        struct ConnectionStatus
        {
            Connection conn;
        };
        std::thread connection_countdown_thread;
        std::atomic<bool> running_ccthread{false};
        std::unordered_map<std::string, Connection> connections;
        std::mutex mutex;

        void ccthread_start();
        void ccthread_stop();
        void ccthread();
    };

}