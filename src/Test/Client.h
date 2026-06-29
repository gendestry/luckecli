#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace Test
{

    struct Client
    {
        struct Engine
        {
            std::string version;
            bool serial_report;
            bool wireless_report;
        };

        struct Wifi
        {
            bool connected = false;
            std::string ip;
            int rssi;
            std::string ssid;
            std::string password;
        };

        struct Fixture
        {
            std::string name;
            std::string type;
        };

        Engine engine;
        Wifi wifi;
        std::vector<Fixture> fixtures;

        std::chrono::system_clock::time_point last_ping;
        std::string last_ping_str = "now";

        Client();
        void updateLastPing();
    };
}