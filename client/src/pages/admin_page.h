#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <nlohmann/json.hpp>

class TcpClient;
struct Session;

class AdminPage : public QWidget {
    Q_OBJECT
public:
    explicit AdminPage(TcpClient* client, Session* session, QWidget* parent = nullptr);
    void handleMessage(const nlohmann::json& msg);
    void onActivated();

signals:
    void logoutRequested();

private:
    void onCreateUser();
    void onRefreshUsers();
    void onLogout();

    TcpClient* client_;
    Session* session_;

    QLineEdit* newUsername_;
    QLineEdit* newPassword_;
    QLineEdit* newName_;
    QComboBox* newRole_;
    QPushButton* createBtn_;
    QLabel* createStatus_;
    QTableWidget* userTable_;
    QPushButton* refreshBtn_;
};
