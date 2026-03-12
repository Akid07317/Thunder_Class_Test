// Console client for verifying statistics and export.
// Requires: server running on localhost:9000 with clean data/.
// Workflow: create accounts -> create/start class -> students join -> check-in ->
//           publish question -> students answer -> end class -> view summary -> export CSV.

#include "test_socket.h"

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

    std::cout << "=== Statistics & Export Tests ===\n\n";

    // --- Setup: create accounts ---
    std::cout << "--- Setup ---\n";
    int adminFd = loginAs(host, port, "admin", "admin123");
    if (adminFd < 0) return 1;

    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "t_stats"}, {"password", "pass"},
                       {"name", "Teacher Stats"}, {"role", "TEACHER"}});
    recvMsg(adminFd);
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_stats1"}, {"password", "pass"},
                       {"name", "Alice"}, {"role", "STUDENT"}});
    recvMsg(adminFd);
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_stats2"}, {"password", "pass"},
                       {"name", "Bob"}, {"role", "STUDENT"}});
    recvMsg(adminFd);
    sockClose(adminFd);

    int teacherFd = loginAs(host, port, "t_stats", "pass");
    int s1Fd = loginAs(host, port, "s_stats1", "pass");
    int s2Fd = loginAs(host, port, "s_stats2", "pass");
    if (teacherFd < 0 || s1Fd < 0 || s2Fd < 0) return 1;

    // Create and start class
    sendMsg(teacherFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Stats Test"}});
    auto r = recvMsg(teacherFd);
    int classId = r.value("classId", 0);
    sendMsg(teacherFd, {{"type", "START_CLASS_REQ"}, {"classId", classId}});
    recvMsg(teacherFd);

    // Students join
    sendMsg(s1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    recvMsg(s1Fd);
    sendMsg(s2Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    recvMsg(s2Fd);

    // Teacher opens check-in, Alice checks in, Bob does not
    sendMsg(teacherFd, {{"type", "OPEN_CHECKIN_REQ"}, {"classId", classId}});
    recvMsg(teacherFd);
    recvMsg(s1Fd); // notification
    recvMsg(s2Fd); // notification

    sendMsg(s1Fd, {{"type", "CHECKIN_REQ"}, {"classId", classId}});
    recvMsg(s1Fd);

    // Teacher publishes question, both students answer
    sendMsg(teacherFd, {{"type", "PUBLISH_QUESTION_REQ"}, {"classId", classId},
                         {"content", "1+1=?"}, {"options", {"1", "2", "3"}}, {"correctAnswer", 1}});
    r = recvMsg(teacherFd);
    int qid = r.value("questionId", 0);
    recvMsg(s1Fd); // notification
    recvMsg(s2Fd); // notification

    sendMsg(s1Fd, {{"type", "SUBMIT_ANSWER_REQ"}, {"questionId", qid}, {"answer", 1}});
    recvMsg(s1Fd);
    sendMsg(s2Fd, {{"type", "SUBMIT_ANSWER_REQ"}, {"questionId", qid}, {"answer", 0}});
    recvMsg(s2Fd);

    // Bob leaves class before it ends
    sendMsg(s2Fd, {{"type", "LEAVE_CLASS_REQ"}, {"classId", classId}});
    recvMsg(s2Fd);

    // End class
    sendMsg(teacherFd, {{"type", "END_CLASS_REQ"}, {"classId", classId}});
    recvMsg(teacherFd);

    std::cout << "  Setup complete.\n\n";

    // === Test 1: Teacher gets class summary ===
    std::cout << "--- Test 1: Teacher gets class summary ---\n";
    sendMsg(teacherFd, {{"type", "CLASS_SUMMARY_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Summary request succeeds", r.value("success", false));

    if (r.contains("summary")) {
        auto& s = r["summary"];
        check("Class name correct", s.value("className", "") == "Stats Test");
        check("Total students = 2", s.value("totalStudents", 0) == 2);
        check("Checked in = 1", s.value("checkedInCount", 0) == 1);
        check("Total questions = 1", s.value("totalQuestions", 0) == 1);
        check("Start time recorded", s.value("startTime", 0) > 0);
        check("End time recorded", s.value("endTime", 0) > 0);

        if (s.contains("students") && s["students"].is_array()) {
            auto& students = s["students"];
            check("2 student records", students.size() == 2);

            // Find Alice and Bob
            bool aliceFound = false, bobFound = false;
            for (auto& st : students) {
                std::string name = st.value("name", "");
                if (name == "Alice") {
                    aliceFound = true;
                    check("Alice checked in on time", st.value("checkinStatus", "") == "on_time");
                    check("Alice duration >= 0", st.value("durationSeconds", -1) >= 0);
                    check("Alice answered 1 question", st.value("questionsAnswered", 0) == 1);
                    check("Alice 1 correct answer", st.value("correctAnswers", 0) == 1);
                }
                if (name == "Bob") {
                    bobFound = true;
                    check("Bob absent", st.value("checkinStatus", "") == "absent");
                    check("Bob duration >= 0", st.value("durationSeconds", -1) >= 0);
                    check("Bob answered 1 question", st.value("questionsAnswered", 0) == 1);
                    check("Bob 0 correct answers", st.value("correctAnswers", 0) == 0);
                }
            }
            check("Alice found in summary", aliceFound);
            check("Bob found in summary", bobFound);
        }
    }
    std::cout << "\n";

    // === Test 2: Student cannot view summary ===
    std::cout << "--- Test 2: Student cannot view summary ---\n";
    sendMsg(s1Fd, {{"type", "CLASS_SUMMARY_REQ"}, {"classId", classId}});
    r = recvMsg(s1Fd);
    check("Student cannot view summary", !r.value("success", false));
    std::cout << "\n";

    // === Test 3: Teacher exports CSV ===
    std::cout << "--- Test 3: Teacher exports CSV ---\n";
    sendMsg(teacherFd, {{"type", "EXPORT_CLASS_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Export request succeeds", r.value("success", false));

    if (r.contains("csv")) {
        std::string csv = r["csv"];
        check("CSV contains class name", csv.find("Stats Test") != std::string::npos);
        check("CSV contains teacher name", csv.find("Teacher Stats") != std::string::npos);
        check("CSV contains Alice", csv.find("Alice") != std::string::npos);
        check("CSV contains Bob", csv.find("Bob") != std::string::npos);
        check("CSV contains header row", csv.find("Student Name,Check-in Status,Online Duration") != std::string::npos);
        check("CSV contains ENDED status", csv.find("ENDED") != std::string::npos);

        std::cout << "\n  --- CSV Output ---\n" << csv << "  --- End CSV ---\n";
    }
    std::cout << "\n";

    // === Test 4: Student cannot export ===
    std::cout << "--- Test 4: Student cannot export ---\n";
    sendMsg(s1Fd, {{"type", "EXPORT_CLASS_REQ"}, {"classId", classId}});
    r = recvMsg(s1Fd);
    check("Student cannot export", !r.value("success", false));
    std::cout << "\n";

    // === Test 5: Wrong teacher cannot view summary ===
    std::cout << "--- Test 5: Non-owner teacher check ---\n";
    // Create another teacher
    adminFd = loginAs(host, port, "admin", "admin123");
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "t_other"}, {"password", "pass"},
                       {"name", "Other Teacher"}, {"role", "TEACHER"}});
    recvMsg(adminFd);
    sockClose(adminFd);

    int otherFd = loginAs(host, port, "t_other", "pass");
    if (otherFd >= 0) {
        sendMsg(otherFd, {{"type", "CLASS_SUMMARY_REQ"}, {"classId", classId}});
        r = recvMsg(otherFd);
        check("Non-owner teacher cannot view summary", !r.value("success", false));
        sockClose(otherFd);
    }
    std::cout << "\n";

    // === Test 6: Summary for nonexistent class ===
    std::cout << "--- Test 6: Summary for nonexistent class ---\n";
    sendMsg(teacherFd, {{"type", "CLASS_SUMMARY_REQ"}, {"classId", 9999}});
    r = recvMsg(teacherFd);
    check("Nonexistent class returns failure", !r.value("success", false));
    std::cout << "\n";

    // Cleanup
    sockClose(teacherFd);
    sockClose(s1Fd);
    sockClose(s2Fd);

    std::cout << "=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
