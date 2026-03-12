#pragma once
#include <string>
#include <nlohmann/json.hpp>

enum class Role {
    ADMIN,
    TEACHER,
    STUDENT
};

inline std::string to_string(Role r) {
    switch (r) {
        case Role::ADMIN:   return "ADMIN";
        case Role::TEACHER: return "TEACHER";
        case Role::STUDENT: return "STUDENT";
    }
    return "STUDENT";
}

inline Role role_from_string(const std::string& s) {
    if (s == "ADMIN")   return Role::ADMIN;
    if (s == "TEACHER") return Role::TEACHER;
    return Role::STUDENT;
}

struct User {
    int userId = 0;
    std::string username;
    std::string password;  // plaintext for MVP (course project)
    std::string name;
    Role role = Role::STUDENT;

    nlohmann::json toJson() const {
        return {
            {"userId", userId},
            {"username", username},
            {"password", password},
            {"name", name},
            {"role", to_string(role)}
        };
    }

    static User fromJson(const nlohmann::json& j) {
        User u;
        u.userId   = j.value("userId", 0);
        u.username = j.value("username", "");
        u.password = j.value("password", "");
        u.name     = j.value("name", "");
        u.role     = role_from_string(j.value("role", "STUDENT"));
        return u;
    }
};
