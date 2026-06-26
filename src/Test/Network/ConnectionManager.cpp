#include "ConnectionManager.h"

namespace Test::Network
{

    void ConnectionManager::ccthread_start()
    {
        running_ccthread = true;
        connection_countdown_thread = std::thread(&ConnectionManager::ccthread, this);
    }

    void ConnectionManager::ccthread_stop()
    {
        running_ccthread = false;
        if (connection_countdown_thread.joinable())
        {
            connection_countdown_thread.join();
        }
    }

    void ConnectionManager::ccthread()
    {
    }
}