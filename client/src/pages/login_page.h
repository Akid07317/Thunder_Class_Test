#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <nlohmann/json.hpp>

class TcpClient;

class LoginPage : public QWidget {
    Q_OBJECT
public:
    explicit LoginPage(TcpClient* client, QWidget* parent = nullptr);
    void handleMessage(const nlohmann::json& msg);

signals:
    void loginSuccess(int userId, const QString& name, const QString& role);

private:
    void onConnectClicked();
    void onLoginClicked();

    TcpClient* client_;
    QLineEdit* hostEdit_;
    QSpinBox* portSpin_;
    QPushButton* connectBtn_;
    QLineEdit* usernameEdit_;
    QLineEdit* passwordEdit_;
    QPushButton* loginBtn_;
    QLabel* statusLabel_;
    QLabel* connStatusLabel_;
};
