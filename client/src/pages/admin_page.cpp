#include "admin_page.h"
#include "network/tcp_client.h"
#include "core/session.h"
#include <common/protocol/message_type.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>

AdminPage::AdminPage(TcpClient* client, Session* session, QWidget* parent)
    : QWidget(parent), client_(client), session_(session)
{
    auto* layout = new QVBoxLayout(this);

    // Header
    auto* header = new QHBoxLayout;
    auto* titleLabel = new QLabel("Admin Panel", this);
    QFont f; f.setPointSize(16); f.setBold(true);
    titleLabel->setFont(f);
    header->addWidget(titleLabel);
    header->addStretch();
    auto* logoutBtn = new QPushButton("Logout", this);
    header->addWidget(logoutBtn);
    layout->addLayout(header);

    // Create user group
    auto* createGroup = new QGroupBox("Create User", this);
    auto* createLayout = new QFormLayout(createGroup);
    newUsername_ = new QLineEdit(this);
    newPassword_ = new QLineEdit(this);
    newPassword_->setEchoMode(QLineEdit::Password);
    newName_ = new QLineEdit(this);
    newRole_ = new QComboBox(this);
    newRole_->addItems({"TEACHER", "STUDENT"});
    createLayout->addRow("Username:", newUsername_);
    createLayout->addRow("Password:", newPassword_);
    createLayout->addRow("Display Name:", newName_);
    createLayout->addRow("Role:", newRole_);
    createBtn_ = new QPushButton("Create", this);
    createLayout->addRow(createBtn_);
    createStatus_ = new QLabel(this);
    createLayout->addRow(createStatus_);
    layout->addWidget(createGroup);

    // User list
    auto* listGroup = new QGroupBox("Users", this);
    auto* listLayout = new QVBoxLayout(listGroup);
    refreshBtn_ = new QPushButton("Refresh", this);
    listLayout->addWidget(refreshBtn_);
    userTable_ = new QTableWidget(this);
    userTable_->setColumnCount(4);
    userTable_->setHorizontalHeaderLabels({"ID", "Username", "Name", "Role"});
    userTable_->horizontalHeader()->setStretchLastSection(true);
    userTable_->setColumnWidth(0, 60);
    userTable_->setColumnWidth(1, 150);
    userTable_->setColumnWidth(2, 150);
    userTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    listLayout->addWidget(userTable_);
    layout->addWidget(listGroup);

    connect(createBtn_, &QPushButton::clicked, this, &AdminPage::onCreateUser);
    connect(refreshBtn_, &QPushButton::clicked, this, &AdminPage::onRefreshUsers);
    connect(logoutBtn, &QPushButton::clicked, this, &AdminPage::onLogout);
}

void AdminPage::onActivated() {
    onRefreshUsers();
}

void AdminPage::onCreateUser() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::CREATE_USER_REQ);
    req["username"] = newUsername_->text().trimmed().toStdString();
    req["password"] = newPassword_->text().toStdString();
    req["name"] = newName_->text().trimmed().toStdString();
    req["role"] = newRole_->currentText().toStdString();
    client_->send(req);
    createStatus_->setText("Creating...");
}

void AdminPage::onRefreshUsers() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::LIST_USERS_REQ);
    client_->send(req);
}

void AdminPage::onLogout() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::LOGOUT_REQ);
    client_->send(req);
    emit logoutRequested();
}

void AdminPage::handleMessage(const nlohmann::json& msg) {
    std::string type = msg.value("type", "");

    if (type == to_string(MessageType::CREATE_USER_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            createStatus_->setStyleSheet("color: green;");
            createStatus_->setText("User created successfully");
            newUsername_->clear();
            newPassword_->clear();
            newName_->clear();
            onRefreshUsers();
        } else {
            createStatus_->setStyleSheet("color: red;");
            createStatus_->setText(QString::fromStdString(msg.value("message", "Failed")));
        }
    }
    else if (type == to_string(MessageType::LIST_USERS_RESP)) {
        auto users = msg.value("users", nlohmann::json::array());
        userTable_->setRowCount(users.size());
        for (int i = 0; i < (int)users.size(); i++) {
            auto& u = users[i];
            userTable_->setItem(i, 0, new QTableWidgetItem(QString::number(u.value("userId", 0))));
            userTable_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(u.value("username", ""))));
            userTable_->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(u.value("name", ""))));
            userTable_->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(u.value("role", ""))));
        }
    }
}
