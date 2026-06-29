#include "Connection.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <utility>

namespace Test::Network
{

    Connection::Connection(std::string ip, uint16_t port)
        : logger("CONN"), ip_(std::move(ip)), port_(port)
    {
    }

    Connection::~Connection()
    {
        closeSocket();
    }

    Connection::Connection(Connection &&other) noexcept
        : logger("CONN"), ip_(std::move(other.ip_)), port_(other.port_), sock_(other.sock_)
    {
        other.sock_ = -1;
    }

    Connection &Connection::operator=(Connection &&other) noexcept
    {
        if (this != &other)
        {
            closeSocket();
            ip_ = std::move(other.ip_);
            port_ = other.port_;
            sock_ = other.sock_;
            other.sock_ = -1;
        }
        return *this;
    }

    void Connection::closeSocket() noexcept
    {
        if (sock_ >= 0)
        {
            ::close(sock_);
            sock_ = -1;
        }
    }

    bool Connection::open()
    {
        sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ < 0)
        {
            logger.error("Socket creation failed: {}", strerror(errno));
            return false;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        if (inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) != 1)
        {
            logger.error("Invalid address: {}", ip_);
            closeSocket();
            return false;
        }

        if (connect(sock_, (sockaddr *)&addr, sizeof(addr)) < 0)
        {
            logger.error("Error connecting to {}:{} - {}", ip_, port_, strerror(errno));
            closeSocket();
            return false;
        }

        return true;
    }

    bool Connection::send(std::string_view payload)
    {
        if (sock_ < 0)
        {
            logger.error("send() on a closed connection to {}", ip_);
            return false;
        }

        // The firmware reads the request with readStringUntil('\n'), so the
        // payload MUST be newline-terminated or the read never completes.
        std::string framed(payload);
        framed.push_back('\n');

        size_t total = 0;
        while (total < framed.size())
        {
            ssize_t n = ::send(sock_, framed.data() + total, framed.size() - total, 0);
            if (n <= 0)
            {
                logger.error("Error sending to {}: {}", ip_, strerror(errno));
                return false;
            }
            total += static_cast<size_t>(n);
        }
        return true;
    }

}
