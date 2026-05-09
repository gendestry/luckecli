#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <optional>
#include "Utils/Text/Stream.h"
#include "SafeQueue.h"
#include "Utils/Logging/Logger.h"

using namespace std::chrono_literals;

class ESPClient {
    Utils::Logger logger;
    static std::thread listen_thread;
    static std::atomic<bool> running;
    static SafeQueue<std::string> queue;

    static bool inited;
    static void listen_ingest();

    std::optional<std::string> sendRequestOpt(const std::string& request, std::chrono::milliseconds timeout) const;
public:
    ESPClient(std::string ip);
    ~ESPClient();

    static void stop();

    void sendRequest(const std::string& request) const;
    void run(const std::string& request, bool jsonres = false, std::chrono::milliseconds timeout = 2000ms);

    const std::string& getIP() const {
        return ip_;
    }

private:
    std::string ip_;
};

//
// class ESPClient {
//     Utils::Logger logger;
// public:
//     ESPClient(std::string ip)
//         : logger("Config"), ip_(std::move(ip)), running_(true)
//     {
//         logger.setLoggerLevel(Utils::Logger::INFO);
//         listener_ = std::thread(&ESPClient::listen, this);
//     }
//
//     ~ESPClient() {
//         stop();
//     }
//
//     void stop() {
//         if (running_) {
//             running_ = false;
//             if (listener_.joinable())
//                 listener_.join();
//
//             // send dummy packet to unblock recvfrom
//             int sock = socket(AF_INET, SOCK_DGRAM, 0);
//             sockaddr_in addr{};
//             addr.sin_family = AF_INET;
//             addr.sin_port = htons(12345);
//             inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
//             sendto(sock, "", 0, 0, (sockaddr*)&addr, sizeof(addr));
//             close(sock);
//         }
//     }
//
//     std::future<void> sendreq(std::string request) {
//         return std::async(std::launch::async, [this, request = std::move(request)]() {
//             int sock = socket(AF_INET, SOCK_STREAM, 0);
//             if (sock < 0) {
//                 perror("socket");
//                 return;
//             }
//
//             sockaddr_in addr{};
//             addr.sin_family = AF_INET;
//             addr.sin_port = htons(8888);
//             inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr);
//
//             if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
//                 perror("connect");
//                 close(sock);
//                 return;
//             }
//
//             send(sock, request.c_str(), request.size(), 0);
//             close(sock);
//             // auto resp = listen_ingest();
//             // resp.wait();
//             // std::cout << resp.get();
//         });
//     }
//
//     void listen() {
//         int sock = socket(AF_INET, SOCK_DGRAM, 0);
//         sockaddr_in addr{};
//         addr.sin_family = AF_INET;
//         addr.sin_port = htons(12345);
//         addr.sin_addr.s_addr = INADDR_ANY;
//
//         if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
//             perror("bind");
//             close(sock);
//             return;
//         }
//
//         logger.debug("Listening on UDP 12345");
//         // std::cout << "Listening on UDP 12345...\n"
//         while (running_) {
//             char buffer[2048];
//             sockaddr_in sender{};
//             socklen_t len = sizeof(sender);
//             ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&sender, &len);
//             if (n <= 0) continue;
//             std::string msg(buffer, n);
//             std::string ip = inet_ntoa(sender.sin_addr);
//             // std::cout << "[" << ip << "] " << msg << "\n";
//             logger.debug("[{}] {}", ip, msg);
//         }
//         close(sock);
//     }
//
//     const std::string& getIP() const {
//         return ip_;
//     }
//
// private:
//     std::string ip_;
//     std::thread listener_;
//     std::atomic<bool> running_;
// };

#include "SafeQueue.h"
