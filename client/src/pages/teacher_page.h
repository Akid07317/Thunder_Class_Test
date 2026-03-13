#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QStackedWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QTimer>
#include <QScreen>
#include <QGuiApplication>
#include <QBuffer>
#include <nlohmann/json.hpp>

#ifdef HAS_QT_MULTIMEDIA
#include <QAudioSource>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QIODevice>
#endif

class TcpClient;
struct Session;

class TeacherPage : public QWidget {
    Q_OBJECT
public:
    explicit TeacherPage(TcpClient* client, Session* session, QWidget* parent = nullptr);
    void handleMessage(const nlohmann::json& msg);
    void onActivated();

signals:
    void logoutRequested();

private:
    void setupClassListView(QWidget* page);
    void setupClassroomView(QWidget* page);

    // Class list actions
    void onCreateClass();
    void onRefreshClasses();
    void onStartClass();
    void onEndClass();
    void onEnterClassroom();

    // In-classroom actions
    void onOpenCheckin();
    void onCheckinStatus();
    void onRandomCall();
    void onPublishQuestion();
    void onGetAnswers();
    void onViewSummary();
    void onExportCSV();
    void onAttentionStatus();
    void onLeaveClassroom();

    // Screen sharing actions
    void onStartScreenShare();
    void onStopScreenShare();
    void captureAndSendFrame();

    // Audio actions
    void onStartAudio();
    void onStopAudio();
    void onGrantMic();
    void onRevokeMic();
    void startAudioCapture();
    void stopAudioCapture();
    void playAudioFrame(const std::string& audioData);

    TcpClient* client_;
    Session* session_;
    QStackedWidget* stack_;

    // Class list view
    QLineEdit* classNameEdit_;
    QTableWidget* classTable_;
    QLabel* classStatus_;

    // Classroom view
    QLabel* classroomTitle_;
    int currentClassId_ = 0;
    std::string currentClassName_;

    // Members list
    QListWidget* memberList_;

    // Question
    QTextEdit* questionText_;
    QLineEdit* optionA_;
    QLineEdit* optionB_;
    QLineEdit* optionC_;
    QLineEdit* optionD_;
    QComboBox* correctAnswer_;

    // Question tracking
    int lastPublishedQuestionId_ = 0;

    // Screen sharing controls
    QPushButton* startShareBtn_;
    QPushButton* stopShareBtn_;
    QLabel* shareStatus_;
    QTimer* shareTimer_ = nullptr;
    bool screenSharing_ = false;
    int shareFrameSeq_ = 0;

    // Audio controls
    QPushButton* startAudioBtn_;
    QPushButton* stopAudioBtn_;
    QPushButton* grantMicBtn_;
    QPushButton* revokeMicBtn_;
    QLabel* audioStatus_;
    bool audioActive_ = false;
    int audioFrameSeq_ = 0;

#ifdef HAS_QT_MULTIMEDIA
    QAudioSource* audioSource_ = nullptr;
    QIODevice* audioIO_ = nullptr;
    QTimer* captureTimer_ = nullptr;
#endif

    // Check-in deadline
    QSpinBox* deadlineSpin_;

    // Status area
    QLabel* actionStatus_;
    QTextEdit* resultArea_;
};
