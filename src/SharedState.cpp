//
// Created by bobi on 20. 05. 26.
//
#include "SharedState.h"

SharedState::SharedState(std::function<void()> onChange)
        : onChangeCallback(onChange)
{}

void SharedState::triggerChange() {
    std::lock_guard lock(callbackMutex);
    if (onChangeCallback)
        onChangeCallback();
}

void SharedState::addClient(ClientInfo client, std::string ip) {
    std::lock_guard lock(mutex);
    if (clients.contains(ip)) {
        clients[ip]->descriptions = std::move(client.descriptions);
    }
    else {
        clients[ip] = std::make_shared<ClientInfo>(std::move(client));
        clients_list.push_back(clients[ip]);
    }

    triggerChange();
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

const std::shared_ptr<ClientInfo> SharedState::getClientByIp(const std::string& ip) {
    std::lock_guard lock(mutex);
    if (clients.contains(ip)) {
        return clients[ip];
    }
    throw std::logic_error("Unknown client ip");
}

std::shared_ptr<ClientInfo> SharedState::getClientByName(const std::string& name) {
    std::lock_guard lock(mutex);
    for (auto& client : clients_list) {
        for (auto& desc : client->descriptions) {
            if (desc.name == name) {
                return client;
            }
        }
    }

    return nullptr;
}

std::vector<std::shared_ptr<ClientInfo>> SharedState::getClients() {
    std::lock_guard lock(mutex);
    return clients_list;
}