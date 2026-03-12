#include "main_window.h"
#include "pages/login_page.h"
#include "pages/admin_page.h"
#include "pages/teacher_page.h"
#include "pages/student_page.h"
#include <common/protocol/message_type.h>
#include <QStatusBar>
#include <QMessageBox>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Thunder Class");
    resize(1100, 750);

    client_ = new TcpClient(this);
    stack_ = new QStackedWidget(this);
    setCentralWidget(stack_);

    loginPage_ = new LoginPage(client_, this);
    adminPage_ = new AdminPage(client_, &session_, this);
    teacherPage_ = new TeacherPage(client_, &session_, this);
    studentPage_ = new StudentPage(client_, &session_, this);

    stack_->addWidget(loginPage_);   // 0
    stack_->addWidget(adminPage_);   // 1
    stack_->addWidget(teacherPage_); // 2
    stack_->addWidget(studentPage_); // 3

    connect(client_, &TcpClient::connected, this, &MainWindow::onConnected);
    connect(client_, &TcpClient::disconnected, this, &MainWindow::onDisconnected);
    connect(client_, &TcpClient::errorOccurred, this, &MainWindow::onError);
    connect(client_, &TcpClient::messageReceived, this, &MainWindow::onMessage);

    connect(loginPage_, &LoginPage::loginSuccess, this, &MainWindow::onLoginSuccess);
    connect(adminPage_, &AdminPage::logoutRequested, this, &MainWindow::onLogout);
    connect(teacherPage_, &TeacherPage::logoutRequested, this, &MainWindow::onLogout);
    connect(studentPage_, &StudentPage::logoutRequested, this, &MainWindow::onLogout);

    statusBar()->showMessage("Configure server and click Connect");
}

void MainWindow::onConnected() {
    statusBar()->showMessage("Connected to server");
}

void MainWindow::onDisconnected() {
    statusBar()->showMessage("Disconnected from server");
    setWindowTitle("Thunder Class");
    session_.clear();
    stack_->setCurrentIndex(LOGIN);
}

void MainWindow::onError(const QString& err) {
    statusBar()->showMessage("Error: " + err);
}

void MainWindow::onMessage(const nlohmann::json& msg) {
    std::string type = msg.value("type", "");

    // Route to the active page
    int currentPage = stack_->currentIndex();
    if (currentPage == LOGIN) {
        loginPage_->handleMessage(msg);
    } else if (currentPage == ADMIN) {
        adminPage_->handleMessage(msg);
    } else if (currentPage == TEACHER) {
        teacherPage_->handleMessage(msg);
    } else if (currentPage == STUDENT) {
        studentPage_->handleMessage(msg);
    }
}

void MainWindow::onLoginSuccess(int userId, const QString& name, const QString& role) {
    session_.loggedIn = true;
    session_.userId = userId;
    session_.name = name.toStdString();
    session_.role = role_from_string(role.toStdString());

    setWindowTitle(QString("Thunder Class - %1 (%2)").arg(name, role));
    statusBar()->showMessage(QString("Logged in as %1 (%2)").arg(name, role));

    if (session_.role == Role::ADMIN) {
        stack_->setCurrentIndex(ADMIN);
        adminPage_->onActivated();
    } else if (session_.role == Role::TEACHER) {
        stack_->setCurrentIndex(TEACHER);
        teacherPage_->onActivated();
    } else {
        stack_->setCurrentIndex(STUDENT);
        studentPage_->onActivated();
    }
}

void MainWindow::onLogout() {
    session_.clear();
    setWindowTitle("Thunder Class");
    stack_->setCurrentIndex(LOGIN);
    statusBar()->showMessage("Logged out");
}
