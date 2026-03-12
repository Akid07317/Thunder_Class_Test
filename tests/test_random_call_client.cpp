// Console client for verifying random calling.
// Requires: server running on localhost:9000 with clean data/.
// Workflow: create accounts -> create/start class -> students join ->
//           teacher triggers random call -> verify result + notification.

#include "test_socket.h"
#include <unordered_set>

static int passed = 0;
static int failed = 0;

static void check(const std::string& name, bool condition) {
    if (condition) {
        std::cout << "  PASS: " << name << "\n"; ++passed;
    } else {
        std::cout << "  FAIL: " << name << "\n"; ++failed;
    }
}

static int loginAs(const char* host, uint16_t port, const std::string& username, const std::string& password) {
    int fd = connectToServer(host, port);
    if (fd < 0) return -1;
    sendMsg(fd, {{"type", "LOGIN_REQ"}, {"username", username}, {"password", password}});
    auto resp = recvMsg(fd);
    if (!resp.value("success", false)) {
        std::cerr << "Login failed for " << username << ": " << resp.dump() << "\n";
        sockClose(fd); return -1;
    }
    return fd;
}

int main(int argc, char* argv[]) {
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    uint16_t port = argc > 2 ? std::atoi(argv[2]) : 9000;

    std::cout << "=== Random Calling Tests ===\n\n";

    // --- Setup: create accounts ---
    std::cout << "--- Setup ---\n";
    int adminFd = loginAs(host, port, "admin", "admin123");
    if (adminFd < 0) return 1;

    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "t_rand"}, {"password", "pass"},
                       {"name", "Teacher Rand"}, {"role", "TEACHER"}});
    recvMsg(adminFd);
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_rand1"}, {"password", "pass"},
                       {"name", "Alice"}, {"role", "STUDENT"}});
    recvMsg(adminFd);
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_rand2"}, {"password", "pass"},
                       {"name", "Bob"}, {"role", "STUDENT"}});
    recvMsg(adminFd);
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_rand3"}, {"password", "pass"},
                       {"name", "Charlie"}, {"role", "STUDENT"}});
    recvMsg(adminFd);
    sockClose(adminFd);

    int teacherFd = loginAs(host, port, "t_rand", "pass");
    int s1Fd = loginAs(host, port, "s_rand1", "pass");
    int s2Fd = loginAs(host, port, "s_rand2", "pass");
    int s3Fd = loginAs(host, port, "s_rand3", "pass");
    if (teacherFd < 0 || s1Fd < 0 || s2Fd < 0 || s3Fd < 0) return 1;

    // Create and start class
    sendMsg(teacherFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Random Call Test"}});
    auto r = recvMsg(teacherFd);
    int classId = r.value("classId", 0);
    sendMsg(teacherFd, {{"type", "START_CLASS_REQ"}, {"classId", classId}});
    recvMsg(teacherFd);

    // Students join
    sendMsg(s1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    recvMsg(s1Fd);
    sendMsg(s2Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    recvMsg(s2Fd);
    sendMsg(s3Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    recvMsg(s3Fd);

    std::cout << "  Setup complete: class=" << classId << ", 3 students joined.\n\n";

    // === Test 1: Student cannot trigger random call ===
    std::cout << "--- Test 1: Student cannot trigger random call ---\n";
    sendMsg(s1Fd, {{"type", "RANDOM_CALL_REQ"}, {"classId", classId}});
    r = recvMsg(s1Fd);
    check("Student cannot random call", !r.value("success", false));
    std::cout << "\n";

    // === Test 2: Teacher triggers random call - valid ===
    std::cout << "--- Test 2: Teacher triggers random call ---\n";
    sendMsg(teacherFd, {{"type", "RANDOM_CALL_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Random call succeeds", r.value("success", false));
    check("Response has classId", r.value("classId", 0) == classId);
    check("Response has userId", r.contains("userId") && r["userId"].is_number());
    check("Response has name", r.contains("name") && r["name"].is_string());

    std::string selectedName = r.value("name", "");
    int selectedUserId = r.value("userId", 0);
    std::unordered_set<std::string> validNames = {"Alice", "Bob", "Charlie"};
    check("Selected student is a class member", validNames.count(selectedName) > 0);
    std::cout << "  Selected: " << selectedName << " (userId=" << selectedUserId << ")\n\n";

    // === Test 3: Selected student receives notification ===
    std::cout << "--- Test 3: Selected student receives notification ---\n";
    // Check all students for the notification
    bool notificationReceived = false;
    int notifiedFds[] = {s1Fd, s2Fd, s3Fd};
    std::string notifiedNames[] = {"Alice", "Bob", "Charlie"};
    for (int i = 0; i < 3; ++i) {
        auto notify = tryRecvMsg(notifiedFds[i]);
        if (!notify.empty() && notify.value("type", "") == "NOTIFY_RANDOM_CALLED") {
            check("Notification type is NOTIFY_RANDOM_CALLED", true);
            check("Notification has correct classId", notify.value("classId", 0) == classId);
            check("Notification has correct name", notify.value("name", "") == selectedName);
            check("Notified student matches selected", notifiedNames[i] == selectedName);
            notificationReceived = true;
        }
    }
    check("Notification was received by selected student", notificationReceived);
    std::cout << "\n";

    // === Test 4: Multiple random calls produce valid results ===
    std::cout << "--- Test 4: Multiple random calls (10x) ---\n";
    std::unordered_set<std::string> seen;
    bool allValid = true;
    for (int i = 0; i < 10; ++i) {
        sendMsg(teacherFd, {{"type", "RANDOM_CALL_REQ"}, {"classId", classId}});
        r = recvMsg(teacherFd);
        if (!r.value("success", false)) { allValid = false; break; }
        std::string name = r.value("name", "");
        if (!validNames.count(name)) { allValid = false; break; }
        seen.insert(name);

        // Drain any notification from students
        for (int j = 0; j < 3; ++j) tryRecvMsg(notifiedFds[j]);
    }
    check("All 10 random calls succeed and return valid names", allValid);
    check("Multiple different students were selected (randomness check)", seen.size() > 1);
    std::cout << "  Unique names seen in 10 calls: " << seen.size() << "\n\n";

    // === Test 5: Random call on non-ACTIVE class fails ===
    std::cout << "--- Test 5: Random call on ended class ---\n";
    sendMsg(teacherFd, {{"type", "END_CLASS_REQ"}, {"classId", classId}});
    recvMsg(teacherFd);
    sendMsg(teacherFd, {{"type", "RANDOM_CALL_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Random call on ENDED class fails", !r.value("success", false));
    std::cout << "\n";

    // === Test 6: Random call on nonexistent class fails ===
    std::cout << "--- Test 6: Random call on nonexistent class ---\n";
    sendMsg(teacherFd, {{"type", "RANDOM_CALL_REQ"}, {"classId", 9999}});
    r = recvMsg(teacherFd);
    check("Random call on nonexistent class fails", !r.value("success", false));
    std::cout << "\n";

    // === Test 7: Random call on empty class fails ===
    std::cout << "--- Test 7: Random call on empty class ---\n";
    sendMsg(teacherFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Empty Class"}});
    r = recvMsg(teacherFd);
    int emptyClassId = r.value("classId", 0);
    sendMsg(teacherFd, {{"type", "START_CLASS_REQ"}, {"classId", emptyClassId}});
    recvMsg(teacherFd);
    sendMsg(teacherFd, {{"type", "RANDOM_CALL_REQ"}, {"classId", emptyClassId}});
    r = recvMsg(teacherFd);
    check("Random call on empty class fails", !r.value("success", false));
    std::cout << "\n";

    // === Test 8: Non-owner teacher cannot random call ===
    std::cout << "--- Test 8: Non-owner teacher cannot random call ---\n";
    adminFd = loginAs(host, port, "admin", "admin123");
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "t_other_rand"}, {"password", "pass"},
                       {"name", "Other Teacher"}, {"role", "TEACHER"}});
    recvMsg(adminFd);
    sockClose(adminFd);

    int otherFd = loginAs(host, port, "t_other_rand", "pass");
    if (otherFd >= 0) {
        // Try on the empty class (still ACTIVE, owned by t_rand)
        sendMsg(otherFd, {{"type", "RANDOM_CALL_REQ"}, {"classId", emptyClassId}});
        r = recvMsg(otherFd);
        check("Non-owner teacher cannot random call", !r.value("success", false));
        sockClose(otherFd);
    }
    std::cout << "\n";

    // Cleanup
    sockClose(teacherFd);
    sockClose(s1Fd);
    sockClose(s2Fd);
    sockClose(s3Fd);

    std::cout << "=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
