#pragma once
#include <nlohmann/json.hpp>
#include "core/domain/Client.h"

namespace core::legacy
{
    // Per-command parsers for engine v0.96 responses.
    //
    // v0.96 differs from v0.98 in two ways:
    //   1. the payload is wrapped in {"status":"OK","response":"<json string>"}
    //      where "response" is an escaped JSON string that must be parsed again;
    //   2. the real data is nested under "engine", split into "settings",
    //      "wifi" and "fixture_handler".
    struct V96
    {
        // Unwrap the {"status","response"} envelope: "response" is itself a
        // JSON-encoded string. Returns `outer` unchanged if there's no string
        // "response" field or it fails to parse — Execute's try/catch only
        // covers the outer parse, so this guards the inner one.
        static nlohmann::json unwrap(const nlohmann::json &outer)
        {
            if (outer.contains("response") && outer["response"].is_string())
            {
                try
                {
                    return nlohmann::json::parse(outer["response"].get<std::string>());
                }
                catch (const nlohmann::json::parse_error &)
                {
                    return outer;
                }
            }
            return outer;
        }

        // describe response (v0.96):
        //   {"status":"OK","response":"{
        //     \"engine\": {
        //       \"settings\": { version, serial_report_task, wireless_report_task,
        //                       wifi_animation, ssid, password },
        //       \"wifi\": { rssi, local_ip, connected },
        //       \"fixture_handler\": { num_fixtures, fixtures:[{id,name,type,...}] },
        //       \"input_handler\": { ... }
        //     }
        //   }"}
        static void describe(const nlohmann::json &outer, core::Client &client)
        {
            const nlohmann::json data = unwrap(outer);

            const auto engine = data.value("engine", nlohmann::json::object());
            const auto settings = engine.value("settings", nlohmann::json::object());

            client.engine.version = settings.value("version", client.engine.version);
            client.engine.serial_report = settings.value("serial_report_task", false);
            client.engine.wireless_report = settings.value("wireless_report_task", false);
            client.engine.wifi_animation = settings.value("wifi_animation", false);

            // In v0.96 ssid/password live under engine.settings (not under wifi).
            client.wifi.ssid = settings.value("ssid", client.wifi.ssid);
            client.wifi.password = settings.value("password", client.wifi.password);

            if (engine.contains("wifi"))
            {
                const auto &wifi = engine["wifi"];
                client.wifi.connected = wifi.value("connected", false);
                client.wifi.ip = wifi.value("local_ip", client.wifi.ip);
                client.wifi.rssi = wifi.value("rssi", 0);
            }

            client.fixtures.clear();
            if (engine.contains("fixture_handler"))
            {
                for (const auto &f : engine["fixture_handler"].value("fixtures", nlohmann::json::array()))
                {
                    core::Client::Fixture fix;
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
        //   {"status":"OK","response":"{ \"val\": { \"selected\": int,
        //       \"presets\": { \"groups\": [{ \"name\": str, \"settings\": [...] }] } } }"}
        // Fills `fix.presets` with the group names.
        static void presets(const nlohmann::json &outer, core::Client::Fixture &fix)
        {
            const nlohmann::json data = unwrap(outer);

            const auto val = data.value("val", nlohmann::json::object());
            const auto presetsObj = val.value("presets", nlohmann::json::object());

            fix.presetIndex = val.value("selected", fix.presetIndex);

            fix.presets.clear();
            for (const auto &group : presetsObj.value("groups", nlohmann::json::array()))
                fix.presets.push_back(group.value("name", "unnamed"));
            fix.numPresets = static_cast<int>(fix.presets.size());
        }
    };
}
