#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "support/log/Log.h"

using namespace std::chrono_literals;

namespace core
{

    class SharedState; // only referenced here; full definition needed in the .cpp

    class ESPClient : public std::enable_shared_from_this<ESPClient>
    {
    public:
        using ResponseHandler = std::function<void(std::optional<std::string>)>;

    private:
        Log logger;

        SharedState &m_sharedState;
        std::string ip_; // value, not a reference into the clients map

        // A queued request: what to send, how long to wait per attempt, how many
        // extra attempts on timeout, and who to hand the reply to.
        struct Job
        {
            std::string request;
            std::chrono::milliseconds timeout;
            ResponseHandler onResponse;
            int retries = 0;
            std::chrono::milliseconds backoff{300};
        };

        // The device is single-connection, so all traffic to it is funneled
        // through one FIFO worked by a single thread — requests run strictly in
        // order, one at a time. Different devices each have their own worker.
        std::deque<Job> jobs_;
        std::mutex jobsMutex_;
        std::condition_variable jobsCv_;
        std::atomic<bool> running_{true};
        std::thread worker_;

        void workerLoop();
        void enqueue(Job job);
        // The blocking request/response round-trip; only the worker calls this.
        std::optional<std::string> doRequest(const std::string &request, std::chrono::milliseconds timeout);

    public:
        ESPClient(std::string ip, SharedState &state);
        ~ESPClient();

        // Enqueue a request and block until its reply (or nullopt on failure).
        // For the synchronous command/REPL path. MUST NOT be called from inside a
        // ResponseHandler (that runs on the worker thread → would deadlock).
        std::optional<std::string> sendRequestOpt(const std::string &request, std::chrono::milliseconds timeout);

        // Enqueue a request and return immediately; onResponse runs on the worker
        // thread when the reply arrives (nullopt on failure). `retries` extra
        // attempts are made on timeout, each after `backoff`.
        void sendRequestAsync(const std::string &req, std::chrono::milliseconds timeout, ResponseHandler onResponse,
                              int retries = 0, std::chrono::milliseconds backoff = 300ms);

        // void sendRequest(const std::string& request);
        // void run(const std::string &request, bool jsonres = false, std::chrono::milliseconds timeout = 4000ms);
        // std::string runStr(const std::string &request, bool jsonres = false, std::chrono::milliseconds timeout = 4000ms, bool useCache = false);

        // const int clientID() const
        // {
        //     return clientInfo.selected;
        // }

        // const ClientInfo &getClientInfo() const
        // {
        //     return clientInfo;
        // }

        const std::string &getIP() const
        {
            return ip_;
        }
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