#include "session_manager.h"

bool SessionManager::login(int fd, const User& user) {
    if (isLoggedIn(user.userId)) return false;
    fdToUser_[fd] = user;
    userIdToFd_[user.userId] = fd;
    return true;
}

void SessionManager::logout(int fd) {
    auto it = fdToUser_.find(fd);
    if (it != fdToUser_.end()) {
        userIdToFd_.erase(it->second.userId);
        fdToUser_.erase(it);
    }
}

std::optional<User> SessionManager::getUser(int fd) const {
    auto it = fdToUser_.find(fd);
    if (it != fdToUser_.end()) return it->second;
    return std::nullopt;
}

bool SessionManager::isLoggedIn(int userId) const {
    return userIdToFd_.count(userId) > 0;
}

int SessionManager::getFd(int userId) const {
    auto it = userIdToFd_.find(userId);
    return it != userIdToFd_.end() ? it->second : -1;
}
