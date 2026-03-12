#pragma once
#include <string>
#include <vector>
#include <ctime>
#include <nlohmann/json.hpp>

struct Question {
    int questionId = 0;
    int classId = 0;
    std::string content;
    std::vector<std::string> options;  // e.g. ["A. ...", "B. ...", "C. ...", "D. ..."]
    int correctAnswer = 0;            // index into options (0-based)
    std::time_t timestamp = 0;

    nlohmann::json toJson() const {
        return {
            {"questionId", questionId},
            {"classId", classId},
            {"content", content},
            {"options", options},
            {"correctAnswer", correctAnswer},
            {"timestamp", timestamp}
        };
    }

    // For sending to students — omits correctAnswer
    nlohmann::json toStudentJson() const {
        return {
            {"questionId", questionId},
            {"classId", classId},
            {"content", content},
            {"options", options}
        };
    }

    static Question fromJson(const nlohmann::json& j) {
        Question q;
        q.questionId    = j.value("questionId", 0);
        q.classId       = j.value("classId", 0);
        q.content       = j.value("content", "");
        q.options       = j.value("options", std::vector<std::string>{});
        q.correctAnswer = j.value("correctAnswer", 0);
        q.timestamp     = j.value("timestamp", static_cast<std::time_t>(0));
        return q;
    }
};

struct AnswerRecord {
    int questionId = 0;
    int userId = 0;
    std::string userName;
    int answer = 0;  // index into options (0-based)
    std::time_t timestamp = 0;

    nlohmann::json toJson() const {
        return {
            {"questionId", questionId},
            {"userId", userId},
            {"userName", userName},
            {"answer", answer},
            {"timestamp", timestamp}
        };
    }

    static AnswerRecord fromJson(const nlohmann::json& j) {
        AnswerRecord a;
        a.questionId = j.value("questionId", 0);
        a.userId     = j.value("userId", 0);
        a.userName   = j.value("userName", "");
        a.answer     = j.value("answer", 0);
        a.timestamp  = j.value("timestamp", static_cast<std::time_t>(0));
        return a;
    }
};
