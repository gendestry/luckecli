#pragma once
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

#include "Client.h"
#include "ESPClient.h"
#include "Utils/JSONtemp.h"
#include "v96.h"
#include "v98.h"

namespace Test::Backwards
{
    enum class Version
    {
        V96,
        V98
    };

    // Backward-compatibility router.
    //
    // Each engine firmware version answers the same command with a slightly
    // different JSON shape. The per-version field mapping lives in v96.h / v98.h
    // (V96::describe, V98::describe, ...); this class only decides which one to
    // call, keyed off the version already stored on the Client
    // (Client::engine.version).
    class Execute
    {
        static Version versionOf(const std::string &version)
        {
            if (version.starts_with("0.96"))
                return Version::V96;
            return Version::V98; // 0.98 and anything newer/unknown
        }

    public:
        // Parse a raw describe response into `client`, dispatching on the
        // client's known engine version. Pure parse, no network — call
        // fetchPresets() afterwards to fill in presets. Returns false on empty
        // input, a JSON parse error, or an explicit {"err": ...} payload.
        static bool describe(const std::optional<std::string> &resp, Test::Client &client)
        {
            if (!resp || resp->empty())
                return false;

            nlohmann::json data;
            try
            {
                data = nlohmann::json::parse(*resp);
            }
            catch (const nlohmann::json::parse_error &)
            {
                return false;
            }

            if (data.contains("err"))
                return false;

            switch (versionOf(client.engine.version))
            {
            case Version::V96:
                V96::describe(data, client);
                break;
            case Version::V98:
                V98::describe(data, client);
                break;
            }
            return true;
        }

        // Fetch every fixture's presets over `conn` and merge them into `client`
        // (presets aren't part of the describe payload — one getfixture request
        // per fixture). Does network I/O, so callers must NOT hold the
        // SharedState lock; operate on a local Client and commit it afterwards.
        static void fetchPresets(Test::Client &client, ESPClient &conn)
        {
            for (auto &fix : client.fixtures)
            {
                auto pr = conn.sendRequestOpt(
                    Utils::JSONtemp::stringify("getfixture", "id", fix.id, "value", "presets"), 4000ms);
                if (pr)
                    presets(pr, client, fix);
            }
        }

        // Parse a raw "getfixture value=presets" response into `fix.presets`,
        // dispatching on `client`'s engine version. Returns false on empty
        // input, a JSON parse error, or an {"err": ...} payload.
        static bool presets(const std::optional<std::string> &resp, const Test::Client &client,
                            Test::Client::Fixture &fix)
        {
            if (!resp || resp->empty())
                return false;

            nlohmann::json data;
            try
            {
                data = nlohmann::json::parse(*resp);
            }
            catch (const nlohmann::json::parse_error &)
            {
                return false;
            }

            if (data.contains("err"))
                return false;

            switch (versionOf(client.engine.version))
            {
            case Version::V96:
                V96::presets(data, fix);
                break;
            case Version::V98:
                V98::presets(data, fix);
                break;
            }
            return true;
        }
    };
}
