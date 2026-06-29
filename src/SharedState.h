//
// Created by bobi on 17. 05. 26.
//

#pragma once
#include <functional>
#include <string>
#include <mutex>
#include <vector>
#include <memory>
#include <unordered_map>

#include "Client.h"
#include "Utils/SafeQueue.h"

#include "Diary/Log.h"

namespace Test
{

    class ESPClient; // forward decl — avoids the circular include with ESPClient.h

    class SharedState
    {
        Log log;
        std::unordered_map<std::string, Client> clients;
        std::mutex mutex;

        std::unordered_map<std::string, std::shared_ptr<ESPClient>> connections;
        std::mutex connMutex;

        Utils::SafeQueue queue;
        std::mutex queueMutex;

        // std::mutex callbackMutex;

        // std::vector<std::string> responses;
        // std::mutex mutexResponse;

        // In-memory response cache: ip -> (request -> raw response).
        // Manual invalidation only: cleared by clearCache()/refresh, never expires on its own.
        // std::unordered_map<std::string, std::unordered_map<std::string, std::string>> responseCache;
        // std::mutex cacheMutex;

        // void triggerChange();

    public:
        std::function<void()> onChangeCallback;

        SharedState(std::function<void()> onChange = []() {});
        // User-declared (defined in .cpp) so the shared_ptr<ESPClient> member is
        // destroyed where ESPClient is a complete type, not in this header.
        ~SharedState();

        std::shared_ptr<ESPClient> getESPClient(const std::string &ip);

        bool isNewClient(const std::string &ip);
        void addClient(Client &&client, const std::string &ip);
        void updateClientPing(const std::string &ip);

        Client &getClient(const std::string &ip)
        {
            std::lock_guard lock(mutex);
            return clients[ip];
        }

        // Run `fn` against the client for `ip` while holding the lock, so async
        // updates (e.g. a describe response arriving on a worker thread) can
        // mutate it without racing the heartbeat thread. No-op if ip is unknown.
        template <typename F>
        void withClient(const std::string &ip, F &&fn)
        {
            std::lock_guard lock(mutex);
            auto it = clients.find(ip);
            if (it != clients.end())
                fn(it->second);
        }

        // Copy out (ip, Client) pairs under the lock for read-only iteration
        // (listing, building a selection). Returns a snapshot so callers never
        // hold references into the map while the heartbeat thread mutates it.
        std::vector<std::pair<std::string, Client>> snapshot()
        {
            std::lock_guard lock(mutex);
            return {clients.begin(), clients.end()};
        }

        Utils::SafeQueue &getQueue()
        {
            std::lock_guard lock(queueMutex);
            return queue;
        };

        // void addClient(ClientInfo client, std::string ip);
        // void setClientName(const std::string& ip, int selected, const std::string& name);
        // void removeClientsByIp(const std::string& ip);
        // void clearAll();
        // bool hasClientWithName(const std::string& name);
        // void addResponse(const std::string& response);

        // const std::vector<std::string>& getResponses();

        // // Response cache (manual invalidation only).
        // std::optional<std::string> getCached(const std::string& ip, const std::string& request);
        // void putCache(const std::string& ip, const std::string& request, const std::string& response);
        // void clearCacheForIp(const std::string& ip);
        // void clearCache();

        // const std::shared_ptr<ClientInfo> getClientByIp(const std::string& ip);
        // // std::unique_ptr<ClientInfo> getClientByName(const std::string& name);
        // const ClientInfo& getClientByName(const std::string& name);

        // const std::list<ClientInfo>& getClients();
    };
}