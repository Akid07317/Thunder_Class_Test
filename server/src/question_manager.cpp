#include "question_manager.h"
#include <fstream>
#include <filesystem>
#include <iostream>

QuestionManager::QuestionManager(const std::string& dataDir) : dataDir_(dataDir) {
    loadQuestions();
    loadAnswers();
}

int QuestionManager::publishQuestion(int classId, const std::string& content,
                                     const std::vector<std::string>& options, int correctAnswer) {
    int id = nextQuestionId_++;
    Question q;
    q.questionId = id;
    q.classId = classId;
    q.content = content;
    q.options = options;
    q.correctAnswer = correctAnswer;
    q.timestamp = std::time(nullptr);

    questions_[id] = q;
    activeQuestion_[classId] = id;
    saveQuestions();
    return id;
}

std::optional<Question> QuestionManager::getActiveQuestion(int classId) const {
    auto it = activeQuestion_.find(classId);
    if (it == activeQuestion_.end()) return std::nullopt;
    auto qit = questions_.find(it->second);
    if (qit == questions_.end()) return std::nullopt;
    return qit->second;
}

bool QuestionManager::submitAnswer(int questionId, int userId, const std::string& userName, int answer) {
    auto qit = questions_.find(questionId);
    if (qit == questions_.end()) return false;

    // Check this is the active question for the class
    auto ait = activeQuestion_.find(qit->second.classId);
    if (ait == activeQuestion_.end() || ait->second != questionId) return false;

    // Check answer is within valid range
    if (answer < 0 || answer >= static_cast<int>(qit->second.options.size())) return false;

    // Check not already answered
    if (answered_[questionId].count(userId)) return false;

    AnswerRecord rec;
    rec.questionId = questionId;
    rec.userId = userId;
    rec.userName = userName;
    rec.answer = answer;
    rec.timestamp = std::time(nullptr);

    answers_[questionId].push_back(rec);
    answered_[questionId].insert(userId);
    saveAnswers();
    return true;
}

std::vector<AnswerRecord> QuestionManager::getAnswers(int questionId) const {
    auto it = answers_.find(questionId);
    if (it != answers_.end()) return it->second;
    return {};
}

std::optional<Question> QuestionManager::getQuestion(int questionId) const {
    auto it = questions_.find(questionId);
    if (it != questions_.end()) return it->second;
    return std::nullopt;
}

std::vector<Question> QuestionManager::getQuestionsForClass(int classId) const {
    std::vector<Question> result;
    for (auto& [_, q] : questions_) {
        if (q.classId == classId) result.push_back(q);
    }
    return result;
}

// --- Persistence ---

void QuestionManager::saveQuestions() const {
    std::filesystem::create_directories(dataDir_);
    nlohmann::json j = nlohmann::json::array();
    for (auto& [_, q] : questions_) j.push_back(q.toJson());
    std::ofstream ofs(dataDir_ + "/questions.json");
    if (!ofs) { std::cerr << "Error: cannot write questions.json\n"; return; }
    ofs << j.dump(2) << "\n";
}

void QuestionManager::saveAnswers() const {
    std::filesystem::create_directories(dataDir_);
    nlohmann::json j = nlohmann::json::array();
    for (auto& [_, recs] : answers_) {
        for (auto& a : recs) j.push_back(a.toJson());
    }
    std::ofstream ofs(dataDir_ + "/answers.json");
    if (!ofs) { std::cerr << "Error: cannot write answers.json\n"; return; }
    ofs << j.dump(2) << "\n";
}

void QuestionManager::loadQuestions() {
    std::ifstream ifs(dataDir_ + "/questions.json");
    if (!ifs.is_open()) return;
    try {
        nlohmann::json j = nlohmann::json::parse(ifs);
        for (auto& item : j) {
            Question q = Question::fromJson(item);
            questions_[q.questionId] = q;
            activeQuestion_[q.classId] = q.questionId;  // last published per class
            if (q.questionId >= nextQuestionId_) nextQuestionId_ = q.questionId + 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: failed to load questions: " << e.what() << "\n";
    }
}

void QuestionManager::loadAnswers() {
    std::ifstream ifs(dataDir_ + "/answers.json");
    if (!ifs.is_open()) return;
    try {
        nlohmann::json j = nlohmann::json::parse(ifs);
        for (auto& item : j) {
            AnswerRecord a = AnswerRecord::fromJson(item);
            answers_[a.questionId].push_back(a);
            answered_[a.questionId].insert(a.userId);
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: failed to load answers: " << e.what() << "\n";
    }
}
