//
// Created by bobi on 6. 05. 26.
//

#include "ESPClient.h"
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <mutex>
#include "Utils/Logging/Logger.h"
#include <nlohmann/json.hpp>

std::thread ESPClient::listen_thread;
std::atomic<bool> ESPClient::running = true;
std::mutex ESPClient::listen_mtx;
std::condition_variable ESPClient::listen_cv;
bool ESPClient::listening = false;
bool ESPClient::inited = false;
SafeQueue<std::string> ESPClient::queue;

ESPClient::ESPClient(const ClientInfo& cinfo, SharedState& state)
    : clientInfo(cinfo), ip_(clientInfo.wifi.ip), logger("CONFIG"), m_sharedState(state)
{
    if (!inited) {
        inited = true;
        listen_thread = std::thread(&ESPClient::listen_ingest_tcp);
    }

    // Don't issue any request until the listener is accepting, otherwise the
    // device's response connection can be refused before we're ready to receive.
    std::unique_lock lock(listen_mtx);
    listen_cv.wait_for(lock, 2000ms, []{ return listening; });
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

    // Requests are serialized (one in-flight at a time), so drop any leftover
    // response from a previously timed-out request. This keeps each reply
    // matched to the request that asked for it and prevents permanent desync.
    queue.clear();

    // The firmware reads the request with readStringUntil('\n'), so the payload
    // MUST be newline-terminated or the read never completes.
    std::string framed = request;
    framed.push_back('\n');
    send(sock, framed.c_str(), framed.size(), 0);

    // Do NOT close the request socket yet. The firmware reads + dispatches
    // inside `while (tc.isConnected())`; closing now drops the connection
    // before the request is read, so no response is ever sent. Hold it open
    // until the reply arrives on the listener (or we time out), then close.
    auto response = queue.pop_for(timeout);
    close(sock);
    return response;
}

void ESPClient::run(const std::string& request, bool jsonres, std::chrono::milliseconds timeout){
    auto result = sendRequestOpt(request, timeout);

    // run() is only used for mutations (reboot, highlight): drop cached reads.
    m_sharedState.clearCacheForIp(ip_);

    if (!result.has_value()) {
        logger.error("No response");
        return;
    }
    using json = nlohmann::json;
    json data = json::parse(result.value());
    if (data.contains("err")) {
        logger.error("{}", data["err"].get<std::string>());
    } else {
        logger.println("{}", data.dump(2));
    }
    m_sharedState.addResponse(result.value());
};

std::string ESPClient::runStr(const std::string& request, bool jsonres, std::chrono::milliseconds timeout, bool useCache){
    if (useCache) {
        if (auto cached = m_sharedState.getCached(ip_, request)) {
            logger.debug("Cache hit: {}", request);
            return *cached;
        }
    }

    auto result = sendRequestOpt(request, timeout);

    // A non-cached request is a mutation (set*/toggle/reboot/wifi): drop this
    // device's cached reads so the next read reflects the new state. This keeps
    // the read cache (otherwise manual-invalidation only) correct after writes.
    if (!useCache)
        m_sharedState.clearCacheForIp(ip_);

    if (!result) {
        logger.error("No response");
        return "";
    }

    // Only cache successful, non-error responses so a transient failure
    // doesn't get pinned in the cache (which is manual-invalidation only).
    if (useCache && !result->empty() && result->find("\"err\"") == std::string::npos) {
        m_sharedState.putCache(ip_, request, *result);
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

    // Announce that we're accepting; unblocks ESPClient's constructor.
    {
        std::lock_guard lock(listen_mtx);
        listening = true;
    }
    listen_cv.notify_all();

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

        // Guard against a device that sends the response but doesn't close the
        // connection: if no data arrives for a bit, treat the message as done.
        struct timeval tv{};
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        // Accumulate the full response (the device closes the connection when
        // done) and push it as ONE message. The previous per-recv push split
        // segmented/partial TCP reads into separate queue items, which broke
        // JSON parsing for anything that didn't arrive in a single segment.
        std::string message;
        while (running)
        {
            char buffer[4096];
            ssize_t n = recv(clientSock, buffer, sizeof(buffer), 0);

            if (n > 0) {
                message.append(buffer, n);
                continue;
            }
            // n == 0: peer closed. n < 0: idle timeout/error → message complete.
            break;
        }

        if (!message.empty())
            queue.push(std::move(message));

        close(clientSock);
    }

    close(serverSock);
}
