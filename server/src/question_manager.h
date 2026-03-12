#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <common/model/question.h>

// Manages question publishing, answer collection, and persistence.
// One active question per class at a time.
class QuestionManager {
public:
    explicit QuestionManager(const std::string& dataDir);

    // Teacher publishes a question to a class. Returns questionId.
    int publishQuestion(int classId, const std::string& content,
                        const std::vector<std::string>& options, int correctAnswer);

    // Get the currently active question for a class (if any).
    std::optional<Question> getActiveQuestion(int classId) const;

    // Student submits an answer. Returns false if already answered or no active question.
    bool submitAnswer(int questionId, int userId, const std::string& userName, int answer);

    // Get all answers for a question.
    std::vector<AnswerRecord> getAnswers(int questionId) const;

    // Get the question by ID (with correct answer, for teacher view).
    std::optional<Question> getQuestion(int questionId) const;

    // Get all questions published for a class.
    std::vector<Question> getQuestionsForClass(int classId) const;

private:
    std::string dataDir_;
    std::unordered_map<int, Question> questions_;               // questionId -> Question
    std::unordered_map<int, int> activeQuestion_;               // classId -> questionId
    std::unordered_map<int, std::vector<AnswerRecord>> answers_; // questionId -> answers
    std::unordered_map<int, std::unordered_set<int>> answered_; // questionId -> set of userIds
    int nextQuestionId_ = 1;

    void saveQuestions() const;
    void saveAnswers() const;
    void loadQuestions();
    void loadAnswers();
};
