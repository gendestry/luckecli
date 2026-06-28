//
// Created by bobi on 17. 05. 26.
//

#pragma once
#include <functional>
#include <list>
#include <string>
#include <mutex>
#include <optional>
#include <vector>
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>

#include "Client.h"
#include "Test/Utils/SafeQueue.h"

#include "Diary/Log.h"

namespace Test
{

    class SharedState
    {
        Log log;
        std::unordered_map<std::string, Client> clients;
        std::mutex mutex;
        std::mutex queueMutex;

        Utils::SafeQueue queue;

        // std::list<Client> clients_list;

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