# Thunder Class - Design Summary

## System Overview

```
┌─────────────────────────────────────────────────────────┐
│                    Thunder Class                         │
│                                                         │
│  ┌──────────┐    TCP/JSON     ┌───────────────────┐    │
│  │ Qt Client ├───────────────►│  Server (select)   │    │
│  │  (GUI)    │◄───────────────┤  Single-threaded   │    │
│  └──────────┘                 └─────────┬─────────┘    │
│                                         │               │
│  ┌──────────┐    TCP/JSON     ┌─────────▼─────────┐    │
│  │ Qt Client ├───────────────►│  data/*.json       │    │
│  │  (GUI)    │◄───────────────┤  (persistence)     │    │
│  └──────────┘                 └───────────────────┘    │
│       ...                                               │
└─────────────────────────────────────────────────────────┘
```

## Module Map

```
┌─────────────────────────────────────────────────────┐
│ Module 1: Login & Permissions                        │
│   UserStore ─── SessionManager ─── Role checks       │
├─────────────────────────────────────────────────────┤
│ Module 2: Class Lifecycle                            │
│   WAITING ──► ACTIVE ──► ENDED                       │
│   Create / Start / Join / Leave / End                │
├─────────────────────────────────────────────────────┤
│ Module 3: Check-in        │ Module 4: Random Call    │
│   Teacher opens           │   Teacher triggers       │
│   Students respond        │   Server picks randomly  │
│   Server records          │   Student notified       │
├───────────────────────────┼─────────────────────────┤
│ Module 5: Q&A             │ Module 6: Statistics     │
│   Publish question        │   Event-based duration   │
│   Students answer         │   Per-student summary    │
│   View correct rate       │   CSV export             │
├───────────────────────────┴─────────────────────────┤
│ Module 7: Screen Sharing  │ Module 8: Audio          │
│   Screenshot → base64     │   PCM → base64 chunks    │
│   Server routes frames    │   Mic grant/revoke       │
│   (server complete)       │   (server + client UI)   │
└─────────────────────────────────────────────────────┘
```

## Class Responsibilities

### Server Side (6 classes)

| Class | Single Responsibility |
|-------|----------------------|
| `TcpServer` | TCP accept, select() loop, read/write with framing |
| `UserStore` | User CRUD + file persistence |
| `SessionManager` | fd ↔ User mapping, login/logout |
| `ClassManager` | Class state machine, membership, check-in, random call, screen/audio state |
| `QuestionManager` | Question publishing, answer collection, one active per class |
| `StatisticsExporter` | Compute summaries from events + answers, format JSON/CSV |

### Client Side (6 classes)

| Class | Single Responsibility |
|-------|----------------------|
| `TcpClient` | QTcpSocket wrapper, protocol framing bridge |
| `MainWindow` | Page routing by role (QStackedWidget) |
| `LoginPage` | Authentication form |
| `AdminPage` | User management |
| `TeacherPage` | Class control, classroom actions, audio controls |
| `StudentPage` | Class browsing, classroom participation, notifications |

### Common (shared by both)

| Component | Purpose |
|-----------|---------|
| `MessageType` | 96 enum values for all request/response/notification types |
| `Protocol` | `frame()` and `extractFrame()` — length-prefixed JSON |
| `User` / `ClassInfo` / `ClassEvent` / `Question` / `AnswerRecord` | Shared data models with JSON serialization |

## Communication Pattern

```
Client                    Server                    Client
  │                         │                         │
  │──── REQUEST ──────────►│                         │
  │◄─── RESPONSE ──────────│                         │
  │                         │──── NOTIFICATION ─────►│
  │                         │       (push)            │
```

Every feature follows this pattern. Server is authoritative — clients never modify state directly.

## Permission Model

```
ADMIN ──── CreateUser, ListUsers
TEACHER ── CreateClass, StartClass, EndClass, OpenCheckin,
           PublishQuestion, RandomCall, StartAudio, GrantMic,
           ViewSummary, ExportCSV
STUDENT ── JoinClass, LeaveClass, Checkin, SubmitAnswer
ALL ────── Login, Logout, ListClasses, ClassMembers
```

Every handler checks: (1) authenticated? (2) correct role? (3) owns this class?

## Data Flow Example: Question & Answer

```
Teacher                     Server                      Student
   │                          │                            │
   │─PUBLISH_QUESTION_REQ───►│                            │
   │  {classId, content,      │                            │
   │   options, correctAnswer} │                            │
   │                          │──NOTIFY_QUESTION_PUBLISHED─►│
   │◄─PUBLISH_QUESTION_RESP──│  {question (no answer)}     │
   │  {questionId}            │                            │
   │                          │                            │
   │                          │◄─SUBMIT_ANSWER_REQ─────────│
   │                          │  {questionId, answer}       │
   │                          │──SUBMIT_ANSWER_RESP────────►│
   │                          │                            │
   │─GET_ANSWERS_REQ────────►│                            │
   │◄─GET_ANSWERS_RESP───────│                            │
   │  {answers[], correctCount}                            │
```

## Persistence

```
data/
  users.json       ── [{userId, username, password, name, role}]
  classes.json     ── [{classId, name, teacherId, teacherName, status}]
  events.json      ── [{classId, eventType, userId, userName, timestamp}]
  questions.json   ── [{questionId, classId, content, options, correctAnswer}]
  answers.json     ── [{questionId, userId, userName, answer, timestamp}]
```

All saved on every write. Loaded on server startup. No database dependency.

## Metrics

| Metric | Value |
|--------|-------|
| Source files | 41 |
| Lines of code | ~6300 |
| Message types | 96 |
| Test suites | 7 |
| Test cases | 157 |
| Functional modules | 8 |
| Third-party deps | 1 (nlohmann/json, header-only) |
| Server framework | None (raw sockets + select) |
| Client framework | Qt6 Widgets + Network |
