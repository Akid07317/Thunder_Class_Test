#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include "network/tcp_client.h"
#include "core/session.h"

class LoginPage;
class AdminPage;
class TeacherPage;
class StudentPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void onConnected();
    void onDisconnected();
    void onError(const QString& err);
    void onMessage(const nlohmann::json& msg);
    void onLoginSuccess(int userId, const QString& name, const QString& role);
    void onLogout();

    TcpClient* client_;
    Session session_;
    QStackedWidget* stack_;

    LoginPage* loginPage_;
    AdminPage* adminPage_;
    TeacherPage* teacherPage_;
    StudentPage* studentPage_;

    enum PageIndex { LOGIN = 0, ADMIN = 1, TEACHER = 2, STUDENT = 3 };
};
