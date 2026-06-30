//
// Created by bobi on 6. 05. 26.
//

#include "core/net/Heartbeat.h"
#include "core/net/ESPClient.h"
#include "core/domain/SharedState.h"
#include "core/domain/Client.h"
#include "support/util/JSONtemp.h"
#include "core/commands/legacy/Commands.h"

#include <cstring>
#include <memory>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <nlohmann/json.hpp>

namespace core
{

    Heartbeat::Heartbeat(core::SharedState &state) : logger("Heartbeat"), m_sharedState(state)
    {
        start();
    }

    Heartbeat::~Heartbeat()
    {
        stop();
    }

    void Heartbeat::start()
    {
        if (inited)
        {
            return;
        }

        running_ingest = true;
        ingest_thread = std::thread(&Heartbeat::packet_ingest, this);
        inited = true;
    }

    void Heartbeat::stop()
    {
        running_ingest = false;

        // The ingest thread is parked in a blocking recvfrom(); clearing the flag
        // alone won't wake it (no packet arrives once devices stop pinging), so
        // join() would hang the whole shutdown. Send a dummy datagram to our own
        // bind port to unblock the recv and let the loop notice running_ingest.
        if (ingest_thread.joinable())
        {
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock >= 0)
            {
                sockaddr_in addr{};
                addr.sin_family = AF_INET;
                addr.sin_port = htons(12343);
                inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
                sendto(sock, "", 0, 0, (sockaddr *)&addr, sizeof(addr));
                close(sock);
            }
            ingest_thread.join();
        }
    }

    void Heartbeat::packet_ingest()
    {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0)
        {
            logger.error("Socket creation failed: {}", strerror(errno));
            return;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(12343);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
        {
            logger.error("Bind failed: {}", strerror(errno));
            close(sock);
            return;
        }

        while (running_ingest.load())
        {
            char buffer[512];
            sockaddr_in sender{};
            socklen_t sender_len = sizeof(sender);

            ssize_t bytes_received = recvfrom(
                sock,
                buffer,
                sizeof(buffer),
                0,
                (sockaddr *)&sender,
                &sender_len);

            if (bytes_received < 0)
            {
                logger.error("Error reading: {}", strerror(errno));
                continue;
            }

            const std::string ip = inet_ntoa(sender.sin_addr);
            std::string data(buffer, bytes_received);

            if (!m_sharedState.isNewClient(ip))
            {
                using json = nlohmann::json;
                // An empty datagram (recvfrom == 0) or garbage would throw and
                // crash the ingest thread — skip anything that isn't valid JSON.
                json jarr;
                try
                {
                    jarr = json::parse(data);
                }
                catch (const json::parse_error &e)
                {
                    logger.error("Bad heartbeat from {}: {}", ip, e.what());
                    continue;
                }
                std::string version = jarr.value("version", "outdated");

                Client client;
                client.engine.version = version;
                client.wifi.ip = ip;

                int id = 0;
                for (const auto &item : jarr["fixtures"])
                {
                    Client::Fixture fix;
                    // info.selected = id++;
                    fix.name = item.value("name", "");
                    fix.type = item.value("type", "");

                    client.fixtures.push_back(std::move(fix));
                }

                m_sharedState.addClient(std::move(client), ip);
                requestDescribe(ip);
            }
            else
            {
                m_sharedState.updateClientPing(ip);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        close(sock);
    }

    void Heartbeat::requestDescribe(const std::string &ip)
    {
        auto client = m_sharedState.getESPClient(ip);
        client->sendRequestAsync(support::JSONtemp::stringify("describe"), 4000ms,
                                 [this, ip](std::optional<std::string> resp)
                                 {
                                     if (!resp)
                                     {
                                         logger.error("describe {} failed", ip);
                                         return;
                                     }

                                     // Parse into a local Client (dispatching on engine version),
                                     // seeded from the cached client so version/ip are known, then
                                     // commit it back under the lock.
                                     Client parsed;
                                     m_sharedState.withClient(ip, [&](Client &c)
                                                              { parsed = c; });

                                     if (core::legacy::Execute::describe(resp, parsed))
                                     {
                                         m_sharedState.withClient(ip, [&](Client &c)
                                                                  { c = std::move(parsed); });
                                         // Now that fixtures are known, fetch their presets —
                                         // chained after describe so the two don't contend for
                                         // the device connection.
                                         requestPresets(ip);
                                     }
                                     else
                                         logger.error("describe parse failed for {}", ip);
                                 });
    }

    void Heartbeat::requestPresets(const std::string &ip)
    {
        std::size_t count = 0;
        m_sharedState.withClient(ip, [&](Client &c)
                                 { count = c.fixtures.size(); });

        // Enqueue one presets request per fixture. The ESPClient worker runs them
        // in order after the describe (single per-device queue) and retries each
        // on timeout, so we don't need our own thread or retry loop here.
        auto client = m_sharedState.getESPClient(ip);
        for (std::size_t i = 0; i < count; ++i)
        {
            const int id = static_cast<int>(i);
            client->sendRequestAsync(
                support::JSONtemp::stringify("getfixture", "id", id, "value", "presets"), 4000ms,
                [this, ip, id](std::optional<std::string> resp)
                {
                    if (!resp)
                    {
                        logger.error("presets {} fixture {} failed", ip, id);
                        return;
                    }
                    m_sharedState.withClient(ip, [&](Client &c)
                                             {
                        if (id >= 0 && id < static_cast<int>(c.fixtures.size()))
                            core::legacy::Execute::presets(resp, c, c.fixtures[id]); });
                },
                /*retries=*/3);
        }
    }

}
