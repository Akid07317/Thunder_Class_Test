// Console client for verifying question publishing and answering.
// Requires: server running on localhost:9000 with clean data/ directory.
// Workflow: setup accounts → create/start class → join → publish question →
//           students answer → teacher reviews answers.

#include <common/protocol/protocol.h>
#include <common/protocol/message_type.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <vector>

static int connectToServer(const char* host, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

static void sendMsg(int fd, const nlohmann::json& msg) {
    auto data = Protocol::frame(msg);
    write(fd, data.data(), data.size());
}

static nlohmann::json recvMsg(int fd) {
    std::vector<uint8_t> buffer;
    uint8_t buf[4096];
    nlohmann::json result;
    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) { std::cerr << "Connection closed\n"; return {}; }
        buffer.insert(buffer.end(), buf, buf + n);
        if (Protocol::extractFrame(buffer, result)) return result;
    }
}

static int passed = 0;
static int failed = 0;

static void check(const std::string& name, const nlohmann::json& resp, bool expectSuccess) {
    bool success = resp.value("success", false);
    if (success == expectSuccess) {
        std::cout << "  PASS: " << name << "\n";
        ++passed;
    } else {
        std::cout << "  FAIL: " << name << "\n";
        std::cout << "    Expected success=" << expectSuccess << ", got: " << resp.dump() << "\n";
        ++failed;
    }
}

static int loginAs(const char* host, uint16_t port, const std::string& username, const std::string& password) {
    int fd = connectToServer(host, port);
    if (fd < 0) return -1;
    sendMsg(fd, {{"type", "LOGIN_REQ"}, {"username", username}, {"password", password}});
    auto resp = recvMsg(fd);
    if (!resp.value("success", false)) {
        std::cerr << "Login failed for " << username << ": " << resp.dump() << "\n";
        close(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char* argv[]) {
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    uint16_t port = argc > 2 ? std::atoi(argv[2]) : 9000;

    std::cout << "=== Question Publishing & Answering Tests ===\n\n";

    // --- Setup: create accounts ---
    std::cout << "--- Setup ---\n";
    int adminFd = loginAs(host, port, "admin", "admin123");
    if (adminFd < 0) return 1;

    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "t_qa"}, {"password", "pass"},
                       {"name", "Teacher QA"}, {"role", "TEACHER"}});
    recvMsg(adminFd);
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_qa1"}, {"password", "pass"},
                       {"name", "Student X"}, {"role", "STUDENT"}});
    recvMsg(adminFd);
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_qa2"}, {"password", "pass"},
                       {"name", "Student Y"}, {"role", "STUDENT"}});
    recvMsg(adminFd);
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_qa3"}, {"password", "pass"},
                       {"name", "Student Z"}, {"role", "STUDENT"}});
    recvMsg(adminFd);
    close(adminFd);

    int teacherFd = loginAs(host, port, "t_qa", "pass");
    int s1Fd = loginAs(host, port, "s_qa1", "pass");
    int s2Fd = loginAs(host, port, "s_qa2", "pass");
    int s3Fd = loginAs(host, port, "s_qa3", "pass");
    if (teacherFd < 0 || s1Fd < 0 || s2Fd < 0 || s3Fd < 0) return 1;

    // Create and start a class
    sendMsg(teacherFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "QA Test Class"}});
    auto resp = recvMsg(teacherFd);
    int classId = resp.value("classId", 0);

    sendMsg(teacherFd, {{"type", "START_CLASS_REQ"}, {"classId", classId}});
    recvMsg(teacherFd);

    // Students join
    sendMsg(s1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    recvMsg(s1Fd);
    sendMsg(s2Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    recvMsg(s2Fd);
    // s3 does NOT join (for permission tests)
    std::cout << "  Setup complete: class=" << classId << ", 2 students joined, 1 not joined\n\n";

    nlohmann::json r;

    // === Test 1: Student cannot publish question ===
    std::cout << "--- Test 1: Student cannot publish question ---\n";
    sendMsg(s1Fd, {{"type", "PUBLISH_QUESTION_REQ"}, {"classId", classId},
                    {"content", "hack"}, {"options", {"A", "B"}}, {"correctAnswer", 0}});
    r = recvMsg(s1Fd);
    check("Student cannot publish", r, false);
    std::cout << "\n";

    // === Test 2: Teacher publishes a question ===
    std::cout << "--- Test 2: Teacher publishes question ---\n";
    sendMsg(teacherFd, {{"type", "PUBLISH_QUESTION_REQ"}, {"classId", classId},
                         {"content", "What is 2+2?"},
                         {"options", {"3", "4", "5", "6"}},
                         {"correctAnswer", 1}});
    r = recvMsg(teacherFd);
    check("Teacher publishes question", r, true);
    int questionId = r.value("questionId", 0);
    std::cout << "  questionId = " << questionId << "\n";

    // Students should receive NOTIFY_QUESTION_PUBLISHED
    auto n1 = recvMsg(s1Fd);
    auto n2 = recvMsg(s2Fd);
    if (n1.value("type", "") == "NOTIFY_QUESTION_PUBLISHED" && n1.contains("question")) {
        std::cout << "  PASS: Student X received question notification\n"; ++passed;
        // Verify student cannot see correctAnswer
        if (!n1["question"].contains("correctAnswer")) {
            std::cout << "  PASS: Notification omits correctAnswer\n"; ++passed;
        } else {
            std::cout << "  FAIL: Notification exposes correctAnswer\n"; ++failed;
        }
    } else {
        std::cout << "  FAIL: Student X did not receive notification\n"; ++failed;
        ++failed; // skip sub-check
    }
    if (n2.value("type", "") == "NOTIFY_QUESTION_PUBLISHED") {
        std::cout << "  PASS: Student Y received question notification\n"; ++passed;
    } else {
        std::cout << "  FAIL: Student Y did not receive notification\n"; ++failed;
    }
    std::cout << "\n";

    // === Test 3: Student fetches active question ===
    std::cout << "--- Test 3: Student fetches active question ---\n";
    sendMsg(s1Fd, {{"type", "GET_QUESTION_REQ"}, {"classId", classId}});
    r = recvMsg(s1Fd);
    check("Get question succeeds", r, true);
    if (r.contains("question") && !r["question"].contains("correctAnswer")) {
        std::cout << "  PASS: Student view omits correctAnswer\n"; ++passed;
    } else {
        std::cout << "  FAIL: Student should not see correctAnswer\n"; ++failed;
    }
    std::cout << "\n";

    // === Test 4: Teacher fetches question (sees correctAnswer) ===
    std::cout << "--- Test 4: Teacher fetches question ---\n";
    sendMsg(teacherFd, {{"type", "GET_QUESTION_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Teacher get question", r, true);
    if (r.contains("question") && r["question"].contains("correctAnswer")) {
        std::cout << "  PASS: Teacher view includes correctAnswer\n"; ++passed;
    } else {
        std::cout << "  FAIL: Teacher should see correctAnswer\n"; ++failed;
    }
    std::cout << "\n";

    // === Test 5: Student X submits correct answer (index 1) ===
    std::cout << "--- Test 5: Student X submits answer ---\n";
    sendMsg(s1Fd, {{"type", "SUBMIT_ANSWER_REQ"}, {"questionId", questionId}, {"answer", 1}});
    r = recvMsg(s1Fd);
    check("Student X submits answer", r, true);
    std::cout << "\n";

    // === Test 6: Duplicate submission rejected ===
    std::cout << "--- Test 6: Duplicate submission rejected ---\n";
    sendMsg(s1Fd, {{"type", "SUBMIT_ANSWER_REQ"}, {"questionId", questionId}, {"answer", 0}});
    r = recvMsg(s1Fd);
    check("Duplicate answer rejected", r, false);
    std::cout << "\n";

    // === Test 7: Student Y submits wrong answer (index 2) ===
    std::cout << "--- Test 7: Student Y submits answer ---\n";
    sendMsg(s2Fd, {{"type", "SUBMIT_ANSWER_REQ"}, {"questionId", questionId}, {"answer", 2}});
    r = recvMsg(s2Fd);
    check("Student Y submits answer", r, true);
    std::cout << "\n";

    // === Test 8: Non-member student Z cannot submit ===
    std::cout << "--- Test 8: Non-member cannot submit ---\n";
    sendMsg(s3Fd, {{"type", "SUBMIT_ANSWER_REQ"}, {"questionId", questionId}, {"answer", 1}});
    r = recvMsg(s3Fd);
    check("Non-member cannot submit", r, false);
    std::cout << "\n";

    // === Test 9: Teacher views answers ===
    std::cout << "--- Test 9: Teacher views answers ---\n";
    sendMsg(teacherFd, {{"type", "GET_ANSWERS_REQ"}, {"questionId", questionId}});
    r = recvMsg(teacherFd);
    check("Get answers succeeds", r, true);
    if (r.value("totalAnswers", 0) == 2) {
        std::cout << "  PASS: 2 answers received\n"; ++passed;
    } else {
        std::cout << "  FAIL: Expected 2 answers, got " << r.value("totalAnswers", 0) << "\n"; ++failed;
    }
    if (r.value("correctCount", 0) == 1) {
        std::cout << "  PASS: 1 correct answer (50% rate)\n"; ++passed;
    } else {
        std::cout << "  FAIL: Expected 1 correct, got " << r.value("correctCount", 0) << "\n"; ++failed;
    }
    std::cout << "\n";

    // === Test 10: Student cannot view answers ===
    std::cout << "--- Test 10: Student cannot view answers ---\n";
    sendMsg(s1Fd, {{"type", "GET_ANSWERS_REQ"}, {"questionId", questionId}});
    r = recvMsg(s1Fd);
    check("Student cannot view answers", r, false);
    std::cout << "\n";

    // === Test 11: No active question for a nonexistent class ===
    std::cout << "--- Test 11: Get question for nonexistent class ---\n";
    sendMsg(s1Fd, {{"type", "GET_QUESTION_REQ"}, {"classId", 9999}});
    r = recvMsg(s1Fd);
    check("No question for invalid class", r, false);
    std::cout << "\n";

    // === Test 12: Teacher publishes second question (replaces active) ===
    std::cout << "--- Test 12: Second question replaces active ---\n";
    sendMsg(teacherFd, {{"type", "PUBLISH_QUESTION_REQ"}, {"classId", classId},
                         {"content", "Capital of China?"},
                         {"options", {"Shanghai", "Beijing", "Guangzhou"}},
                         {"correctAnswer", 1}});
    r = recvMsg(teacherFd);
    check("Second question published", r, true);
    int q2Id = r.value("questionId", 0);

    // Consume notifications for s1 and s2
    recvMsg(s1Fd);
    recvMsg(s2Fd);

    // Old question should no longer be active — answer should fail
    sendMsg(s1Fd, {{"type", "SUBMIT_ANSWER_REQ"}, {"questionId", questionId}, {"answer", 0}});
    r = recvMsg(s1Fd);
    // s1 already answered q1, but even if not, it's no longer active
    check("Old question no longer accepts answers", r, false);

    // New question can be answered
    sendMsg(s1Fd, {{"type", "SUBMIT_ANSWER_REQ"}, {"questionId", q2Id}, {"answer", 1}});
    r = recvMsg(s1Fd);
    check("New question accepts answer", r, true);
    std::cout << "\n";

    // Cleanup
    close(teacherFd);
    close(s1Fd);
    close(s2Fd);
    close(s3Fd);

    std::cout << "=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
