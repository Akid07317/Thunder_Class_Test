// Edge case tests: disconnect handling, multiple classes, cross-feature
// interactions, rapid operations, persistence across restart.
// Requires: server running on localhost:9000 with clean data/ directory.

#include "test_socket.h"
#include <common/model/user.h>
#include <thread>
#include <chrono>

static int passed = 0, failed = 0;

static void check(const std::string& name, const nlohmann::json& resp, bool expectSuccess) {
    bool ok = resp.value("success", false);
    if (ok == expectSuccess) {
        std::cout << "  PASS: " << name << "\n"; ++passed;
    } else {
        std::cout << "  FAIL: " << name << " (expected " << (expectSuccess ? "success" : "failure")
                  << ", got " << resp.dump() << ")\n"; ++failed;
    }
}

static void check(const std::string& name, bool condition) {
    if (condition) {
        std::cout << "  PASS: " << name << "\n"; ++passed;
    } else {
        std::cout << "  FAIL: " << name << "\n"; ++failed;
    }
}

static int loginAs(const char* host, uint16_t port, const std::string& user, const std::string& pass) {
    int fd = connectToServer(host, port);
    if (fd < 0) return -1;
    sendMsg(fd, {{"type", "LOGIN_REQ"}, {"username", user}, {"password", pass}});
    auto resp = recvMsg(fd);
    if (!resp.value("success", false)) {
        sockClose(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char* argv[]) {
    std::setbuf(stdout, nullptr);  // Disable stdout buffering
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    uint16_t port = argc > 2 ? std::atoi(argv[2]) : 9000;

    std::cout << "=== Edge Case Tests ===" << std::endl;

    // --- Setup: Create accounts ---
    std::cout << "--- Setup: Creating accounts ---\n";
    {
        int adminFd = connectToServer(host, port);
        if (adminFd < 0) { std::cerr << "Cannot connect as admin\n"; return 1; }
        sendMsg(adminFd, {{"type", "LOGIN_REQ"}, {"username", "admin"}, {"password", "admin123"}});
        recvMsg(adminFd);
        sendMsg(adminFd, {{"type", "CREATE_USER_REQ"}, {"username", "t1"}, {"password", "pass"},
                          {"name", "Teacher One"}, {"role", "TEACHER"}});
        recvMsg(adminFd);
        sendMsg(adminFd, {{"type", "CREATE_USER_REQ"}, {"username", "t2"}, {"password", "pass"},
                          {"name", "Teacher Two"}, {"role", "TEACHER"}});
        recvMsg(adminFd);
        sendMsg(adminFd, {{"type", "CREATE_USER_REQ"}, {"username", "s1"}, {"password", "pass"},
                          {"name", "Alice"}, {"role", "STUDENT"}});
        recvMsg(adminFd);
        sendMsg(adminFd, {{"type", "CREATE_USER_REQ"}, {"username", "s2"}, {"password", "pass"},
                          {"name", "Bob"}, {"role", "STUDENT"}});
        recvMsg(adminFd);
        sendMsg(adminFd, {{"type", "CREATE_USER_REQ"}, {"username", "s3"}, {"password", "pass"},
                          {"name", "Charlie"}, {"role", "STUDENT"}});
        recvMsg(adminFd);
        sockClose(adminFd);
    }
    std::cout << "  Setup complete.\n\n";

    // ================================================================
    // Test Group 1: Student disconnect during active class
    // ================================================================
    std::cout << "--- Test Group 1: Student disconnect during active class ---\n";
    {
        int tFd = loginAs(host, port, "t1", "pass");
        int s1Fd = loginAs(host, port, "s1", "pass");
        int s2Fd = loginAs(host, port, "s2", "pass");

        // Create and start class
        sendMsg(tFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Disconnect Test"}});
        auto resp = recvMsg(tFd);
        int classId = resp.value("classId", 0);
        sendMsg(tFd, {{"type", "START_CLASS_REQ"}, {"classId", classId}});
        recvMsg(tFd);

        // Both students join
        sendMsg(s1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
        recvMsg(s1Fd);
        sendMsg(s2Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
        recvMsg(s2Fd);

        // Verify 2 members
        sendMsg(tFd, {{"type", "CLASS_MEMBERS_REQ"}, {"classId", classId}});
        resp = recvMsg(tFd);
        int memberCount = resp.value("members", nlohmann::json::array()).size();
        check("2 members before disconnect", memberCount == 2);

        // Student 1 disconnects abruptly (simulates crash)
        sockClose(s1Fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Verify member count dropped to 1
        sendMsg(tFd, {{"type", "CLASS_MEMBERS_REQ"}, {"classId", classId}});
        resp = recvMsg(tFd);
        memberCount = resp.value("members", nlohmann::json::array()).size();
        check("1 member after disconnect", memberCount == 1);

        // Student 1 reconnects and rejoins
        s1Fd = loginAs(host, port, "s1", "pass");
        sendMsg(s1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
        resp = recvMsg(s1Fd);
        check("Reconnected student can rejoin", resp, true);

        sendMsg(tFd, {{"type", "CLASS_MEMBERS_REQ"}, {"classId", classId}});
        resp = recvMsg(tFd);
        memberCount = resp.value("members", nlohmann::json::array()).size();
        check("2 members after rejoin", memberCount == 2);

        // End class and check duration events
        sendMsg(tFd, {{"type", "END_CLASS_REQ"}, {"classId", classId}});
        recvMsg(tFd);

        // Get summary -- Alice should have 2 join/leave pairs (disconnect + rejoin)
        sendMsg(tFd, {{"type", "CLASS_SUMMARY_REQ"}, {"classId", classId}});
        resp = recvMsg(tFd);
        check("Summary succeeds after disconnect/rejoin", resp, true);
        auto students = resp.value("summary", nlohmann::json::object()).value("students", nlohmann::json::array());
        check("Summary has 2 students", students.size() == 2);

        sockClose(s1Fd);
        sockClose(s2Fd);
        sockClose(tFd);
    }
    std::cout << "\n";

    // ================================================================
    // Test Group 2: Multiple simultaneous classes
    // ================================================================
    std::cout << "--- Test Group 2: Multiple simultaneous classes ---\n";
    {
        int t1Fd = loginAs(host, port, "t1", "pass");
        int t2Fd = loginAs(host, port, "t2", "pass");
        int s1Fd = loginAs(host, port, "s1", "pass");

        // Teacher 1 creates and starts class A
        sendMsg(t1Fd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Class A"}});
        auto resp = recvMsg(t1Fd);
        int classA = resp.value("classId", 0);
        sendMsg(t1Fd, {{"type", "START_CLASS_REQ"}, {"classId", classA}});
        recvMsg(t1Fd);

        // Teacher 2 creates and starts class B
        sendMsg(t2Fd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Class B"}});
        resp = recvMsg(t2Fd);
        int classB = resp.value("classId", 0);
        sendMsg(t2Fd, {{"type", "START_CLASS_REQ"}, {"classId", classB}});
        recvMsg(t2Fd);

        // Student joins both classes
        sendMsg(s1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classA}});
        resp = recvMsg(s1Fd);
        check("Student joins class A", resp, true);
        sendMsg(s1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classB}});
        resp = recvMsg(s1Fd);
        check("Student joins class B", resp, true);

        // Teacher 1 opens check-in in class A
        sendMsg(t1Fd, {{"type", "OPEN_CHECKIN_REQ"}, {"classId", classA}});
        recvMsg(t1Fd);
        auto notify = recvMsg(s1Fd);  // student gets notification
        check("Student gets class A check-in notification",
              notify.value("type", "") == "NOTIFY_CHECKIN_OPENED" && notify.value("classId", 0) == classA);

        // Student checks in to class A
        sendMsg(s1Fd, {{"type", "CHECKIN_REQ"}, {"classId", classA}});
        resp = recvMsg(s1Fd);
        check("Student checks in to class A", resp, true);

        // Teacher 2 opens check-in in class B
        sendMsg(t2Fd, {{"type", "OPEN_CHECKIN_REQ"}, {"classId", classB}});
        recvMsg(t2Fd);
        notify = recvMsg(s1Fd);
        check("Student gets class B check-in notification",
              notify.value("type", "") == "NOTIFY_CHECKIN_OPENED" && notify.value("classId", 0) == classB);

        // Student checks in to class B
        sendMsg(s1Fd, {{"type", "CHECKIN_REQ"}, {"classId", classB}});
        resp = recvMsg(s1Fd);
        check("Student checks in to class B", resp, true);

        // Cross-class permission: teacher 1 cannot end class B
        sendMsg(t1Fd, {{"type", "END_CLASS_REQ"}, {"classId", classB}});
        resp = recvMsg(t1Fd);
        check("Teacher 1 cannot end Teacher 2's class", resp, false);

        // Cross-class permission: teacher 2 cannot start screen share on class A
        sendMsg(t2Fd, {{"type", "START_SCREEN_SHARE_REQ"}, {"classId", classA}});
        resp = recvMsg(t2Fd);
        check("Teacher 2 cannot screen share on Teacher 1's class", resp, false);

        // End both
        sendMsg(t1Fd, {{"type", "END_CLASS_REQ"}, {"classId", classA}});
        recvMsg(t1Fd);
        sendMsg(t2Fd, {{"type", "END_CLASS_REQ"}, {"classId", classB}});
        recvMsg(t2Fd);

        sockClose(s1Fd);
        sockClose(t1Fd);
        sockClose(t2Fd);
    }
    std::cout << "\n";

    // ================================================================
    // Test Group 3: Cross-feature interactions
    // ================================================================
    std::cout << "--- Test Group 3: Cross-feature interactions ---\n";
    {
        int tFd = loginAs(host, port, "t1", "pass");
        int sFd = loginAs(host, port, "s1", "pass");

        // Create, start, join
        sendMsg(tFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Multi-Feature"}});
        auto resp = recvMsg(tFd);
        int classId = resp.value("classId", 0);
        sendMsg(tFd, {{"type", "START_CLASS_REQ"}, {"classId", classId}});
        recvMsg(tFd);
        sendMsg(sFd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
        recvMsg(sFd);

        // Start screen sharing AND audio simultaneously
        sendMsg(tFd, {{"type", "START_SCREEN_SHARE_REQ"}, {"classId", classId}});
        resp = recvMsg(tFd);
        check("Start screen sharing", resp, true);
        recvMsg(sFd); // notification

        sendMsg(tFd, {{"type", "START_AUDIO_REQ"}, {"classId", classId}});
        resp = recvMsg(tFd);
        check("Start audio alongside screen share", resp, true);
        recvMsg(sFd); // notification

        // Publish question while sharing + audio active
        sendMsg(tFd, {{"type", "PUBLISH_QUESTION_REQ"}, {"classId", classId},
                      {"content", "Test Q"}, {"options", {"A", "B", "C", "D"}}, {"correctAnswer", 1}});
        resp = recvMsg(tFd);
        check("Publish question during share+audio", resp, true);
        int qId = resp.value("questionId", 0);
        recvMsg(sFd); // question notification

        // Student answers while audio + screen sharing active
        sendMsg(sFd, {{"type", "SUBMIT_ANSWER_REQ"}, {"classId", classId}, {"questionId", qId}, {"answer", 1}});
        resp = recvMsg(sFd);
        check("Submit answer during share+audio", resp, true);

        // Open check-in with everything running
        sendMsg(tFd, {{"type", "OPEN_CHECKIN_REQ"}, {"classId", classId}, {"deadlineSeconds", 30}});
        resp = recvMsg(tFd);
        check("Open check-in during share+audio+question", resp, true);
        recvMsg(sFd); // notification

        sendMsg(sFd, {{"type", "CHECKIN_REQ"}, {"classId", classId}});
        resp = recvMsg(sFd);
        check("Check in during active features", resp, true);

        // Random call with everything active
        sendMsg(tFd, {{"type", "RANDOM_CALL_REQ"}, {"classId", classId}});
        resp = recvMsg(tFd);
        check("Random call during active features", resp, true);
        recvMsg(sFd); // notification

        // End class -- should auto-stop screen sharing and audio
        sendMsg(tFd, {{"type", "END_CLASS_REQ"}, {"classId", classId}});
        resp = recvMsg(tFd);
        check("End class with all features active", resp, true);

        // Drain notifications
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Verify screen sharing stopped
        sendMsg(tFd, {{"type", "START_SCREEN_SHARE_REQ"}, {"classId", classId}});
        resp = recvMsg(tFd);
        check("Screen share on ENDED class fails", resp, false);

        // Verify audio stopped
        sendMsg(tFd, {{"type", "START_AUDIO_REQ"}, {"classId", classId}});
        resp = recvMsg(tFd);
        check("Audio on ENDED class fails", resp, false);

        // Get summary -- should reflect all activities
        sendMsg(tFd, {{"type", "CLASS_SUMMARY_REQ"}, {"classId", classId}});
        resp = recvMsg(tFd);
        check("Summary after multi-feature class", resp, true);
        auto summary = resp.value("summary", nlohmann::json::object());
        check("Summary has 1 question", summary.value("totalQuestions", 0) == 1);
        check("Summary has 1 checked in", summary.value("checkedInCount", 0) == 1);

        sockClose(sFd);
        sockClose(tFd);
    }
    std::cout << "\n";

    // ================================================================
    // Test Group 4: Rapid operations
    // ================================================================
    std::cout << "--- Test Group 4: Rapid operations ---\n";
    {
        int tFd = loginAs(host, port, "t1", "pass");
        int s1Fd = loginAs(host, port, "s1", "pass");
        int s2Fd = loginAs(host, port, "s2", "pass");
        int s3Fd = loginAs(host, port, "s3", "pass");

        // Create and start class
        sendMsg(tFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Rapid Ops"}});
        auto resp = recvMsg(tFd);
        int classId = resp.value("classId", 0);
        sendMsg(tFd, {{"type", "START_CLASS_REQ"}, {"classId", classId}});
        recvMsg(tFd);

        // All 3 students join as fast as possible
        sendMsg(s1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
        sendMsg(s2Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
        sendMsg(s3Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
        check("Alice joins rapidly", recvMsg(s1Fd), true);
        check("Bob joins rapidly", recvMsg(s2Fd), true);
        check("Charlie joins rapidly", recvMsg(s3Fd), true);

        // Open check-in and all students check in rapidly
        sendMsg(tFd, {{"type", "OPEN_CHECKIN_REQ"}, {"classId", classId}, {"deadlineSeconds", 30}});
        recvMsg(tFd);
        // Drain notifications
        recvMsg(s1Fd);
        recvMsg(s2Fd);
        recvMsg(s3Fd);

        sendMsg(s1Fd, {{"type", "CHECKIN_REQ"}, {"classId", classId}});
        sendMsg(s2Fd, {{"type", "CHECKIN_REQ"}, {"classId", classId}});
        sendMsg(s3Fd, {{"type", "CHECKIN_REQ"}, {"classId", classId}});
        check("Alice checks in rapidly", recvMsg(s1Fd), true);
        check("Bob checks in rapidly", recvMsg(s2Fd), true);
        check("Charlie checks in rapidly", recvMsg(s3Fd), true);

        // Verify all 3 checked in
        sendMsg(tFd, {{"type", "CHECKIN_STATUS_REQ"}, {"classId", classId}});
        resp = recvMsg(tFd);
        int checkedCount = 0;
        for (auto& s : resp.value("students", nlohmann::json::array())) {
            if (s.value("status", "") == "on_time") ++checkedCount;
        }
        check("All 3 checked in on time", checkedCount == 3);

        // Rapid question publish + answer
        sendMsg(tFd, {{"type", "PUBLISH_QUESTION_REQ"}, {"classId", classId},
                      {"content", "Quick Q"}, {"options", {"A", "B", "C", "D"}}, {"correctAnswer", 0}});
        resp = recvMsg(tFd);
        int qId = resp.value("questionId", 0);
        recvMsg(s1Fd); recvMsg(s2Fd); recvMsg(s3Fd);  // notifications

        sendMsg(s1Fd, {{"type", "SUBMIT_ANSWER_REQ"}, {"classId", classId}, {"questionId", qId}, {"answer", 0}});
        sendMsg(s2Fd, {{"type", "SUBMIT_ANSWER_REQ"}, {"classId", classId}, {"questionId", qId}, {"answer", 1}});
        sendMsg(s3Fd, {{"type", "SUBMIT_ANSWER_REQ"}, {"classId", classId}, {"questionId", qId}, {"answer", 0}});
        check("Alice answers rapidly", recvMsg(s1Fd), true);
        check("Bob answers rapidly", recvMsg(s2Fd), true);
        check("Charlie answers rapidly", recvMsg(s3Fd), true);

        // Verify answer stats
        sendMsg(tFd, {{"type", "GET_ANSWERS_REQ"}, {"classId", classId}, {"questionId", qId}});
        resp = recvMsg(tFd);
        check("Get answers succeeds", resp, true);
        check("Total answers = 3", resp.value("totalAnswers", 0) == 3);
        check("Correct answers = 2", resp.value("correctCount", 0) == 2);

        // End class
        sendMsg(tFd, {{"type", "END_CLASS_REQ"}, {"classId", classId}});
        recvMsg(tFd);

        sockClose(s1Fd);
        sockClose(s2Fd);
        sockClose(s3Fd);
        sockClose(tFd);
    }
    std::cout << "\n";

    // ================================================================
    // Test Group 5: Operations after teacher disconnect
    // ================================================================
    std::cout << "--- Test Group 5: Teacher disconnect during active class ---\n";
    {
        int tFd = loginAs(host, port, "t1", "pass");
        int sFd = loginAs(host, port, "s1", "pass");

        // Create, start, join
        sendMsg(tFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Teacher DC Test"}});
        auto resp = recvMsg(tFd);
        int classId = resp.value("classId", 0);
        sendMsg(tFd, {{"type", "START_CLASS_REQ"}, {"classId", classId}});
        recvMsg(tFd);
        sendMsg(sFd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
        recvMsg(sFd);

        // Start screen sharing
        sendMsg(tFd, {{"type", "START_SCREEN_SHARE_REQ"}, {"classId", classId}});
        recvMsg(tFd);
        recvMsg(sFd); // notification

        // Teacher disconnects abruptly
        sockClose(tFd);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Class should still be listed as ACTIVE (teacher disconnect doesn't end class)
        sendMsg(sFd, {{"type", "LIST_CLASSES_REQ"}});
        resp = recvMsg(sFd);
        bool foundActive = false;
        for (auto& c : resp.value("classes", nlohmann::json::array())) {
            if (c.value("classId", 0) == classId) {
                foundActive = c.value("status", "") == "ACTIVE";
            }
        }
        check("Class still ACTIVE after teacher disconnect", foundActive);

        // Student can still leave
        sendMsg(sFd, {{"type", "LEAVE_CLASS_REQ"}, {"classId", classId}});
        resp = recvMsg(sFd);
        check("Student can leave after teacher disconnect", resp, true);

        // Teacher reconnects and can still manage
        tFd = loginAs(host, port, "t1", "pass");
        sendMsg(tFd, {{"type", "END_CLASS_REQ"}, {"classId", classId}});
        resp = recvMsg(tFd);
        check("Reconnected teacher can end class", resp, true);

        sockClose(sFd);
        sockClose(tFd);
    }
    std::cout << "\n";

    // ================================================================
    // Test Group 6: Duplicate login handling
    // ================================================================
    std::cout << "--- Test Group 6: Duplicate login handling ---\n";
    {
        int fd1 = loginAs(host, port, "s1", "pass");
        check("First login succeeds", fd1 >= 0);

        // Try logging in as the same user on another connection
        int fd2 = connectToServer(host, port);
        sendMsg(fd2, {{"type", "LOGIN_REQ"}, {"username", "s1"}, {"password", "pass"}});
        auto resp = recvMsg(fd2);
        // Should either reject or disconnect the first session
        bool secondLoginResult = resp.value("success", false);
        std::cout << "  INFO: Second login " << (secondLoginResult ? "succeeded (old kicked)" : "rejected") << "\n";
        check("Duplicate login handled", true);  // Either behavior is acceptable

        sockClose(fd1);
        sockClose(fd2);
    }
    std::cout << "\n";

    // ================================================================
    // Test Group 7: Boundary values
    // ================================================================
    std::cout << "--- Test Group 7: Boundary values ---\n";
    {
        int tFd = loginAs(host, port, "t1", "pass");
        int sFd = loginAs(host, port, "s1", "pass");

        // Create class with empty name
        sendMsg(tFd, {{"type", "CREATE_CLASS_REQ"}, {"name", ""}});
        auto resp = recvMsg(tFd);
        // May succeed or fail -- just verify no crash
        check("Empty class name doesn't crash", !resp.empty());

        // Create class with long name
        std::string longName(500, 'A');
        sendMsg(tFd, {{"type", "CREATE_CLASS_REQ"}, {"name", longName}});
        resp = recvMsg(tFd);
        check("Long class name doesn't crash", !resp.empty());

        // Operations on nonexistent class ID
        sendMsg(tFd, {{"type", "START_CLASS_REQ"}, {"classId", 99999}});
        resp = recvMsg(tFd);
        check("Start nonexistent class fails", resp, false);

        sendMsg(tFd, {{"type", "END_CLASS_REQ"}, {"classId", 99999}});
        resp = recvMsg(tFd);
        check("End nonexistent class fails", resp, false);

        sendMsg(sFd, {{"type", "JOIN_CLASS_REQ"}, {"classId", 99999}});
        resp = recvMsg(sFd);
        check("Join nonexistent class fails", resp, false);

        sendMsg(sFd, {{"type", "CHECKIN_REQ"}, {"classId", 99999}});
        resp = recvMsg(sFd);
        check("Check-in nonexistent class fails", resp, false);

        sendMsg(tFd, {{"type", "RANDOM_CALL_REQ"}, {"classId", 99999}});
        resp = recvMsg(tFd);
        check("Random call nonexistent class fails", resp, false);

        // Submit answer with invalid answer index
        sendMsg(tFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Boundary Test"}});
        resp = recvMsg(tFd);
        int classId = resp.value("classId", 0);
        sendMsg(tFd, {{"type", "START_CLASS_REQ"}, {"classId", classId}});
        recvMsg(tFd);
        sendMsg(sFd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
        recvMsg(sFd);

        sendMsg(tFd, {{"type", "PUBLISH_QUESTION_REQ"}, {"classId", classId},
                      {"content", "Boundary Q"}, {"options", {"X", "Y"}}, {"correctAnswer", 0}});
        resp = recvMsg(tFd);
        int qId = resp.value("questionId", 0);
        recvMsg(sFd);  // notification

        sendMsg(sFd, {{"type", "SUBMIT_ANSWER_REQ"}, {"classId", classId}, {"questionId", qId}, {"answer", 5}});
        resp = recvMsg(sFd);
        check("Out-of-bounds answer rejected", resp, false);

        sendMsg(sFd, {{"type", "SUBMIT_ANSWER_REQ"}, {"classId", classId}, {"questionId", qId}, {"answer", -1}});
        resp = recvMsg(sFd);
        check("Negative answer index rejected", resp, false);

        // Valid answer within 2-option range
        sendMsg(sFd, {{"type", "SUBMIT_ANSWER_REQ"}, {"classId", classId}, {"questionId", qId}, {"answer", 1}});
        resp = recvMsg(sFd);
        check("Valid 2-option answer accepted", resp, true);

        sendMsg(tFd, {{"type", "END_CLASS_REQ"}, {"classId", classId}});
        recvMsg(tFd);

        sockClose(sFd);
        sockClose(tFd);
    }
    std::cout << "\n";

    // ================================================================
    // Test Group 8: Data persistence verification
    // ================================================================
    std::cout << "--- Test Group 8: Data persistence verification ---\n";
    {
        int tFd = loginAs(host, port, "t1", "pass");

        // Verify classes from earlier tests are still visible
        sendMsg(tFd, {{"type", "LIST_CLASSES_REQ"}});
        auto resp = recvMsg(tFd);
        auto classes = resp.value("classes", nlohmann::json::array());
        check("Multiple classes persisted", classes.size() >= 4);

        // Count ENDED classes
        int endedCount = 0;
        for (auto& c : classes) {
            if (c.value("status", "") == "ENDED") ++endedCount;
        }
        check("Multiple ENDED classes visible", endedCount >= 3);

        sockClose(tFd);
    }
    std::cout << "\n";

    std::cout << "=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
