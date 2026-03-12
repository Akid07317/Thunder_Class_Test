// Console client for verifying audio broadcast and student mic.
// Requires: server running on localhost:9000 with clean data/.
// Workflow: create accounts -> create/start class -> students join ->
//           teacher starts audio -> sends frames -> students receive ->
//           teacher grants mic -> student sends frames -> teacher receives ->
//           teacher revokes mic -> teacher stops audio.

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

static int loginAs(const char* host, uint16_t port,
                   const std::string& username, const std::string& password) {
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

static int getUserId(const char* host, uint16_t port,
                     const std::string& username, const std::string& password) {
    int fd = connectToServer(host, port);
    if (fd < 0) return -1;
    sendMsg(fd, {{"type", "LOGIN_REQ"}, {"username", username}, {"password", password}});
    auto resp = recvMsg(fd);
    int uid = resp.value("userId", -1);
    close(fd);
    return uid;
}

int main(int argc, char* argv[]) {
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    uint16_t port = argc > 2 ? std::atoi(argv[2]) : 9000;

    std::cout << "=== Audio Module Tests ===\n\n";

    // --- Setup ---
    std::cout << "--- Setup ---\n";
    int adminFd = loginAs(host, port, "admin", "admin123");
    if (adminFd < 0) return 1;

    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "t_audio"}, {"password", "pass"},
                       {"name", "Teacher Audio"}, {"role", "TEACHER"}});
    recvMsg(adminFd);
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_audio1"}, {"password", "pass"},
                       {"name", "Alice"}, {"role", "STUDENT"}});
    recvMsg(adminFd);
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "s_audio2"}, {"password", "pass"},
                       {"name", "Bob"}, {"role", "STUDENT"}});
    recvMsg(adminFd);
    close(adminFd);

    // Get Alice's userId for mic grant
    int aliceUid = getUserId(host, port, "s_audio1", "pass");

    int teacherFd = loginAs(host, port, "t_audio", "pass");
    int s1Fd = loginAs(host, port, "s_audio1", "pass");
    int s2Fd = loginAs(host, port, "s_audio2", "pass");
    if (teacherFd < 0 || s1Fd < 0 || s2Fd < 0) return 1;

    // Create, start, students join
    sendMsg(teacherFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Audio Test"}});
    auto r = recvMsg(teacherFd);
    int classId = r.value("classId", 0);
    sendMsg(teacherFd, {{"type", "START_CLASS_REQ"}, {"classId", classId}});
    recvMsg(teacherFd);

    sendMsg(s1Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    recvMsg(s1Fd);
    sendMsg(s2Fd, {{"type", "JOIN_CLASS_REQ"}, {"classId", classId}});
    recvMsg(s2Fd);

    std::cout << "  Setup complete: class=" << classId
              << ", Alice uid=" << aliceUid << "\n\n";

    // === Test 1: Student cannot start audio ===
    std::cout << "--- Test 1: Student cannot start audio ---\n";
    sendMsg(s1Fd, {{"type", "START_AUDIO_REQ"}, {"classId", classId}});
    r = recvMsg(s1Fd);
    check("Student cannot start audio", !r.value("success", false));
    std::cout << "\n";

    // === Test 2: Teacher starts audio ===
    std::cout << "--- Test 2: Teacher starts audio ---\n";
    sendMsg(teacherFd, {{"type", "START_AUDIO_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Start audio succeeds", r.value("success", false));

    auto n1 = tryRecvMsg(s1Fd);
    auto n2 = tryRecvMsg(s2Fd);
    check("Alice received audio started", n1.value("type", "") == "NOTIFY_AUDIO_STARTED");
    check("Bob received audio started", n2.value("type", "") == "NOTIFY_AUDIO_STARTED");
    std::cout << "\n";

    // === Test 3: Duplicate start fails ===
    std::cout << "--- Test 3: Duplicate start ---\n";
    sendMsg(teacherFd, {{"type", "START_AUDIO_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Duplicate start fails", !r.value("success", false));
    std::cout << "\n";

    // === Test 4: Teacher sends audio frame, students receive ===
    std::cout << "--- Test 4: Teacher audio broadcast ---\n";
    sendMsg(teacherFd, {{"type", "AUDIO_FRAME"},
                         {"classId", classId},
                         {"audioData", "TEACHER_PCM_CHUNK_1"},
                         {"frameSeq", 1}});
    usleep(50000);

    auto f1 = tryRecvMsg(s1Fd);
    auto f2 = tryRecvMsg(s2Fd);
    check("Alice received audio frame", f1.value("type", "") == "NOTIFY_AUDIO_FRAME");
    check("Alice audio data correct", f1.value("audioData", "") == "TEACHER_PCM_CHUNK_1");
    check("Alice frame seq correct", f1.value("frameSeq", 0) == 1);
    check("Frame has sender info", f1.value("senderName", "") == "Teacher Audio");
    check("Bob received audio frame", f2.value("type", "") == "NOTIFY_AUDIO_FRAME");

    // Teacher should NOT receive own frame
    auto selfFrame = tryRecvMsg(teacherFd);
    check("Teacher does not receive own frame", selfFrame.empty());
    std::cout << "\n";

    // === Test 5: Student without mic cannot send frames ===
    std::cout << "--- Test 5: Student without mic cannot send ---\n";
    sendMsg(s1Fd, {{"type", "AUDIO_FRAME"},
                    {"classId", classId},
                    {"audioData", "UNAUTHORIZED_AUDIO"},
                    {"frameSeq", 99}});
    usleep(50000);
    auto spurious = tryRecvMsg(s2Fd);
    check("Unauthorized student frame not broadcast",
          spurious.empty() || spurious.value("audioData", "") != "UNAUTHORIZED_AUDIO");
    // Drain teacher
    tryRecvMsg(teacherFd);
    std::cout << "\n";

    // === Test 6: Teacher grants mic to Alice ===
    std::cout << "--- Test 6: Grant mic to Alice ---\n";
    sendMsg(teacherFd, {{"type", "GRANT_MIC_REQ"},
                         {"classId", classId},
                         {"studentId", aliceUid}});
    r = recvMsg(teacherFd);
    check("Grant mic succeeds", r.value("success", false));
    check("Response has studentId", r.value("studentId", 0) == aliceUid);

    auto micNotify = tryRecvMsg(s1Fd);
    check("Alice received mic granted notification",
          micNotify.value("type", "") == "NOTIFY_MIC_GRANTED");
    std::cout << "\n";

    // === Test 7: Alice sends audio, teacher + Bob receive ===
    std::cout << "--- Test 7: Alice speaks with mic ---\n";
    sendMsg(s1Fd, {{"type", "AUDIO_FRAME"},
                    {"classId", classId},
                    {"audioData", "ALICE_SPEAKING"},
                    {"frameSeq", 1}});
    usleep(50000);

    // Teacher should receive Alice's audio
    auto teacherFrame = tryRecvMsg(teacherFd);
    check("Teacher received Alice's audio",
          teacherFrame.value("type", "") == "NOTIFY_AUDIO_FRAME");
    check("Teacher got correct audio data",
          teacherFrame.value("audioData", "") == "ALICE_SPEAKING");
    check("Sender is Alice", teacherFrame.value("senderName", "") == "Alice");

    // Bob should receive Alice's audio
    auto bobFrame = tryRecvMsg(s2Fd);
    check("Bob received Alice's audio",
          bobFrame.value("type", "") == "NOTIFY_AUDIO_FRAME");
    check("Bob got correct audio data",
          bobFrame.value("audioData", "") == "ALICE_SPEAKING");

    // Alice should NOT receive own frame
    auto aliceSelf = tryRecvMsg(s1Fd);
    check("Alice does not receive own frame", aliceSelf.empty());
    std::cout << "\n";

    // === Test 8: Bob still cannot send (no mic) ===
    std::cout << "--- Test 8: Bob still cannot send ---\n";
    sendMsg(s2Fd, {{"type", "AUDIO_FRAME"},
                    {"classId", classId},
                    {"audioData", "BOB_UNAUTHORIZED"},
                    {"frameSeq", 1}});
    usleep(50000);
    auto bobAttempt = tryRecvMsg(s1Fd);
    check("Bob's frame not broadcast",
          bobAttempt.empty() || bobAttempt.value("audioData", "") != "BOB_UNAUTHORIZED");
    tryRecvMsg(teacherFd); // drain
    std::cout << "\n";

    // === Test 9: Teacher revokes mic ===
    std::cout << "--- Test 9: Revoke mic ---\n";
    sendMsg(teacherFd, {{"type", "REVOKE_MIC_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Revoke mic succeeds", r.value("success", false));

    auto revokeNotify = tryRecvMsg(s1Fd);
    check("Alice received mic revoked notification",
          revokeNotify.value("type", "") == "NOTIFY_MIC_REVOKED");
    std::cout << "\n";

    // === Test 10: Alice cannot send after revoke ===
    std::cout << "--- Test 10: Alice cannot send after revoke ---\n";
    sendMsg(s1Fd, {{"type", "AUDIO_FRAME"},
                    {"classId", classId},
                    {"audioData", "ALICE_AFTER_REVOKE"},
                    {"frameSeq", 2}});
    usleep(50000);
    auto afterRevoke = tryRecvMsg(s2Fd);
    check("Alice's post-revoke frame not broadcast",
          afterRevoke.empty() || afterRevoke.value("audioData", "") != "ALICE_AFTER_REVOKE");
    tryRecvMsg(teacherFd); // drain
    std::cout << "\n";

    // === Test 11: Duplicate revoke fails ===
    std::cout << "--- Test 11: Duplicate revoke ---\n";
    sendMsg(teacherFd, {{"type", "REVOKE_MIC_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Duplicate revoke fails", !r.value("success", false));
    std::cout << "\n";

    // === Test 12: Grant mic without audio active fails ===
    std::cout << "--- Test 12: Grant mic without audio ---\n";
    sendMsg(teacherFd, {{"type", "STOP_AUDIO_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Stop audio succeeds", r.value("success", false));
    // Drain notifications
    tryRecvMsg(s1Fd);
    tryRecvMsg(s2Fd);

    sendMsg(teacherFd, {{"type", "GRANT_MIC_REQ"},
                         {"classId", classId},
                         {"studentId", aliceUid}});
    r = recvMsg(teacherFd);
    check("Cannot grant mic when audio not active", !r.value("success", false));
    std::cout << "\n";

    // === Test 13: Teacher stops audio, students notified ===
    std::cout << "--- Test 13: Stop audio notification ---\n";
    sendMsg(teacherFd, {{"type", "START_AUDIO_REQ"}, {"classId", classId}});
    recvMsg(teacherFd);
    tryRecvMsg(s1Fd); // drain start notification
    tryRecvMsg(s2Fd);

    sendMsg(teacherFd, {{"type", "STOP_AUDIO_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Stop audio succeeds", r.value("success", false));

    n1 = tryRecvMsg(s1Fd);
    n2 = tryRecvMsg(s2Fd);
    check("Alice received audio stopped", n1.value("type", "") == "NOTIFY_AUDIO_STOPPED");
    check("Bob received audio stopped", n2.value("type", "") == "NOTIFY_AUDIO_STOPPED");
    std::cout << "\n";

    // === Test 14: Frames not routed after stop ===
    std::cout << "--- Test 14: No frames after stop ---\n";
    sendMsg(teacherFd, {{"type", "AUDIO_FRAME"},
                         {"classId", classId},
                         {"audioData", "SHOULD_NOT_ARRIVE"},
                         {"frameSeq", 99}});
    usleep(50000);
    auto late = tryRecvMsg(s1Fd);
    check("No audio frame after stop", late.empty());
    std::cout << "\n";

    // === Test 15: Audio auto-stops when class ends ===
    std::cout << "--- Test 15: Audio auto-stops on class end ---\n";
    sendMsg(teacherFd, {{"type", "START_AUDIO_REQ"}, {"classId", classId}});
    recvMsg(teacherFd);
    tryRecvMsg(s1Fd);
    tryRecvMsg(s2Fd);

    sendMsg(teacherFd, {{"type", "END_CLASS_REQ"}, {"classId", classId}});
    recvMsg(teacherFd);

    sendMsg(teacherFd, {{"type", "STOP_AUDIO_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Audio auto-stopped when class ended", !r.value("success", false));
    std::cout << "\n";

    // === Test 16: Audio on ended class fails ===
    std::cout << "--- Test 16: Audio on ended class ---\n";
    sendMsg(teacherFd, {{"type", "START_AUDIO_REQ"}, {"classId", classId}});
    r = recvMsg(teacherFd);
    check("Cannot start audio on ENDED class", !r.value("success", false));
    std::cout << "\n";

    // === Test 17: Non-owner teacher ===
    std::cout << "--- Test 17: Non-owner teacher ---\n";
    adminFd = loginAs(host, port, "admin", "admin123");
    sendMsg(adminFd, {{"type", "CREATE_USER_REQ"},
                       {"username", "t_other_audio"}, {"password", "pass"},
                       {"name", "Other Teacher"}, {"role", "TEACHER"}});
    recvMsg(adminFd);
    close(adminFd);

    // Create a new active class
    sendMsg(teacherFd, {{"type", "CREATE_CLASS_REQ"}, {"name", "Audio Owner Test"}});
    r = recvMsg(teacherFd);
    int classId2 = r.value("classId", 0);
    sendMsg(teacherFd, {{"type", "START_CLASS_REQ"}, {"classId", classId2}});
    recvMsg(teacherFd);

    int otherFd = loginAs(host, port, "t_other_audio", "pass");
    if (otherFd >= 0) {
        sendMsg(otherFd, {{"type", "START_AUDIO_REQ"}, {"classId", classId2}});
        r = recvMsg(otherFd);
        check("Non-owner cannot start audio", !r.value("success", false));
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
