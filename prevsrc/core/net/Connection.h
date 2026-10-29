#pragma once
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <cstdint>

#include "support/log/Log.h"

namespace core::net
{

    // RAII wrapper over a single per-request TCP connection to an ESP device.
    // Owns one socket fd: open() connects, send() frames + writes, and the
    // destructor closes the fd on every path. Non-copyable, movable.
    class Connection
    {
        Log logger{"conn"};
        std::string ip_;
        uint16_t port_;
        int sock_ = -1;

    public:
        explicit Connection(std::string ip, uint16_t port = 8888);
        ~Connection();

        Connection(const Connection &) = delete;
        Connection &operator=(const Connection &) = delete;
        Connection(Connection &&other) noexcept;
        Connection &operator=(Connection &&other) noexcept;

        // socket() + connect(). Returns false (and logs) on failure.
        bool open();

        // Appends the '\n' the firmware's readStringUntil('\n') expects, then
        // writes the whole payload. Returns false (and logs) on failure.
        bool send(std::string_view payload);

        // Waits up to `wait` for a full '\n'-terminated line on this socket
        // (firmware >= 0.99 replies here). nullopt if none yet or the peer
        // closed; see peerClosed().
        std::optional<std::string> readLine(std::chrono::milliseconds wait);

        bool isOpen() const { return sock_ >= 0; }
        bool peerClosed() const { return peerClosed_; }

    private:
        void closeSocket() noexcept;

        std::string rx_; // received bytes not yet returned as a line
        bool peerClosed_ = false;
    };

}
