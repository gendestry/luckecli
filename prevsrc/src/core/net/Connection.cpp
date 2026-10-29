#include "core/net/Connection.h"

#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <utility>

namespace core::net
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
        : logger("CONN"), ip_(std::move(other.ip_)), port_(other.port_), sock_(other.sock_),
          rx_(std::move(other.rx_)), peerClosed_(other.peerClosed_)
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
            rx_ = std::move(other.rx_);
            peerClosed_ = other.peerClosed_;
            other.sock_ = -1;
        }
        return *this;
    }

    std::optional<std::string> Connection::readLine(std::chrono::milliseconds wait)
    {
        auto takeLine = [this]() -> std::optional<std::string>
        {
            size_t end = rx_.find('\n');
            if (end == std::string::npos)
                return std::nullopt;
            std::string line = rx_.substr(0, end);
            rx_.erase(0, end + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            return line;
        };

        if (auto line = takeLine())
            return line;
        if (sock_ < 0 || peerClosed_)
            return std::nullopt;

        pollfd pfd{sock_, POLLIN, 0};
        if (::poll(&pfd, 1, static_cast<int>(wait.count())) <= 0)
            return std::nullopt;

        char buf[4096];
        ssize_t n = ::recv(sock_, buf, sizeof(buf), 0);
        if (n <= 0)
        {
            // 0 = peer closed; < 0 = error. Either way nothing more comes here.
            peerClosed_ = true;
            return std::nullopt;
        }
        rx_.append(buf, static_cast<size_t>(n));
        return takeLine();
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
