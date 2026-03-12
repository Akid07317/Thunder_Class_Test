# Architecture

## Overview

Thunder Class is a C/S (client-server) online teaching system. Communication uses TCP sockets with a custom JSON-based message protocol. The server is authoritative — all state changes go through the server.

## Directory Structure

```
THU_CPP/
├── client/          # Qt-based GUI client
│   ├── src/         # Client-side logic (networking, controllers)
│   └── ui/          # Qt UI forms / widgets
├── server/          # Console server application
│   └── src/         # Server logic (session, routing, persistence)
├── common/          # Shared between client and server
│   ├── protocol/    # Message types, serialization, constants
│   └── model/       # Shared data structures (User, Class, Question…)
├── docs/            # Project documentation
└── third_party/     # Vendored lightweight libraries (if any)
```

## Three-Layer Design

### Layer 1 — Common (`common/`)

Defines everything shared between client and server:

| Component | Responsibility |
|-----------|---------------|
| `MessageType` enum | All request/response types (LOGIN_REQ, LOGIN_RESP, …) |
| `Message` struct | Header (type, length) + JSON body |
| `User` | userId, name, role (ADMIN / TEACHER / STUDENT) |
| `ClassInfo` | classId, name, teacherId, status (WAITING / ACTIVE / ENDED) |
| `ClassEvent` | classId, eventType, userId, userName, timestamp |
| `Question` | questionId, classId, content, options[], correctAnswer, timestamp |
| `AnswerRecord` | questionId, userId, userName, answer, timestamp |
| `AttendanceRecord` | studentId, classId, checkInTime, duration |

### Layer 2 — Server (`server/`)

Single-process, single-threaded event loop (select/poll). No framework required.

| Component | Responsibility |
|-----------|---------------|
| `TcpServer` | Accept connections, read/write messages |
| `SessionManager` | Map connections to authenticated users |
| `ClassManager` | Create / start / end classes; membership, check-in, random call, screen sharing state |
| `QuestionManager` | Publish questions, collect answers, grade |
| `StatisticsExporter` | Build class summaries from events/questions/answers; export JSON & CSV |
| `UserStore` | Load/save user credentials (file-based, MVP) |

### Layer 3 — Client (`client/`)

Qt Widgets application. One window, multiple pages (stacked widget or similar).

| Component | Responsibility |
|-----------|---------------|
| `TcpClient` | Connect to server, send/receive messages |
| `LoginPage` | Username + password input |
| `ClassListPage` | Show available / active classes |
| `ClassroomPage` | Active class view (attendance, questions, stats) |
| `AdminPage` | User management (create accounts, assign roles) |
| `ClientController` | Dispatch server responses to UI updates |

## Communication Protocol (MVP)

All messages are length-prefixed JSON over TCP:

```
[4 bytes: body length][JSON body]
```

JSON body always contains a `"type"` field matching `MessageType`.

Example — login:
```json
// Request
{"type": "LOGIN_REQ", "username": "alice", "password": "123456"}

// Response
{"type": "LOGIN_RESP", "success": true, "userId": 1, "role": "STUDENT", "name": "Alice"}
```

## Persistence (MVP)

File-based storage using plain JSON files:
- `data/users.json` — user accounts
- `data/classes.json` — class records (classId, name, teacherId, status)
- `data/events.json` — class session event log (create, start, join, leave, check-in, end)
- `data/questions.json` — published questions (questionId, classId, content, options, correctAnswer)
- `data/answers.json` — student answer records (questionId, userId, answer, timestamp)

No database dependency for MVP. Can upgrade to SQLite in phase 2 if needed.

## Screen Sharing (Simplified)

Implementation uses the "periodic screenshot + image frame transfer" fallback from CLAUDE.md:

```
Teacher Client                Server                  Student Client
  |                            |                          |
  |--START_SCREEN_SHARE_REQ--->|                          |
  |<--START_SCREEN_SHARE_RESP--|                          |
  |                            |--NOTIFY_SHARE_STARTED--->|
  |                            |                          |
  |----SCREEN_FRAME----------->|                          |
  |  (base64 JPEG + seq)       |--NOTIFY_SCREEN_FRAME---->|
  |                            |  (broadcast to all)      | (decode + display)
  |----SCREEN_FRAME----------->|                          |
  |                            |--NOTIFY_SCREEN_FRAME---->|
  |                            |                          |
  |--STOP_SCREEN_SHARE_REQ---->|                          |
  |<--STOP_SCREEN_SHARE_RESP---|                          |
  |                            |--NOTIFY_SHARE_STOPPED--->|
```

- Frame limit: 4 MB (protocol.cpp). Compressed JPEG screenshots at ~50-150 KB fit well.
- Server is a pure router — no image processing, no storage.
- SCREEN_FRAME is fire-and-forget (no response) for throughput.
- Sharing auto-stops when class ends.
- Limitations: ~1-2 fps, not suitable for video playback. Adequate for slides and documents.

## Audio Broadcast & Student Mic (Simplified)

Implementation uses "one-way broadcast + called student goes on mic" fallback from CLAUDE.md:

```
Teacher Client                Server                  Student Client
  |                            |                          |
  |----START_AUDIO_REQ-------->|                          |
  |<---START_AUDIO_RESP--------|                          |
  |                            |--NOTIFY_AUDIO_STARTED--->|
  |                            |                          |
  |----AUDIO_FRAME------------>|                          |
  |  (base64 audio chunk)      |--NOTIFY_AUDIO_FRAME----->|  (all students)
  |                            |                          |
  |----GRANT_MIC_REQ---------->|                          |
  |  (studentId)               |--NOTIFY_MIC_GRANTED----->|  (to student)
  |                            |                          |
  |                            |<---AUDIO_FRAME-----------|  (mic holder only)
  |<---NOTIFY_AUDIO_FRAME------|  (to teacher + others)   |
  |                            |                          |
  |----REVOKE_MIC_REQ--------->|                          |
  |                            |--NOTIFY_MIC_REVOKED----->|
  |                            |                          |
  |----STOP_AUDIO_REQ--------->|                          |
  |<---STOP_AUDIO_RESP---------|                          |
  |                            |--NOTIFY_AUDIO_STOPPED--->|
```

- Same chunked transfer pattern as screen sharing — base64-encoded audio in JSON.
- Server is a pure router — no audio processing.
- Sender is excluded from receiving their own frames.
- Only one student can hold the mic at a time. Teacher controls grant/revoke.
- Audio and mic auto-stop when class ends.
- Client UI: teacher has Start/Stop Audio, Grant/Revoke Mic buttons; student sees audio status + mic notifications.
- Audio capture/playback: conditional on Qt6 Multimedia (`find_package OPTIONAL_COMPONENTS`).
  - If available: `QAudioSource` (16kHz mono Int16) captures, `QAudioSink` plays back. 200ms chunk interval.
  - If unavailable: UI controls still work (protocol flow demonstrable), status shows "no Qt Multimedia".
- Device handling: uses `QMediaDevices::defaultAudioInput()/Output()`. Null device gracefully handled.
- Limitations: not real-time VoIP. Latency depends on chunk size (~200ms + network). Adequate for demo.

## Phase 2 Features (Remaining)

- Advanced attention tracking
- SQLite persistence

## Design Decisions

1. **Why JSON over TCP instead of HTTP?** Simpler to implement from scratch in C++; no need for an HTTP library. Demonstrates socket programming knowledge.
2. **Why file-based persistence?** Minimal dependency. Sufficient for demo scale (< 50 users).
3. **Why single-threaded server?** Adequate for course demo load. Avoids concurrency bugs. Can use select/poll for multiplexing.
4. **Why Qt?** Standard choice for C++ GUI in academic settings. Well-documented. Provides cross-platform networking as a bonus (but we implement raw sockets in common/ to demonstrate understanding).
