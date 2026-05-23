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

ESPClient::ESPClient(const ClientInfo& cinfo, SharedState& state)
    : clientInfo(cinfo), ip_(clientInfo.ip), logger("CONFIG"), m_sharedState(state)
{
    if (!inited) {
        inited = true;
        listen_thread = std::thread(&ESPClient::listen_ingest_tcp);
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

std::optional<std::string> ESPClient::sendRequestOpt(const std::string& request, std::chrono::milliseconds timeout) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logger.error("Socket creation failed: {}", strerror(errno));
        return std::nullopt;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        logger.error("Error connecting: {}", strerror(errno));
        close(sock);
        return std::nullopt;
    }

    send(sock, request.c_str(), request.size(), 0);
    close(sock);

    return queue.pop_for(timeout);
}

void ESPClient::run(const std::string& request, bool jsonres, std::chrono::milliseconds timeout){
    auto result = sendRequestOpt(request, timeout);
    if (!result.has_value()) {
        logger.error("No response");
        return;
    }
    using json = nlohmann::json;
    json data = json::parse(result.value());
    std::string response = data["response"];
    std::string status = data["status"];
    if (status == "OK") {
        // logger.toggleScope();
        if (jsonres) {
            json responseJson = json::parse(response);
            logger.println("{}", responseJson.dump(2));
            m_sharedState.addResponse(responseJson.dump(2));
        }
        else {
            logger.println("{}", response.empty() ? "Done" : response);
            m_sharedState.addResponse(response.empty() ? "Done" : response);
        }
        // logger.toggleScope();
    }
    else {
        logger.error("{}", response);
    }
};

std::string ESPClient::runStr(const std::string& request, bool jsonres, std::chrono::milliseconds timeout){
    auto result = sendRequestOpt(request, timeout);
    if (!result) {
        logger.error("No response");
        return "";
    }

    return result.value();
};

void ESPClient::listen_ingest_udp() {
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

void ESPClient::listen_ingest_tcp()
{
    Utils::Logger logger("LISTEN");
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0)
    {
        logger.error("Socket creation failed: {}", strerror(errno));
        return;
    }

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        logger.error("Cannot bind: {}", strerror(errno));
        close(serverSock);
        return;
    }

    if (listen(serverSock, 5) < 0)
    {
        logger.error("Cannot listen: {}", strerror(errno));
        close(serverSock);
        return;
    }

    while (running)
    {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);

        int clientSock = accept(serverSock, (sockaddr*)&clientAddr, &len);

        if (clientSock < 0)
        {
            if (running) {
                logger.error("Accept: {}", strerror(errno));
            }
            continue;
        }

        char ipbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipbuf, sizeof(ipbuf));

        while (running)
        {
            char buffer[4096];
            ssize_t n = recv(clientSock, buffer, sizeof(buffer), 0);

            if (n <= 0)
                break;

            std::string msg(buffer, n);
            queue.push(msg);
        }

        close(clientSock);
    }

    close(serverSock);
}