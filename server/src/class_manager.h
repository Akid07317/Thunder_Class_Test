#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <utility>
#include <common/model/class_info.h>
#include <common/model/class_event.h>

struct AttentionState {
    bool focused = true;
    int idleSeconds = 0;
    std::time_t lastReportTime = 0;
    long totalFocusedSeconds = 0;
    long totalUnfocusedSeconds = 0;
    long totalActiveSeconds = 0;
    long totalIdleSeconds = 0;
    int presenceChecksSent = 0;
    int presenceChecksResponded = 0;
};

struct PresenceCheckState {
    bool active = false;
    std::time_t sentTime = 0;
    int deadlineSeconds = 30;
    std::unordered_set<int> responded;
};

struct CheckinState {
    bool open = false;
    std::time_t openTime = 0;
    int deadlineSeconds = 60;  // default 60s
    std::unordered_map<int, std::time_t> checkedIn;  // userId -> checkin timestamp
};

// Manages class lifecycle, membership, check-in, and event logging.
// All state is in-memory with JSON file persistence.
class ClassManager {
public:
    explicit ClassManager(const std::string& dataDir);

    // --- Class lifecycle (teacher only) ---
    // Returns classId on success, -1 on failure.
    int createClass(const std::string& name, int teacherId, const std::string& teacherName);
    bool startClass(int classId, int teacherId);
    bool endClass(int classId, int teacherId);

    // --- Membership (student) ---
    bool joinClass(int classId, int userId, const std::string& userName);
    bool leaveClass(int classId, int userId, const std::string& userName);
    // Called when a student disconnects — remove from all active classes.
    void removeFromAll(int userId, const std::string& userName);

    // --- Check-in ---
    bool openCheckin(int classId, int teacherId, int deadlineSeconds = 60);
    bool checkin(int classId, int userId);
    // Returns {studentName, status} pairs. status: "on_time", "late", or "absent"
    std::vector<std::pair<std::string, std::string>> getCheckinStatus(int classId) const;
    // Returns userIds of current members (for notification broadcast).
    std::vector<int> getMembers(int classId) const;
    bool isCheckinOpen(int classId) const;

    // --- Random calling ---
    // Picks a random student from the class. Returns {userId, name} or nullopt if empty.
    std::optional<std::pair<int, std::string>> randomCall(int classId, int teacherId);

    // --- Screen sharing ---
    bool startScreenShare(int classId, int teacherId);
    bool stopScreenShare(int classId, int teacherId);
    bool isScreenSharing(int classId) const;

    // --- Audio ---
    bool startAudio(int classId, int teacherId);
    bool stopAudio(int classId, int teacherId);
    bool isAudioActive(int classId) const;
    bool grantMic(int classId, int teacherId, int studentId);
    bool revokeMic(int classId, int teacherId);
    // Returns userId of student with mic, or -1 if none.
    int getMicHolder(int classId) const;

    // --- Attention tracking ---
    void updateAttention(int classId, int userId, bool focused, int idleSeconds);
    AttentionState getAttention(int classId, int userId) const;
    std::vector<std::pair<int, AttentionState>> getAllAttention(int classId) const;

    // Presence check
    void startPresenceCheck(int classId);
    bool respondPresenceCheck(int classId, int userId);
    std::vector<int> getPresenceCheckNonResponders(int classId) const;
    void clearPresenceCheck(int classId);

    // --- Queries ---
    std::optional<ClassInfo> getClass(int classId) const;
    std::vector<ClassInfo> getAllClasses() const;
    std::vector<std::pair<int, std::string>> getMemberList(int classId) const;
    std::vector<ClassEvent> getEventsForClass(int classId) const;

private:
    std::string dataDir_;
    std::unordered_map<int, ClassInfo> classes_;
    std::unordered_map<int, std::unordered_map<int, std::string>> members_; // classId -> {userId -> name}
    std::unordered_map<int, CheckinState> checkins_; // classId -> checkin state
    std::unordered_set<int> screenSharing_; // classIds currently sharing screen
    std::unordered_set<int> audioActive_;  // classIds with audio broadcast active
    std::unordered_map<int, int> micHolder_; // classId -> userId holding mic (-1 = none)
    std::unordered_map<int, std::unordered_map<int, AttentionState>> attention_; // classId -> {userId -> state}
    std::unordered_map<int, PresenceCheckState> presenceChecks_; // classId -> state
    std::vector<ClassEvent> events_;
    int nextClassId_ = 1;

    void addEvent(int classId, const std::string& eventType, int userId, const std::string& userName, int detail = 0);
    void saveClasses() const;
    void saveEvents() const;
    void loadClasses();
    void loadEvents();
};
