#pragma once
#include <string>

enum class MessageType {
    // Login
    LOGIN_REQ,
    LOGIN_RESP,
    LOGOUT_REQ,
    LOGOUT_RESP,

    // User management (admin only)
    CREATE_USER_REQ,
    CREATE_USER_RESP,
    LIST_USERS_REQ,
    LIST_USERS_RESP,

    // Class lifecycle (teacher)
    CREATE_CLASS_REQ,
    CREATE_CLASS_RESP,
    START_CLASS_REQ,
    START_CLASS_RESP,
    END_CLASS_REQ,
    END_CLASS_RESP,

    // Class participation (student)
    JOIN_CLASS_REQ,
    JOIN_CLASS_RESP,
    LEAVE_CLASS_REQ,
    LEAVE_CLASS_RESP,

    // Class queries (any authenticated user)
    LIST_CLASSES_REQ,
    LIST_CLASSES_RESP,
    CLASS_MEMBERS_REQ,
    CLASS_MEMBERS_RESP,

    // Check-in
    OPEN_CHECKIN_REQ,
    OPEN_CHECKIN_RESP,
    CHECKIN_REQ,
    CHECKIN_RESP,
    CHECKIN_STATUS_REQ,
    CHECKIN_STATUS_RESP,

    // Questions (teacher publishes, student answers)
    PUBLISH_QUESTION_REQ,
    PUBLISH_QUESTION_RESP,
    GET_QUESTION_REQ,
    GET_QUESTION_RESP,
    SUBMIT_ANSWER_REQ,
    SUBMIT_ANSWER_RESP,
    GET_ANSWERS_REQ,
    GET_ANSWERS_RESP,

    // Statistics & export (teacher)
    CLASS_SUMMARY_REQ,
    CLASS_SUMMARY_RESP,
    EXPORT_CLASS_REQ,
    EXPORT_CLASS_RESP,

    // Random calling (teacher)
    RANDOM_CALL_REQ,
    RANDOM_CALL_RESP,

    // Screen sharing (teacher)
    START_SCREEN_SHARE_REQ,
    START_SCREEN_SHARE_RESP,
    STOP_SCREEN_SHARE_REQ,
    STOP_SCREEN_SHARE_RESP,
    SCREEN_FRAME,

    // Audio (teacher broadcast + student mic)
    START_AUDIO_REQ,
    START_AUDIO_RESP,
    STOP_AUDIO_REQ,
    STOP_AUDIO_RESP,
    AUDIO_FRAME,
    GRANT_MIC_REQ,
    GRANT_MIC_RESP,
    REVOKE_MIC_REQ,
    REVOKE_MIC_RESP,

    // Notifications (server → client push)
    NOTIFY_CHECKIN_OPENED,
    NOTIFY_QUESTION_PUBLISHED,
    NOTIFY_RANDOM_CALLED,
    NOTIFY_SCREEN_SHARE_STARTED,
    NOTIFY_SCREEN_SHARE_STOPPED,
    NOTIFY_SCREEN_FRAME,
    NOTIFY_AUDIO_STARTED,
    NOTIFY_AUDIO_STOPPED,
    NOTIFY_AUDIO_FRAME,
    NOTIFY_MIC_GRANTED,
    NOTIFY_MIC_REVOKED,

    // Attention tracking
    ATTENTION_REPORT,           // student -> server: periodic focus/idle report
    NOTIFY_PRESENCE_CHECK,      // server -> student: "are you there?" push
    PRESENCE_CHECK_RESP,        // student -> server: "I'm here"
    ATTENTION_STATUS_REQ,       // teacher -> server: query attention data
    ATTENTION_STATUS_RESP,      // server -> teacher: attention data

    UNKNOWN
};

inline std::string to_string(MessageType t) {
    switch (t) {
        case MessageType::LOGIN_REQ:            return "LOGIN_REQ";
        case MessageType::LOGIN_RESP:           return "LOGIN_RESP";
        case MessageType::LOGOUT_REQ:           return "LOGOUT_REQ";
        case MessageType::LOGOUT_RESP:          return "LOGOUT_RESP";
        case MessageType::CREATE_USER_REQ:      return "CREATE_USER_REQ";
        case MessageType::CREATE_USER_RESP:     return "CREATE_USER_RESP";
        case MessageType::LIST_USERS_REQ:       return "LIST_USERS_REQ";
        case MessageType::LIST_USERS_RESP:      return "LIST_USERS_RESP";
        case MessageType::CREATE_CLASS_REQ:     return "CREATE_CLASS_REQ";
        case MessageType::CREATE_CLASS_RESP:    return "CREATE_CLASS_RESP";
        case MessageType::START_CLASS_REQ:      return "START_CLASS_REQ";
        case MessageType::START_CLASS_RESP:     return "START_CLASS_RESP";
        case MessageType::END_CLASS_REQ:        return "END_CLASS_REQ";
        case MessageType::END_CLASS_RESP:       return "END_CLASS_RESP";
        case MessageType::JOIN_CLASS_REQ:       return "JOIN_CLASS_REQ";
        case MessageType::JOIN_CLASS_RESP:      return "JOIN_CLASS_RESP";
        case MessageType::LEAVE_CLASS_REQ:      return "LEAVE_CLASS_REQ";
        case MessageType::LEAVE_CLASS_RESP:     return "LEAVE_CLASS_RESP";
        case MessageType::LIST_CLASSES_REQ:     return "LIST_CLASSES_REQ";
        case MessageType::LIST_CLASSES_RESP:    return "LIST_CLASSES_RESP";
        case MessageType::CLASS_MEMBERS_REQ:    return "CLASS_MEMBERS_REQ";
        case MessageType::CLASS_MEMBERS_RESP:   return "CLASS_MEMBERS_RESP";
        case MessageType::OPEN_CHECKIN_REQ:     return "OPEN_CHECKIN_REQ";
        case MessageType::OPEN_CHECKIN_RESP:    return "OPEN_CHECKIN_RESP";
        case MessageType::CHECKIN_REQ:          return "CHECKIN_REQ";
        case MessageType::CHECKIN_RESP:         return "CHECKIN_RESP";
        case MessageType::CHECKIN_STATUS_REQ:   return "CHECKIN_STATUS_REQ";
        case MessageType::CHECKIN_STATUS_RESP:  return "CHECKIN_STATUS_RESP";
        case MessageType::PUBLISH_QUESTION_REQ:  return "PUBLISH_QUESTION_REQ";
        case MessageType::PUBLISH_QUESTION_RESP: return "PUBLISH_QUESTION_RESP";
        case MessageType::GET_QUESTION_REQ:     return "GET_QUESTION_REQ";
        case MessageType::GET_QUESTION_RESP:    return "GET_QUESTION_RESP";
        case MessageType::SUBMIT_ANSWER_REQ:    return "SUBMIT_ANSWER_REQ";
        case MessageType::SUBMIT_ANSWER_RESP:   return "SUBMIT_ANSWER_RESP";
        case MessageType::GET_ANSWERS_REQ:      return "GET_ANSWERS_REQ";
        case MessageType::GET_ANSWERS_RESP:     return "GET_ANSWERS_RESP";
        case MessageType::CLASS_SUMMARY_REQ:     return "CLASS_SUMMARY_REQ";
        case MessageType::CLASS_SUMMARY_RESP:    return "CLASS_SUMMARY_RESP";
        case MessageType::EXPORT_CLASS_REQ:      return "EXPORT_CLASS_REQ";
        case MessageType::EXPORT_CLASS_RESP:     return "EXPORT_CLASS_RESP";
        case MessageType::RANDOM_CALL_REQ:       return "RANDOM_CALL_REQ";
        case MessageType::RANDOM_CALL_RESP:      return "RANDOM_CALL_RESP";
        case MessageType::START_SCREEN_SHARE_REQ: return "START_SCREEN_SHARE_REQ";
        case MessageType::START_SCREEN_SHARE_RESP:return "START_SCREEN_SHARE_RESP";
        case MessageType::STOP_SCREEN_SHARE_REQ:  return "STOP_SCREEN_SHARE_REQ";
        case MessageType::STOP_SCREEN_SHARE_RESP: return "STOP_SCREEN_SHARE_RESP";
        case MessageType::SCREEN_FRAME:           return "SCREEN_FRAME";
        case MessageType::START_AUDIO_REQ:        return "START_AUDIO_REQ";
        case MessageType::START_AUDIO_RESP:       return "START_AUDIO_RESP";
        case MessageType::STOP_AUDIO_REQ:         return "STOP_AUDIO_REQ";
        case MessageType::STOP_AUDIO_RESP:        return "STOP_AUDIO_RESP";
        case MessageType::AUDIO_FRAME:            return "AUDIO_FRAME";
        case MessageType::GRANT_MIC_REQ:          return "GRANT_MIC_REQ";
        case MessageType::GRANT_MIC_RESP:         return "GRANT_MIC_RESP";
        case MessageType::REVOKE_MIC_REQ:         return "REVOKE_MIC_REQ";
        case MessageType::REVOKE_MIC_RESP:        return "REVOKE_MIC_RESP";
        case MessageType::NOTIFY_CHECKIN_OPENED:return "NOTIFY_CHECKIN_OPENED";
        case MessageType::NOTIFY_QUESTION_PUBLISHED: return "NOTIFY_QUESTION_PUBLISHED";
        case MessageType::NOTIFY_RANDOM_CALLED:  return "NOTIFY_RANDOM_CALLED";
        case MessageType::NOTIFY_SCREEN_SHARE_STARTED: return "NOTIFY_SCREEN_SHARE_STARTED";
        case MessageType::NOTIFY_SCREEN_SHARE_STOPPED: return "NOTIFY_SCREEN_SHARE_STOPPED";
        case MessageType::NOTIFY_SCREEN_FRAME:    return "NOTIFY_SCREEN_FRAME";
        case MessageType::NOTIFY_AUDIO_STARTED:   return "NOTIFY_AUDIO_STARTED";
        case MessageType::NOTIFY_AUDIO_STOPPED:   return "NOTIFY_AUDIO_STOPPED";
        case MessageType::NOTIFY_AUDIO_FRAME:     return "NOTIFY_AUDIO_FRAME";
        case MessageType::NOTIFY_MIC_GRANTED:     return "NOTIFY_MIC_GRANTED";
        case MessageType::NOTIFY_MIC_REVOKED:     return "NOTIFY_MIC_REVOKED";
        case MessageType::ATTENTION_REPORT:       return "ATTENTION_REPORT";
        case MessageType::NOTIFY_PRESENCE_CHECK:  return "NOTIFY_PRESENCE_CHECK";
        case MessageType::PRESENCE_CHECK_RESP:    return "PRESENCE_CHECK_RESP";
        case MessageType::ATTENTION_STATUS_REQ:   return "ATTENTION_STATUS_REQ";
        case MessageType::ATTENTION_STATUS_RESP:  return "ATTENTION_STATUS_RESP";
        default:                                return "UNKNOWN";
    }
}

inline MessageType message_type_from_string(const std::string& s) {
    if (s == "LOGIN_REQ")            return MessageType::LOGIN_REQ;
    if (s == "LOGIN_RESP")           return MessageType::LOGIN_RESP;
    if (s == "LOGOUT_REQ")           return MessageType::LOGOUT_REQ;
    if (s == "LOGOUT_RESP")          return MessageType::LOGOUT_RESP;
    if (s == "CREATE_USER_REQ")      return MessageType::CREATE_USER_REQ;
    if (s == "CREATE_USER_RESP")     return MessageType::CREATE_USER_RESP;
    if (s == "LIST_USERS_REQ")       return MessageType::LIST_USERS_REQ;
    if (s == "LIST_USERS_RESP")      return MessageType::LIST_USERS_RESP;
    if (s == "CREATE_CLASS_REQ")     return MessageType::CREATE_CLASS_REQ;
    if (s == "CREATE_CLASS_RESP")    return MessageType::CREATE_CLASS_RESP;
    if (s == "START_CLASS_REQ")      return MessageType::START_CLASS_REQ;
    if (s == "START_CLASS_RESP")     return MessageType::START_CLASS_RESP;
    if (s == "END_CLASS_REQ")        return MessageType::END_CLASS_REQ;
    if (s == "END_CLASS_RESP")       return MessageType::END_CLASS_RESP;
    if (s == "JOIN_CLASS_REQ")       return MessageType::JOIN_CLASS_REQ;
    if (s == "JOIN_CLASS_RESP")      return MessageType::JOIN_CLASS_RESP;
    if (s == "LEAVE_CLASS_REQ")      return MessageType::LEAVE_CLASS_REQ;
    if (s == "LEAVE_CLASS_RESP")     return MessageType::LEAVE_CLASS_RESP;
    if (s == "LIST_CLASSES_REQ")     return MessageType::LIST_CLASSES_REQ;
    if (s == "LIST_CLASSES_RESP")    return MessageType::LIST_CLASSES_RESP;
    if (s == "CLASS_MEMBERS_REQ")    return MessageType::CLASS_MEMBERS_REQ;
    if (s == "CLASS_MEMBERS_RESP")   return MessageType::CLASS_MEMBERS_RESP;
    if (s == "OPEN_CHECKIN_REQ")     return MessageType::OPEN_CHECKIN_REQ;
    if (s == "OPEN_CHECKIN_RESP")    return MessageType::OPEN_CHECKIN_RESP;
    if (s == "CHECKIN_REQ")          return MessageType::CHECKIN_REQ;
    if (s == "CHECKIN_RESP")         return MessageType::CHECKIN_RESP;
    if (s == "CHECKIN_STATUS_REQ")   return MessageType::CHECKIN_STATUS_REQ;
    if (s == "CHECKIN_STATUS_RESP")  return MessageType::CHECKIN_STATUS_RESP;
    if (s == "PUBLISH_QUESTION_REQ")  return MessageType::PUBLISH_QUESTION_REQ;
    if (s == "PUBLISH_QUESTION_RESP") return MessageType::PUBLISH_QUESTION_RESP;
    if (s == "GET_QUESTION_REQ")     return MessageType::GET_QUESTION_REQ;
    if (s == "GET_QUESTION_RESP")    return MessageType::GET_QUESTION_RESP;
    if (s == "SUBMIT_ANSWER_REQ")    return MessageType::SUBMIT_ANSWER_REQ;
    if (s == "SUBMIT_ANSWER_RESP")   return MessageType::SUBMIT_ANSWER_RESP;
    if (s == "GET_ANSWERS_REQ")      return MessageType::GET_ANSWERS_REQ;
    if (s == "GET_ANSWERS_RESP")     return MessageType::GET_ANSWERS_RESP;
    if (s == "CLASS_SUMMARY_REQ")     return MessageType::CLASS_SUMMARY_REQ;
    if (s == "CLASS_SUMMARY_RESP")    return MessageType::CLASS_SUMMARY_RESP;
    if (s == "EXPORT_CLASS_REQ")      return MessageType::EXPORT_CLASS_REQ;
    if (s == "EXPORT_CLASS_RESP")     return MessageType::EXPORT_CLASS_RESP;
    if (s == "RANDOM_CALL_REQ")      return MessageType::RANDOM_CALL_REQ;
    if (s == "RANDOM_CALL_RESP")     return MessageType::RANDOM_CALL_RESP;
    if (s == "START_SCREEN_SHARE_REQ") return MessageType::START_SCREEN_SHARE_REQ;
    if (s == "START_SCREEN_SHARE_RESP")return MessageType::START_SCREEN_SHARE_RESP;
    if (s == "STOP_SCREEN_SHARE_REQ")  return MessageType::STOP_SCREEN_SHARE_REQ;
    if (s == "STOP_SCREEN_SHARE_RESP") return MessageType::STOP_SCREEN_SHARE_RESP;
    if (s == "SCREEN_FRAME")           return MessageType::SCREEN_FRAME;
    if (s == "START_AUDIO_REQ")       return MessageType::START_AUDIO_REQ;
    if (s == "START_AUDIO_RESP")      return MessageType::START_AUDIO_RESP;
    if (s == "STOP_AUDIO_REQ")        return MessageType::STOP_AUDIO_REQ;
    if (s == "STOP_AUDIO_RESP")       return MessageType::STOP_AUDIO_RESP;
    if (s == "AUDIO_FRAME")           return MessageType::AUDIO_FRAME;
    if (s == "GRANT_MIC_REQ")         return MessageType::GRANT_MIC_REQ;
    if (s == "GRANT_MIC_RESP")        return MessageType::GRANT_MIC_RESP;
    if (s == "REVOKE_MIC_REQ")        return MessageType::REVOKE_MIC_REQ;
    if (s == "REVOKE_MIC_RESP")       return MessageType::REVOKE_MIC_RESP;
    if (s == "NOTIFY_CHECKIN_OPENED")return MessageType::NOTIFY_CHECKIN_OPENED;
    if (s == "NOTIFY_QUESTION_PUBLISHED") return MessageType::NOTIFY_QUESTION_PUBLISHED;
    if (s == "NOTIFY_RANDOM_CALLED") return MessageType::NOTIFY_RANDOM_CALLED;
    if (s == "NOTIFY_SCREEN_SHARE_STARTED") return MessageType::NOTIFY_SCREEN_SHARE_STARTED;
    if (s == "NOTIFY_SCREEN_SHARE_STOPPED") return MessageType::NOTIFY_SCREEN_SHARE_STOPPED;
    if (s == "NOTIFY_SCREEN_FRAME")  return MessageType::NOTIFY_SCREEN_FRAME;
    if (s == "NOTIFY_AUDIO_STARTED") return MessageType::NOTIFY_AUDIO_STARTED;
    if (s == "NOTIFY_AUDIO_STOPPED") return MessageType::NOTIFY_AUDIO_STOPPED;
    if (s == "NOTIFY_AUDIO_FRAME")   return MessageType::NOTIFY_AUDIO_FRAME;
    if (s == "NOTIFY_MIC_GRANTED")   return MessageType::NOTIFY_MIC_GRANTED;
    if (s == "NOTIFY_MIC_REVOKED")   return MessageType::NOTIFY_MIC_REVOKED;
    if (s == "ATTENTION_REPORT")     return MessageType::ATTENTION_REPORT;
    if (s == "NOTIFY_PRESENCE_CHECK") return MessageType::NOTIFY_PRESENCE_CHECK;
    if (s == "PRESENCE_CHECK_RESP")  return MessageType::PRESENCE_CHECK_RESP;
    if (s == "ATTENTION_STATUS_REQ") return MessageType::ATTENTION_STATUS_REQ;
    if (s == "ATTENTION_STATUS_RESP") return MessageType::ATTENTION_STATUS_RESP;
    return MessageType::UNKNOWN;
}
