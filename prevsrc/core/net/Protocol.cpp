#include "core/net/Protocol.h"

#include <nlohmann/json.hpp>

namespace core::net
{
    using json = nlohmann::json;

    namespace
    {
        // Protocol 1 report-task commands and their protocol 2 settings keys.
        std::optional<std::string> settingKey(const std::string &v1Cmd)
        {
            if (v1Cmd == "serial_report_task")
                return "serial_report";
            if (v1Cmd == "wireless_report_task")
                return "wireless_report";
            if (v1Cmd == "wifi_animation")
                return "wifi_animation";
            return std::nullopt;
        }

        // Value of setting `key` in an output's settings list, or `fallback`.
        json settingValue(const json &output, const std::string &key, json fallback = nullptr)
        {
            for (const auto &s : output.value("settings", json::array()))
                if (s.value("key", "") == key)
                    return s.value("value", fallback);
            return fallback;
        }

        // Protocol 2 fixture object -> protocol 1 (camelCase preset fields).
        json fixtureToV1(json f)
        {
            if (!f.is_object())
                return f;
            if (f.contains("preset_index"))
                f["presetIndex"] = f["preset_index"];
            if (f.contains("num_presets"))
                f["numPresets"] = f["num_presets"];
            f.erase("preset_index");
            f.erase("num_presets");
            f.erase("outputs");
            f.erase("presets");
            return f;
        }

        // Protocol 1 "config" outputs: one flat object per output.
        json configOutputs(const json &outputs)
        {
            json out = json::array();
            for (const auto &o : outputs)
                out.push_back({{"uuid", o.value("index", 0)},
                               {"type", o.value("type", "")},
                               {"hardware_pin", o.value("hardware_pin", 0)},
                               {"num_leds", settingValue(o, "num_leds", 0)},
                               {"num_groups", settingValue(o, "num_groups", 0)}});
            return out;
        }

        json getFixtureToV1(const std::string &value, const json &f)
        {
            if (value.empty())
                return fixtureToV1(f);

            json val;
            if (value == "presets")
            {
                json groups = json::array();
                for (const auto &p : f.value("presets", json::array()))
                    groups.push_back(p);
                val = {{"selected", f.value("preset_index", 0)}, {"presets", {{"groups", groups}}}};
            }
            else if (value == "outputs")
            {
                val = json::array();
                for (const auto &o : f.value("outputs", json::array()))
                    val.push_back({{"name", o.value("name", "")},
                                   {"footprint", o.value("footprints", json::array())},
                                   {"hardware_pin", o.value("hardware_pin", 0)},
                                   {"num_leds", settingValue(o, "num_leds", 0)}});
            }
            else if (value == "config")
            {
                val = fixtureToV1(f);
                val["outputs"] = configOutputs(f.value("outputs", json::array()));
            }
            else if (value == "preset")
                val = f.value("preset_index", 0);
            else
                val = f.value(value, json());
            return {{"req", value}, {"val", val}};
        }

        json describeToV1(const json &d)
        {
            json out = d;
            const json settings = d.value("settings", json::object());
            out["serial_report_task"] = settings.value("serial_report", false);
            out["wireless_report_task"] = settings.value("wireless_report", false);
            out["wifi_animation"] = settings.value("wifi_animation", false);
            out.erase("settings");

            const json wifi = d.value("wifi", json::object());
            out["ssid"] = wifi.value("ssid", "");

            json fixtures = json::array();
            for (const auto &f : d.value("fixtures", json::array()))
                fixtures.push_back(fixtureToV1(f));
            out["fixtures"] = {{"fixtures", fixtures}};

            const json inputs = d.value("inputs", json::array());
            out["inputs"] = {{"num_inputs", inputs.size()}, {"inputs", inputs}};
            return out;
        }
    }

    std::string Protocol::toV2(const std::string &v1Request)
    {
        json r = json::parse(v1Request, nullptr, false);
        if (!r.is_object() || !r.contains("request") || !r["request"].is_string())
            return v1Request;

        const std::string cmd = r["request"];
        r.erase("request");
        json out = {{"cmd", cmd}};

        if (cmd == "getfixture")
        {
            if (!r.contains("id"))
                return json{{"cmd", "fixtures"}}.dump();
            out["cmd"] = "get_fixture";
            out["id"] = r["id"];
            const std::string value = r.value("value", "");
            if (value == "presets")
                out["include"] = {"presets"};
            else if (value == "outputs" || value == "config")
                out["include"] = {"outputs"};
        }
        else if (cmd == "setfixture")
        {
            out["cmd"] = "set_fixture";
            for (auto &[k, v] : r.items())
            {
                if (k == "presetIndex")
                    out["preset_index"] = v;
                else if (k == "leds")
                {
                    // Protocol 1 strips had a single output; num_leds is its setting.
                    out["output"] = 0;
                    out["settings"] = {{"num_leds", v}};
                }
                else
                    out[k] = v;
            }
        }
        else if (cmd == "wifi")
        {
            const bool write = r.contains("ssid") || r.contains("password");
            out["cmd"] = write ? "set_wifi" : "get_wifi";
            if (write)
            {
                out.update(r);
                // The CLI has already asked the user; protocol 2 wants it explicit.
                out["confirm"] = true;
            }
        }
        else if (cmd == "factoryreset" || cmd == "factory_reset")
        {
            out["cmd"] = "factory_reset";
            out["confirm"] = true;
        }
        else if (auto key = settingKey(cmd))
        {
            if (r.contains("value"))
            {
                out["cmd"] = "set_settings";
                out[*key] = r["value"];
            }
            else
                out["cmd"] = "get_settings";
        }
        else
            out.update(r); // describe, fixtures, inputs, reboot, ota, logs: same args

        return out.dump();
    }

    std::optional<std::string> Protocol::fromV2(const std::string &v1Request,
                                                const std::optional<std::string> &v2Reply)
    {
        if (!v2Reply)
            return v2Reply;

        const json reply = json::parse(*v2Reply, nullptr, false);
        if (!reply.is_object() || !reply.contains("ok"))
            return v2Reply; // not an envelope (or a protocol 1 reply): pass through

        if (!reply.value("ok", false))
        {
            const json err = reply.value("err", json::object());
            std::string msg = err.is_object() ? err.value("msg", err.value("code", "error")) : err.dump();
            return json{{"err", msg}}.dump();
        }

        const json req = json::parse(v1Request, nullptr, false);
        const std::string cmd = req.is_object() ? req.value("request", "") : "";
        const json data = reply.value("data", json::object());
        const bool rebooting = reply.value("rebooting", false);

        json out;
        if (cmd == "describe")
            out = describeToV1(data);
        else if (cmd == "fixtures" || (cmd == "getfixture" && !req.contains("id")))
        {
            json fixtures = json::array();
            for (const auto &f : data)
                fixtures.push_back(fixtureToV1(f));
            out = {{"fixtures", fixtures}};
        }
        else if (cmd == "getfixture")
            out = getFixtureToV1(req.value("value", ""), data);
        else if (cmd == "setfixture")
        {
            out = fixtureToV1(data.value("fixture", json::object()));
            out["status"] = rebooting ? "ok, rebooting" : data.value("changed", false) ? "ok" : "no changes";
        }
        else if (cmd == "inputs")
            out = {{"num_inputs", data.size()}, {"inputs", data}};
        else if (cmd == "wifi")
        {
            out = data;
            if (req.contains("ssid") || req.contains("password"))
                out["status"] = "ok";
        }
        else if (auto key = settingKey(cmd))
            out = {{"req", cmd}, {"val", data.value(*key, false)}};
        else if (cmd == "reboot")
            out = {{"status", "rebooting"}};
        else if (cmd == "factoryreset" || cmd == "factory_reset")
            out = {{"status", "factory reset, rebooting"}};
        else if (cmd == "ota")
        {
            out = data;
            out["status"] = "ota open";
        }
        else
            out = data; // logs and anything new: data is already the protocol 1 shape

        return out.dump();
    }

}
