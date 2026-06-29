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
            int id = 0;
            std::string name;
            std::string type;
            int universe = 0;
            int address = 0;
            int presetIndex = 0;
            int numPresets = 0;
            int footprint = 0;
            std::vector<std::string> presets; // preset/group names, fetched on demand
        };

        Engine engine;
        Wifi wifi;
        std::vector<Fixture> fixtures;

        std::chrono::system_clock::time_point last_ping;
        std::string last_ping_str = "now";

        Client();
        void updateLastPing();

        // Themed, multi-line summary of engine/wifi/fixtures (uses Test/Theme.h).
        std::string toString() const;
    };
}