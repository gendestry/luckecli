//
// Created by bobi on 6. 05. 26.
//

#include "Heartbeat.h"

#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <nlohmann/json.hpp>


Heartbeat::Heartbeat(SharedState& state) : logger("Heartbeat"), m_sharedState(state){
    start();
}

Heartbeat::~Heartbeat() {
    stop();
}

void Heartbeat::start() {
    if (inited) {
        return;
    }

    running_ingest = true;
    ingest_thread = std::thread(&Heartbeat::packet_ingest, this);
    inited = true;
}

void Heartbeat::stop() {
    running_ingest = false;

    if (ingest_thread.joinable()) {
        ingest_thread.join();
    }
}

void Heartbeat::packet_ingest() {
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

        const std::string ip = inet_ntoa(sender.sin_addr);
        std::string data(buffer, bytes_received);

        using json = nlohmann::json;

        json jarr = json::parse(data);
        std::string version = jarr.value("version", "outdated");

        int id = 0;
        for (const auto& item : jarr["fixtures"]) {
            // ClientInfo::Description f;
            ClientInfo info;
            info.selected = id++;
            info.ip = ip;
            info.version = version;
            info.description.name = item.value("name", "");
            info.description.type = item.value("type", "");
            if(item.contains("basic"))
            {
                std::string b = item["basic"];
                json bjson = json::parse(b);
                info.description.num_leds = bjson.value("num_leds", 0);
            }

            m_sharedState.addClient(std::move(info), ip);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    close(sock);
}