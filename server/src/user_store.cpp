#include "user_store.h"
#include <fstream>
#include <filesystem>
#include <iostream>

UserStore::UserStore(const std::string& filePath) : filePath_(filePath) {
    load();
    ensureDefaults();
}

void UserStore::load() {
    std::ifstream ifs(filePath_);
    if (!ifs.is_open()) return;

    try {
        nlohmann::json j = nlohmann::json::parse(ifs);
        for (auto& item : j) {
            User u = User::fromJson(item);
            users_.push_back(u);
            if (u.userId >= nextId_) nextId_ = u.userId + 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: failed to load users: " << e.what() << "\n";
    }
}

void UserStore::save() const {
    std::filesystem::create_directories(
        std::filesystem::path(filePath_).parent_path());

    nlohmann::json j = nlohmann::json::array();
    for (auto& u : users_) {
        j.push_back(u.toJson());
    }

    std::ofstream ofs(filePath_);
    ofs << j.dump(2) << "\n";
}

void UserStore::ensureDefaults() {
    // Create default admin if no users exist
    for (auto& u : users_) {
        if (u.role == Role::ADMIN) return;
    }
    createUser("admin", "admin123", "Administrator", Role::ADMIN);
    std::cout << "Created default admin account (admin / admin123)\n";
}

std::optional<User> UserStore::authenticate(const std::string& username,
                                            const std::string& password) const {
    for (auto& u : users_) {
        if (u.username == username && u.password == password) {
            return u;
        }
    }
    return std::nullopt;
}

bool UserStore::createUser(const std::string& username, const std::string& password,
                           const std::string& name, Role role) {
    for (auto& u : users_) {
        if (u.username == username) return false;
    }

    User u;
    u.userId = nextId_++;
    u.username = username;
    u.password = password;
    u.name = name;
    u.role = role;
    users_.push_back(u);
    save();
    return true;
}

std::vector<User> UserStore::getAllUsers() const {
    return users_;
}
