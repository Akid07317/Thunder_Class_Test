#include "teacher_page.h"
#include "network/tcp_client.h"
#include "core/session.h"
#include <common/protocol/message_type.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QSplitter>
#include <QByteArray>

#ifdef HAS_QT_MULTIMEDIA
#include <QAudioDevice>
#endif

TeacherPage::TeacherPage(TcpClient* client, Session* session, QWidget* parent)
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

void TeacherPage::setupClassListView(QWidget* page) {
    auto* layout = new QVBoxLayout(page);

    // Header
    auto* header = new QHBoxLayout;
    auto* title = new QLabel("Teacher Dashboard", page);
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

    // Create class
    auto* createRow = new QHBoxLayout;
    classNameEdit_ = new QLineEdit(page);
    classNameEdit_->setPlaceholderText("New class name");
    createRow->addWidget(classNameEdit_);
    auto* createBtn = new QPushButton("Create Class", page);
    createRow->addWidget(createBtn);
    layout->addLayout(createRow);
    connect(createBtn, &QPushButton::clicked, this, &TeacherPage::onCreateClass);

    // Class table
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

    // Buttons row
    auto* btnRow = new QHBoxLayout;
    auto* refreshBtn = new QPushButton("Refresh", page);
    auto* startBtn = new QPushButton("Start Class", page);
    auto* endBtn = new QPushButton("End Class", page);
    auto* enterBtn = new QPushButton("Enter Classroom", page);
    btnRow->addWidget(refreshBtn);
    btnRow->addWidget(startBtn);
    btnRow->addWidget(endBtn);
    btnRow->addWidget(enterBtn);
    layout->addLayout(btnRow);

    classStatus_ = new QLabel(page);
    layout->addWidget(classStatus_);

    connect(refreshBtn, &QPushButton::clicked, this, &TeacherPage::onRefreshClasses);
    connect(startBtn, &QPushButton::clicked, this, &TeacherPage::onStartClass);
    connect(endBtn, &QPushButton::clicked, this, &TeacherPage::onEndClass);
    connect(enterBtn, &QPushButton::clicked, this, &TeacherPage::onEnterClassroom);
}

void TeacherPage::setupClassroomView(QWidget* page) {
    auto* layout = new QVBoxLayout(page);

    // Header
    auto* header = new QHBoxLayout;
    classroomTitle_ = new QLabel("Classroom", page);
    QFont f; f.setPointSize(14); f.setBold(true);
    classroomTitle_->setFont(f);
    header->addWidget(classroomTitle_);
    header->addStretch();
    auto* backBtn = new QPushButton("Back to List", page);
    header->addWidget(backBtn);
    layout->addLayout(header);
    connect(backBtn, &QPushButton::clicked, this, &TeacherPage::onLeaveClassroom);

    auto* splitter = new QSplitter(Qt::Horizontal, page);

    // Left: members + actions
    auto* leftPanel = new QWidget;
    auto* leftLayout = new QVBoxLayout(leftPanel);

    auto* membersGroup = new QGroupBox("Members", leftPanel);
    auto* membersLayout = new QVBoxLayout(membersGroup);
    memberList_ = new QListWidget(leftPanel);
    membersLayout->addWidget(memberList_);
    leftLayout->addWidget(membersGroup);

    // Action buttons
    auto* actionsGroup = new QGroupBox("Actions", leftPanel);
    auto* actionsLayout = new QVBoxLayout(actionsGroup);
    auto* checkinBtn = new QPushButton("Open Check-in", leftPanel);
    auto* deadlineLayout = new QHBoxLayout();
    deadlineLayout->addWidget(new QLabel("Deadline:", leftPanel));
    deadlineSpin_ = new QSpinBox(leftPanel);
    deadlineSpin_->setRange(10, 600);
    deadlineSpin_->setValue(60);
    deadlineSpin_->setSuffix(" sec");
    deadlineLayout->addWidget(deadlineSpin_);
    auto* checkinStatusBtn = new QPushButton("Check-in Status", leftPanel);
    auto* randomBtn = new QPushButton("Random Call", leftPanel);
    auto* summaryBtn = new QPushButton("View Summary", leftPanel);
    auto* exportBtn = new QPushButton("Export CSV", leftPanel);
    actionsLayout->addWidget(checkinBtn);
    actionsLayout->addLayout(deadlineLayout);
    actionsLayout->addWidget(checkinStatusBtn);
    actionsLayout->addWidget(randomBtn);
    auto* attentionBtn = new QPushButton("Attention Status", leftPanel);
    actionsLayout->addWidget(summaryBtn);
    actionsLayout->addWidget(exportBtn);
    actionsLayout->addWidget(attentionBtn);
    leftLayout->addWidget(actionsGroup);

    // Audio controls
    auto* audioGroup = new QGroupBox("Audio", leftPanel);
    auto* audioLayout = new QVBoxLayout(audioGroup);
    audioStatus_ = new QLabel("Audio: inactive", leftPanel);
    audioLayout->addWidget(audioStatus_);
    startAudioBtn_ = new QPushButton("Start Audio", leftPanel);
    stopAudioBtn_ = new QPushButton("Stop Audio", leftPanel);
    stopAudioBtn_->setEnabled(false);
    auto* audioRow = new QHBoxLayout;
    audioRow->addWidget(startAudioBtn_);
    audioRow->addWidget(stopAudioBtn_);
    audioLayout->addLayout(audioRow);
    grantMicBtn_ = new QPushButton("Grant Mic to Selected", leftPanel);
    revokeMicBtn_ = new QPushButton("Revoke Mic", leftPanel);
    grantMicBtn_->setEnabled(false);
    revokeMicBtn_->setEnabled(false);
    auto* micRow = new QHBoxLayout;
    micRow->addWidget(grantMicBtn_);
    micRow->addWidget(revokeMicBtn_);
    audioLayout->addLayout(micRow);
    leftLayout->addWidget(audioGroup);

    connect(startAudioBtn_, &QPushButton::clicked, this, &TeacherPage::onStartAudio);
    connect(stopAudioBtn_, &QPushButton::clicked, this, &TeacherPage::onStopAudio);
    connect(grantMicBtn_, &QPushButton::clicked, this, &TeacherPage::onGrantMic);
    connect(revokeMicBtn_, &QPushButton::clicked, this, &TeacherPage::onRevokeMic);

    // Screen sharing controls
    auto* shareGroup = new QGroupBox("Screen Sharing", leftPanel);
    auto* shareLayout = new QVBoxLayout(shareGroup);
    shareStatus_ = new QLabel("Share: inactive", leftPanel);
    shareLayout->addWidget(shareStatus_);
    startShareBtn_ = new QPushButton("Start Sharing", leftPanel);
    stopShareBtn_ = new QPushButton("Stop Sharing", leftPanel);
    stopShareBtn_->setEnabled(false);
    auto* shareRow = new QHBoxLayout;
    shareRow->addWidget(startShareBtn_);
    shareRow->addWidget(stopShareBtn_);
    shareLayout->addLayout(shareRow);
    leftLayout->addWidget(shareGroup);

    connect(startShareBtn_, &QPushButton::clicked, this, &TeacherPage::onStartScreenShare);
    connect(stopShareBtn_, &QPushButton::clicked, this, &TeacherPage::onStopScreenShare);

    splitter->addWidget(leftPanel);

    // Right: question publishing + result area
    auto* rightPanel = new QWidget;
    auto* rightLayout = new QVBoxLayout(rightPanel);

    auto* qGroup = new QGroupBox("Publish Question", rightPanel);
    auto* qLayout = new QFormLayout(qGroup);
    questionText_ = new QTextEdit(rightPanel);
    questionText_->setMaximumHeight(60);
    qLayout->addRow("Question:", questionText_);
    optionA_ = new QLineEdit(rightPanel);
    optionB_ = new QLineEdit(rightPanel);
    optionC_ = new QLineEdit(rightPanel);
    optionD_ = new QLineEdit(rightPanel);
    qLayout->addRow("A:", optionA_);
    qLayout->addRow("B:", optionB_);
    qLayout->addRow("C:", optionC_);
    qLayout->addRow("D:", optionD_);
    correctAnswer_ = new QComboBox(rightPanel);
    correctAnswer_->addItems({"A", "B", "C", "D"});
    qLayout->addRow("Correct:", correctAnswer_);
    auto* publishBtn = new QPushButton("Publish", rightPanel);
    qLayout->addRow(publishBtn);
    auto* getAnswersBtn = new QPushButton("Get Answers", rightPanel);
    qLayout->addRow(getAnswersBtn);
    rightLayout->addWidget(qGroup);

    actionStatus_ = new QLabel(rightPanel);
    rightLayout->addWidget(actionStatus_);

    resultArea_ = new QTextEdit(rightPanel);
    resultArea_->setReadOnly(true);
    rightLayout->addWidget(resultArea_);

    splitter->addWidget(rightPanel);
    layout->addWidget(splitter);

    connect(checkinBtn, &QPushButton::clicked, this, &TeacherPage::onOpenCheckin);
    connect(checkinStatusBtn, &QPushButton::clicked, this, &TeacherPage::onCheckinStatus);
    connect(randomBtn, &QPushButton::clicked, this, &TeacherPage::onRandomCall);
    connect(publishBtn, &QPushButton::clicked, this, &TeacherPage::onPublishQuestion);
    connect(getAnswersBtn, &QPushButton::clicked, this, &TeacherPage::onGetAnswers);
    connect(summaryBtn, &QPushButton::clicked, this, &TeacherPage::onViewSummary);
    connect(exportBtn, &QPushButton::clicked, this, &TeacherPage::onExportCSV);
    connect(attentionBtn, &QPushButton::clicked, this, &TeacherPage::onAttentionStatus);
}

void TeacherPage::onActivated() {
    onRefreshClasses();
}

void TeacherPage::onCreateClass() {
    QString name = classNameEdit_->text().trimmed();
    if (name.isEmpty()) return;
    classStatus_->clear();
    nlohmann::json req;
    req["type"] = to_string(MessageType::CREATE_CLASS_REQ);
    req["name"] = name.toStdString();
    client_->send(req);
}

void TeacherPage::onRefreshClasses() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::LIST_CLASSES_REQ);
    client_->send(req);
}

int getSelectedClassId(QTableWidget* table) {
    auto sel = table->selectedItems();
    if (sel.isEmpty()) return 0;
    int row = sel.first()->row();
    return table->item(row, 0)->text().toInt();
}

void TeacherPage::onStartClass() {
    int id = getSelectedClassId(classTable_);
    if (!id) { classStatus_->setText("Select a class first"); return; }
    nlohmann::json req;
    req["type"] = to_string(MessageType::START_CLASS_REQ);
    req["classId"] = id;
    client_->send(req);
}

void TeacherPage::onEndClass() {
    int id = getSelectedClassId(classTable_);
    if (!id) { classStatus_->setText("Select a class first"); return; }
    auto answer = QMessageBox::question(this, "End Class",
        "Are you sure you want to end this class? This cannot be undone.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    nlohmann::json req;
    req["type"] = to_string(MessageType::END_CLASS_REQ);
    req["classId"] = id;
    client_->send(req);
}

void TeacherPage::onEnterClassroom() {
    int id = getSelectedClassId(classTable_);
    if (!id) { classStatus_->setText("Select a class first"); return; }
    auto sel = classTable_->selectedItems();
    int row = sel.first()->row();
    currentClassId_ = id;
    currentClassName_ = classTable_->item(row, 1)->text().toStdString();
    classroomTitle_->setText(QString("Classroom: %1").arg(QString::fromStdString(currentClassName_)));
    stack_->setCurrentIndex(1);

    // Refresh members
    nlohmann::json req;
    req["type"] = to_string(MessageType::CLASS_MEMBERS_REQ);
    req["classId"] = currentClassId_;
    client_->send(req);
}

void TeacherPage::onLeaveClassroom() {
    stack_->setCurrentIndex(0);
    onRefreshClasses();
}

void TeacherPage::onOpenCheckin() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::OPEN_CHECKIN_REQ);
    req["classId"] = currentClassId_;
    req["deadlineSeconds"] = deadlineSpin_->value();
    client_->send(req);
}

void TeacherPage::onCheckinStatus() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::CHECKIN_STATUS_REQ);
    req["classId"] = currentClassId_;
    client_->send(req);
}

void TeacherPage::onRandomCall() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::RANDOM_CALL_REQ);
    req["classId"] = currentClassId_;
    client_->send(req);
}

void TeacherPage::onPublishQuestion() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::PUBLISH_QUESTION_REQ);
    req["classId"] = currentClassId_;
    req["content"] = questionText_->toPlainText().toStdString();
    req["options"] = nlohmann::json::array({
        optionA_->text().toStdString(),
        optionB_->text().toStdString(),
        optionC_->text().toStdString(),
        optionD_->text().toStdString()
    });
    req["correctAnswer"] = correctAnswer_->currentIndex();
    client_->send(req);
}

void TeacherPage::onGetAnswers() {
    if (lastPublishedQuestionId_ <= 0) {
        actionStatus_->setText("No question published yet");
        return;
    }
    nlohmann::json req;
    req["type"] = to_string(MessageType::GET_ANSWERS_REQ);
    req["questionId"] = lastPublishedQuestionId_;
    client_->send(req);
}

void TeacherPage::onViewSummary() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::CLASS_SUMMARY_REQ);
    req["classId"] = currentClassId_;
    client_->send(req);
}

void TeacherPage::onExportCSV() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::EXPORT_CLASS_REQ);
    req["classId"] = currentClassId_;
    client_->send(req);
}

void TeacherPage::onAttentionStatus() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::ATTENTION_STATUS_REQ);
    req["classId"] = currentClassId_;
    client_->send(req);
}

void TeacherPage::onStartScreenShare() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::START_SCREEN_SHARE_REQ);
    req["classId"] = currentClassId_;
    client_->send(req);
}

void TeacherPage::onStopScreenShare() {
    auto answer = QMessageBox::question(this, "Stop Sharing",
        "Stop screen sharing for all students?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    nlohmann::json req;
    req["type"] = to_string(MessageType::STOP_SCREEN_SHARE_REQ);
    req["classId"] = currentClassId_;
    client_->send(req);
}

void TeacherPage::captureAndSendFrame() {
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    QPixmap pixmap = screen->grabWindow(0);
    if (pixmap.isNull()) return;

    // Scale down for bandwidth — max 800px wide
    if (pixmap.width() > 800) {
        pixmap = pixmap.scaledToWidth(800, Qt::SmoothTransformation);
    }

    // Compress to JPEG
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "JPEG", 50); // quality 50 for small size
    buffer.close();

    // Send as base64
    nlohmann::json frame;
    frame["type"] = to_string(MessageType::SCREEN_FRAME);
    frame["classId"] = currentClassId_;
    frame["frameData"] = bytes.toBase64().toStdString();
    frame["frameSeq"] = ++shareFrameSeq_;
    client_->send(frame);

    shareStatus_->setText(QString("Share: active (frame %1, %2 KB)")
        .arg(shareFrameSeq_).arg(bytes.size() / 1024));
}

void TeacherPage::onStartAudio() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::START_AUDIO_REQ);
    req["classId"] = currentClassId_;
    client_->send(req);
}

void TeacherPage::onStopAudio() {
    auto answer = QMessageBox::question(this, "Stop Audio",
        "Stop audio broadcast for all students?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    nlohmann::json req;
    req["type"] = to_string(MessageType::STOP_AUDIO_REQ);
    req["classId"] = currentClassId_;
    client_->send(req);
}

void TeacherPage::onGrantMic() {
    auto* item = memberList_->currentItem();
    if (!item) {
        actionStatus_->setText("Select a student from the member list first");
        return;
    }
    // Extract userId from "Name (ID: 123)" format
    QString text = item->text();
    int start = text.indexOf("ID: ") + 4;
    int end = text.indexOf(")", start);
    int studentId = text.mid(start, end - start).toInt();
    if (studentId <= 0) {
        actionStatus_->setText("Could not parse student ID");
        return;
    }
    nlohmann::json req;
    req["type"] = to_string(MessageType::GRANT_MIC_REQ);
    req["classId"] = currentClassId_;
    req["studentId"] = studentId;
    client_->send(req);
}

void TeacherPage::onRevokeMic() {
    nlohmann::json req;
    req["type"] = to_string(MessageType::REVOKE_MIC_REQ);
    req["classId"] = currentClassId_;
    client_->send(req);
}

void TeacherPage::startAudioCapture() {
    audioFrameSeq_ = 0;
#ifdef HAS_QT_MULTIMEDIA
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    auto device = QMediaDevices::defaultAudioInput();
    if (device.isNull() || !device.isFormatSupported(format)) {
        audioStatus_->setText("Audio: broadcasting (no capture device)");
        return;
    }

    audioSource_ = new QAudioSource(device, format, this);
    audioIO_ = audioSource_->start();
    if (!audioIO_) {
        audioStatus_->setText("Audio: broadcasting (capture failed to start)");
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
        frame["frameSeq"] = ++audioFrameSeq_;
        client_->send(frame);
    });
    captureTimer_->start(200); // send audio every 200ms
    audioStatus_->setText("Audio: broadcasting (capturing)");
#else
    audioStatus_->setText("Audio: broadcasting (no Qt Multimedia — capture unavailable)");
#endif
}

void TeacherPage::stopAudioCapture() {
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
    audioFrameSeq_ = 0;
}

void TeacherPage::playAudioFrame(const std::string& /*audioData*/) {
    // Playback of student audio received by teacher
    // With Qt Multimedia: decode base64 → write to QAudioSink
    // Without: silently ignore (protocol flow still visible in UI)
}

void TeacherPage::handleMessage(const nlohmann::json& msg) {
    std::string type = msg.value("type", "");

    if (type == to_string(MessageType::CREATE_CLASS_RESP)) {
        bool ok = msg.value("success", false);
        classStatus_->setText(ok ? "Class created" : QString::fromStdString(msg.value("message", "Failed")));
        if (ok) { classNameEdit_->clear(); onRefreshClasses(); }
    }
    else if (type == to_string(MessageType::LIST_CLASSES_RESP)) {
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
    else if (type == to_string(MessageType::START_CLASS_RESP) ||
             type == to_string(MessageType::END_CLASS_RESP)) {
        bool ok = msg.value("success", false);
        classStatus_->setText(ok ? "Success" : QString::fromStdString(msg.value("message", "Failed")));
        if (ok) onRefreshClasses();
    }
    else if (type == to_string(MessageType::CLASS_MEMBERS_RESP)) {
        memberList_->clear();
        auto members = msg.value("members", nlohmann::json::array());
        for (auto& m : members) {
            QString entry = QString("%1 (ID: %2)")
                .arg(QString::fromStdString(m.value("name", "")))
                .arg(m.value("userId", 0));
            memberList_->addItem(entry);
        }
    }
    else if (type == to_string(MessageType::OPEN_CHECKIN_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            int deadline = msg.value("deadlineSeconds", 60);
            actionStatus_->setText(QString("Check-in opened (deadline: %1s)").arg(deadline));
        } else {
            actionStatus_->setText(QString::fromStdString(msg.value("message", "Failed")));
        }
    }
    else if (type == to_string(MessageType::CHECKIN_STATUS_RESP)) {
        auto students = msg.value("students", nlohmann::json::array());
        int onTimeCount = 0, lateCount = 0, absentCount = 0;
        QString text;
        for (auto& s : students) {
            std::string status = s.value("status", "absent");
            if (status == "on_time") ++onTimeCount;
            else if (status == "late") ++lateCount;
            else ++absentCount;
            text += QString("  %1: %2\n")
                .arg(QString::fromStdString(s.value("name", "")))
                .arg(QString::fromStdString(status));
        }
        text = QString("Check-in: %1 on time, %2 late, %3 absent (total %4)\n")
            .arg(onTimeCount).arg(lateCount).arg(absentCount).arg(students.size()) + text;
        resultArea_->setText(text);
    }
    else if (type == to_string(MessageType::RANDOM_CALL_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            QString name = QString::fromStdString(msg.value("name", ""));
            int id = msg.value("userId", 0);
            actionStatus_->setText(QString("Random call: %1 (ID: %2)").arg(name).arg(id));
            QMessageBox::information(this, "Random Call",
                QString("Selected student: %1 (ID: %2)").arg(name).arg(id));
        } else {
            actionStatus_->setText(QString::fromStdString(msg.value("message", "Failed")));
        }
    }
    else if (type == to_string(MessageType::PUBLISH_QUESTION_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            lastPublishedQuestionId_ = msg.value("questionId", 0);
            actionStatus_->setText(QString("Question published (ID: %1)").arg(lastPublishedQuestionId_));
        } else {
            actionStatus_->setText(QString::fromStdString(msg.value("message", "Failed")));
        }
    }
    else if (type == to_string(MessageType::GET_ANSWERS_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            auto answers = msg.value("answers", nlohmann::json::array());
            int correctCount = msg.value("correctCount", 0);
            int total = answers.size();
            QString text = QString("Answers: %1 total, %2 correct\n\n").arg(total).arg(correctCount);
            for (auto& a : answers) {
                text += QString("%1: option %2\n")
                    .arg(QString::fromStdString(a.value("userName", "")))
                    .arg(a.value("answer", 0));
            }
            resultArea_->setText(text);
        } else {
            actionStatus_->setText(QString::fromStdString(msg.value("message", "Failed")));
        }
    }
    else if (type == to_string(MessageType::CLASS_SUMMARY_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            auto summary = msg.value("summary", nlohmann::json::object());
            QString text;
            text += QString("Class: %1\n").arg(QString::fromStdString(summary.value("className", "")));
            text += QString("Total students: %1\n").arg(summary.value("totalStudents", 0));
            text += QString("Checked in: %1\n").arg(summary.value("checkedInCount", 0));
            text += QString("Questions: %1\n\n").arg(summary.value("totalQuestions", 0));

            auto students = summary.value("students", nlohmann::json::array());
            for (auto& s : students) {
                text += QString("%1: checkin=%2, duration=%3s, answered=%4, correct=%5, focus=%6%, active=%7%, presence=%8/%9\n")
                    .arg(QString::fromStdString(s.value("name", "")))
                    .arg(QString::fromStdString(s.value("checkinStatus", "absent")))
                    .arg(s.value("durationSeconds", 0))
                    .arg(s.value("questionsAnswered", 0))
                    .arg(s.value("correctAnswers", 0))
                    .arg(s.value("focusRate", 100.0), 0, 'f', 0)
                    .arg(s.value("activeRate", 100.0), 0, 'f', 0)
                    .arg(s.value("presenceResponded", 0))
                    .arg(s.value("presenceTotal", 0));
            }
            resultArea_->setText(text);
        } else {
            actionStatus_->setText(QString::fromStdString(msg.value("message", "Failed")));
        }
    }
    else if (type == to_string(MessageType::EXPORT_CLASS_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            std::string csv = msg.value("csv", "");
            resultArea_->setText(QString::fromStdString(csv));

            QString path = QFileDialog::getSaveFileName(this, "Save CSV", "class_export.csv", "CSV (*.csv)");
            if (!path.isEmpty()) {
                QFile file(path);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    file.write(csv.c_str());
                    file.close();
                    actionStatus_->setText("CSV saved to " + path);
                }
            }
        } else {
            actionStatus_->setText(QString::fromStdString(msg.value("message", "Failed")));
        }
    }
    // Screen sharing responses
    else if (type == to_string(MessageType::START_SCREEN_SHARE_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            screenSharing_ = true;
            shareFrameSeq_ = 0;
            startShareBtn_->setEnabled(false);
            stopShareBtn_->setEnabled(true);
            shareStatus_->setText("Share: active");
            actionStatus_->setText("Screen sharing started");

            shareTimer_ = new QTimer(this);
            connect(shareTimer_, &QTimer::timeout, this, &TeacherPage::captureAndSendFrame);
            shareTimer_->start(1000); // 1 fps
        } else {
            actionStatus_->setText(QString::fromStdString(msg.value("message", "Failed to start sharing")));
        }
    }
    else if (type == to_string(MessageType::STOP_SCREEN_SHARE_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            screenSharing_ = false;
            if (shareTimer_) {
                shareTimer_->stop();
                delete shareTimer_;
                shareTimer_ = nullptr;
            }
            startShareBtn_->setEnabled(true);
            stopShareBtn_->setEnabled(false);
            shareStatus_->setText("Share: inactive");
            actionStatus_->setText("Screen sharing stopped");
        } else {
            actionStatus_->setText(QString::fromStdString(msg.value("message", "Failed to stop sharing")));
        }
    }
    // Audio responses
    else if (type == to_string(MessageType::START_AUDIO_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            audioActive_ = true;
            startAudioBtn_->setEnabled(false);
            stopAudioBtn_->setEnabled(true);
            grantMicBtn_->setEnabled(true);
            revokeMicBtn_->setEnabled(true);
            audioStatus_->setText("Audio: active");
            actionStatus_->setText("Audio broadcast started");
            startAudioCapture();
        } else {
            actionStatus_->setText(QString::fromStdString(msg.value("message", "Failed to start audio")));
        }
    }
    else if (type == to_string(MessageType::STOP_AUDIO_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            audioActive_ = false;
            startAudioBtn_->setEnabled(true);
            stopAudioBtn_->setEnabled(false);
            grantMicBtn_->setEnabled(false);
            revokeMicBtn_->setEnabled(false);
            audioStatus_->setText("Audio: inactive");
            actionStatus_->setText("Audio broadcast stopped");
            stopAudioCapture();
        } else {
            actionStatus_->setText(QString::fromStdString(msg.value("message", "Failed to stop audio")));
        }
    }
    else if (type == to_string(MessageType::GRANT_MIC_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            int sid = msg.value("studentId", 0);
            actionStatus_->setText(QString("Mic granted to student ID %1").arg(sid));
            audioStatus_->setText(QString("Audio: active | Mic: student %1").arg(sid));
        } else {
            actionStatus_->setText(QString::fromStdString(msg.value("message", "Failed to grant mic")));
        }
    }
    else if (type == to_string(MessageType::REVOKE_MIC_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            actionStatus_->setText("Mic revoked");
            audioStatus_->setText("Audio: active | Mic: none");
        } else {
            actionStatus_->setText(QString::fromStdString(msg.value("message", "Failed to revoke mic")));
        }
    }
    else if (type == to_string(MessageType::ATTENTION_STATUS_RESP)) {
        bool ok = msg.value("success", false);
        if (ok) {
            auto students = msg.value("students", nlohmann::json::array());
            QString text = "Attention Status:\n\n";
            text += QString("%-20s %-8s %-8s %-10s %-10s %-12s\n")
                .arg("Name").arg("Focus").arg("Idle(s)").arg("Focus%").arg("Active%").arg("Presence");
            text += QString("-").repeated(70) + "\n";
            for (auto& s : students) {
                QString name = QString::fromStdString(s.value("name", ""));
                bool focused = s.value("focused", true);
                int idle = s.value("idleSeconds", 0);
                double focusRate = s.value("focusRate", 100.0);
                double activeRate = s.value("activeRate", 100.0);
                int presResp = s.value("presenceResponded", 0);
                int presTotal = s.value("presenceTotal", 0);
                text += QString("%1  %2  %3s  %4%  %5%  %6/%7\n")
                    .arg(name, -20)
                    .arg(focused ? "Yes" : "No ", -6)
                    .arg(idle, 4)
                    .arg(focusRate, 5, 'f', 0)
                    .arg(activeRate, 5, 'f', 0)
                    .arg(presResp)
                    .arg(presTotal);
            }
            if (students.empty()) text += "(No attention data yet)\n";
            resultArea_->setText(text);
        } else {
            actionStatus_->setText(QString::fromStdString(msg.value("message", "Failed")));
        }
    }
    else if (type == to_string(MessageType::NOTIFY_AUDIO_FRAME)) {
        std::string senderName = msg.value("senderName", "");
        playAudioFrame(msg.value("audioData", ""));
        actionStatus_->setText(QString("Receiving audio from %1").arg(
            QString::fromStdString(senderName)));
    }
}
