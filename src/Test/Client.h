#pragma once
#include <functional>
#include <list>
#include <string>
#include <mutex>
#include <optional>
#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <format>

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

        Client()
        {
            last_ping = std::chrono::system_clock::now();
        }

        void updateLastPing()
        {
            auto now = std::chrono::system_clock::now();
            auto duration = now - last_ping;

            // Convert to seconds or milliseconds (optional)
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
            auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

            last_ping_str = std::format("{} ms", milliseconds);
            last_ping = now;
            // std::cout << "Last ping was " << seconds << " seconds ago.\n";
        }
    };
}