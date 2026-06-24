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

#include "Diary/Log.h"


struct ClientInfo {
    struct Description 
    {
        std::string name;
        std::string type;
        uint8_t universe;
        uint16_t address;
        uint8_t preset;
        // int num_leds = 0;
    } description;

    struct Wifi {
        bool connected = false;
        std::string ip;
        int rssi;
        std::string ssid;
        std::string password;
    } wifi;

    struct Engine {
        std::string version = "outdated";
        bool serial_print = false;
        bool wifi_print = false;
        bool wifi_animation = false;
    } engine;

    int selected = 0;
    
    // std::string ip;
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

    // In-memory response cache: ip -> (request -> raw response).
    // Manual invalidation only: cleared by clearCache()/refresh, never expires on its own.
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> responseCache;
    std::mutex cacheMutex;

    void triggerChange();
public:
    std::function<void()> onChangeCallback;

    SharedState(std::function<void()> onChange = [](){});

    void addClient(ClientInfo client, std::string ip);
    void setClientName(const std::string& ip, int selected, const std::string& name);
    void removeClientsByIp(const std::string& ip);
    void clearAll();
    bool hasClientWithName(const std::string& name);
    void addResponse(const std::string& response);

    const std::vector<std::string>& getResponses();

    // Response cache (manual invalidation only).
    std::optional<std::string> getCached(const std::string& ip, const std::string& request);
    void putCache(const std::string& ip, const std::string& request, const std::string& response);
    void clearCacheForIp(const std::string& ip);
    void clearCache();

    const std::shared_ptr<ClientInfo> getClientByIp(const std::string& ip);
    // std::unique_ptr<ClientInfo> getClientByName(const std::string& name);
    const ClientInfo& getClientByName(const std::string& name);

    const std::list<ClientInfo>& getClients();
};
