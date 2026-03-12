# Thunder Class - Course Defense Script

## Opening (1 min)

Thunder Class is a C/S online teaching system built in C++17 with Qt. It covers user management, class lifecycle, real-time interaction (check-in, random call, Q&A), statistics export, and simplified audio/screen sharing.

**Scale:** ~6500 lines of source code, 42 source files, 8 automated test suites (214 test cases), 8 functional modules + integration hardening + edge case testing.

---

## Part 1 - Architecture Overview (3 min)

### Three-layer design

```
common/          Shared protocol + data models (both sides link to this)
  protocol/      MessageType enum, length-prefixed JSON framing
  model/         User, ClassInfo, ClassEvent, Question, AnswerRecord

server/          Single-process, select()-based TCP server
  TcpServer      Accept, read/write with per-fd buffers
  SessionManager fd-to-user mapping, duplicate login prevention
  ClassManager   Class lifecycle, membership, check-in, random call, screen/audio state
  QuestionManager  Publish questions, collect answers, one active per class
  StatisticsExporter  Build summaries from events + answers, export JSON/CSV
  UserStore      File-based user persistence

client/          Qt Widgets application
  TcpClient      QTcpSocket wrapper, bridges to Protocol::frame/extractFrame
  MainWindow     QStackedWidget routing by role
  LoginPage / AdminPage / TeacherPage / StudentPage
```

### Key design decisions (anticipate questions)

| Decision | Why |
|----------|-----|
| JSON over TCP, not HTTP | Simpler from-scratch implementation; demonstrates socket programming |
| Length-prefixed framing | `[4 bytes: body length][JSON body]` — clean, no HTTP dependency |
| Single-threaded server | Adequate for demo scale; avoids concurrency bugs; uses select() for multiplexing |
| File-based persistence | No database dependency; JSON files for users, classes, events, questions, answers |
| Server-authoritative | All state changes go through server; client is presentation + request initiation |
| Qt for GUI | Standard C++ GUI in academic settings; cross-platform; well-documented |

### Protocol example

```
Client -> Server:  {"type": "LOGIN_REQ", "username": "alice", "password": "123456"}
Server -> Client:  {"type": "LOGIN_RESP", "success": true, "userId": 1, "role": "STUDENT"}
```

All 96 message types follow this request/response + server-push notification pattern.

---

## Part 2 - Live Demo (9-11 min)

### Preparation

Open 3 terminals:
```
Terminal 1: cd THU_CPP && rm -rf data && ./build/server/thunder_server
Terminal 2: ./build/client/thunder_client
Terminal 3: ./build/client/thunder_client
```

### Demo Script

#### Step 1: Login & User Management (1 min)
1. **Terminal 2**: Log in as `admin` / `admin123` -> Show admin page
2. Create teacher: `teacher1` / `pass` / `Teacher Wang` / TEACHER
3. Create student: `student1` / `pass` / `Alice` / STUDENT
4. Show user list -> 3 users with correct roles

> **Point out:** Role-based UI — admin sees user management, teacher sees class management, student sees class list.

#### Step 2: Class Lifecycle (1 min)
5. **Terminal 2**: Log out, log in as `teacher1`
6. Create class "C++ Lecture 1"
7. Click Start Class -> status becomes ACTIVE
8. **Terminal 3**: Log in as `student1`, Refresh, see the class
9. Student joins class -> "Joined class"
10. Both enter classroom

> **Point out:** Only ACTIVE classes can be joined. Only the owning teacher can start/end.

#### Step 3: Check-in (1 min)
11. Teacher sets deadline (e.g. 60s), clicks "Open Check-in"
12. Student gets popup notification with deadline -> clicks "Check In"
13. Teacher clicks "Check-in Status" -> shows Alice: on_time

> **Point out:** Server records check-in with deadline. Status is "on_time", "late", or "absent" based on whether student checked in within the deadline. Statistics and CSV export show the same three states.

#### Step 4: Random Calling (30 sec)
14. Teacher clicks "Random Call"
15. Teacher sees popup "Selected student: Alice"
16. Student sees popup "You have been randomly called!"

> **Point out:** Server uses std::mt19937 for random selection, logs event.

#### Step 5: Question & Answer (1.5 min)
17. Teacher fills in question: "What is 2+2?", options A:3 B:4 C:5 D:6, correct: B
18. Click Publish -> student sees question with radio buttons
19. Student selects B, clicks Submit -> "Answer submitted"
20. Teacher clicks "Get Answers" -> shows 1 total, 1 correct

> **Point out:** correctAnswer is hidden from students (toStudentJson omits it). One answer per student per question enforced server-side.

#### Step 6: Screen Sharing (1 min)
21. Teacher clicks "Start Sharing" -> student sees "Screen sharing started"
22. Student sees teacher's screen (JPEG frames at ~1fps) in the display area
23. Teacher clicks "Stop Sharing" -> student sees "Screen sharing stopped"

> **Point out:** QScreen::grabWindow captures desktop, JPEG compressed at quality 50, scaled to 800px max width, base64-encoded via JSON protocol. ~1fps is adequate for slides/code review.

#### Step 7: Audio (1 min)
24. Teacher clicks "Start Audio" -> student sees "[Audio] Teacher started audio broadcast"
25. Teacher selects Alice in member list, clicks "Grant Mic" -> student gets popup
26. Teacher clicks "Revoke Mic" -> student sees notification
27. Teacher clicks "Stop Audio"

> **Point out:** Server is a pure router. Audio would work with Qt Multimedia installed (conditional compilation). UI demonstrates complete protocol flow.

#### Step 8: Statistics & Export (1 min)
28. Teacher clicks "View Summary" -> shows class stats, per-student detail
29. Teacher clicks "Export CSV" -> save dialog, CSV with metadata + student table

> **Point out:** Duration calculated from STUDENT_JOINED/LEFT event pairs. CLASS_ENDED auto-closes open sessions.

#### Step 9: End Class (30 sec)
30. Teacher goes back to list, clicks "End Class" -> status ENDED
31. Show server terminal log — all events logged

> **Point out:** endClass clears screen sharing, audio, mic holder, and membership state. Events persisted to events.json.

---

## Part 3 - Testing (2 min)

**8 automated test suites, 214 test cases total:**

| Suite | Tests | Covers |
|-------|-------|--------|
| test_login_client | 9 | Auth, roles, permissions |
| test_class_client | 23 | Create/start/end, join/leave, check-in |
| test_question_client | 21 | Publish, submit, answers, permissions |
| test_stats_client | 29 | Duration, summary, CSV, permissions |
| test_random_call_client | 17 | Random selection, permissions, edge cases |
| test_screen_share_client | 23 | Start/stop, frame routing, auto-stop |
| test_audio_client | 35 | Broadcast, mic grant/revoke, permissions |
| test_edge_cases | 57 | Disconnect/reconnect, multi-class, cross-feature, rapid ops, boundaries |

Run all:
```bash
rm -rf data && ./build/server/thunder_server &
sleep 1
for t in build/tests/test_*; do $t 2>&1 | tail -1; done
```

All 214 pass with 0 failures.

Each test suite is a standalone console client that connects via raw TCP, exercises the full protocol, and verifies responses. No test framework dependency — just assert-style checks.

The edge case suite (57 tests) covers: student disconnect/reconnect during class, multiple simultaneous classes with cross-permission checks, all features running simultaneously (screen share + audio + questions + check-in + random call), rapid concurrent operations from 3 clients, teacher disconnect recovery, duplicate login handling, and boundary values (empty names, nonexistent IDs, out-of-bounds answers).

---

## Part 4 - Limitations & Honest Assessment (1 min)

| Area | Limitation | Mitigation |
|------|-----------|------------|
| Screen sharing | ~1fps periodic screenshots, not real-time video | Adequate for slides/code review; base64 keeps protocol uniform |
| Audio | Real capture/playback needs Qt Multimedia (not installed) | UI controls work; protocol flow fully verified |
| Persistence | File-based JSON, no transactions | Adequate for demo scale (<50 users) |
| Security | Plaintext passwords, no TLS | Course project scope, not production |
| Scalability | Single-threaded, select()-based | Sufficient for demo; select() demonstrates understanding |
| Late check-in | Deadline-based (default 60s) | On-time/late/absent distinguished in status, stats, and CSV |

---

## Anticipated Questions & Answers

**Q: Why not use a database?**
A: File-based JSON persistence keeps dependencies minimal. All 5 data files (users, classes, events, questions, answers) survive restarts. For demo scale this is sufficient. Documented upgrade path to SQLite.

**Q: How does the server handle multiple clients?**
A: Single-threaded event loop using select(). Per-connection receive buffers handle partial reads. Length-prefixed framing ensures message boundaries. Adequate for <50 concurrent connections.

**Q: How is permission control implemented?**
A: Every request handler checks `sessionMgr.getUser(fd)` for authentication, then `caller->role` for authorization. Class ownership is enforced by comparing `caller->userId == classInfo.teacherId`. No operation bypasses these checks.

**Q: What happens if a student disconnects mid-class?**
A: `TcpServer::closeClient` triggers `onDisconnect`, which calls `classMgr.removeFromAll(userId, userName)`. This logs a STUDENT_LEFT event and removes the student from all active classes. Duration calculation uses this event.

**Q: How does the protocol handle large messages (screen/audio frames)?**
A: Protocol has a 4MB frame limit (protocol.cpp). Screen frames are JPEG-compressed (~50-150KB). Audio chunks are ~6KB per 200ms at 16kHz mono. Base64 adds ~33% overhead but keeps the JSON protocol uniform.

**Q: Could this scale to a real classroom?**
A: The architecture separates concerns cleanly — TcpServer handles I/O, managers handle logic, protocol is uniform. To scale: replace select() with epoll/kqueue, add thread pool for message handlers, swap JSON files for SQLite. The module boundaries wouldn't change.
