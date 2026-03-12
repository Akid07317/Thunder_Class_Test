#include "class_manager.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <random>

ClassManager::ClassManager(const std::string& dataDir) : dataDir_(dataDir) {
    loadClasses();
    loadEvents();
}

// --- Class lifecycle ---

int ClassManager::createClass(const std::string& name, int teacherId, const std::string& teacherName) {
    int id = nextClassId_++;
    ClassInfo ci;
    ci.classId = id;
    ci.name = name;
    ci.teacherId = teacherId;
    ci.teacherName = teacherName;
    ci.status = ClassStatus::WAITING;
    classes_[id] = ci;
    addEvent(id, "CLASS_CREATED", teacherId, teacherName);
    saveClasses();
    return id;
}

bool ClassManager::startClass(int classId, int teacherId) {
    auto it = classes_.find(classId);
    if (it == classes_.end()) return false;
    if (it->second.teacherId != teacherId) return false;
    if (it->second.status != ClassStatus::WAITING) return false;

    it->second.status = ClassStatus::ACTIVE;
    addEvent(classId, "CLASS_STARTED", teacherId, it->second.teacherName);
    saveClasses();
    return true;
}

bool ClassManager::endClass(int classId, int teacherId) {
    auto it = classes_.find(classId);
    if (it == classes_.end()) return false;
    if (it->second.teacherId != teacherId) return false;
    if (it->second.status != ClassStatus::ACTIVE) return false;

    it->second.status = ClassStatus::ENDED;
    checkins_.erase(classId);
    screenSharing_.erase(classId);
    audioActive_.erase(classId);
    micHolder_.erase(classId);
    members_.erase(classId);
    addEvent(classId, "CLASS_ENDED", teacherId, it->second.teacherName);
    saveClasses();
    return true;
}

// --- Membership ---

bool ClassManager::joinClass(int classId, int userId, const std::string& userName) {
    auto it = classes_.find(classId);
    if (it == classes_.end()) return false;
    if (it->second.status != ClassStatus::ACTIVE) return false;
    if (members_[classId].count(userId)) return false;  // already in class

    members_[classId][userId] = userName;
    addEvent(classId, "STUDENT_JOINED", userId, userName);
    return true;
}

bool ClassManager::leaveClass(int classId, int userId, const std::string& userName) {
    auto it = members_.find(classId);
    if (it == members_.end()) return false;
    if (!it->second.count(userId)) return false;

    it->second.erase(userId);
    addEvent(classId, "STUDENT_LEFT", userId, userName);
    return true;
}

void ClassManager::removeFromAll(int userId, const std::string& userName) {
    for (auto& [classId, memberMap] : members_) {
        if (memberMap.erase(userId)) {
            addEvent(classId, "STUDENT_LEFT", userId, userName);
        }
    }
}

// --- Check-in ---

bool ClassManager::openCheckin(int classId, int teacherId, int deadlineSeconds) {
    auto it = classes_.find(classId);
    if (it == classes_.end()) return false;
    if (it->second.teacherId != teacherId) return false;
    if (it->second.status != ClassStatus::ACTIVE) return false;

    if (deadlineSeconds <= 0) deadlineSeconds = 60;
    CheckinState state;
    state.open = true;
    state.openTime = std::time(nullptr);
    state.deadlineSeconds = deadlineSeconds;
    checkins_[classId] = state;
    addEvent(classId, "CHECKIN_OPENED", teacherId, it->second.teacherName, deadlineSeconds);
    return true;
}

bool ClassManager::checkin(int classId, int userId) {
    auto it = checkins_.find(classId);
    if (it == checkins_.end() || !it->second.open) return false;

    // Must be a member of the class
    auto mit = members_.find(classId);
    if (mit == members_.end() || !mit->second.count(userId)) return false;

    if (it->second.checkedIn.count(userId)) return false;  // already checked in

    it->second.checkedIn[userId] = std::time(nullptr);
    addEvent(classId, "STUDENT_CHECKED_IN", userId, mit->second[userId]);
    return true;
}

std::vector<std::pair<std::string, std::string>> ClassManager::getCheckinStatus(int classId) const {
    std::vector<std::pair<std::string, std::string>> result;
    auto mit = members_.find(classId);
    if (mit == members_.end()) return result;

    auto cit = checkins_.find(classId);
    for (auto& [uid, name] : mit->second) {
        std::string status = "absent";
        if (cit != checkins_.end() && cit->second.checkedIn.count(uid)) {
            std::time_t checkinTime = cit->second.checkedIn.at(uid);
            if (checkinTime <= cit->second.openTime + cit->second.deadlineSeconds) {
                status = "on_time";
            } else {
                status = "late";
            }
        }
        result.push_back({name, status});
    }
    return result;
}

std::vector<int> ClassManager::getMembers(int classId) const {
    std::vector<int> result;
    auto it = members_.find(classId);
    if (it != members_.end()) {
        for (auto& [uid, _] : it->second) result.push_back(uid);
    }
    return result;
}

bool ClassManager::isCheckinOpen(int classId) const {
    auto it = checkins_.find(classId);
    return it != checkins_.end() && it->second.open;
}

// --- Random calling ---

std::optional<std::pair<int, std::string>> ClassManager::randomCall(int classId, int teacherId) {
    auto it = classes_.find(classId);
    if (it == classes_.end()) return std::nullopt;
    if (it->second.teacherId != teacherId) return std::nullopt;
    if (it->second.status != ClassStatus::ACTIVE) return std::nullopt;

    auto mit = members_.find(classId);
    if (mit == members_.end() || mit->second.empty()) return std::nullopt;

    // Collect members into a vector for random access
    std::vector<std::pair<int, std::string>> memberVec(mit->second.begin(), mit->second.end());

    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, memberVec.size() - 1);
    auto& selected = memberVec[dist(rng)];

    addEvent(classId, "RANDOM_CALLED", selected.first, selected.second);
    return selected;
}

// --- Screen sharing ---

bool ClassManager::startScreenShare(int classId, int teacherId) {
    auto it = classes_.find(classId);
    if (it == classes_.end()) return false;
    if (it->second.teacherId != teacherId) return false;
    if (it->second.status != ClassStatus::ACTIVE) return false;
    if (screenSharing_.count(classId)) return false; // already sharing

    screenSharing_.insert(classId);
    addEvent(classId, "SCREEN_SHARE_STARTED", teacherId, it->second.teacherName);
    return true;
}

bool ClassManager::stopScreenShare(int classId, int teacherId) {
    auto it = classes_.find(classId);
    if (it == classes_.end()) return false;
    if (it->second.teacherId != teacherId) return false;
    if (!screenSharing_.count(classId)) return false; // not sharing

    screenSharing_.erase(classId);
    addEvent(classId, "SCREEN_SHARE_STOPPED", teacherId, it->second.teacherName);
    return true;
}

bool ClassManager::isScreenSharing(int classId) const {
    return screenSharing_.count(classId) > 0;
}

// --- Audio ---

bool ClassManager::startAudio(int classId, int teacherId) {
    auto it = classes_.find(classId);
    if (it == classes_.end()) return false;
    if (it->second.teacherId != teacherId) return false;
    if (it->second.status != ClassStatus::ACTIVE) return false;
    if (audioActive_.count(classId)) return false;

    audioActive_.insert(classId);
    addEvent(classId, "AUDIO_STARTED", teacherId, it->second.teacherName);
    return true;
}

bool ClassManager::stopAudio(int classId, int teacherId) {
    auto it = classes_.find(classId);
    if (it == classes_.end()) return false;
    if (it->second.teacherId != teacherId) return false;
    if (!audioActive_.count(classId)) return false;

    audioActive_.erase(classId);
    micHolder_.erase(classId);
    addEvent(classId, "AUDIO_STOPPED", teacherId, it->second.teacherName);
    return true;
}

bool ClassManager::isAudioActive(int classId) const {
    return audioActive_.count(classId) > 0;
}

bool ClassManager::grantMic(int classId, int teacherId, int studentId) {
    auto it = classes_.find(classId);
    if (it == classes_.end()) return false;
    if (it->second.teacherId != teacherId) return false;
    if (!audioActive_.count(classId)) return false;

    // Student must be a member
    auto mit = members_.find(classId);
    if (mit == members_.end() || !mit->second.count(studentId)) return false;

    // Revoke previous mic holder if any
    auto prev = micHolder_.find(classId);
    if (prev != micHolder_.end() && prev->second != studentId) {
        addEvent(classId, "MIC_REVOKED", prev->second, "");
    }
    micHolder_[classId] = studentId;
    addEvent(classId, "MIC_GRANTED", studentId, mit->second.at(studentId));
    return true;
}

bool ClassManager::revokeMic(int classId, int teacherId) {
    auto it = classes_.find(classId);
    if (it == classes_.end()) return false;
    if (it->second.teacherId != teacherId) return false;

    auto mit = micHolder_.find(classId);
    if (mit == micHolder_.end()) return false;

    int studentId = mit->second;
    micHolder_.erase(classId);

    // Find student name for event
    auto memIt = members_.find(classId);
    std::string name;
    if (memIt != members_.end()) {
        auto nit = memIt->second.find(studentId);
        if (nit != memIt->second.end()) name = nit->second;
    }
    addEvent(classId, "MIC_REVOKED", studentId, name);
    return true;
}

int ClassManager::getMicHolder(int classId) const {
    auto it = micHolder_.find(classId);
    if (it != micHolder_.end()) return it->second;
    return -1;
}

// --- Queries ---

std::optional<ClassInfo> ClassManager::getClass(int classId) const {
    auto it = classes_.find(classId);
    if (it != classes_.end()) return it->second;
    return std::nullopt;
}

std::vector<ClassInfo> ClassManager::getAllClasses() const {
    std::vector<ClassInfo> result;
    for (auto& [_, ci] : classes_) result.push_back(ci);
    return result;
}

std::vector<std::pair<int, std::string>> ClassManager::getMemberList(int classId) const {
    std::vector<std::pair<int, std::string>> result;
    auto it = members_.find(classId);
    if (it != members_.end()) {
        for (auto& [uid, name] : it->second) result.push_back({uid, name});
    }
    return result;
}

std::vector<ClassEvent> ClassManager::getEventsForClass(int classId) const {
    std::vector<ClassEvent> result;
    for (auto& e : events_) {
        if (e.classId == classId) result.push_back(e);
    }
    return result;
}

// --- Persistence & events ---

void ClassManager::addEvent(int classId, const std::string& eventType, int userId, const std::string& userName, int detail) {
    ClassEvent e;
    e.classId = classId;
    e.eventType = eventType;
    e.userId = userId;
    e.userName = userName;
    e.timestamp = std::time(nullptr);
    e.detail = detail;
    events_.push_back(e);
    saveEvents();
}

void ClassManager::saveClasses() const {
    std::filesystem::create_directories(dataDir_);
    nlohmann::json j = nlohmann::json::array();
    for (auto& [_, ci] : classes_) j.push_back(ci.toJson());
    std::ofstream ofs(dataDir_ + "/classes.json");
    if (!ofs) { std::cerr << "Error: cannot write classes.json\n"; return; }
    ofs << j.dump(2) << "\n";
}

void ClassManager::saveEvents() const {
    std::filesystem::create_directories(dataDir_);
    nlohmann::json j = nlohmann::json::array();
    for (auto& e : events_) j.push_back(e.toJson());
    std::ofstream ofs(dataDir_ + "/events.json");
    if (!ofs) { std::cerr << "Error: cannot write events.json\n"; return; }
    ofs << j.dump(2) << "\n";
}

void ClassManager::loadClasses() {
    std::ifstream ifs(dataDir_ + "/classes.json");
    if (!ifs.is_open()) return;
    try {
        nlohmann::json j = nlohmann::json::parse(ifs);
        for (auto& item : j) {
            ClassInfo ci = ClassInfo::fromJson(item);
            classes_[ci.classId] = ci;
            if (ci.classId >= nextClassId_) nextClassId_ = ci.classId + 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: failed to load classes: " << e.what() << "\n";
    }
}

void ClassManager::loadEvents() {
    std::ifstream ifs(dataDir_ + "/events.json");
    if (!ifs.is_open()) return;
    try {
        nlohmann::json j = nlohmann::json::parse(ifs);
        for (auto& item : j) {
            events_.push_back(ClassEvent::fromJson(item));
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: failed to load events: " << e.what() << "\n";
    }
}
