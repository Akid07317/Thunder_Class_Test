#include "tcp_server.h"
#include <common/protocol/protocol.h>
#include <QHostAddress>
#include <iostream>

TcpServer::TcpServer(uint16_t port, QObject* parent)
    : QObject(parent), port_(port)
{
    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);

    // Periodic idle check every 30 seconds
    idleTimer_ = new QTimer(this);
    connect(idleTimer_, &QTimer::timeout, this, &TcpServer::onIdleCheck);
    idleTimer_->start(30000);
}

TcpServer::~TcpServer() {
    for (auto& [id, socket] : clients_) {
        socket->disconnectFromHost();
    }
}

void TcpServer::setMessageHandler(MessageHandler handler) {
    onMessage_ = std::move(handler);
}

void TcpServer::setDisconnectHandler(DisconnectHandler handler) {
    onDisconnect_ = std::move(handler);
}

void TcpServer::send(int id, const nlohmann::json& msg) {
    auto it = clients_.find(id);
    if (it == clients_.end()) return;

    QTcpSocket* socket = it->second;
    if (socket->state() != QAbstractSocket::ConnectedState) return;

    auto data = Protocol::frame(msg);
    qint64 totalSent = 0;
    qint64 dataSize = static_cast<qint64>(data.size());
    while (totalSent < dataSize) {
        qint64 n = socket->write(
            reinterpret_cast<const char*>(data.data()) + totalSent,
            dataSize - totalSent);
        if (n < 0) {
            std::cerr << "Write error on client " << id << ": "
                      << socket->errorString().toStdString() << "\n";
            return;
        }
        totalSent += n;
    }
    socket->flush();
}

bool TcpServer::start() {
    if (!server_->listen(QHostAddress::Any, port_)) {
        std::cerr << "Failed to listen on port " << port_ << ": "
                  << server_->errorString().toStdString() << "\n";
        return false;
    }
    std::cout << "Server listening on port " << port_ << "\n";
    return true;
}

void TcpServer::onNewConnection() {
    while (server_->hasPendingConnections()) {
        QTcpSocket* socket = server_->nextPendingConnection();
        int id = nextClientId_++;

        clients_[id] = socket;
        socketToId_[socket] = id;
        buffers_[id] = {};
        lastActivity_[id] = std::time(nullptr);

        std::cout << "Client connected: id=" << id
                  << " from " << socket->peerAddress().toString().toStdString() << "\n";

        connect(socket, &QTcpSocket::readyRead, this, &TcpServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &TcpServer::onDisconnected);
    }
}

void TcpServer::onReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    auto sit = socketToId_.find(socket);
    if (sit == socketToId_.end()) return;
    int id = sit->second;

    lastActivity_[id] = std::time(nullptr);

    QByteArray data = socket->readAll();
    auto& buffer = buffers_[id];
    buffer.insert(buffer.end(), data.begin(), data.end());

    nlohmann::json msg;
    while (Protocol::extractFrame(buffer, msg)) {
        if (onMessage_) {
            onMessage_(id, msg);
        }
    }
}

void TcpServer::onDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    auto sit = socketToId_.find(socket);
    if (sit == socketToId_.end()) return;
    int id = sit->second;

    std::cout << "Client disconnected: id=" << id << "\n";
    if (onDisconnect_) onDisconnect_(id);

    removeClient(id);
}

void TcpServer::onIdleCheck() {
    std::time_t now = std::time(nullptr);
    std::vector<int> idleIds;
    for (auto& [id, lastTime] : lastActivity_) {
        if (now - lastTime > IDLE_TIMEOUT_SECONDS) idleIds.push_back(id);
    }
    for (int id : idleIds) {
        std::cout << "Closing idle connection: id=" << id << "\n";
        if (onDisconnect_) onDisconnect_(id);
        auto it = clients_.find(id);
        if (it != clients_.end()) {
            it->second->disconnectFromHost();
        }
        removeClient(id);
    }
}

void TcpServer::removeClient(int id) {
    auto it = clients_.find(id);
    if (it != clients_.end()) {
        QTcpSocket* socket = it->second;
        socketToId_.erase(socket);
        socket->deleteLater();
        clients_.erase(it);
    }
    buffers_.erase(id);
    lastActivity_.erase(id);
}
