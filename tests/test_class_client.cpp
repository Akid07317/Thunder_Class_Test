// Console client for verifying class lifecycle management.
// Requires: server running on localhost:9000 with clean data/ directory.
// Tests: create class, start, join, leave, member list, check-in, end class.

#include <common/protocol/protocol.h>
#include <common/protocol/message_type.h>
#include <common/model/user.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>

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
        if (n <= 0) {
            std::cerr << "Connection closed\n";
            return {};
        }
        buffer.insert(buffer.end(), buf, buf + n);
        if (Protocol::extractFrame(buffer, result)) {
            return result;
        }
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

// Helper: login on a new connection, return the fd
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

    std::cout << "=== Class Lifecycle Tests ===\n\n";

    // --- Setup: create teacher and student accounts via admin ---
    std::cout << "--- Setup: Creating accounts ---\n";
    int adminFd = loginAs(host, port, "admin", "admin123");
    if (adminFd < 0) { std::cerr << "Cannot connect as admin\n"; return 1; }

    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "t_class"}, {"password", "pass"},
                       {"name", "Teacher Wang"}, {"role", "TEACHER"}});
    recvMsg(adminFd);

    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_class1"}, {"password", "pass"},
                       {"name", "Student A"}, {"role", "STUDENT"}});
    recvMsg(adminFd);

    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_class2"}, {"password", "pass"},
                       {"name", "Student B"}, {"role", "STUDENT"}});
    recvMsg(adminFd);
    close(adminFd);
    std::cout << "  Accounts created.\n\n";

    // --- Login as teacher and students ---
    int teacherFd = loginAs(host, port, "t_class", "pass");
    int student1Fd = loginAs(host, port, "s_class1", "pass");
    int student2Fd = loginAs(host, port, "s_class2", "pass");
    if (teacherFd < 0 || student1Fd < 0 || student2Fd < 0) {
        std::cerr << "Login failed\n"; return 1;
    }

    nlohmann::json resp;

    // === Test 1: Teacher creates a class ===
    std::cout << "--- Test 1: Teacher creates a class ---\n";
    sendMsg(teacherFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "C++ Lecture 1"}});
    resp = recvMsg(teacherFd);
    check("Teacher creates class", resp, true);
    int classId = resp.value("classId", 0);
    std::cout << "  classId = " << classId << "\n\n";

    // === Test 2: Student cannot create a class ===
    std::cout << "--- Test 2: Student cannot create a class ---\n";
    sendMsg(student1Fd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Hack Class"}});
    resp = recvMsg(student1Fd);
    check("Student cannot create class", resp, false);
    std::cout << "\n";

    // === Test 3: Student cannot join a WAITING class ===
    std::cout << "--- Test 3: Student cannot join WAITING class ---\n";
    sendMsg(student1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    resp = recvMsg(student1Fd);
    check("Cannot join WAITING class", resp, false);
    std::cout << "\n";

    // === Test 4: Teacher starts the class ===
    std::cout << "--- Test 4: Teacher starts the class ---\n";
    sendMsg(teacherFd, {{"type", "START_CLASS_REQ"}, {"classId", classId}});
    resp = recvMsg(teacherFd);
    check("Teacher starts class", resp, true);
    std::cout << "\n";

    // === Test 5: Student joins the active class ===
    std::cout << "--- Test 5: Student A joins the class ---\n";
    sendMsg(student1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    resp = recvMsg(student1Fd);
    check("Student A joins class", resp, true);
    std::cout << "\n";

    // === Test 6: Student B joins the active class ===
    std::cout << "--- Test 6: Student B joins the class ---\n";
    sendMsg(student2Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    resp = recvMsg(student2Fd);
    check("Student B joins class", resp, true);
    std::cout << "\n";

    // === Test 7: Duplicate join rejected ===
    std::cout << "--- Test 7: Duplicate join rejected ---\n";
    sendMsg(student1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    resp = recvMsg(student1Fd);
    check("Duplicate join rejected", resp, false);
    std::cout << "\n";

    // === Test 8: List classes shows ACTIVE class ===
    std::cout << "--- Test 8: List classes ---\n";
    sendMsg(student1Fd, {{"type", "LIST_CLASSES_REQ"}});
    resp = recvMsg(student1Fd);
    check("List classes succeeds", resp, true);
    if (resp.contains("classes")) {
        bool found = false;
        for (auto& c : resp["classes"]) {
            if (c.value("classId", 0) == classId) {
                std::string status = c.value("status", "");
                found = true;
                if (status == "ACTIVE") {
                    std::cout << "  PASS: Class status is ACTIVE\n"; ++passed;
                } else {
                    std::cout << "  FAIL: Expected ACTIVE, got " << status << "\n"; ++failed;
                }
            }
        }
        if (!found) { std::cout << "  FAIL: Class not found in list\n"; ++failed; }
    }
    std::cout << "\n";

    // === Test 9: Member list shows 2 students ===
    std::cout << "--- Test 9: Member list ---\n";
    sendMsg(teacherFd, {{"type", "CLASS_MEMBERS_REQ"}, {"classId", classId}});
    resp = recvMsg(teacherFd);
    check("Member list succeeds", resp, true);
    if (resp.contains("members")) {
        int count = resp["members"].size();
        if (count == 2) {
            std::cout << "  PASS: 2 members in class\n"; ++passed;
        } else {
            std::cout << "  FAIL: Expected 2 members, got " << count << "\n"; ++failed;
        }
    }
    std::cout << "\n";

    // === Test 10: Teacher opens check-in ===
    std::cout << "--- Test 10: Teacher opens check-in ---\n";
    sendMsg(teacherFd, {{"type", "OPEN_CHECKIN_REQ"}, {"classId", classId}});
    resp = recvMsg(teacherFd);
    check("Open check-in", resp, true);

    // Students should receive NOTIFY_CHECKIN_OPENED
    auto notify1 = recvMsg(student1Fd);
    auto notify2 = recvMsg(student2Fd);
    if (notify1.value("type", "") == "NOTIFY_CHECKIN_OPENED") {
        std::cout << "  PASS: Student A received check-in notification\n"; ++passed;
    } else {
        std::cout << "  FAIL: Student A did not receive notification\n"; ++failed;
    }
    if (notify2.value("type", "") == "NOTIFY_CHECKIN_OPENED") {
        std::cout << "  PASS: Student B received check-in notification\n"; ++passed;
    } else {
        std::cout << "  FAIL: Student B did not receive notification\n"; ++failed;
    }
    std::cout << "\n";

    // === Test 11: Student A checks in ===
    std::cout << "--- Test 11: Student A checks in ---\n";
    sendMsg(student1Fd, {{"type", "CHECKIN_REQ"}, {"classId", classId}});
    resp = recvMsg(student1Fd);
    check("Student A checks in", resp, true);
    std::cout << "\n";

    // === Test 12: Student A duplicate check-in rejected ===
    std::cout << "--- Test 12: Duplicate check-in rejected ---\n";
    sendMsg(student1Fd, {{"type", "CHECKIN_REQ"}, {"classId", classId}});
    resp = recvMsg(student1Fd);
    check("Duplicate check-in rejected", resp, false);
    std::cout << "\n";

    // === Test 13: Check-in status ===
    std::cout << "--- Test 13: Check-in status ---\n";
    sendMsg(teacherFd, {{"type", "CHECKIN_STATUS_REQ"}, {"classId", classId}});
    resp = recvMsg(teacherFd);
    check("Check-in status query", resp, true);
    if (resp.contains("students")) {
        int checkedCount = 0;
        for (auto& s : resp["students"]) {
            std::string status = s.value("status", "absent");
            if (status == "on_time" || status == "late") ++checkedCount;
        }
        if (checkedCount == 1) {
            std::cout << "  PASS: 1 of 2 students checked in\n"; ++passed;
        } else {
            std::cout << "  FAIL: Expected 1 checked in, got " << checkedCount << "\n"; ++failed;
        }
    }
    std::cout << "\n";

    // === Test 14: Student B leaves the class ===
    std::cout << "--- Test 14: Student B leaves ---\n";
    sendMsg(student2Fd, {{"type", "LEAVE_CLASS_REQ"}, {"classId", classId}});
    resp = recvMsg(student2Fd);
    check("Student B leaves", resp, true);
    std::cout << "\n";

    // === Test 15: Member list now shows 1 ===
    std::cout << "--- Test 15: Member list after leave ---\n";
    sendMsg(teacherFd, {{"type", "CLASS_MEMBERS_REQ"}, {"classId", classId}});
    resp = recvMsg(teacherFd);
    if (resp.contains("members") && resp["members"].size() == 1) {
        std::cout << "  PASS: 1 member remaining\n"; ++passed;
    } else {
        std::cout << "  FAIL: Expected 1 member\n"; ++failed;
    }
    std::cout << "\n";

    // === Test 16: Teacher ends the class ===
    std::cout << "--- Test 16: Teacher ends the class ---\n";
    sendMsg(teacherFd, {{"type", "END_CLASS_REQ"}, {"classId", classId}});
    resp = recvMsg(teacherFd);
    check("Teacher ends class", resp, true);
    std::cout << "\n";

    // === Test 17: Cannot join ENDED class ===
    std::cout << "--- Test 17: Cannot join ended class ---\n";
    sendMsg(student2Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    resp = recvMsg(student2Fd);
    check("Cannot join ENDED class", resp, false);
    std::cout << "\n";

    // === Test 18: Cannot start already-ended class ===
    std::cout << "--- Test 18: Cannot restart ended class ---\n";
    sendMsg(teacherFd, {{"type", "START_CLASS_REQ"}, {"classId", classId}});
    resp = recvMsg(teacherFd);
    check("Cannot restart ENDED class", resp, false);
    std::cout << "\n";

    // Cleanup
    close(teacherFd);
    close(student1Fd);
    close(student2Fd);

    std::cout << "=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
