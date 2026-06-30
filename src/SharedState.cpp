//
// Created by bobi on 20. 05. 26.
//
#include "SharedState.h"
#include "ESPClient.h" // complete type, so the shared_ptr<ESPClient> map can be destroyed here

namespace Test
{

    SharedState::SharedState(std::function<void()> onChange)
        : onChangeCallback(onChange), log("State")
    {
        // log.setLocalLevel(Loggable::DEBUG);
    }

    SharedState::~SharedState()
    {
        // Destroy the ESPClients first — each ~ESPClient joins its worker thread.
        // Workers touch our other members (the response queue, client map) via the
        // back-reference, so they must all be stopped while those are still alive;
        // otherwise a worker mid-request races member destruction (use-after-free).
        connections.clear();
    }

    std::shared_ptr<ESPClient> SharedState::getESPClient(const std::string &ip)
    {
        std::lock_guard lock(connMutex);
        auto it = connections.find(ip);
        if (it == connections.end())
            it = connections.emplace(ip, std::make_shared<ESPClient>(ip, *this)).first;
        return it->second;
    }

    // void SharedState::triggerChange() {
    //     std::lock_guard lock(callbackMutex);
    //     if (onChangeCallback)
    //         onChangeCallback();
    // }

    bool SharedState::isNewClient(const std::string &ip)
    {
        std::lock_guard lock(mutex);
        return clients.contains(ip);
    }

    void SharedState::addClient(Client &&client, const std::string &ip)
    {
        std::lock_guard lock(mutex);
        clients[ip] = std::move(client);
        auto &c = clients[ip];
    }

    void SharedState::updateClientPing(const std::string &ip)
    {
        std::lock_guard lock(mutex);
        auto &c = clients[ip];
        c.updateLastPing();
        // log.println("Update: IP: {}, ping: {}", ip, c.last_ping_str);
    }

    // void SharedState::addClient(ClientInfo client, std::string ip) {
    //     bool changed = false;
    //     {
    //         std::lock_guard lock(mutex);
    //         if (clients.contains(ip)) {
    //             auto& vec = clients[ip];
    //             if(vec.size() > client.selected)
    //             {
    //                 // Known fixture: only update (and notify) if the heartbeat data
    //                 // actually changed. Identical repeat heartbeats are ignored so
    //                 // the grid view doesn't flicker by rebuilding on every packet;
    //                 // a genuine change (e.g. a renamed fixture) still propagates.
    //                 auto& existing = vec[client.selected].get().description;
    //                 if (existing.name != client.description.name ||
    //                     existing.type != client.description.type)
    //                 {
    //                     existing.name = std::move(client.description.name);
    //                     existing.type = std::move(client.description.type);
    //                     changed = true;
    //                 }
    //             }
    //             else
    //             {
    //                 clients_list.push_back(std::move(client));
    //                 clients[ip].push_back(clients_list.back());
    //                 changed = true;
    //             }
    //         }
    //         else {
    //             clients_list.push_back(std::move(client));
    //             clients[ip].push_back(clients_list.back());
    //             // clients[ip] = std::make_shared<ClientInfo>(std::move(client));
    //             // clients_list.push_back(clients[ip]);
    //             log.debug("Added new client at ip: {}", ip);
    //             changed = true;
    //         }
    //     }

    //     // Only notify when something actually changed (new device/fixture or
    //     // updated heartbeat data).
    //     if (changed)
    //         triggerChange();
    // }

    // void SharedState::setClientName(const std::string& ip, int selected, const std::string& name) {
    //     bool changed = false;
    //     {
    //         std::lock_guard lock(mutex);
    //         auto it = clients.find(ip);
    //         if (it != clients.end() && selected >= 0 && selected < (int)it->second.size()) {
    //             auto& desc = it->second[selected].get().description;
    //             if (desc.name != name) {
    //                 desc.name = name;
    //                 changed = true;
    //             }
    //         }
    //     }
    //     // Reflect the rename in the grid/header without waiting for a heartbeat
    //     // (which on this firmware may keep advertising the old name).
    //     if (changed)
    //         triggerChange();
    // }

    // void SharedState::removeClientsByIp(const std::string& ip) {
    //     std::lock_guard lock(mutex);
    //     if (!clients.contains(ip)) return;

    //     clients.erase(ip);
    //     clients_list.remove_if([&](const ClientInfo& c) { return c.wifi.ip == ip; });
    //     clearCacheForIp(ip);

    //     triggerChange();
    // }

    // void SharedState::clearAll() {
    //     std::lock_guard lock(mutex);
    //     clients.clear();
    //     clients_list.clear();
    //     clearCache();
    //     triggerChange();
    // }

    // bool SharedState::hasClientWithName(const std::string& name) {
    //     std::lock_guard lock(mutex);
    //     for (auto& c : clients_list) {
    //         if (c.description.name == name) return true;
    //     }
    //     return false;
    // }

    // void SharedState::addResponse(const std::string& response) {
    //     std::lock_guard lock(mutexResponse);
    //     responses.push_back(response);

    //     triggerChange();
    // }

    // const std::vector<std::string>& SharedState::getResponses() {
    //     std::lock_guard lock(mutexResponse);
    //     return responses;
    // }

    // std::optional<std::string> SharedState::getCached(const std::string& ip, const std::string& request) {
    //     std::lock_guard lock(cacheMutex);
    //     auto ipIt = responseCache.find(ip);
    //     if (ipIt == responseCache.end()) return std::nullopt;
    //     auto reqIt = ipIt->second.find(request);
    //     if (reqIt == ipIt->second.end()) return std::nullopt;
    //     return reqIt->second;
    // }

    // void SharedState::putCache(const std::string& ip, const std::string& request, const std::string& response) {
    //     std::lock_guard lock(cacheMutex);
    //     responseCache[ip][request] = response;
    // }

    // void SharedState::clearCacheForIp(const std::string& ip) {
    //     std::lock_guard lock(cacheMutex);
    //     responseCache.erase(ip);
    // }

    // void SharedState::clearCache() {
    //     std::lock_guard lock(cacheMutex);
    //     responseCache.clear();
    // }

    // // const std::shared_ptr<ClientInfo> SharedState::getClientByIp(const std::string& ip) {
    // //     std::lock_guard lock(mutex);
    // //     if (clients.contains(ip)) {
    // //         return clients[ip];
    // //     }
    // //     throw std::logic_error("Unknown client ip");
    // // }

    // const ClientInfo& SharedState::getClientByName(const std::string& name) {
    //     std::lock_guard lock(mutex);
    //     // auto& cls = clients_list | std::ranges::
    //     for (auto& client : clients_list) {
    //         if(client.description.name == name)
    //         {
    //             // return std::make_unique<ClientInfo>(client);
    //             return client;
    //         }
    //         // for (auto& desc : client->descriptions) {
    //         //     if (desc.name == name) {
    //         //         client->selected = i;
    //         //         return client;
    //         //     }
    //         //     i++;
    //         // }
    //     }

    //     throw std::runtime_error("WTF");
    //     // return nullptr;
    // }

    // const std::list<ClientInfo>& SharedState::getClients() {
    //     std::lock_guard lock(mutex);
    //     return clients_list;
    // }
}