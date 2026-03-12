// Minimal console client for verifying login and permission control.
// Usage: ./test_login_client [host] [port]

#include "test_socket.h"
#include <common/model/user.h>

int main(int argc, char* argv[]) {
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    uint16_t port = argc > 2 ? std::atoi(argv[2]) : 9000;

    std::cout << "Connecting to " << host << ":" << port << "...\n";
    int fd = connectToServer(host, port);
    if (fd < 0) return 1;
    std::cout << "Connected.\n";

    // Test 1: Login with wrong password
    std::cout << "\n--- Test 1: Login with wrong password ---\n";
    sendMsg(fd, {{"type", "LOGIN_REQ"}, {"username", "admin"}, {"password", "wrong"}});
    printResp("Expected: failure", recvMsg(fd));

    // Test 2: Login as admin
    std::cout << "\n--- Test 2: Login as admin ---\n";
    sendMsg(fd, {{"type", "LOGIN_REQ"}, {"username", "admin"}, {"password", "admin123"}});
    auto loginResp = recvMsg(fd);
    printResp("Expected: success, role=ADMIN", loginResp);

    if (!loginResp.value("success", false)) {
        std::cerr << "Admin login failed, aborting.\n";
        sockClose(fd);
        return 1;
    }

    // Test 3: Create a teacher account
    std::cout << "\n--- Test 3: Create teacher account ---\n";
    sendMsg(fd, {{"type", "CREATE_USER_REQ"},
                 {"username", "teacher1"}, {"password", "pass123"},
                 {"name", "Zhang San"}, {"role", "TEACHER"}});
    printResp("Expected: success", recvMsg(fd));

    // Test 4: Create a student account
    std::cout << "\n--- Test 4: Create student account ---\n";
    sendMsg(fd, {{"type", "CREATE_USER_REQ"},
                 {"username", "student1"}, {"password", "pass123"},
                 {"name", "Li Si"}, {"role", "STUDENT"}});
    printResp("Expected: success", recvMsg(fd));

    // Test 5: Create duplicate user
    std::cout << "\n--- Test 5: Create duplicate user ---\n";
    sendMsg(fd, {{"type", "CREATE_USER_REQ"},
                 {"username", "student1"}, {"password", "pass123"},
                 {"name", "Li Si"}, {"role", "STUDENT"}});
    printResp("Expected: failure (duplicate)", recvMsg(fd));

    // Test 6: List all users
    std::cout << "\n--- Test 6: List all users ---\n";
    sendMsg(fd, {{"type", "LIST_USERS_REQ"}});
    printResp("Expected: 3 users", recvMsg(fd));

    // Test 7: Logout and test permission check
    std::cout << "\n--- Test 7: Logout ---\n";
    sendMsg(fd, {{"type", "LOGOUT_REQ"}});
    printResp("Expected: success", recvMsg(fd));

    // Test 8: Try to create user without being logged in
    std::cout << "\n--- Test 8: Create user without login (should fail) ---\n";
    sendMsg(fd, {{"type", "CREATE_USER_REQ"},
                 {"username", "hacker"}, {"password", "x"},
                 {"name", "Hacker"}, {"role", "ADMIN"}});
    printResp("Expected: permission denied", recvMsg(fd));

    // Test 9: Login as student and try admin operation
    std::cout << "\n--- Test 9: Student tries admin operation ---\n";
    sendMsg(fd, {{"type", "LOGIN_REQ"}, {"username", "student1"}, {"password", "pass123"}});
    auto studentLogin = recvMsg(fd);
    printResp("Student login", studentLogin);

    sendMsg(fd, {{"type", "CREATE_USER_REQ"},
                 {"username", "hacker2"}, {"password", "x"},
                 {"name", "Hacker2"}, {"role", "ADMIN"}});
    printResp("Expected: permission denied", recvMsg(fd));

    sockClose(fd);
    std::cout << "\n=== All tests complete ===\n";
    return 0;
}
