#pragma once
#include <QObject>
#include <QTcpSocket>
#include <nlohmann/json.hpp>
#include <vector>
#include <cstdint>

class TcpClient : public QObject {
    Q_OBJECT
public:
    explicit TcpClient(QObject* parent = nullptr);

    void connectToServer(const QString& host, quint16 port);
    void disconnect();
    bool isConnected() const;
    void send(const nlohmann::json& msg);

signals:
    void connected();
    void disconnected();
    void messageReceived(const nlohmann::json& msg);
    void errorOccurred(const QString& error);

private slots:
    void onReadyRead();
    void onError(QAbstractSocket::SocketError err);

private:
    QTcpSocket* socket_;
    std::vector<uint8_t> recvBuffer_;
};
