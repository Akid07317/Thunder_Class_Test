#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <common/model/class_info.h>
#include <common/model/class_event.h>
#include <common/model/question.h>
#include "class_manager.h"

// Computes class session statistics from existing event, question, and answer data.
// Does not own any data — reads from ClassManager and QuestionManager.
class StatisticsExporter {
public:
    struct StudentStat {
        int userId = 0;
        std::string name;
        std::string checkinStatus = "absent";  // "on_time", "late", or "absent"
        long durationSeconds = 0;
        int questionsAnswered = 0;
        int correctAnswers = 0;
        double focusRate = 100.0;
        double activeRate = 100.0;
        int presenceResponded = 0;
        int presenceTotal = 0;
    };

    struct ClassSummary {
        ClassInfo classInfo;
        std::time_t startTime = 0;
        std::time_t endTime = 0;
        int totalStudents = 0;
        int checkedInCount = 0;
        int totalQuestions = 0;
        std::vector<StudentStat> students;
    };

    // Build a summary from raw data.
    static ClassSummary buildSummary(
        const ClassInfo& classInfo,
        const std::vector<ClassEvent>& events,
        const std::vector<Question>& questions,
        const std::vector<std::vector<AnswerRecord>>& answersPerQuestion,
        const std::vector<std::pair<int, AttentionState>>& attentionData = {});

    // Convert summary to JSON.
    static nlohmann::json summaryToJson(const ClassSummary& summary);

    // Convert summary to CSV string.
    static std::string summaryToCSV(const ClassSummary& summary);
};
