//
// Created by bobi on 17. 05. 26.
//

#ifndef LUCKECLI_SHAREDSTATE_H
#define LUCKECLI_SHAREDSTATE_H
#include <functional>
#include <string>
#include <mutex>
#include <vector>
#include <memory>
#include <unordered_map>

struct ClientInfo {
    struct Description {
        std::string name;
        std::string type;
    };
    // uint32_t index;
    std::string version = "outdated";
    std::string ip;
    std::vector<Description>descriptions;

    ClientInfo() {
        // static uint32_t sindex = 0;
        // index = sindex++;
    }
    // std::string data;
};

class SharedState {
    std::unordered_map<std::string, std::shared_ptr<ClientInfo>> clients;
    std::vector<std::shared_ptr<ClientInfo>> clients_list;
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
    std::shared_ptr<ClientInfo> getClientByName(const std::string& name);

    std::vector<std::shared_ptr<ClientInfo>> getClients();
};

#endif //LUCKECLI_SHAREDSTATE_H