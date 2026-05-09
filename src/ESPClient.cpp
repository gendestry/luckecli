//
// Created by bobi on 6. 05. 26.
//

#include "ESPClient.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <mutex>
#include "Utils/Logging/Logger.h"
#include <nlohmann/json.hpp>

std::thread ESPClient::listen_thread;
std::atomic<bool> ESPClient::running = true;
bool ESPClient::inited = false;
SafeQueue<std::string> ESPClient::queue;

ESPClient::ESPClient(std::string ip)
    : ip_(std::move(ip)), logger("RESPONSE")
{
    if (!inited) {
        inited = true;
        listen_thread = std::thread(&ESPClient::listen_ingest);
    }
}

ESPClient::~ESPClient() {
    // send dummy packet to unblock recvfrom
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    sendto(sock, "", 0, 0, (sockaddr*)&addr, sizeof(addr));
    close(sock);
}

void ESPClient::stop() {
    if (inited) {
        running = false;
        listen_thread.detach();
    }
}

void ESPClient::sendRequest(const std::string& request) const {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return;
    }

    send(sock, request.c_str(), request.size(), 0);
    close(sock);
}

std::optional<std::string> ESPClient::sendRequestOpt(const std::string& request, std::chrono::milliseconds timeout) const {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return std::nullopt;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return std::nullopt;
    }

    send(sock, request.c_str(), request.size(), 0);
    close(sock);

    return queue.pop_for(timeout);
}

void ESPClient::run(const std::string& request, bool jsonres, std::chrono::milliseconds timeout){
    auto result = sendRequestOpt(request, timeout);
    if (!result) {
        logger.error("No response");
        return;
    }
    using json = nlohmann::json;
    json data = json::parse(*result);
    std::string response = data["response"];
    std::string status = data["status"];
    if (status == "OK") {
        logger.toggleScope();
        if (jsonres) {
            json responseJson = json::parse(response);
            logger.println("{}", responseJson.dump(2));
        }
        else {
            logger.println("{}", response.empty() ? "Done" : response);
        }
        logger.toggleScope();
    }
    else {
        logger.error("{}", response);
    }

};

void ESPClient::listen_ingest() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return;
    }

    while (running) {
        char buffer[4096];
        sockaddr_in sender{};
        socklen_t len = sizeof(sender);

        ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&sender, &len);

        if (n <= 0) continue;

        std::string msg(buffer, n);
        std::string ip = inet_ntoa(sender.sin_addr);
        queue.push(msg);
    }

    close(sock);
}