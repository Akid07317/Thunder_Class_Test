#pragma once
// Cross-platform socket helpers for test clients

#include <common/protocol/protocol.h>
#include <common/protocol/message_type.h>
#include <iostream>
#include <vector>
#include <cstring>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using ssize_t = int;

  static inline void initSockets() {
      WSADATA wsa;
      WSAStartup(MAKEWORD(2, 2), &wsa);
  }
  static inline void cleanupSockets() { WSACleanup(); }
  static inline int  sockRead(int fd, void* buf, size_t len)  { return recv(fd, (char*)buf, (int)len, 0); }
  static inline int  sockWrite(int fd, const void* buf, size_t len) { return ::send(fd, (const char*)buf, (int)len, 0); }
  static inline void sockClose(int fd) { closesocket(fd); }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>

  static inline void initSockets() {}
  static inline void cleanupSockets() {}
  static inline ssize_t sockRead(int fd, void* buf, size_t len)  { return read(fd, buf, len); }
  static inline ssize_t sockWrite(int fd, const void* buf, size_t len) { return write(fd, buf, len); }
  static inline void sockClose(int fd) { close(fd); }
#endif

static int connectToServer(const char* host, uint16_t port) {
    initSockets();
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        sockClose(fd);
        return -1;
    }
    return fd;
}

static void sendMsg(int fd, const nlohmann::json& msg) {
    auto data = Protocol::frame(msg);
    sockWrite(fd, data.data(), data.size());
}

static nlohmann::json recvMsg(int fd) {
    std::vector<uint8_t> buffer;
    uint8_t buf[4096];
    nlohmann::json result;

    while (true) {
        ssize_t n = sockRead(fd, buf, sizeof(buf));
        if (n <= 0) {
            std::cerr << "Connection closed\n";
            return {};
        }
        buffer.insert(buffer.end(), buf, buf + n);
        if (Protocol::extractFrame(buffer, result)) {
            return result;
        }
    }
}

// Receive with timeout (try to read, return empty if nothing available)
// Used for push notifications that may or may not arrive
static nlohmann::json tryRecvMsg(int fd, int timeoutMs = 500) {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(fd, &readSet);
    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    if (select(fd + 1, &readSet, nullptr, nullptr, &tv) <= 0) {
        return {};
    }
    return recvMsg(fd);
}

static void printResp(const std::string& label, const nlohmann::json& resp) {
    std::cout << "\n=== " << label << " ===\n" << resp.dump(2) << "\n";
}
