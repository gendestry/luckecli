//
// Created by bobi on 17. 05. 26.
//

#pragma once
#include <functional>
#include <list>
#include <string>
#include <mutex>
#include <vector>
#include <memory>
#include <unordered_map>

#include "Diary/Log.h"


struct ClientInfo {
    struct Description {
        std::string name;
        std::string type;
        int num_leds = 0;
    } description;
    int selected = 0;
    std::string version = "outdated";
    std::string ip;
    // std::vector<Description>descriptions;
};

class SharedState {
    Log log;
    std::unordered_map<std::string, std::vector<std::reference_wrapper<ClientInfo>>> clients;
    std::list<ClientInfo> clients_list;
    std::mutex mutex;

    std::mutex callbackMutex;

    std::vector<std::string> responses;
    std::mutex mutexResponse;

    void triggerChange();
public:
    std::function<void()> onChangeCallback;

    SharedState(std::function<void()> onChange = [](){});

    void addClient(ClientInfo client, std::string ip);
    void addResponse(const std::string& response);

    const std::vector<std::string>& getResponses();
    const std::shared_ptr<ClientInfo> getClientByIp(const std::string& ip);
    // std::unique_ptr<ClientInfo> getClientByName(const std::string& name);
    const ClientInfo& getClientByName(const std::string& name);

    const std::list<ClientInfo>& getClients();
};
