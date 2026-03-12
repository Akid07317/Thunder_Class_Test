#include "student_page.h"
#include "network/tcp_client.h"
#include "core/session.h"
#include <common/protocol/message_type.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QButtonGroup>
#include <QByteArray>

#ifdef HAS_QT_MULTIMEDIA
#include <QAudioDevice>
#endif

StudentPage::StudentPage(TcpClient* client, Session* session, QWidget* parent)
    : QWidget(parent), client_(client), session_(session)
{
    auto* layout = new QVBoxLayout(this);
    stack_ = new QStackedWidget(this);

    auto* classListPage = new QWidget;
    setupClassListView(classListPage);
    stack_->addWidget(classListPage);

    auto* classroomPage = new QWidget;
    setupClassroomView(classroomPage);
    stack_->addWidget(classroomPage);

    layout->addWidget(stack_);
}

void StudentPage::setupClassListView(QWidget* page) {
    auto* layout = new QVBoxLayout(page);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel("Student Dashboard", page);
    QFont f; f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    header->addWidget(title);
    header->addStretch();
    auto* logoutBtn = new QPushButton("Logout", page);
    header->addWidget(logoutBtn);
    layout->addLayout(header);
    connect(logoutBtn, &QPushButton::clicked, this, [this]{
        nlohmann::json req;
        req["type"] = to_string(MessageType::LOGOUT_REQ);
        client_->send(req);
        emit logoutRequested();
    });

    classTable_ = new QTableWidget(page);
    classTable_->setColumnCount(4);
    classTable_->setHorizontalHeaderLabels({"ID", "Name", "Status", "Teacher"});
    classTable_->horizontalHeader()->setStretchLastSection(true);
    classTable_->setColumnWidth(0, 60);
    classTable_->setColumnWidth(1, 200);
    classTable_->setColumnWidth(2, 100);
    classTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    classTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    classTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(classTable_);

    auto* btnRow = new QHBoxLayout;
    auto* refreshBtn = new QPushButton("Refresh", page);
    auto* joinBtn = new QPushButton("Join Class", page);
    auto* leaveBtn = new QPushButton("Leave Class", page);
    auto* enterBtn = new QPushButton("Enter Classroom", page);
    btnRow->addWidget(refreshBtn);
    btnRow->addWidget(joinBtn);
    btnRow->addWidget(leaveBtn);
    btnRow->addWidget(enterBtn);
    layout->addLayout(btnRow);

    classStatus_ = new QLabel(page);
    layout->addWidget(classStatus_);

    connect(refreshBtn, &QPushButton::clicked, this, &StudentPage::onRefreshClasses);
    connect(joinBtn, &QPushButton::clicked, this, &StudentPage::onJoinClass);
    connect(leaveBtn, &QPushButton::clicked, this, &StudentPage::onLeaveClass);
    connect(enterBtn, &QPushButton::clicked, this, &StudentPage::onEnterClassroom);
}

void StudentPage::setupClassroomView(QWidget* page) {
    auto* layout = new QVBoxLayout(page);

    auto* header = new QHBoxLayout;
    classroomTitle_ = new QLabel("Classroom", page);
    QFont f; f.setPointSize(14); f.setBold(true);
    classroomTitle_->setFont(f);
    header->addWidget(classroomTitle_);
    header->addStretch();
    auto* backBtn = new QPushButton("Back to List", page);
    header->addWidget(backBtn);
    layout->addLayout(header);
    connect(backBtn, &QPushButton::clicked, this, &StudentPage::onLeaveClassroom);

    // Notifications
    auto* notifGroup = new QGroupBox("Notifications", page);
    auto* notifLayout = new QVBoxLayout(notifGroup);
    notificationArea_ = new QTextEdit(page);
    notificationArea_->setReadOnly(true);
    notificationArea_->setMaximumHeight(120);
    notifLayout->addWidget(notificationArea_);
    auto* checkinBtn = new QPushButton("Check In", page);
    notifLayout->addWidget(checkinBtn);
    layout->addWidget(notifGroup);
    connect(checkinBtn, &QPushButton::clicked, this, &StudentPage::onCheckin);

    // Screen sharing display
    auto* shareGroup = new QGroupBox("Screen Share", page);
    auto* shareGroupLayout = new QVBoxLayout(shareGroup);
    shareStatus_ = new QLabel("No screen share active", page);
    shareGroupLayout->addWidget(shareStatus_);
    auto* scrollArea = new QScrollArea(page);
    screenLabel_ = new QLabel(page);
    screenLabel_->setAlignment(Qt::AlignCenter);
    screenLabel_->setMinimumSize(320, 180);
    screenLabel_->setStyleSheet("background-color: #1a1a1a;");
    scrollArea->setWidget(screenLabel_);
    scrollArea->setWidgetResizable(true);
    scrollArea->setMaximumHeight(300);
    shareGroupLayout->addWidget(scrollArea);
    layout->addWidget(shareGroup);

    // Question area
    auto* qGroup = new QGroupBox("Active Question", page);
    auto* qLayout = new QVBoxLayout(qGroup);
    questionLabel_ = new QLabel("No active question", page);
    questionLabel_->setWordWrap(true);
    qLayout->addWidget(questionLabel_);

    auto* btnGroup = new QButtonGroup(this);
    for (int i = 0; i < 4; i++) {
        optionBtns_[i] = new QRadioButton(page);
        qLayout->addWidget(optionBtns_[i]);
        btnGroup->addButton(optionBtns_[i], i);
    }
    submitBtn_ = new QPushButton("Submit Answer", page);
    qLayout->addWidget(submitBtn_);
    layout->addWidget(qGroup);
    connect(submitBtn_, &QPushButton::clicked, this, &StudentPage::onSubmitAnswer);

    // Audio status
    auto* audioGroup = new QGroupBox("Audio", page);
    auto* audioLayout = new QVBoxLayout(audioGroup);
    audioStatus_ = new QLabel("Audio: inactive", page);
    audioLayout->addWidget(audioStatus_);
    layout->addWidget(audioGroup);

    actionStatus_ = new QLabel(page);
    layout->addWidget(actionStatus_);

    layout->addStretch();
}

void StudentPage::onActivated() {
    onRefreshClasses();
}

void StudentPage::onRefreshClasses() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::LIST_CLASSES_REQ);
    client_->send(req);
}

void StudentPage::onJoinClass() {
    auto sel = classTable_->selectedItems();
    if (sel.isEmpty()) { classStatus_->setText("Select a class first"); return; }
    int row = sel.first()->row();
    int classId = classTable_->item(row, 0)->text().toInt();
    nlohmann::json req;
    req["type"] = to_string(MessageType::JOIN_CLASS_REQ);
    req["classId"] = classId;
    client_->send(req);
}

void StudentPage::onLeaveClass() {
    auto sel = classTable_->selectedItems();
    if (sel.isEmpty()) { classStatus_->setText("Select a class first"); return; }
    int row = sel.first()->row();
    int classId = classTable_->item(row, 0)->text().toInt();
    auto answer = QMessageBox::question(this, "Leave Class",
        "Are you sure you want to leave this class?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    nlohmann::json req;
    req["type"] = to_string(MessageType::LEAVE_CLASS_REQ);
    req["classId"] = classId;
    client_->send(req);
}

void StudentPage::onEnterClassroom() {
    auto sel = classTable_->selectedItems();
    if (sel.isEmpty()) { classStatus_->setText("Select a class first"); return; }
    int row = sel.first()->row();
    currentClassId_ = classTable_->item(row, 0)->text().toInt();
    QString name = classTable_->item(row, 1)->text();
    classroomTitle_->setText(QString("Classroom: %1").arg(name));
    notificationArea_->clear();
    questionLabel_->setText("No active question");
    stack_->setCurrentIndex(1);
}

void StudentPage::onLeaveClassroom() {
    stack_->setCurrentIndex(0);
    onRefreshClasses();
}

void StudentPage::onCheckin() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::CHECKIN_REQ);
    req["classId"] = currentClassId_;
    client_->send(req);
}

void StudentPage::onSubmitAnswer() {
    int selected = -1;
    for (int i = 0; i < 4; i++) {
        if (optionBtns_[i]->isChecked()) { selected = i; break; }
    }
    if (selected < 0) {
        actionStatus_->setText("Select an answer first");
        return;
    }
    if (currentQuestionId_ <= 0) {
        actionStatus_->setText("No active question to answer");
        return;
    }
    nlohmann::json req;
    req["type"] = to_string(MessageType::SUBMIT_ANSWER_REQ);
    req["questionId"] = currentQuestionId_;
    req["answer"] = selected;
    client_->send(req);
}

void StudentPage::startMicCapture() {
    micFrameSeq_ = 0;
#ifdef HAS_QT_MULTIMEDIA
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    auto device = QMediaDevices::defaultAudioInput();
    if (device.isNull() || !device.isFormatSupported(format)) {
        audioStatus_->setText("Mic: granted (no capture device available)");
        return;
    }

    audioSource_ = new QAudioSource(device, format, this);
    audioIO_ = audioSource_->start();
    if (!audioIO_) {
        audioStatus_->setText("Mic: granted (capture failed to start)");
        delete audioSource_;
        audioSource_ = nullptr;
        return;
    }

    captureTimer_ = new QTimer(this);
    connect(captureTimer_, &QTimer::timeout, this, [this]{
        if (!audioIO_) return;
        QByteArray data = audioIO_->readAll();
        if (data.isEmpty()) return;
        nlohmann::json frame;
        frame["type"] = to_string(MessageType::AUDIO_FRAME);
        frame["classId"] = currentClassId_;
        frame["audioData"] = data.toBase64().toStdString();
        frame["frameSeq"] = ++micFrameSeq_;
        client_->send(frame);
    });
    captureTimer_->start(200);
    audioStatus_->setText("Mic: active (capturing)");
#else
    audioStatus_->setText("Mic: granted (no Qt Multimedia — capture unavailable)");
#endif
}

void StudentPage::stopMicCapture() {
#ifdef HAS_QT_MULTIMEDIA
    if (captureTimer_) {
        captureTimer_->stop();
        delete captureTimer_;
        captureTimer_ = nullptr;
    }
    if (audioSource_) {
        audioSource_->stop();
        delete audioSource_;
        audioSource_ = nullptr;
        audioIO_ = nullptr;
    }
#endif
    micFrameSeq_ = 0;
}

void StudentPage::playAudioFrame(const std::string& audioData) {
#ifdef HAS_QT_MULTIMEDIA
    if (!audioSink_) {
        QAudioFormat format;
        format.setSampleRate(16000);
        format.setChannelCount(1);
        format.setSampleFormat(QAudioFormat::Int16);

        auto device = QMediaDevices::defaultAudioOutput();
        if (device.isNull() || !device.isFormatSupported(format)) return;

        audioSink_ = new QAudioSink(device, format, this);
        playbackIO_ = audioSink_->start();
    }
    if (playbackIO_) {
        QByteArray pcm = QByteArray::fromBase64(QByteArray::fromStdString(audioData));
        playbackIO_->write(pcm);
    }
#else
    (void)audioData;
#endif
}

void StudentPage::stopAudioPlayback() {
#ifdef HAS_QT_MULTIMEDIA
    if (audioSink_) {
        audioSink_->stop();
        delete audioSink_;
        audioSink_ = nullptr;
        playbackIO_ = nullptr;
    }
#endif
}

void StudentPage::handleMessage(const nlohmann::json& msg) {
    std::string type = msg.value("type", "");

    if (type == to_string(MessageType::LIST_CLASSES_RESP)) {
        auto classes = msg.value("classes", nlohmann::json::array());
        classTable_->setRowCount(classes.size());
        for (int i = 0; i < (int)classes.size(); i++) {
            auto& c = classes[i];
            classTable_->setItem(i, 0, new QTableWidgetItem(QString::number(c.value("classId", 0))));
            classTable_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(c.value("name", ""))));
            classTable_->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(c.value("status", ""))));
            classTable_->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(c.value("teacherName", ""))));
        }
    }
    else if (type == to_string(MessageType::JOIN_CLASS_RESP)) {
        bool ok = msg.value("success", false);
        classStatus_->setText(ok ? "Joined class" : QString::fromStdString(msg.value("message", "Failed")));
        if (ok) onRefreshClasses();
    }
    else if (type == to_string(MessageType::LEAVE_CLASS_RESP)) {
        bool ok = msg.value("success", false);
        classStatus_->setText(ok ? "Left class" : QString::fromStdString(msg.value("message", "Failed")));
        if (ok) onRefreshClasses();
    }
    else if (type == to_string(MessageType::CHECKIN_RESP)) {
        bool ok = msg.value("success", false);
        actionStatus_->setText(ok ? "Checked in!" : QString::fromStdString(msg.value("message", "Failed")));
    }
    else if (type == to_string(MessageType::SUBMIT_ANSWER_RESP)) {
        bool ok = msg.value("success", false);
        actionStatus_->setText(ok ? "Answer submitted" : QString::fromStdString(msg.value("message", "Failed")));
    }
    // Push notifications
    else if (type == to_string(MessageType::NOTIFY_CHECKIN_OPENED)) {
        int deadline = msg.value("deadlineSeconds", 60);
        notificationArea_->append(QString("[Check-in] Teacher opened check-in (deadline: %1s). Click 'Check In' to respond.").arg(deadline));
        QMessageBox::information(this, "Check-in",
            QString("Teacher has opened check-in!\nDeadline: %1 seconds").arg(deadline));
    }
    else if (type == to_string(MessageType::NOTIFY_QUESTION_PUBLISHED)) {
        notificationArea_->append("[Question] A new question has been published.");
        // Server sends question data nested under "question" key
        auto q = msg.value("question", nlohmann::json::object());
        std::string content = q.value("content", "");
        auto options = q.value("options", nlohmann::json::array());
        currentQuestionId_ = q.value("questionId", 0);
        questionLabel_->setText(QString::fromStdString(content));
        const char* labels[] = {"A", "B", "C", "D"};
        for (int i = 0; i < 4 && i < (int)options.size(); i++) {
            optionBtns_[i]->setText(QString("%1: %2").arg(labels[i]).arg(
                QString::fromStdString(options[i].get<std::string>())));
            optionBtns_[i]->setChecked(false);
        }
    }
    else if (type == to_string(MessageType::NOTIFY_RANDOM_CALLED)) {
        notificationArea_->append("[Random Call] You have been called on!");
        QMessageBox::warning(this, "Random Call", "You have been randomly called!");
    }
    // Screen sharing notifications
    else if (type == to_string(MessageType::NOTIFY_SCREEN_SHARE_STARTED)) {
        notificationArea_->append("[Screen] Teacher started screen sharing.");
        shareStatus_->setText("Receiving screen share...");
        screenLabel_->setText("Waiting for frames...");
    }
    else if (type == to_string(MessageType::NOTIFY_SCREEN_SHARE_STOPPED)) {
        notificationArea_->append("[Screen] Teacher stopped screen sharing.");
        shareStatus_->setText("No screen share active");
        screenLabel_->clear();
        screenLabel_->setText("");
    }
    else if (type == to_string(MessageType::NOTIFY_SCREEN_FRAME)) {
        std::string frameData = msg.value("frameData", "");
        int frameSeq = msg.value("frameSeq", 0);
        if (!frameData.empty()) {
            QByteArray decoded = QByteArray::fromBase64(
                QByteArray::fromStdString(frameData));
            QPixmap pixmap;
            if (pixmap.loadFromData(decoded, "JPEG")) {
                // Scale to fit the label width while preserving aspect ratio
                QWidget* parent = screenLabel_->parentWidget();
                int labelWidth = parent ? parent->width() - 20 : 600;
                if (labelWidth > 0 && pixmap.width() > labelWidth) {
                    pixmap = pixmap.scaledToWidth(labelWidth, Qt::SmoothTransformation);
                }
                screenLabel_->setPixmap(pixmap);
                shareStatus_->setText(QString("Screen share: frame %1 (%2 KB)")
                    .arg(frameSeq).arg(decoded.size() / 1024));
            }
        }
    }
    // Audio notifications
    else if (type == to_string(MessageType::NOTIFY_AUDIO_STARTED)) {
        notificationArea_->append("[Audio] Teacher started audio broadcast.");
        audioStatus_->setText("Audio: listening to teacher broadcast");
    }
    else if (type == to_string(MessageType::NOTIFY_AUDIO_STOPPED)) {
        notificationArea_->append("[Audio] Teacher stopped audio broadcast.");
        if (micGranted_) {
            micGranted_ = false;
            stopMicCapture();
        }
        stopAudioPlayback();
        audioStatus_->setText("Audio: inactive");
    }
    else if (type == to_string(MessageType::NOTIFY_AUDIO_FRAME)) {
        std::string senderName = msg.value("senderName", "");
        playAudioFrame(msg.value("audioData", ""));
        audioStatus_->setText(QString("Audio: receiving from %1").arg(
            QString::fromStdString(senderName)));
    }
    else if (type == to_string(MessageType::NOTIFY_MIC_GRANTED)) {
        micGranted_ = true;
        notificationArea_->append("[Audio] You have been granted the microphone!");
        audioStatus_->setText("Mic: granted — you can speak");
        startMicCapture();
        QMessageBox::information(this, "Microphone", "Teacher granted you the microphone!");
    }
    else if (type == to_string(MessageType::NOTIFY_MIC_REVOKED)) {
        micGranted_ = false;
        notificationArea_->append("[Audio] Your microphone has been revoked.");
        audioStatus_->setText("Audio: listening (mic revoked)");
        stopMicCapture();
    }
}
