//
// Created by bobi on 6. 05. 26.
//

#pragma once
#include <memory>
#include <thread>

#include "Diary/Log.h"
#include "SharedState.h"


class Heartbeat {
    std::atomic<bool> running_ingest{false};
    bool inited = false;
    std::thread ingest_thread;

    Log logger;
    SharedState& m_sharedState;

    void start();
    void packet_ingest();

public:
    Heartbeat(SharedState& state);
    ~Heartbeat();

    void stop();
};