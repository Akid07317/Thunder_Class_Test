#include "login_page.h"
#include "network/tcp_client.h"
#include <common/protocol/message_type.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

LoginPage::LoginPage(TcpClient* client, QWidget* parent)
    : QWidget(parent), client_(client)
{
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    // Title
    auto* title = new QLabel("Thunder Class", this);
    title->setAlignment(Qt::AlignCenter);
    title->setObjectName("loginTitle");
    layout->addWidget(title);
    layout->addSpacing(20);

    // Server connection group
    auto* serverGroup = new QGroupBox("Server", this);
    auto* serverLayout = new QHBoxLayout(serverGroup);
    hostEdit_ = new QLineEdit(this);
    hostEdit_->setText("127.0.0.1");
    hostEdit_->setPlaceholderText("Host");
    hostEdit_->setMinimumWidth(160);
    portSpin_ = new QSpinBox(this);
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(9000);
    connectBtn_ = new QPushButton("Connect", this);
    connectBtn_->setObjectName("connectBtn");
    serverLayout->addWidget(new QLabel("Host:", this));
    serverLayout->addWidget(hostEdit_);
    serverLayout->addWidget(new QLabel("Port:", this));
    serverLayout->addWidget(portSpin_);
    serverLayout->addWidget(connectBtn_);
    layout->addWidget(serverGroup);

    connStatusLabel_ = new QLabel(this);
    connStatusLabel_->setAlignment(Qt::AlignCenter);
    connStatusLabel_->setObjectName("connStatus");
    layout->addWidget(connStatusLabel_);
    layout->addSpacing(10);

    // Login form
    auto* loginGroup = new QGroupBox("Login", this);
    auto* loginLayout = new QVBoxLayout(loginGroup);
    auto* form = new QFormLayout;
    usernameEdit_ = new QLineEdit(this);
    usernameEdit_->setPlaceholderText("Username");
    usernameEdit_->setMinimumWidth(250);
    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setPlaceholderText("Password");
    passwordEdit_->setEchoMode(QLineEdit::Password);
    form->addRow("Username:", usernameEdit_);
    form->addRow("Password:", passwordEdit_);
    loginLayout->addLayout(form);

    loginBtn_ = new QPushButton("Login", this);
    loginBtn_->setObjectName("loginBtn");
    loginBtn_->setEnabled(false);
    loginLayout->addWidget(loginBtn_);
    layout->addWidget(loginGroup);

    statusLabel_ = new QLabel(this);
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setObjectName("errorLabel");
    layout->addWidget(statusLabel_);

    connect(connectBtn_, &QPushButton::clicked, this, &LoginPage::onConnectClicked);
    connect(loginBtn_, &QPushButton::clicked, this, &LoginPage::onLoginClicked);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, &LoginPage::onLoginClicked);
    connect(usernameEdit_, &QLineEdit::returnPressed, [this]() {
        passwordEdit_->setFocus();
    });

    connect(client_, &TcpClient::connected, this, [this]() {
        connStatusLabel_->setText("Connected");
        connStatusLabel_->setStyleSheet("color: #27ae60; font-weight: bold;");
        connectBtn_->setText("Reconnect");
        connectBtn_->setEnabled(true);
        loginBtn_->setEnabled(true);
        usernameEdit_->setFocus();
    });
    connect(client_, &TcpClient::disconnected, this, [this]() {
        connStatusLabel_->setText("Disconnected");
        connStatusLabel_->setStyleSheet("color: #e74c3c;");
        connectBtn_->setEnabled(true);
        loginBtn_->setEnabled(false);
    });
    connect(client_, &TcpClient::errorOccurred, this, [this](const QString& err) {
        connStatusLabel_->setText("Error: " + err);
        connStatusLabel_->setStyleSheet("color: #e74c3c;");
        connectBtn_->setEnabled(true);
    });
}

void LoginPage::onConnectClicked() {
    QString host = hostEdit_->text().trimmed();
    if (host.isEmpty()) host = "127.0.0.1";
    quint16 port = static_cast<quint16>(portSpin_->value());

    connStatusLabel_->setText("Connecting...");
    connStatusLabel_->setStyleSheet("color: #f39c12;");
    connectBtn_->setEnabled(false);
    client_->connectToServer(host, port);
}

void LoginPage::onLoginClicked() {
    QString user = usernameEdit_->text().trimmed();
    QString pass = passwordEdit_->text();
    if (user.isEmpty() || pass.isEmpty()) {
        statusLabel_->setText("Please enter username and password");
        return;
    }
    statusLabel_->setText("Logging in...");
    statusLabel_->setStyleSheet("color: #f39c12;");
    loginBtn_->setEnabled(false);

    nlohmann::json req;
    req["type"] = to_string(MessageType::LOGIN_REQ);
    req["username"] = user.toStdString();
    req["password"] = pass.toStdString();
    client_->send(req);
}

void LoginPage::handleMessage(const nlohmann::json& msg) {
    std::string type = msg.value("type", "");
    if (type != to_string(MessageType::LOGIN_RESP)) return;

    loginBtn_->setEnabled(true);
    bool ok = msg.value("success", false);
    if (ok) {
        statusLabel_->setText("");
        int uid = msg.value("userId", 0);
        QString name = QString::fromStdString(msg.value("name", ""));
        QString role = QString::fromStdString(msg.value("role", "STUDENT"));
        emit loginSuccess(uid, name, role);
    } else {
        std::string reason = msg.value("message", "Login failed");
        statusLabel_->setText(QString::fromStdString(reason));
        statusLabel_->setStyleSheet("color: #e74c3c;");
    }
}
