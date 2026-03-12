#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QListWidget>
#include <QStackedWidget>
#include <QTextEdit>
#include <QRadioButton>
#include <QTimer>
#include <QScrollArea>
#include <nlohmann/json.hpp>

#ifdef HAS_QT_MULTIMEDIA
#include <QAudioSource>
#include <QAudioSink>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QIODevice>
#endif

class TcpClient;
struct Session;

class StudentPage : public QWidget {
    Q_OBJECT
public:
    explicit StudentPage(TcpClient* client, Session* session, QWidget* parent = nullptr);
    void handleMessage(const nlohmann::json& msg);
    void onActivated();

signals:
    void logoutRequested();

private:
    void setupClassListView(QWidget* page);
    void setupClassroomView(QWidget* page);

    void onRefreshClasses();
    void onJoinClass();
    void onLeaveClass();
    void onEnterClassroom();

    void onCheckin();
    void onSubmitAnswer();
    void onLeaveClassroom();

    // Audio
    void startMicCapture();
    void stopMicCapture();
    void playAudioFrame(const std::string& audioData);
    void stopAudioPlayback();

    TcpClient* client_;
    Session* session_;
    QStackedWidget* stack_;

    // Class list
    QTableWidget* classTable_;
    QLabel* classStatus_;

    // Classroom
    QLabel* classroomTitle_;
    int currentClassId_ = 0;

    // Notifications
    QTextEdit* notificationArea_;

    // Question area
    QLabel* questionLabel_;
    QRadioButton* optionBtns_[4];
    QPushButton* submitBtn_;
    int currentQuestionId_ = 0;

    // Screen sharing display
    QLabel* screenLabel_;
    QLabel* shareStatus_;

    // Audio
    QLabel* audioStatus_;
    bool micGranted_ = false;
    int micFrameSeq_ = 0;

#ifdef HAS_QT_MULTIMEDIA
    QAudioSource* audioSource_ = nullptr;
    QIODevice* audioIO_ = nullptr;
    QTimer* captureTimer_ = nullptr;
    QAudioSink* audioSink_ = nullptr;
    QIODevice* playbackIO_ = nullptr;
#endif

    QLabel* actionStatus_;
};
