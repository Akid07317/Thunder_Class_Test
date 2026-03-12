#include "statistics_exporter.h"
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

StatisticsExporter::ClassSummary StatisticsExporter::buildSummary(
    const ClassInfo& classInfo,
    const std::vector<ClassEvent>& events,
    const std::vector<Question>& questions,
    const std::vector<std::vector<AnswerRecord>>& answersPerQuestion)
{
    ClassSummary summary;
    summary.classInfo = classInfo;
    summary.totalQuestions = static_cast<int>(questions.size());

    // Extract start/end times from events
    for (auto& e : events) {
        if (e.eventType == "CLASS_STARTED") summary.startTime = e.timestamp;
        if (e.eventType == "CLASS_ENDED") summary.endTime = e.timestamp;
    }

    // Collect all students who ever joined, and compute durations
    // Track join timestamps per student; pair with leave or class end
    struct JoinInfo {
        std::string name;
        std::time_t lastJoin = 0;
        bool inClass = false;
        long totalDuration = 0;
        std::string checkinStatus = "absent";  // "on_time", "late", or "absent"
    };
    std::unordered_map<int, JoinInfo> studentInfo;

    // Track check-in deadline from CHECKIN_OPENED event
    std::time_t checkinOpenTime = 0;
    int checkinDeadline = 60;

    for (auto& e : events) {
        if (e.eventType == "STUDENT_JOINED") {
            auto& info = studentInfo[e.userId];
            info.name = e.userName;
            info.lastJoin = e.timestamp;
            info.inClass = true;
        } else if (e.eventType == "STUDENT_LEFT") {
            auto it = studentInfo.find(e.userId);
            if (it != studentInfo.end() && it->second.inClass) {
                it->second.totalDuration += (e.timestamp - it->second.lastJoin);
                it->second.inClass = false;
            }
        } else if (e.eventType == "CHECKIN_OPENED") {
            checkinOpenTime = e.timestamp;
            checkinDeadline = (e.detail > 0) ? e.detail : 60;
        } else if (e.eventType == "STUDENT_CHECKED_IN") {
            auto& info = studentInfo[e.userId];
            if (checkinOpenTime > 0 && e.timestamp <= checkinOpenTime + checkinDeadline) {
                info.checkinStatus = "on_time";
            } else {
                info.checkinStatus = "late";
            }
        } else if (e.eventType == "CLASS_ENDED") {
            // Close any open sessions
            for (auto& [uid, info] : studentInfo) {
                if (info.inClass) {
                    info.totalDuration += (e.timestamp - info.lastJoin);
                    info.inClass = false;
                }
            }
        }
    }

    // Build per-student answer stats
    // Map userId -> {answered, correct}
    std::unordered_map<int, int> answeredCount;
    std::unordered_map<int, int> correctCount;

    for (size_t i = 0; i < questions.size(); ++i) {
        if (i < answersPerQuestion.size()) {
            for (auto& a : answersPerQuestion[i]) {
                answeredCount[a.userId]++;
                if (a.answer == questions[i].correctAnswer) {
                    correctCount[a.userId]++;
                }
            }
        }
    }

    // Build student stats
    int checkedInTotal = 0;
    std::unordered_set<int> seen;
    for (auto& [uid, info] : studentInfo) {
        if (seen.count(uid)) continue;
        seen.insert(uid);

        StudentStat s;
        s.userId = uid;
        s.name = info.name;
        s.checkinStatus = info.checkinStatus;
        s.durationSeconds = info.totalDuration;
        s.questionsAnswered = answeredCount[uid];
        s.correctAnswers = correctCount[uid];

        if (s.checkinStatus != "absent") ++checkedInTotal;
        summary.students.push_back(s);
    }

    // Sort students by userId for deterministic output
    std::sort(summary.students.begin(), summary.students.end(),
              [](const StudentStat& a, const StudentStat& b) { return a.userId < b.userId; });

    summary.totalStudents = static_cast<int>(summary.students.size());
    summary.checkedInCount = checkedInTotal;
    return summary;
}

nlohmann::json StatisticsExporter::summaryToJson(const ClassSummary& summary) {
    nlohmann::json j;
    j["classId"] = summary.classInfo.classId;
    j["className"] = summary.classInfo.name;
    j["teacherName"] = summary.classInfo.teacherName;
    j["status"] = to_string(summary.classInfo.status);
    j["startTime"] = summary.startTime;
    j["endTime"] = summary.endTime;
    j["totalStudents"] = summary.totalStudents;
    j["checkedInCount"] = summary.checkedInCount;
    j["totalQuestions"] = summary.totalQuestions;

    nlohmann::json arr = nlohmann::json::array();
    for (auto& s : summary.students) {
        arr.push_back({
            {"userId", s.userId},
            {"name", s.name},
            {"checkinStatus", s.checkinStatus},
            {"durationSeconds", s.durationSeconds},
            {"questionsAnswered", s.questionsAnswered},
            {"correctAnswers", s.correctAnswers}
        });
    }
    j["students"] = arr;
    return j;
}

// Escape a string for CSV: quote if it contains comma, quote, newline, or starts with =+-@
static std::string csvEscape(const std::string& s) {
    bool needsQuote = false;
    for (char c : s) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') { needsQuote = true; break; }
    }
    if (!s.empty() && (s[0] == '=' || s[0] == '+' || s[0] == '-' || s[0] == '@')) {
        needsQuote = true;
    }
    if (!needsQuote) return s;
    std::string escaped = "\"";
    for (char c : s) {
        if (c == '"') escaped += "\"\"";
        else escaped += c;
    }
    escaped += "\"";
    return escaped;
}

static std::string formatTime(std::time_t t) {
    if (t == 0) return "";
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

std::string StatisticsExporter::summaryToCSV(const ClassSummary& summary) {
    std::ostringstream oss;

    // Header section
    oss << "Class Report\n";
    oss << "Class Name," << csvEscape(summary.classInfo.name) << "\n";
    oss << "Teacher," << csvEscape(summary.classInfo.teacherName) << "\n";
    oss << "Status," << to_string(summary.classInfo.status) << "\n";
    oss << "Start Time," << formatTime(summary.startTime) << "\n";
    oss << "End Time," << formatTime(summary.endTime) << "\n";
    oss << "Total Students," << summary.totalStudents << "\n";
    oss << "Checked In," << summary.checkedInCount << "\n";
    oss << "Total Questions," << summary.totalQuestions << "\n";
    oss << "\n";

    // Student detail table
    oss << "Student Name,Check-in Status,Online Duration (s),Questions Answered,Correct Answers\n";
    for (auto& s : summary.students) {
        oss << csvEscape(s.name) << ","
            << s.checkinStatus << ","
            << s.durationSeconds << ","
            << s.questionsAnswered << ","
            << s.correctAnswers << "\n";
    }

    return oss.str();
}
