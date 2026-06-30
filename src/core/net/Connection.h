#pragma once
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
        Log logger;
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

        bool isOpen() const { return sock_ >= 0; }

    private:
        void closeSocket() noexcept;
    };

}
