#pragma once
#include <string>
#include <ctime>
#include <nlohmann/json.hpp>

struct ClassEvent {
    int classId = 0;
    std::string eventType;  // CLASS_CREATED, CLASS_STARTED, CLASS_ENDED,
                            // STUDENT_JOINED, STUDENT_LEFT,
                            // CHECKIN_OPENED, STUDENT_CHECKED_IN
    int userId = 0;
    std::string userName;
    std::time_t timestamp = 0;
    int detail = 0;  // Extra info: for CHECKIN_OPENED, stores deadlineSeconds

    nlohmann::json toJson() const {
        nlohmann::json j = {
            {"classId", classId},
            {"eventType", eventType},
            {"userId", userId},
            {"userName", userName},
            {"timestamp", timestamp}
        };
        if (detail != 0) j["detail"] = detail;
        return j;
    }

    static ClassEvent fromJson(const nlohmann::json& j) {
        ClassEvent e;
        e.classId   = j.value("classId", 0);
        e.eventType = j.value("eventType", "");
        e.userId    = j.value("userId", 0);
        e.userName  = j.value("userName", "");
        e.timestamp = j.value("timestamp", static_cast<std::time_t>(0));
        e.detail    = j.value("detail", 0);
        return e;
    }
};
