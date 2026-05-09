//
// Created by bobi on 6. 05. 26.
//

#include "Heartbeat.h"
// std::atomic<bool> Heartbeat::running_ingest = false;
// bool Heartbeat::inited = false;
// std::thread Heartbeat::ingest_thread;

#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <nlohmann/json.hpp>


Heartbeat::Heartbeat() : logger("Heartbeat") {
    start();
}

Heartbeat::~Heartbeat() {
    stop();
}

void Heartbeat::start() {
    if (inited) return;

    running_ingest = true;

    ingest_thread = std::thread(&Heartbeat::packet_ingest, this);

    inited = true;
}

void Heartbeat::stop() {
    running_ingest = false;

    if (ingest_thread.joinable())
        ingest_thread.join();
}

void Heartbeat::packet_ingest() {
    uint32_t currentIndex = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        logger.error("Socket creation failed: {}", strerror(errno));
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12343);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        logger.error("Bind failed: {}", strerror(errno));
        close(sock);
        return;
    }

    while (running_ingest.load()) {
        char buffer[512];
        sockaddr_in sender{};
        socklen_t sender_len = sizeof(sender);

        ssize_t bytes_received = recvfrom(
            sock,
            buffer,
            sizeof(buffer),
            0,
            (sockaddr*)&sender,
            &sender_len
        );

        if (bytes_received < 0) {
            logger.error("Error reading: {}", strerror(errno));
            continue;
        }

        std::string ip = inet_ntoa(sender.sin_addr);
        std::string data(buffer, bytes_received);

        {
            std::lock_guard lock(clients_mutex);
            using json = nlohmann::json;
            json jarr = json::parse(data);

            if (!clients.contains(ip)) {
                ClientInfo info;
                info.index = currentIndex++;
                info.ip = ip;


                for (const auto& item : jarr["fixtures"]) {
                    ClientInfo::Description f;
                    f.name = item["name"];
                    f.type = item["type"];

                    info.descriptions.push_back(std::move(f));
                }
                clients[ip] = std::move(info);
                clients_list.push_back(&clients[ip]);
            } else {
                std::vector<ClientInfo::Description> descs;
                for (const auto& item : jarr["fixtures"]) {
                    ClientInfo::Description f;
                    f.name = item["name"];
                    f.type = item["type"];

                    descs.push_back(std::move(f));
                }
                clients[ip].descriptions = std::move(descs);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    close(sock);
}

std::vector<ClientInfo*> Heartbeat::getClients() {
    std::lock_guard lock(clients_mutex);
    return clients_list;
}

uint32_t Heartbeat::size() {
    std::lock_guard lock(clients_mutex);
    return clients_list.size();
}
