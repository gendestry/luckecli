//
// Created by bobi on 6. 05. 26.
//

#include "ESPClient.h"
#include "SharedState.h"
#include "Network/Connection.h"
#include "Utils/SafeQueue.h"

#include <thread>

namespace Test
{

    ESPClient::ESPClient(std::string ip, SharedState &state)
        : logger("CONFIG"), m_sharedState(state), ip_(std::move(ip))
    {
    }

    ESPClient::~ESPClient()
    {
    }

    std::optional<std::string> ESPClient::sendRequestOpt(const std::string &request, std::chrono::milliseconds timeout)
    {
        // Serialize: only one request in-flight to this device at a time.
        // Without this, two callers' clear()/pop_for() on the same IP race and
        // steal each other's replies (the queue is keyed only by IP).
        std::lock_guard lock(sendMutex_);

        Network::Connection conn(ip_);
        if (!conn.open())
            return std::nullopt;

        // Requests are serialized (one in-flight at a time), so drop any leftover
        // response from a previously timed-out request. This keeps each reply
        // matched to the request that asked for it and prevents permanent desync.
        m_sharedState.getQueue().clear(ip_);

        if (!conn.send(request))
            return std::nullopt;

        // Do NOT close the request socket yet. The firmware reads + dispatches
        // inside `while (tc.isConnected())`; closing now drops the connection
        // before the request is read, so no response is ever sent. Hold it open
        // until the reply arrives on the listener (or we time out); `conn`'s
        // destructor closes it on scope exit, covering every return path.
        return m_sharedState.getQueue().pop_for(ip_, timeout);
    }

    void ESPClient::sendRequestAsync(const std::string &req, std::chrono::milliseconds timeout, ResponseHandler onResponse)
    {
        auto self = shared_from_this();
        std::thread([self, req, timeout, onResponse = std::move(onResponse)]()
                    {
                        auto resp = self->sendRequestOpt(req, timeout); // blocks on this thread only
                        if (onResponse)
                            onResponse(std::move(resp)); })
            .detach();
    }

    // void ESPClient::run(const std::string &request, bool jsonres, std::chrono::milliseconds timeout)
    // {
    //     auto result = sendRequestOpt(request, timeout);

    //     // run() is only used for mutations (reboot, highlight): drop cached reads.
    //     // m_sharedState.clearCacheForIp(ip_);

    //     if (!result.has_value())
    //     {
    //         logger.error("No response");
    //         return;
    //     }
    //     using json = nlohmann::json;
    //     json data = json::parse(result.value());
    //     if (data.contains("err"))
    //     {
    //         logger.error("{}", data["err"].get<std::string>());
    //     }
    //     else
    //     {
    //         logger.println("{}", data.dump(2));
    //     }
    //     // m_sharedState.addResponse(result.value());
    // };

    // std::string ESPClient::runStr(const std::string &request, bool jsonres, std::chrono::milliseconds timeout, bool useCache)
    // {
    //     if (useCache)
    //     {
    //         // if (auto cached = m_sharedState.getCached(ip_, request))
    //         {
    //             logger.debug("Cache hit: {}", request);
    //             return *cached;
    //         }
    //     }

    //     auto result = sendRequestOpt(request, timeout);

    //     // A non-cached request is a mutation (set*/toggle/reboot/wifi): drop this
    //     // device's cached reads so the next read reflects the new state. This keeps
    //     // the read cache (otherwise manual-invalidation only) correct after writes.
    //     if (!useCache)
    //         m_sharedState.clearCacheForIp(ip_);

    //     if (!result)
    //     {
    //         logger.error("No response");
    //         return "";
    //     }

    //     // Only cache successful, non-error responses so a transient failure
    //     // doesn't get pinned in the cache (which is manual-invalidation only).
    //     if (useCache && !result->empty() && result->find("\"err\"") == std::string::npos)
    //     {
    //         m_sharedState.putCache(ip_, request, *result);
    //     }

    //     return result.value();
    // };
}