#pragma once
#include <nlohmann/json.hpp>
#include "Client.h"
#include <iostream>

namespace Test::Backwards
{
    // Per-command parsers for engine v0.98 responses.
    //
    // One static function per command whose response shape is version-specific.
    // Add more (setwifi, setfixture, status, ...) alongside describe() as they
    // diverge between firmware versions.
    struct V98
    {
        // describe response (v0.98): flat top-level fields (no envelope, no
        // "engine" nesting like v0.96), with fixtures nested under
        // "fixtures":{"fixtures":[...]}.
        //   {
        //     "version": "0.98", "serial_report_task": bool,
        //     "wireless_report_task": bool, "wifi_animation": bool,
        //     "ssid": str, "password": str,
        //     "wifi": { "rssi": int, "local_ip": str, "connected": bool },
        //     "fixtures": { "fixtures": [{ id, name, type, universe, address,
        //                                  presetIndex, numPresets }] },
        //     "inputs": { "num_inputs": int, "inputs": [...] }
        //   }
        //
        // Populates `client` in place; existing values are kept as defaults so a
        // partial response never clobbers known data.
        static void describe(const nlohmann::json &data, Test::Client &client)
        {
            client.engine.version = data.value("version", client.engine.version);
            client.engine.serial_report = data.value("serial_report_task", false);
            client.engine.wireless_report = data.value("wireless_report_task", false);

            if (data.contains("wifi"))
            {
                const auto &wifi = data["wifi"];
                client.wifi.connected = wifi.value("connected", false);
                client.wifi.ip = wifi.value("local_ip", client.wifi.ip);
                client.wifi.rssi = wifi.value("rssi", 0);
            }

            client.wifi.ssid = data.value("ssid", client.wifi.ssid);
            client.wifi.password = data.value("password", client.wifi.password);

            client.fixtures.clear();
            if (data.contains("fixtures"))
            {
                for (const auto &f : data["fixtures"].value("fixtures", nlohmann::json::array()))
                {
                    Test::Client::Fixture fix;
                    fix.id = f.value("id", 0);
                    fix.name = f.value("name", "");
                    fix.type = f.value("type", "");
                    fix.universe = f.value("universe", 0);
                    fix.address = f.value("address", 0);
                    fix.presetIndex = f.value("presetIndex", 0);
                    fix.numPresets = f.value("numPresets", 0);
                    fix.footprint = f.value("footprint", 0);
                    client.fixtures.push_back(std::move(fix));
                }
            }
        }

        // presets response (getfixture value=presets):
        //   { "val": { "selected": int,
        //       "presets": { "groups": [{ "name": str, "settings": [...] }] } } }
        // Fills `fix.presets` with the group names.
        static void presets(const nlohmann::json &data, Test::Client::Fixture &fix)
        {
            const auto val = data.value("val", nlohmann::json::object());
            const auto presetsObj = val.value("presets", nlohmann::json::object());

            fix.presets.clear();
            for (const auto &group : presetsObj.value("groups", nlohmann::json::array()))
                fix.presets.push_back(group.value("name", "unnamed"));
        }
    };
}
