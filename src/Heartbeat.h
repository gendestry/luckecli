// //
// // Created by bobi on 6. 05. 26.
// //
//
// #ifndef UTILS_HEARTBEAT_H
// #define UTILS_HEARTBEAT_H
//
// #include <iostream>
// #include <string>
// #include <cstring>
// #include <cerrno>
//
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <unistd.h>
// #include <thread>
// #include <list>
//
// #include <memory>
// #include <mutex>
// #include <chrono>
// #include <unordered_map>
//
// struct ClientInfo {
//     uint32_t index;
//     std::string ip;
//     std::string data;
// };
//
// class Heartbeat {
//     static std::atomic<bool> running_ingest;
//     static bool inited;
//     static std::thread ingest_thread;
//
//     std::unordered_map<std::string, ClientInfo> clients;
//     std::mutex clients_mutex;
//     std::vector<ClientInfo*> clients_list;
//
//     void packet_ingest() {
//         uint32_t currentIndex = 0;
//         int sock = socket(AF_INET, SOCK_DGRAM, 0);
//         if (sock < 0) {
//             std::cerr << "Socket creation failed: " << strerror(errno) << "\n";
//             return;
//         }
//
//         sockaddr_in addr{};
//         addr.sin_family = AF_INET;
//         addr.sin_port = htons(12343);
//         addr.sin_addr.s_addr = INADDR_ANY;
//
//         if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
//             std::cerr << "Bind failed: " << strerror(errno) << "\n";
//             close(sock);
//             return;
//         }
//
//         // std::cout << "Listening on UDP port 12343 for heartbeat\n";
//
//         while (running_ingest) {
//             char buffer[512];
//             sockaddr_in sender{};
//             socklen_t sender_len = sizeof(sender);
//
//             ssize_t bytes_received = recvfrom(
//                 sock,
//                 buffer,
//                 sizeof(buffer),
//                 0,
//                 reinterpret_cast<sockaddr*>(&sender),
//                 &sender_len
//             );
//
//             if (bytes_received < 0) {
//                 std::cerr << "ERR reading: " << strerror(errno) << "\n";
//                 continue;
//             }
//
//             std::string ip = inet_ntoa(sender.sin_addr);
//             std::string data(buffer, bytes_received);
//
//             // insert only if new
//             std::lock_guard lock(clients_mutex);
//             if (!clients.contains(ip)) {
//                 clients[ip] = ClientInfo{currentIndex++, ip, data};
//                 clients_list.push_back(&clients[ip]);
//             }
//             else {
//                 clients[ip].data = data;
//             }
//
//             std::this_thread::sleep_for(std::chrono::milliseconds(10));
//         }
//
//         close(sock);
//     }
// public:
//     static void init() {
//         if (!inited) {
//             running_ingest = true;
//             ingest_thread = std::thread(&Heartbeat::packet_ingest, this);
//             inited = true;
//         }
//     }
//
//
//     void start() {
//         if (inited) return;
//
//         running_ingest = true;
//
//         ingest_thread = std::thread(&Heartbeat::packet_ingest, this);
//
//         inited = true;
//     }
//
//     void stop() {
//         running_ingest = false;
//
//         if (ingest_thread.joinable())
//             ingest_thread.join();
//     }
//
// };
//
//
//
// #endif //UTILS_HEARTBEAT_H
#pragma once
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_map>

#include "Utils/Logging/Logger.h"



struct ClientInfo {
    struct Description {
        std::string name;
        std::string type;
    };
    uint32_t index;
    std::string ip;
    std::vector<Description>descriptions;
    // std::string data;
};

class Heartbeat {
    std::unordered_map<std::string, ClientInfo> clients;
    std::vector<ClientInfo*> clients_list;

    std::atomic<bool> running_ingest{false};
    std::mutex clients_mutex;
    bool inited = false;
    std::thread ingest_thread;

    Utils::Logger logger;

    void start();
    void packet_ingest();

public:
    Heartbeat();
    ~Heartbeat();

    void stop();

    std::vector<ClientInfo*> getClients();
    uint32_t size();

    const ClientInfo& operator[] (const std::string& ip) {
        if (clients.contains(ip)) {
            return clients[ip];
        }
        throw std::logic_error("Unknown client ip");

    }
};