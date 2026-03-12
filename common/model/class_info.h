#pragma once
#include <string>
#include <nlohmann/json.hpp>

enum class ClassStatus {
    WAITING,  // Created but not started
    ACTIVE,   // In session
    ENDED     // Session finished
};

inline std::string to_string(ClassStatus s) {
    switch (s) {
        case ClassStatus::WAITING: return "WAITING";
        case ClassStatus::ACTIVE:  return "ACTIVE";
        case ClassStatus::ENDED:   return "ENDED";
    }
    return "WAITING";
}

inline ClassStatus class_status_from_string(const std::string& s) {
    if (s == "ACTIVE") return ClassStatus::ACTIVE;
    if (s == "ENDED")  return ClassStatus::ENDED;
    return ClassStatus::WAITING;
}

struct ClassInfo {
    int classId = 0;
    std::string name;
    int teacherId = 0;
    std::string teacherName;
    ClassStatus status = ClassStatus::WAITING;

    nlohmann::json toJson() const {
        return {
            {"classId", classId},
            {"name", name},
            {"teacherId", teacherId},
            {"teacherName", teacherName},
            {"status", to_string(status)}
        };
    }

    static ClassInfo fromJson(const nlohmann::json& j) {
        ClassInfo c;
        c.classId     = j.value("classId", 0);
        c.name        = j.value("name", "");
        c.teacherId   = j.value("teacherId", 0);
        c.teacherName = j.value("teacherName", "");
        c.status      = class_status_from_string(j.value("status", "WAITING"));
        return c;
    }
};
