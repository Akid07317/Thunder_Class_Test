#pragma once
#include <string>
#include <common/model/user.h>

struct Session {
    bool loggedIn = false;
    int userId = 0;
    std::string username;
    std::string name;
    Role role = Role::STUDENT;

    void clear() {
        loggedIn = false;
        userId = 0;
        username.clear();
        name.clear();
        role = Role::STUDENT;
    }
};
