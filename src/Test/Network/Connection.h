#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <optional>

#include "Diary/Log.h"
#include "SafeQueue.h"
#include "SharedState.h"

using namespace std::chrono_literals;

namespace Test::Network
{

    class Connection
    {
        Log logger;
        std::thread listen_thread;
        std::atomic<bool> running;
        // static SafeQueue<std::string> queue;

        // // Signals when the response listener is actually bound and accepting, so
        // // the first request can't race the listener's startup (dropped response).
        std::mutex listen_mtx;
        std::condition_variable listen_cv;
        bool listening;

        // static bool inited;
        // static void listen_ingest_udp();
        void listen_ingest_tcp();

        // SharedState& m_sharedState;
        // const ClientInfo& clientInfo;

        // std::optional<std::string> sendRequestOpt(const std::string& request, std::chrono::milliseconds timeout);
    public:
        Connection(const std::string &ip);

        std::optional<std::string> Connection::sendRequestOpt(const std::string &request, std::chrono::milliseconds timeout);
        // ESPClient(const ClientInfo& cinfo, SharedState& state);
        // ~ESPClient();

        // static void stop();

        // // void sendRequest(const std::string& request);
        // void run(const std::string& request, bool jsonres = false, std::chrono::milliseconds timeout = 4000ms);
        // std::string runStr(const std::string& request, bool jsonres = false, std::chrono::milliseconds timeout = 4000ms, bool useCache = false);

        // const int clientID() const
        // {
        //     return clientInfo.selected;
        // }

        // const ClientInfo& getClientInfo() const
        // {
        //     return clientInfo;
        // }

        // const std::string& getIP() const {
        //     return ip_;
        // }

    private:
        std::string m_ip;
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

}