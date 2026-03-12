#include "tcp_client.h"
#include <common/protocol/protocol.h>

TcpClient::TcpClient(QObject* parent)
    : QObject(parent)
    , socket_(new QTcpSocket(this))
{
    connect(socket_, &QTcpSocket::connected, this, &TcpClient::connected);
    connect(socket_, &QTcpSocket::disconnected, this, &TcpClient::disconnected);
    connect(socket_, &QTcpSocket::readyRead, this, &TcpClient::onReadyRead);
    connect(socket_, &QTcpSocket::errorOccurred, this, &TcpClient::onError);
}

void TcpClient::connectToServer(const QString& host, quint16 port) {
    recvBuffer_.clear();
    socket_->connectToHost(host, port);
}

void TcpClient::disconnect() {
    socket_->disconnectFromHost();
}

bool TcpClient::isConnected() const {
    return socket_->state() == QAbstractSocket::ConnectedState;
}

void TcpClient::send(const nlohmann::json& msg) {
    if (socket_->state() != QAbstractSocket::ConnectedState) return;
    auto frame = Protocol::frame(msg);
    socket_->write(reinterpret_cast<const char*>(frame.data()), frame.size());
}

void TcpClient::onReadyRead() {
    QByteArray data = socket_->readAll();
    recvBuffer_.insert(recvBuffer_.end(), data.begin(), data.end());

    nlohmann::json out;
    while (Protocol::extractFrame(recvBuffer_, out)) {
        emit messageReceived(out);
    }
}

void TcpClient::onError(QAbstractSocket::SocketError) {
    emit errorOccurred(socket_->errorString());
}
