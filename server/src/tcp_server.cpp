#include "tcp_server.h"
#include <common/protocol/protocol.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <ctime>

TcpServer::TcpServer(uint16_t port) : port_(port) {}

TcpServer::~TcpServer() {
    if (listenFd_ >= 0) close(listenFd_);
    for (auto& [fd, _] : buffers_) close(fd);
}

void TcpServer::setMessageHandler(MessageHandler handler) {
    onMessage_ = std::move(handler);
}

void TcpServer::setDisconnectHandler(DisconnectHandler handler) {
    onDisconnect_ = std::move(handler);
}

void TcpServer::send(int fd, const nlohmann::json& msg) {
    auto data = Protocol::frame(msg);
    size_t totalSent = 0;
    while (totalSent < data.size()) {
        ssize_t n = ::write(fd, data.data() + totalSent, data.size() - totalSent);
        if (n < 0) {
            if (errno == EINTR) continue;
            // Write failed (broken pipe, etc.) — let select() detect disconnect
            std::cerr << "Write error on fd=" << fd << ": " << strerror(errno) << "\n";
            return;
        }
        totalSent += n;
    }
}

void TcpServer::run() {
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        std::cerr << "Failed to create socket\n";
        return;
    }

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listenFd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind on port " << port_ << "\n";
        close(listenFd_);
        listenFd_ = -1;
        return;
    }

    listen(listenFd_, 16);
    std::cout << "Server listening on port " << port_ << "\n";

    running_ = true;
    while (running_) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenFd_, &readSet);

        int maxFd = listenFd_;
        for (auto& [fd, _] : buffers_) {
            FD_SET(fd, &readSet);
            maxFd = std::max(maxFd, fd);
        }

        timeval tv{1, 0};  // 1 second timeout for clean shutdown
        int ready = select(maxFd + 1, &readSet, nullptr, nullptr, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;
            std::cerr << "select() error\n";
            break;
        }

        if (FD_ISSET(listenFd_, &readSet)) {
            acceptConnection();
        }

        // Close idle connections
        {
            std::time_t now = std::time(nullptr);
            std::vector<int> idleFds;
            for (auto& [fd, lastTime] : lastActivity_) {
                if (now - lastTime > IDLE_TIMEOUT_SECONDS) idleFds.push_back(fd);
            }
            for (int fd : idleFds) {
                std::cout << "Closing idle connection: fd=" << fd << "\n";
                closeClient(fd);
            }
        }

        // Copy keys to avoid iterator invalidation during closeClient
        std::vector<int> fds;
        for (auto& [fd, _] : buffers_) fds.push_back(fd);

        for (int fd : fds) {
            if (FD_ISSET(fd, &readSet)) {
                readFromClient(fd);
            }
        }
    }
}

void TcpServer::stop() {
    running_ = false;
}

void TcpServer::acceptConnection() {
    sockaddr_in clientAddr{};
    socklen_t len = sizeof(clientAddr);
    int clientFd = accept(listenFd_, (sockaddr*)&clientAddr, &len);
    if (clientFd < 0) return;

    std::cout << "Client connected: fd=" << clientFd
              << " from " << inet_ntoa(clientAddr.sin_addr) << "\n";
    buffers_[clientFd] = {};
    lastActivity_[clientFd] = std::time(nullptr);
}

void TcpServer::readFromClient(int fd) {
    uint8_t buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n <= 0) {
        closeClient(fd);
        return;
    }

    lastActivity_[fd] = std::time(nullptr);
    auto& buffer = buffers_[fd];
    buffer.insert(buffer.end(), buf, buf + n);

    nlohmann::json msg;
    while (Protocol::extractFrame(buffer, msg)) {
        if (onMessage_) {
            onMessage_(fd, msg);
        }
    }
}

void TcpServer::closeClient(int fd) {
    std::cout << "Client disconnected: fd=" << fd << "\n";
    if (onDisconnect_) onDisconnect_(fd);
    close(fd);
    buffers_.erase(fd);
    lastActivity_.erase(fd);
}
