#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <ctime>
#include <nlohmann/json.hpp>

class TcpServer : public QObject {
    Q_OBJECT
public:
    using MessageHandler = std::function<void(int id, const nlohmann::json& msg)>;
    using DisconnectHandler = std::function<void(int id)>;

    explicit TcpServer(uint16_t port, QObject* parent = nullptr);
    ~TcpServer() override;

    void setMessageHandler(MessageHandler handler);
    void setDisconnectHandler(DisconnectHandler handler);

    // Send a JSON message to a specific client id.
    void send(int id, const nlohmann::json& msg);

    // Start listening. Returns true on success.
    bool start();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onIdleCheck();

private:
    uint16_t port_;
    QTcpServer* server_ = nullptr;

    MessageHandler onMessage_;
    DisconnectHandler onDisconnect_;

    // Map client id -> socket and buffers
    int nextClientId_ = 1;
    std::unordered_map<int, QTcpSocket*> clients_;        // id -> socket
    std::unordered_map<QTcpSocket*, int> socketToId_;      // socket -> id
    std::unordered_map<int, std::vector<uint8_t>> buffers_; // id -> recv buffer
    std::unordered_map<int, std::time_t> lastActivity_;    // id -> last activity

    QTimer* idleTimer_ = nullptr;
    static constexpr int IDLE_TIMEOUT_SECONDS = 300;  // 5 minutes

    void removeClient(int id);
};
