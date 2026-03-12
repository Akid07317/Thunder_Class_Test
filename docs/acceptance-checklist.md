# Acceptance Checklist

Each item must be demonstrable end-to-end in the final defense.

## Module 1 — Login & Permission System

- [x] Admin can create user accounts (username, password, role)
- [x] User can log in with username and password
- [x] Server rejects invalid credentials
- [x] Role-based access: admin / teacher / student see different UI
- [x] Duplicate login is rejected or handled

## Module 2 — Class Lifecycle

- [x] Teacher can create a new class (with name)
- [x] Teacher can start a class → status becomes ACTIVE
- [x] Teacher can end a class → status becomes ENDED
- [x] Student can see available classes and join an active class
- [x] Student can leave a class
- [x] Server tracks which students are in each active class
- [x] Only the teacher who created a class can start/end it

## Module 3 — Attendance Check-in

- [x] Teacher can initiate attendance check-in for an active class
- [x] Students in the class receive check-in prompt
- [x] Student can confirm check-in
- [x] Server records check-in time
- [x] Late or missing check-in is distinguishable

## Module 4 — Random Calling

- [x] Teacher can trigger random call in an active class
- [x] Server randomly selects a student from the class roster
- [x] Selected student is notified via NOTIFY_RANDOM_CALLED
- [x] Result (userId, name) is returned to the teacher
- [x] Student cannot trigger random call
- [x] Non-owner teacher cannot trigger random call
- [x] Random call fails on ENDED or nonexistent class
- [x] Random call fails on empty class (no students)

## Module 5 — Question Publishing & Answering

- [x] Teacher can publish a question (text + options + correct answer)
- [x] Students in the class receive the question
- [x] Student can submit an answer
- [x] Server records answers with timestamps
- [x] Teacher can view answer statistics (count per option, correct rate)

## Module 6 — Statistics & Export

- [x] Server records STUDENT_JOINED and STUDENT_LEFT events with timestamps
- [x] Online duration is calculated per student per class session (join/leave pairing)
- [x] CLASS_ENDED auto-closes open sessions for duration calculation
- [x] Teacher can view class summary (attendance, check-in, questions, per-student stats)
- [x] Summary includes: class name, teacher, start/end time, total students, check-in count, question count, per-student detail
- [x] Per-student detail includes: checked-in status, online duration (seconds), questions answered, correct answers
- [x] Only the owning teacher can view summary or export
- [x] Student cannot view summary or export
- [x] Non-owner teacher cannot view summary or export
- [x] Teacher can export class record as CSV (returned inline)
- [x] CSV includes class metadata section and student detail table
- [x] Attendance records (events.json) persisted across server restarts
- [x] Answer records (answers.json) persisted across server restarts
- [x] Question records (questions.json) persisted across server restarts

## Module 7 — Simplified Screen Sharing (Phase 2 Extension)

- [x] Teacher can start screen sharing for an active owned class
- [x] Teacher can stop screen sharing
- [x] Students receive NOTIFY_SCREEN_SHARE_STARTED / STOPPED notifications
- [x] Teacher sends periodic screen frames (base64-encoded JPEG via SCREEN_FRAME)
- [x] Server routes frames to all students in the class (NOTIFY_SCREEN_FRAME)
- [x] Student cannot start sharing or send frames
- [x] Non-owner teacher cannot start sharing
- [x] Duplicate start / duplicate stop handled correctly
- [x] Sharing on non-ACTIVE class rejected
- [x] Sharing auto-stops when class ends
- [x] Frames not routed after sharing stops
- [x] Qt client: teacher screenshot capture + periodic send
- [x] Qt client: student frame display widget

## Module 8 — Audio Broadcast & Student Mic (Phase 2 Extension)

- [x] Teacher can start audio broadcast for an active owned class
- [x] Teacher can stop audio broadcast
- [x] Students receive NOTIFY_AUDIO_STARTED / STOPPED notifications
- [x] Teacher sends audio frames (base64-encoded chunks via AUDIO_FRAME)
- [x] Server routes teacher frames to all students (NOTIFY_AUDIO_FRAME, excluding sender)
- [x] Student without mic cannot send audio frames
- [x] Teacher can grant mic to a specific student (GRANT_MIC_REQ)
- [x] Granted student receives NOTIFY_MIC_GRANTED
- [x] Granted student can send audio frames — routed to teacher + other students
- [x] Teacher can revoke mic (REVOKE_MIC_REQ)
- [x] Student receives NOTIFY_MIC_REVOKED
- [x] Revoked student can no longer send audio frames
- [x] Cannot grant mic when audio is not active
- [x] Duplicate start / stop / revoke handled correctly
- [x] Audio on non-ACTIVE class rejected
- [x] Audio auto-stops (including mic) when class ends
- [x] Non-owner teacher cannot start audio
- [x] Frames not routed after audio stops
- [x] Qt client: audio control UI (start/stop, grant/revoke mic buttons + status)
- [x] Qt client: audio capture + playback (conditional on Qt6 Multimedia; graceful fallback)
- [ ] Audio device switching (deferred — uses default device only)

## Module 9 — Integration & Client-Server Consistency

- [x] Server error messages display correctly in all client pages (field: "message")
- [x] Class list shows correct class names on both teacher and student pages (field: "name")
- [x] Question notification displays content and options on student page (nested under "question")
- [x] Student can submit answers with correct questionId
- [x] Teacher can view answers with correct questionId
- [x] Random call result shows student name and ID correctly (fields: "name", "userId")
- [x] Check-in status display shows all students with checked/not checked (field: "students")
- [x] Class summary shows correct question count and online duration (fields: "totalQuestions", "durationSeconds")
- [x] Server validates answer index is within option bounds
- [x] 214/214 tests pass across 8 suites (including 57 edge case tests) with 0 regressions

## Phase 2 (Remaining)

- [ ] Audio device switching (deferred — uses default device only)
- [x] Late or missing check-in distinction
- [ ] Advanced attention tracking
