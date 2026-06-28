//
// Created by bobi on 6. 05. 26.
//

#include "ESPClient.h"
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include "Utils/Logging/Logger.h"
#include <nlohmann/json.hpp>

namespace Test
{

    ESPClient::ESPClient(const Client &cinfo, SharedState &state)
        : clientInfo(cinfo), ip_(clientInfo.wifi.ip), logger("CONFIG"), m_sharedState(state)
    {
    }

    ESPClient::~ESPClient()
    {
    }

    std::optional<std::string> ESPClient::sendRequestOpt(const std::string &request, std::chrono::milliseconds timeout)
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            logger.error("Socket creation failed: {}", strerror(errno));
            return std::nullopt;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(8888);
        inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr);

        if (connect(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
        {
            logger.error("Error connecting: {}", strerror(errno));
            close(sock);
            return std::nullopt;
        }

        // Requests are serialized (one in-flight at a time), so drop any leftover
        // response from a previously timed-out request. This keeps each reply
        // matched to the request that asked for it and prevents permanent desync.
        m_sharedState.getQueue().clear(ip_);

        // The firmware reads the request with readStringUntil('\n'), so the payload
        // MUST be newline-terminated or the read never completes.
        std::string framed = request;
        framed.push_back('\n');
        send(sock, framed.c_str(), framed.size(), 0);

        // Do NOT close the request socket yet. The firmware reads + dispatches
        // inside `while (tc.isConnected())`; closing now drops the connection
        // before the request is read, so no response is ever sent. Hold it open
        // until the reply arrives on the listener (or we time out), then close.
        auto response = m_sharedState.getQueue().pop_for(ip_, timeout);
        close(sock);
        return response;
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