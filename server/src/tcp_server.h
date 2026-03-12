#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <nlohmann/json.hpp>

class TcpServer {
public:
    using MessageHandler = std::function<void(int fd, const nlohmann::json& msg)>;
    using DisconnectHandler = std::function<void(int fd)>;

    explicit TcpServer(uint16_t port);
    ~TcpServer();

    void setMessageHandler(MessageHandler handler);
    void setDisconnectHandler(DisconnectHandler handler);

    // Send a JSON message to a specific client fd.
    void send(int fd, const nlohmann::json& msg);

    // Run the event loop (blocks). Call stop() from a handler to exit.
    void run();
    void stop();

private:
    uint16_t port_;
    int listenFd_ = -1;
    bool running_ = false;

    MessageHandler onMessage_;
    DisconnectHandler onDisconnect_;

    // Per-connection receive buffers
    std::unordered_map<int, std::vector<uint8_t>> buffers_;
    // Per-connection last activity timestamp (for idle timeout)
    std::unordered_map<int, std::time_t> lastActivity_;
    static constexpr int IDLE_TIMEOUT_SECONDS = 300;  // 5 minutes

    void acceptConnection();
    void readFromClient(int fd);
    void closeClient(int fd);
};
