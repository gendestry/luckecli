#include "Client.h"
#include <format>

namespace Test
{
    Client::Client()
    {
        last_ping = std::chrono::system_clock::now();
    }

    // void Client::connect()
    // {
    //     connection = std::make_shared<Test::Network::Connection>(wifi.ip, )
    // }

    void Client::updateLastPing()
    {
        auto now = std::chrono::system_clock::now();
        auto duration = now - last_ping;

        // Convert to seconds or milliseconds (optional)
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        last_ping_str = std::format("{} ms", milliseconds);
        last_ping = now;
    }
}