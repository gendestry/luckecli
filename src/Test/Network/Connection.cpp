#include "Connection.h"
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <mutex>
#include "Utils/Logging/Logger.h"
#include <nlohmann/json.hpp>

namespace Test::Network
{
    Connection::Connection(const std::string &ip)
        : m_ip(ip), logger(ip)
    {
        listen_thread = std::thread(&Connection::listen_ingest_tcp, this);
    }

    void Connection::listen_ingest_tcp()
    {
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

        if (bind(serverSock, (sockaddr *)&addr, sizeof(addr)) < 0)
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

            int clientSock = accept(serverSock, (sockaddr *)&clientAddr, &len);

            if (clientSock < 0)
            {
                if (running)
                {
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

                if (n > 0)
                {
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

    std::optional<std::string> Connection::sendRequestOpt(const std::string &request, std::chrono::milliseconds timeout)
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            logger.error("Socket creation failed: {}", strerror(errno));
            return std::nullopt;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(8888);
        inet_pton(AF_INET, m_ip.c_str(), &addr.sin_addr);

        if (connect(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
        {
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

}