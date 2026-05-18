#pragma once

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket() { close(); }

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool open() {
        fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd_ < 0) return false;
        int on = 1;
        setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = 0;
        if (bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            close();
            return false;
        }
        socklen_t len = sizeof(addr);
        getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &len);
        localPort_ = ntohs(addr.sin_port);
        return true;
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    int fd() const { return fd_; }
    int localPort() const { return localPort_; }

    bool sendTo(const std::vector<uint8_t>& data, const std::string& ip, int port) {
        sockaddr_in dest{};
        dest.sin_family = AF_INET;
        dest.sin_port = htons(static_cast<uint16_t>(port));
        if (inet_pton(AF_INET, ip.c_str(), &dest.sin_addr) != 1) return false;
        ssize_t n = sendto(fd_, data.data(), data.size(), 0, reinterpret_cast<sockaddr*>(&dest),
                           sizeof(dest));
        return n == static_cast<ssize_t>(data.size());
    }

    ssize_t recvFrom(std::vector<uint8_t>& buf, std::string& fromIp, int& fromPort) {
        buf.resize(4096);
        sockaddr_in src{};
        socklen_t slen = sizeof(src);
        ssize_t n = recvfrom(fd_, buf.data(), buf.size(), 0, reinterpret_cast<sockaddr*>(&src), &slen);
        if (n < 0) return n;
        buf.resize(static_cast<size_t>(n));
        char ipstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src.sin_addr, ipstr, sizeof(ipstr));
        fromIp = ipstr;
        fromPort = ntohs(src.sin_port);
        return n;
    }

private:
    int fd_{-1};
    int localPort_{0};
};
