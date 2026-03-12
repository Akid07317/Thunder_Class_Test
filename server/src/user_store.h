#pragma once
#include <string>
#include <vector>
#include <optional>
#include <common/model/user.h>

class UserStore {
public:
    explicit UserStore(const std::string& filePath);

    // Authenticate user. Returns User on success, nullopt on failure.
    std::optional<User> authenticate(const std::string& username,
                                     const std::string& password) const;

    // Create a new user. Returns false if username already exists.
    bool createUser(const std::string& username, const std::string& password,
                    const std::string& name, Role role);

    std::vector<User> getAllUsers() const;

private:
    std::string filePath_;
    std::vector<User> users_;
    int nextId_ = 1;

    void load();
    void save() const;
    void ensureDefaults();
};
