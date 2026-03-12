#pragma once
#include <unordered_map>
#include <optional>
#include <common/model/user.h>

class SessionManager {
public:
    // Register a logged-in user on a connection fd.
    // Returns false if the user is already logged in elsewhere.
    bool login(int fd, const User& user);

    // Remove session for a connection fd.
    void logout(int fd);

    // Get the user associated with a connection, if any.
    std::optional<User> getUser(int fd) const;

    // Check if a userId is currently logged in.
    bool isLoggedIn(int userId) const;

    // Look up the connection fd for a logged-in user. Returns -1 if not found.
    int getFd(int userId) const;

private:
    std::unordered_map<int, User> fdToUser_;
    std::unordered_map<int, int> userIdToFd_;  // userId -> fd
};
