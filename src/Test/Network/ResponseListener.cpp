//
// Created by bobi on 6. 05. 26.
//

#include "ResponseListener.h"
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
    // std::thread ResponseListener::listen_thread;
    // std::atomic<bool> ResponseListener::running = true;
    // std::mutex ResponseListener::listen_mtx;
    // std::condition_variable ResponseListener::listen_cv;
    // bool ResponseListener::listening = false;
    // bool ResponseListener::inited = false;

    ResponseListener::ResponseListener(Test::SharedState &state)
        : logger("CONFIG"), m_sharedState(state)
    {
        if (!inited)
        {
            inited = true;
            listen_thread = std::thread(&ResponseListener::listen_ingest_tcp, this);
        }

        // Don't issue any request until the listener is accepting, otherwise the
        // device's response connection can be refused before we're ready to receive.
        std::unique_lock lock(listen_mtx);
        listen_cv.wait_for(lock, 2000ms, [&]
                           { return listening; });
    }

    ResponseListener::~ResponseListener()
    {
        // send dummy packet to unblock recvfrom
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(12345);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        sendto(sock, "", 0, 0, (sockaddr *)&addr, sizeof(addr));
        close(sock);
    }

    void ResponseListener::stop()
    {
        if (inited)
        {
            running = false;
            listen_thread.detach();
        }
    }

    // void ResponseListener::listen_ingest_udp()
    // {
    //     int sock = socket(AF_INET, SOCK_DGRAM, 0);

    //     sockaddr_in addr{};
    //     addr.sin_family = AF_INET;
    //     addr.sin_port = htons(12345);
    //     addr.sin_addr.s_addr = INADDR_ANY;

    //     if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
    //     {
    //         perror("bind");
    //         close(sock);
    //         return;
    //     }

    //     while (running)
    //     {
    //         char buffer[4096];
    //         sockaddr_in sender{};
    //         socklen_t len = sizeof(sender);

    //         ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr *)&sender, &len);

    //         if (n <= 0)
    //             continue;

    //         std::string msg(buffer, n);
    //         std::string ip = inet_ntoa(sender.sin_addr);
    //         queue.push(msg);
    //     }

    //     close(sock);
    // }

    void ResponseListener::listen_ingest_tcp()
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

        // Announce that we're accepting; unblocks ResponseListener's constructor.
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
            std::string ip(ipbuf);

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
                m_sharedState.getQueue().push(ip, std::move(message));

            close(clientSock);
        }

        close(serverSock);
    }
}