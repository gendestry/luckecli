//
// Created by bobi on 20. 05. 26.
//
#include "SharedState.h"

#include <iostream>

SharedState::SharedState(std::function<void()> onChange)
    : onChangeCallback(onChange), log("Shared state",
    [](const Loggable& l) {
        std::cout << l.typeToString()<< l.toString();
        return true;
    }) {
    // log.setLocalLevel(Loggable::DEBUG);
}

void SharedState::triggerChange() {
    std::lock_guard lock(callbackMutex);
    if (onChangeCallback)
        onChangeCallback();
}

void SharedState::addClient(ClientInfo client, std::string ip) {
    std::lock_guard lock(mutex);
    if (clients.contains(ip)) {
        auto& vec = clients[ip];
        if(vec.size() > client.selected)
        {
            auto& m = clients[ip][client.selected];
            m.get().description = std::move(client.description);
        }
        else
        {
            clients_list.push_back(std::move(client));
            clients[ip].push_back(clients_list.back());        
        }
    }
    else {
        clients_list.push_back(std::move(client));
        clients[ip].push_back(clients_list.back());
        // clients[ip] = std::make_shared<ClientInfo>(std::move(client));
        // clients_list.push_back(clients[ip]);
        log.debug("Added new client at ip: {}", ip);
    }

    triggerChange();
}

void SharedState::removeClientsByIp(const std::string& ip) {
    std::lock_guard lock(mutex);
    if (!clients.contains(ip)) return;

    clients.erase(ip);
    clients_list.remove_if([&](const ClientInfo& c) { return c.ip == ip; });

    triggerChange();
}

void SharedState::clearAll() {
    std::lock_guard lock(mutex);
    clients.clear();
    clients_list.clear();
    triggerChange();
}

bool SharedState::hasClientWithName(const std::string& name) {
    std::lock_guard lock(mutex);
    for (auto& c : clients_list) {
        if (c.description.name == name) return true;
    }
    return false;
}

void SharedState::addResponse(const std::string& response) {
    std::lock_guard lock(mutexResponse);
    responses.push_back(response);

    triggerChange();
}

const std::vector<std::string>& SharedState::getResponses() {
    std::lock_guard lock(mutexResponse);
    return responses;
}

// const std::shared_ptr<ClientInfo> SharedState::getClientByIp(const std::string& ip) {
//     std::lock_guard lock(mutex);
//     if (clients.contains(ip)) {
//         return clients[ip];
//     }
//     throw std::logic_error("Unknown client ip");
// }

const ClientInfo& SharedState::getClientByName(const std::string& name) {
    std::lock_guard lock(mutex);
    // auto& cls = clients_list | std::ranges::
    for (auto& client : clients_list) {
        if(client.description.name == name)
        {
            // return std::make_unique<ClientInfo>(client);
            return client;
        }
        // for (auto& desc : client->descriptions) {
        //     if (desc.name == name) {
        //         client->selected = i;
        //         return client;
        //     }
        //     i++;
        // }
    }

    throw std::runtime_error("WTF");
    // return nullptr;
}

const std::list<ClientInfo>& SharedState::getClients() {
    std::lock_guard lock(mutex);
    return clients_list;
}