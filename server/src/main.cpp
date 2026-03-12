#include "tcp_server.h"
#include "user_store.h"
#include "session_manager.h"
#include "class_manager.h"
#include "question_manager.h"
#include "statistics_exporter.h"
#include <common/protocol/message_type.h>
#include <iostream>

static const uint16_t DEFAULT_PORT = 9000;
static const std::string DATA_DIR = "data";
static const size_t MAX_STRING_LEN = 1024;  // max length for names, content, etc.

// Truncate a string to the max allowed length
static std::string clampStr(const std::string& s) {
    return s.size() > MAX_STRING_LEN ? s.substr(0, MAX_STRING_LEN) : s;
}

int main() {
    UserStore userStore(DATA_DIR + "/users.json");
    SessionManager sessionMgr;
    ClassManager classMgr(DATA_DIR);
    QuestionManager questionMgr(DATA_DIR);
    TcpServer server(DEFAULT_PORT);

    // Helper: look up fd for a userId (for push notifications)
    auto findFd = [&](int userId) -> int {
        // SessionManager doesn't expose this, so we track via a simple scan.
        // For MVP scale this is fine.
        return sessionMgr.getFd(userId);
    };

    server.setDisconnectHandler([&](int fd) {
        auto user = sessionMgr.getUser(fd);
        if (user) {
            std::cout << "User disconnected: " << user->name << "\n";
            classMgr.removeFromAll(user->userId, user->name);
        }
        sessionMgr.logout(fd);
    });

    server.setMessageHandler([&](int fd, const nlohmann::json& msg) {
      try {
        std::string typeStr = msg.value("type", "");
        MessageType type = message_type_from_string(typeStr);

        switch (type) {

        // ==================== Login ====================
        case MessageType::LOGIN_REQ: {
            std::string username = msg.value("username", "");
            std::string password = msg.value("password", "");

            auto user = userStore.authenticate(username, password);
            nlohmann::json resp;
            resp["type"] = "LOGIN_RESP";

            if (!user) {
                resp["success"] = false;
                resp["message"] = "Invalid username or password";
            } else if (!sessionMgr.login(fd, *user)) {
                resp["success"] = false;
                resp["message"] = "User already logged in";
            } else {
                resp["success"] = true;
                resp["userId"] = user->userId;
                resp["role"] = to_string(user->role);
                resp["name"] = user->name;
                std::cout << "Login: " << user->name
                          << " (" << to_string(user->role) << ")\n";
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::LOGOUT_REQ: {
            auto user = sessionMgr.getUser(fd);
            if (user) classMgr.removeFromAll(user->userId, user->name);
            sessionMgr.logout(fd);
            nlohmann::json resp;
            resp["type"] = "LOGOUT_RESP";
            resp["success"] = true;
            server.send(fd, resp);
            break;
        }

        // ==================== User management ====================
        case MessageType::CREATE_USER_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "CREATE_USER_RESP";

            if (!caller || caller->role != Role::ADMIN) {
                resp["success"] = false;
                resp["message"] = "Permission denied: admin only";
            } else {
                std::string username = clampStr(msg.value("username", ""));
                std::string password = clampStr(msg.value("password", ""));
                std::string name = clampStr(msg.value("name", ""));
                Role role = role_from_string(msg.value("role", "STUDENT"));

                if (userStore.createUser(username, password, name, role)) {
                    resp["success"] = true;
                    resp["message"] = "User created";
                } else {
                    resp["success"] = false;
                    resp["message"] = "Username already exists";
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::LIST_USERS_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "LIST_USERS_RESP";

            if (!caller || caller->role != Role::ADMIN) {
                resp["success"] = false;
                resp["message"] = "Permission denied: admin only";
            } else {
                resp["success"] = true;
                nlohmann::json arr = nlohmann::json::array();
                for (auto& u : userStore.getAllUsers()) {
                    arr.push_back({
                        {"userId", u.userId},
                        {"username", u.username},
                        {"name", u.name},
                        {"role", to_string(u.role)}
                    });
                }
                resp["users"] = arr;
            }
            server.send(fd, resp);
            break;
        }

        // ==================== Class lifecycle ====================
        case MessageType::CREATE_CLASS_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "CREATE_CLASS_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                std::string name = clampStr(msg.value("name", ""));
                if (name.empty()) {
                    resp["success"] = false;
                    resp["message"] = "Class name required";
                } else {
                    int classId = classMgr.createClass(name, caller->userId, caller->name);
                    resp["success"] = true;
                    resp["classId"] = classId;
                    std::cout << "Class created: " << name << " (id=" << classId << ")\n";
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::START_CLASS_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "START_CLASS_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int classId = msg.value("classId", 0);
                if (classMgr.startClass(classId, caller->userId)) {
                    resp["success"] = true;
                    resp["classId"] = classId;
                    std::cout << "Class started: id=" << classId << "\n";
                } else {
                    resp["success"] = false;
                    resp["message"] = "Cannot start class (not found, not owner, or not WAITING)";
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::END_CLASS_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "END_CLASS_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int classId = msg.value("classId", 0);
                if (classMgr.endClass(classId, caller->userId)) {
                    resp["success"] = true;
                    resp["classId"] = classId;
                    std::cout << "Class ended: id=" << classId << "\n";
                } else {
                    resp["success"] = false;
                    resp["message"] = "Cannot end class (not found, not owner, or not ACTIVE)";
                }
            }
            server.send(fd, resp);
            break;
        }

        // ==================== Class participation ====================
        case MessageType::JOIN_CLASS_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "JOIN_CLASS_RESP";

            if (!caller || caller->role != Role::STUDENT) {
                resp["success"] = false;
                resp["message"] = "Permission denied: student only";
            } else {
                int classId = msg.value("classId", 0);
                if (classMgr.joinClass(classId, caller->userId, caller->name)) {
                    resp["success"] = true;
                    resp["classId"] = classId;
                } else {
                    resp["success"] = false;
                    resp["message"] = "Cannot join class (not found, not ACTIVE, or already joined)";
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::LEAVE_CLASS_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "LEAVE_CLASS_RESP";

            if (!caller || caller->role != Role::STUDENT) {
                resp["success"] = false;
                resp["message"] = "Permission denied: student only";
            } else {
                int classId = msg.value("classId", 0);
                if (classMgr.leaveClass(classId, caller->userId, caller->name)) {
                    resp["success"] = true;
                } else {
                    resp["success"] = false;
                    resp["message"] = "Not in this class";
                }
            }
            server.send(fd, resp);
            break;
        }

        // ==================== Queries ====================
        case MessageType::LIST_CLASSES_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "LIST_CLASSES_RESP";

            if (!caller) {
                resp["success"] = false;
                resp["message"] = "Not authenticated";
            } else {
                resp["success"] = true;
                nlohmann::json arr = nlohmann::json::array();
                for (auto& ci : classMgr.getAllClasses()) {
                    arr.push_back(ci.toJson());
                }
                resp["classes"] = arr;
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::CLASS_MEMBERS_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "CLASS_MEMBERS_RESP";

            if (!caller) {
                resp["success"] = false;
                resp["message"] = "Not authenticated";
            } else {
                int classId = msg.value("classId", 0);
                auto members = classMgr.getMemberList(classId);
                resp["success"] = true;
                resp["classId"] = classId;
                nlohmann::json arr = nlohmann::json::array();
                for (auto& [uid, name] : members) {
                    arr.push_back({{"userId", uid}, {"name", name}});
                }
                resp["members"] = arr;
            }
            server.send(fd, resp);
            break;
        }

        // ==================== Check-in ====================
        case MessageType::OPEN_CHECKIN_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "OPEN_CHECKIN_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int classId = msg.value("classId", 0);
                int deadline = msg.value("deadlineSeconds", 60);
                if (classMgr.openCheckin(classId, caller->userId, deadline)) {
                    resp["success"] = true;
                    resp["classId"] = classId;
                    resp["deadlineSeconds"] = deadline;
                    std::cout << "Check-in opened for class " << classId << " (deadline: " << deadline << "s)\n";

                    // Notify all students in the class
                    nlohmann::json notify;
                    notify["type"] = "NOTIFY_CHECKIN_OPENED";
                    notify["classId"] = classId;
                    notify["deadlineSeconds"] = deadline;
                    for (int uid : classMgr.getMembers(classId)) {
                        int memberFd = findFd(uid);
                        if (memberFd >= 0) server.send(memberFd, notify);
                    }
                } else {
                    resp["success"] = false;
                    resp["message"] = "Cannot open check-in (not owner or not ACTIVE)";
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::CHECKIN_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "CHECKIN_RESP";

            if (!caller || caller->role != Role::STUDENT) {
                resp["success"] = false;
                resp["message"] = "Permission denied: student only";
            } else {
                int classId = msg.value("classId", 0);
                if (classMgr.checkin(classId, caller->userId)) {
                    resp["success"] = true;
                } else {
                    resp["success"] = false;
                    resp["message"] = "Cannot check in (not open, not member, or already checked in)";
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::CHECKIN_STATUS_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "CHECKIN_STATUS_RESP";

            if (!caller) {
                resp["success"] = false;
                resp["message"] = "Not authenticated";
            } else {
                int classId = msg.value("classId", 0);
                resp["success"] = true;
                resp["classId"] = classId;
                resp["open"] = classMgr.isCheckinOpen(classId);
                nlohmann::json arr = nlohmann::json::array();
                for (auto& [name, status] : classMgr.getCheckinStatus(classId)) {
                    arr.push_back({{"name", name}, {"status", status}});
                }
                resp["students"] = arr;
            }
            server.send(fd, resp);
            break;
        }

        // ==================== Questions ====================
        case MessageType::PUBLISH_QUESTION_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "PUBLISH_QUESTION_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int classId = msg.value("classId", 0);
                auto ci = classMgr.getClass(classId);
                if (!ci || ci->teacherId != caller->userId || ci->status != ClassStatus::ACTIVE) {
                    resp["success"] = false;
                    resp["message"] = "Class not found, not owned, or not ACTIVE";
                } else {
                    std::string content = clampStr(msg.value("content", ""));
                    auto rawOptions = msg.value("options", std::vector<std::string>{});
                    std::vector<std::string> options;
                    for (auto& o : rawOptions) options.push_back(clampStr(o));
                    int correctAnswer = msg.value("correctAnswer", 0);

                    if (content.empty() || options.size() < 2) {
                        resp["success"] = false;
                        resp["message"] = "Question must have content and at least 2 options";
                    } else if (correctAnswer < 0 || correctAnswer >= static_cast<int>(options.size())) {
                        resp["success"] = false;
                        resp["message"] = "correctAnswer index out of range";
                    } else {
                        int qid = questionMgr.publishQuestion(classId, content, options, correctAnswer);
                        resp["success"] = true;
                        resp["questionId"] = qid;
                        std::cout << "Question published: id=" << qid << " class=" << classId << "\n";

                        // Notify all students in the class
                        auto q = questionMgr.getQuestion(qid);
                        nlohmann::json notify;
                        notify["type"] = "NOTIFY_QUESTION_PUBLISHED";
                        notify["classId"] = classId;
                        notify["question"] = q->toStudentJson();
                        for (int uid : classMgr.getMembers(classId)) {
                            int memberFd = findFd(uid);
                            if (memberFd >= 0) server.send(memberFd, notify);
                        }
                    }
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::GET_QUESTION_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "GET_QUESTION_RESP";

            if (!caller) {
                resp["success"] = false;
                resp["message"] = "Not authenticated";
            } else {
                int classId = msg.value("classId", 0);
                auto q = questionMgr.getActiveQuestion(classId);
                if (!q) {
                    resp["success"] = false;
                    resp["message"] = "No active question for this class";
                } else {
                    resp["success"] = true;
                    // Students see no correct answer; teachers see it
                    if (caller->role == Role::TEACHER) {
                        resp["question"] = q->toJson();
                    } else {
                        resp["question"] = q->toStudentJson();
                    }
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::SUBMIT_ANSWER_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "SUBMIT_ANSWER_RESP";

            if (!caller || caller->role != Role::STUDENT) {
                resp["success"] = false;
                resp["message"] = "Permission denied: student only";
            } else {
                int questionId = msg.value("questionId", 0);
                int answer = msg.value("answer", -1);

                // Check student is a member of the class this question belongs to
                auto q = questionMgr.getQuestion(questionId);
                if (!q) {
                    resp["success"] = false;
                    resp["message"] = "Question not found";
                } else {
                    auto members = classMgr.getMembers(q->classId);
                    bool isMember = false;
                    for (int uid : members) {
                        if (uid == caller->userId) { isMember = true; break; }
                    }
                    if (!isMember) {
                        resp["success"] = false;
                        resp["message"] = "Not a member of this class";
                    } else if (questionMgr.submitAnswer(questionId, caller->userId, caller->name, answer)) {
                        resp["success"] = true;
                    } else {
                        resp["success"] = false;
                        resp["message"] = "Already answered or question not active";
                    }
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::GET_ANSWERS_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "GET_ANSWERS_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int questionId = msg.value("questionId", 0);
                auto q = questionMgr.getQuestion(questionId);
                if (!q) {
                    resp["success"] = false;
                    resp["message"] = "Question not found";
                } else {
                    auto answers = questionMgr.getAnswers(questionId);
                    resp["success"] = true;
                    resp["questionId"] = questionId;
                    resp["correctAnswer"] = q->correctAnswer;
                    resp["totalOptions"] = static_cast<int>(q->options.size());

                    nlohmann::json arr = nlohmann::json::array();
                    int correctCount = 0;
                    for (auto& a : answers) {
                        arr.push_back(a.toJson());
                        if (a.answer == q->correctAnswer) ++correctCount;
                    }
                    resp["answers"] = arr;
                    resp["totalAnswers"] = static_cast<int>(answers.size());
                    resp["correctCount"] = correctCount;
                }
            }
            server.send(fd, resp);
            break;
        }

        // ==================== Random Calling ====================
        case MessageType::RANDOM_CALL_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "RANDOM_CALL_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int classId = msg.value("classId", 0);
                auto result = classMgr.randomCall(classId, caller->userId);
                if (!result) {
                    resp["success"] = false;
                    resp["message"] = "Cannot random call (class not found, not owned, not ACTIVE, or no students)";
                } else {
                    resp["success"] = true;
                    resp["classId"] = classId;
                    resp["userId"] = result->first;
                    resp["name"] = result->second;
                    std::cout << "Random call: class=" << classId
                              << " selected=" << result->second << "\n";

                    // Notify the selected student
                    int studentFd = findFd(result->first);
                    if (studentFd >= 0) {
                        nlohmann::json notify;
                        notify["type"] = "NOTIFY_RANDOM_CALLED";
                        notify["classId"] = classId;
                        notify["userId"] = result->first;
                        notify["name"] = result->second;
                        server.send(studentFd, notify);
                    }
                }
            }
            server.send(fd, resp);
            break;
        }

        // ==================== Screen Sharing ====================
        case MessageType::START_SCREEN_SHARE_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "START_SCREEN_SHARE_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int classId = msg.value("classId", 0);
                if (classMgr.startScreenShare(classId, caller->userId)) {
                    resp["success"] = true;
                    resp["classId"] = classId;
                    std::cout << "Screen sharing started: class=" << classId << "\n";

                    // Notify all students
                    nlohmann::json notify;
                    notify["type"] = "NOTIFY_SCREEN_SHARE_STARTED";
                    notify["classId"] = classId;
                    for (int uid : classMgr.getMembers(classId)) {
                        int memberFd = findFd(uid);
                        if (memberFd >= 0) server.send(memberFd, notify);
                    }
                } else {
                    resp["success"] = false;
                    resp["message"] = "Cannot start sharing (not found, not owned, not ACTIVE, or already sharing)";
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::STOP_SCREEN_SHARE_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "STOP_SCREEN_SHARE_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int classId = msg.value("classId", 0);
                if (classMgr.stopScreenShare(classId, caller->userId)) {
                    resp["success"] = true;
                    resp["classId"] = classId;
                    std::cout << "Screen sharing stopped: class=" << classId << "\n";

                    // Notify all students
                    nlohmann::json notify;
                    notify["type"] = "NOTIFY_SCREEN_SHARE_STOPPED";
                    notify["classId"] = classId;
                    for (int uid : classMgr.getMembers(classId)) {
                        int memberFd = findFd(uid);
                        if (memberFd >= 0) server.send(memberFd, notify);
                    }
                } else {
                    resp["success"] = false;
                    resp["message"] = "Cannot stop sharing (not found, not owned, or not sharing)";
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::SCREEN_FRAME: {
            auto caller = sessionMgr.getUser(fd);
            if (!caller || caller->role != Role::TEACHER) break;

            int classId = msg.value("classId", 0);
            if (!classMgr.isScreenSharing(classId)) break;

            auto ci = classMgr.getClass(classId);
            if (!ci || ci->teacherId != caller->userId) break;

            // Broadcast frame to all students in the class
            nlohmann::json notify;
            notify["type"] = "NOTIFY_SCREEN_FRAME";
            notify["classId"] = classId;
            notify["frameData"] = msg.value("frameData", "");
            notify["frameSeq"] = msg.value("frameSeq", 0);

            for (int uid : classMgr.getMembers(classId)) {
                int memberFd = findFd(uid);
                if (memberFd >= 0) server.send(memberFd, notify);
            }
            break;
        }

        // ==================== Audio ====================
        case MessageType::START_AUDIO_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "START_AUDIO_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int classId = msg.value("classId", 0);
                if (classMgr.startAudio(classId, caller->userId)) {
                    resp["success"] = true;
                    resp["classId"] = classId;
                    std::cout << "Audio started: class=" << classId << "\n";

                    nlohmann::json notify;
                    notify["type"] = "NOTIFY_AUDIO_STARTED";
                    notify["classId"] = classId;
                    for (int uid : classMgr.getMembers(classId)) {
                        int memberFd = findFd(uid);
                        if (memberFd >= 0) server.send(memberFd, notify);
                    }
                } else {
                    resp["success"] = false;
                    resp["message"] = "Cannot start audio (not found, not owned, not ACTIVE, or already active)";
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::STOP_AUDIO_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "STOP_AUDIO_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int classId = msg.value("classId", 0);
                if (classMgr.stopAudio(classId, caller->userId)) {
                    resp["success"] = true;
                    resp["classId"] = classId;
                    std::cout << "Audio stopped: class=" << classId << "\n";

                    nlohmann::json notify;
                    notify["type"] = "NOTIFY_AUDIO_STOPPED";
                    notify["classId"] = classId;
                    for (int uid : classMgr.getMembers(classId)) {
                        int memberFd = findFd(uid);
                        if (memberFd >= 0) server.send(memberFd, notify);
                    }
                } else {
                    resp["success"] = false;
                    resp["message"] = "Cannot stop audio (not found, not owned, or not active)";
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::AUDIO_FRAME: {
            auto caller = sessionMgr.getUser(fd);
            if (!caller) break;

            int classId = msg.value("classId", 0);
            if (!classMgr.isAudioActive(classId)) break;

            auto ci = classMgr.getClass(classId);
            if (!ci) break;

            // Determine if sender is allowed
            bool isTeacher = (caller->role == Role::TEACHER && ci->teacherId == caller->userId);
            bool isMicHolder = (caller->role == Role::STUDENT &&
                                classMgr.getMicHolder(classId) == caller->userId);

            if (!isTeacher && !isMicHolder) break;

            // Build notification
            nlohmann::json notify;
            notify["type"] = "NOTIFY_AUDIO_FRAME";
            notify["classId"] = classId;
            notify["audioData"] = msg.value("audioData", "");
            notify["frameSeq"] = msg.value("frameSeq", 0);
            notify["senderId"] = caller->userId;
            notify["senderName"] = caller->name;

            // Broadcast to all members except sender
            for (int uid : classMgr.getMembers(classId)) {
                if (uid == caller->userId) continue;
                int memberFd = findFd(uid);
                if (memberFd >= 0) server.send(memberFd, notify);
            }
            // Also send to teacher if sender is a student
            if (isMicHolder) {
                int teacherFd = findFd(ci->teacherId);
                if (teacherFd >= 0) server.send(teacherFd, notify);
            }
            break;
        }

        case MessageType::GRANT_MIC_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "GRANT_MIC_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int classId = msg.value("classId", 0);
                int studentId = msg.value("studentId", 0);
                int prevHolder = classMgr.getMicHolder(classId);
                if (classMgr.grantMic(classId, caller->userId, studentId)) {
                    // Notify previous holder they lost mic
                    if (prevHolder >= 0 && prevHolder != studentId) {
                        int prevFd = findFd(prevHolder);
                        if (prevFd >= 0) {
                            nlohmann::json revokeNotify;
                            revokeNotify["type"] = "NOTIFY_MIC_REVOKED";
                            revokeNotify["classId"] = classId;
                            revokeNotify["studentId"] = prevHolder;
                            server.send(prevFd, revokeNotify);
                        }
                    }
                    resp["success"] = true;
                    resp["classId"] = classId;
                    resp["studentId"] = studentId;
                    std::cout << "Mic granted: class=" << classId
                              << " student=" << studentId << "\n";

                    // Notify the student
                    int studentFd = findFd(studentId);
                    if (studentFd >= 0) {
                        nlohmann::json notify;
                        notify["type"] = "NOTIFY_MIC_GRANTED";
                        notify["classId"] = classId;
                        notify["studentId"] = studentId;
                        server.send(studentFd, notify);
                    }
                } else {
                    resp["success"] = false;
                    resp["message"] = "Cannot grant mic (audio not active, student not member, or class issue)";
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::REVOKE_MIC_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "REVOKE_MIC_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int classId = msg.value("classId", 0);
                int prevHolder = classMgr.getMicHolder(classId);
                if (classMgr.revokeMic(classId, caller->userId)) {
                    resp["success"] = true;
                    resp["classId"] = classId;
                    std::cout << "Mic revoked: class=" << classId << "\n";

                    // Notify the student who lost mic
                    if (prevHolder >= 0) {
                        int studentFd = findFd(prevHolder);
                        if (studentFd >= 0) {
                            nlohmann::json notify;
                            notify["type"] = "NOTIFY_MIC_REVOKED";
                            notify["classId"] = classId;
                            notify["studentId"] = prevHolder;
                            server.send(studentFd, notify);
                        }
                    }
                } else {
                    resp["success"] = false;
                    resp["message"] = "Cannot revoke mic (no mic holder or class issue)";
                }
            }
            server.send(fd, resp);
            break;
        }

        // ==================== Statistics & Export ====================
        case MessageType::CLASS_SUMMARY_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "CLASS_SUMMARY_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int classId = msg.value("classId", 0);
                auto ci = classMgr.getClass(classId);
                if (!ci || ci->teacherId != caller->userId) {
                    resp["success"] = false;
                    resp["message"] = "Class not found or not owned by you";
                } else {
                    auto events = classMgr.getEventsForClass(classId);
                    auto questions = questionMgr.getQuestionsForClass(classId);
                    std::vector<std::vector<AnswerRecord>> answersPerQ;
                    for (auto& q : questions) {
                        answersPerQ.push_back(questionMgr.getAnswers(q.questionId));
                    }
                    auto summary = StatisticsExporter::buildSummary(*ci, events, questions, answersPerQ);
                    resp["success"] = true;
                    resp["summary"] = StatisticsExporter::summaryToJson(summary);
                }
            }
            server.send(fd, resp);
            break;
        }

        case MessageType::EXPORT_CLASS_REQ: {
            auto caller = sessionMgr.getUser(fd);
            nlohmann::json resp;
            resp["type"] = "EXPORT_CLASS_RESP";

            if (!caller || caller->role != Role::TEACHER) {
                resp["success"] = false;
                resp["message"] = "Permission denied: teacher only";
            } else {
                int classId = msg.value("classId", 0);
                auto ci = classMgr.getClass(classId);
                if (!ci || ci->teacherId != caller->userId) {
                    resp["success"] = false;
                    resp["message"] = "Class not found or not owned by you";
                } else {
                    auto events = classMgr.getEventsForClass(classId);
                    auto questions = questionMgr.getQuestionsForClass(classId);
                    std::vector<std::vector<AnswerRecord>> answersPerQ;
                    for (auto& q : questions) {
                        answersPerQ.push_back(questionMgr.getAnswers(q.questionId));
                    }
                    auto summary = StatisticsExporter::buildSummary(*ci, events, questions, answersPerQ);
                    resp["success"] = true;
                    resp["csv"] = StatisticsExporter::summaryToCSV(summary);
                    std::cout << "Export generated for class " << classId << "\n";
                }
            }
            server.send(fd, resp);
            break;
        }

        default:
            std::cerr << "Unknown message type: " << typeStr << "\n";
            break;
        }
      } catch (const std::exception& ex) {
        std::cerr << "Error handling message from fd=" << fd << ": " << ex.what() << "\n";
      }
    });

    std::cout << "Thunder Class Server starting...\n";
    server.run();
    return 0;
}
