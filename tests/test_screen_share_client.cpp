// Console client for verifying simplified screen sharing.
// Requires: server running on localhost:9000 with clean data/.
// Workflow: create accounts -> create/start class -> students join ->
//           teacher starts sharing -> sends frames -> students receive ->
//           teacher stops sharing -> students notified.

#include <common/protocol/protocol.h>
#include <common/protocol/message_type.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <string>

static int connectToServer(const char* host, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect"); close(fd); return -1;
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

static nlohmann::json tryRecvMsg(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    std::vector<uint8_t> buffer;
    uint8_t buf[4096];
    nlohmann::json result;

    for (int attempt = 0; attempt < 10; ++attempt) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            buffer.insert(buffer.end(), buf, buf + n);
            if (Protocol::extractFrame(buffer, result)) {
                fcntl(fd, F_SETFL, flags);
                return result;
            }
        }
        usleep(10000);
    }

    fcntl(fd, F_SETFL, flags);
    return {};
}

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
        close(fd); return -1;
    }
    return fd;
}

// Simulated base64 frame data (small fake JPEG-like payload for testing)
static std::string makeFakeFrame(int seq) {
    // In a real client this would be a base64-encoded JPEG screenshot.
    // For testing, we use a short distinctive string.
    return "FAKE_JPEG_FRAME_" + std::to_string(seq) + "_DATA_AABBCCDD";
}

int main(int argc, char* argv[]) {
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    uint16_t port = argc > 2 ? std::atoi(argv[2]) : 9000;

    std::cout << "=== Screen Sharing Tests ===\n\n";

    // --- Setup ---
    std::cout << "--- Setup ---\n";
    int adminFd = loginAs(host, port, "admin", "admin123");
    if (adminFd < 0) return 1;

    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "t_screen"}, {"password", "pass"},
                       {"name", "Teacher Screen"}, {"role", "TEACHER"}});
    recvMsg(adminFd);
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_screen1"}, {"password", "pass"},
                       {"name", "Alice"}, {"role", "STUDENT"}});
    recvMsg(adminFd);
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_screen2"}, {"password", "pass"},
                       {"name", "Bob"}, {"role", "STUDENT"}});
    recvMsg(adminFd);
    close(adminFd);

    int teacherFd = loginAs(host, port, "t_screen", "pass");
    int s1Fd = loginAs(host, port, "s_screen1", "pass");
    int s2Fd = loginAs(host, port, "s_screen2", "pass");
    if (teacherFd < 0 || s1Fd < 0 || s2Fd < 0) return 1;

    // Create, start, students join
    sendMsg(teacherFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Screen Test"}});
    auto r = recvMsg(teacherFd);
    int classId = r.value("classId", 0);
    sendMsg(teacherFd, {{"type", "START_CLASS_REQ"}, {"classId", classId}});
    recvMsg(teacherFd);

    sendMsg(s1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    recvMsg(s1Fd);
    sendMsg(s2Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    recvMsg(s2Fd);

    std::cout << "  Setup complete: class=" << classId << ", 2 students joined.\n\n";

    // === Test 1: Student cannot start sharing ===
    std::cout << "--- Test 1: Student cannot start sharing ---\n";
    sendMsg(s1Fd, {{"type", "START_SCREEN_SHARE_REQ"}, {"classId", classId}});
    r = recvMsg(s1Fd);
    check("Student cannot start screen share", !r.value("success", false));
    std::cout << "\n";

    // === Test 2: Teacher starts sharing ===
    std::cout << "--- Test 2: Teacher starts sharing ---\n";
    sendMsg(teacherFd, {{"type", "START_SCREEN_SHARE_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Start sharing succeeds", r.value("success", false));
    check("Response has classId", r.value("classId", 0) == classId);

    // Students should receive notification
    auto n1 = tryRecvMsg(s1Fd);
    auto n2 = tryRecvMsg(s2Fd);
    check("Alice received share started notification",
          n1.value("type", "") == "NOTIFY_SCREEN_SHARE_STARTED");
    check("Bob received share started notification",
          n2.value("type", "") == "NOTIFY_SCREEN_SHARE_STARTED");
    std::cout << "\n";

    // === Test 3: Duplicate start fails ===
    std::cout << "--- Test 3: Duplicate start sharing ---\n";
    sendMsg(teacherFd, {{"type", "START_SCREEN_SHARE_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Duplicate start sharing fails", !r.value("success", false));
    std::cout << "\n";

    // === Test 4: Teacher sends frames, students receive them ===
    std::cout << "--- Test 4: Frame transfer ---\n";
    std::string frame1 = makeFakeFrame(1);
    sendMsg(teacherFd, {{"type", "SCREEN_FRAME"},
                         {"classId", classId},
                         {"frameData", frame1},
                         {"frameSeq", 1}});
    usleep(50000); // 50ms for delivery

    auto f1 = tryRecvMsg(s1Fd);
    auto f2 = tryRecvMsg(s2Fd);
    check("Alice received frame", f1.value("type", "") == "NOTIFY_SCREEN_FRAME");
    check("Alice frame data correct", f1.value("frameData", "") == frame1);
    check("Alice frame seq correct", f1.value("frameSeq", 0) == 1);
    check("Bob received frame", f2.value("type", "") == "NOTIFY_SCREEN_FRAME");
    check("Bob frame data correct", f2.value("frameData", "") == frame1);

    // Send another frame
    std::string frame2 = makeFakeFrame(2);
    sendMsg(teacherFd, {{"type", "SCREEN_FRAME"},
                         {"classId", classId},
                         {"frameData", frame2},
                         {"frameSeq", 2}});
    usleep(50000);

    f1 = tryRecvMsg(s1Fd);
    check("Alice received second frame", f1.value("frameData", "") == frame2);
    check("Second frame seq correct", f1.value("frameSeq", 0) == 2);
    // Drain Bob's
    tryRecvMsg(s2Fd);
    std::cout << "\n";

    // === Test 5: Student cannot send frames ===
    std::cout << "--- Test 5: Student cannot send frames ---\n";
    sendMsg(s1Fd, {{"type", "SCREEN_FRAME"},
                    {"classId", classId},
                    {"frameData", "STUDENT_ATTACK"},
                    {"frameSeq", 99}});
    usleep(50000);
    // Bob should NOT receive anything from student
    auto spurious = tryRecvMsg(s2Fd);
    check("Student frame not broadcast", spurious.empty() || spurious.value("type", "") != "NOTIFY_SCREEN_FRAME");
    std::cout << "\n";

    // === Test 6: Teacher stops sharing ===
    std::cout << "--- Test 6: Teacher stops sharing ---\n";
    sendMsg(teacherFd, {{"type", "STOP_SCREEN_SHARE_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Stop sharing succeeds", r.value("success", false));

    n1 = tryRecvMsg(s1Fd);
    n2 = tryRecvMsg(s2Fd);
    check("Alice received share stopped notification",
          n1.value("type", "") == "NOTIFY_SCREEN_SHARE_STOPPED");
    check("Bob received share stopped notification",
          n2.value("type", "") == "NOTIFY_SCREEN_SHARE_STOPPED");
    std::cout << "\n";

    // === Test 7: Frames not routed after stop ===
    std::cout << "--- Test 7: No frames after stop ---\n";
    sendMsg(teacherFd, {{"type", "SCREEN_FRAME"},
                         {"classId", classId},
                         {"frameData", "SHOULD_NOT_ARRIVE"},
                         {"frameSeq", 99}});
    usleep(50000);
    auto late = tryRecvMsg(s1Fd);
    check("No frame after sharing stopped", late.empty());
    std::cout << "\n";

    // === Test 8: Duplicate stop fails ===
    std::cout << "--- Test 8: Duplicate stop fails ---\n";
    sendMsg(teacherFd, {{"type", "STOP_SCREEN_SHARE_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Duplicate stop fails", !r.value("success", false));
    std::cout << "\n";

    // === Test 9: Sharing on non-ACTIVE class fails ===
    std::cout << "--- Test 9: Sharing on ended class ---\n";
    sendMsg(teacherFd, {{"type", "END_CLASS_REQ"}, {"classId", classId}});
    recvMsg(teacherFd);
    sendMsg(teacherFd, {{"type", "START_SCREEN_SHARE_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Cannot share on ENDED class", !r.value("success", false));
    std::cout << "\n";

    // === Test 10: Sharing auto-stops when class ends ===
    std::cout << "--- Test 10: Sharing auto-stops on class end ---\n";
    // Create new class, start, join, share, then end
    sendMsg(teacherFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Auto Stop Test"}});
    r = recvMsg(teacherFd);
    int classId2 = r.value("classId", 0);
    sendMsg(teacherFd, {{"type", "START_CLASS_REQ"}, {"classId", classId2}});
    recvMsg(teacherFd);

    // Reconnect students for the new class
    close(s1Fd);
    s1Fd = loginAs(host, port, "s_screen1", "pass");
    sendMsg(s1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId2}});
    recvMsg(s1Fd);

    sendMsg(teacherFd, {{"type", "START_SCREEN_SHARE_REQ"}, {"classId", classId2}});
    r = recvMsg(teacherFd);
    check("Sharing started on new class", r.value("success", false));
    tryRecvMsg(s1Fd); // drain notification

    // End the class (should auto-stop sharing)
    sendMsg(teacherFd, {{"type", "END_CLASS_REQ"}, {"classId", classId2}});
    recvMsg(teacherFd);

    // Try to stop sharing — should fail because already auto-stopped
    sendMsg(teacherFd, {{"type", "STOP_SCREEN_SHARE_REQ"}, {"classId", classId2}});
    r = recvMsg(teacherFd);
    check("Sharing auto-stopped when class ended", !r.value("success", false));
    std::cout << "\n";

    // === Test 11: Non-owner teacher cannot start sharing ===
    std::cout << "--- Test 11: Non-owner teacher ---\n";
    adminFd = loginAs(host, port, "admin", "admin123");
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "t_other_screen"}, {"password", "pass"},
                       {"name", "Other Teacher"}, {"role", "TEACHER"}});
    recvMsg(adminFd);
    close(adminFd);

    // Create a new active class for the test
    sendMsg(teacherFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Ownership Test"}});
    r = recvMsg(teacherFd);
    int classId3 = r.value("classId", 0);
    sendMsg(teacherFd, {{"type", "START_CLASS_REQ"}, {"classId", classId3}});
    recvMsg(teacherFd);

    int otherFd = loginAs(host, port, "t_other_screen", "pass");
    if (otherFd >= 0) {
        sendMsg(otherFd, {{"type", "START_SCREEN_SHARE_REQ"}, {"classId", classId3}});
        r = recvMsg(otherFd);
        check("Non-owner teacher cannot start sharing", !r.value("success", false));
        close(otherFd);
    }
    std::cout << "\n";

    // Cleanup
    close(teacherFd);
    close(s1Fd);
    close(s2Fd);

    std::cout << "=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
