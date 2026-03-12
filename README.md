# Thunder Class / 雷课堂

An online teaching system built as a C++ capstone project, inspired by Tsinghua University's 2020 curriculum design.

基于清华大学 2020 年 C++ 大作业要求实现的"雷课堂"在线教学系统。

---

## Project Goal / 项目目标

Build a runnable, demoable, and defensible online classroom system with clear object-oriented design, covering:

实现一个可运行、可演示、可答辩的在线课堂系统，涵盖以下核心功能：

- User login & role-based permissions (登录与权限管理)
- Class creation, start, and end (课堂生命周期)
- Attendance check-in with deadline (签到，支持迟到判定)
- Random student calling (随机点名)
- Question publishing & answer collection (题目发布与答题)
- Statistics & CSV export (统计与导出)
- Screen sharing via periodic screenshots (屏幕共享，截图方式)
- Audio broadcast & student mic control (语音广播与学生发言)

## Completion Status / 完成情况

| Module | Status | Tests |
|--------|--------|-------|
| Login & Permissions | Done | 9 |
| Class Lifecycle | Done | 23 |
| Attendance Check-in | Done | included above |
| Random Calling | Done | 17 |
| Question & Answer | Done | 21 |
| Statistics & Export | Done | 29 |
| Screen Sharing | Done | 23 |
| Audio Broadcast | Done | 35 |
| Edge Cases | Done | 57 |
| **Total** | **All 9 modules complete** | **214** |

All acceptance checklist items are checked. See `docs/acceptance-checklist.md` for details.

所有验收清单项均已完成，详见 `docs/acceptance-checklist.md`。

## Architecture / 系统架构

```
┌──────────────┐     JSON/TCP     ┌──────────────┐
│    Client    │ ◄──────────────► │    Server    │
│   (Qt6 GUI)  │   length-prefix  │  (select())  │
└──────────────┘                  └──────────────┘
        │                                │
        └──────── common/ ───────────────┘
              protocol + models
```

Three-layer structure / 三层架构：

- **`client/`** — Qt6 GUI, page-based navigation, role-separated views (admin/teacher/student)
- **`server/`** — TCP server with `select()`, business logic, JSON file persistence
- **`common/`** — Shared protocol definitions, message types, data models

## Quick Start / 快速开始

### Requirements / 依赖

- C++17 compiler (clang++ or g++)
- CMake 3.16+
- Qt6 (qtbase); optional: qt6-multimedia for audio capture

### Build / 编译

```bash
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)   # macOS
# or: make -j$(nproc)         # Linux
```

### Run / 运行

```bash
# Terminal 1: start server
cd build
./server/thunder_server

# Terminal 2+: start client(s)
./client/thunder_client
```

Default admin account: `admin` / `admin123`

默认管理员账号：`admin` / `admin123`

### Run Tests / 运行测试

```bash
cd build
./tests/test_login_client
./tests/test_class_client
./tests/test_question_client
./tests/test_stats_client
./tests/test_random_call_client
./tests/test_screen_share_client
./tests/test_audio_client
./tests/test_edge_cases
```

## Project Statistics / 项目统计

| Item | Count |
|------|-------|
| Source files (.cpp/.h) | 42 |
| Project files (total, excl. third_party) | 64 |
| Source code lines | 7,436 |
| — server/ | 2,169 |
| — client/ | 2,129 |
| — common/ | 514 |
| — tests/ | 2,624 |
| Documentation lines | 1,070 |
| Third-party (nlohmann/json) | 24,765 |
| Test suites | 8 |
| Test cases | 214 |
| Message types | 40+ |

## Known Limitations / 已知限制

| Feature | Limitation | 说明 |
|---------|-----------|------|
| Screen sharing | Periodic screenshots at ~1 fps, not real-time video | 截图方式，非实时视频 |
| Audio | Requires qt6-multimedia package; uses default device only | 需要安装 qt6-multimedia；仅使用默认设备 |
| Persistence | JSON files, no database | 文件持久化，无数据库 |
| Passwords | Stored in plaintext (MVP scope) | 明文存储（课程项目范围） |
| Platform | macOS / Linux only (POSIX sockets) | 仅支持 macOS / Linux |
| Concurrency | Single-threaded select() loop | 单线程模型 |

## Documentation / 文档

- `docs/architecture.md` — System design and class responsibilities
- `docs/acceptance-checklist.md` — Full acceptance criteria with pass/fail status
- `docs/progress.md` — Development log with all phases
- `docs/defense-script.md` — Defense presentation walkthrough
- `docs/design-summary.md` — One-page design reference

## AI-Assisted Development / AI 辅助开发

This project was developed with the assistance of **Claude Code** (Claude Opus 4.6) by Anthropic. Claude assisted with:

本项目在 **Claude Code**（Claude Opus 4.6，Anthropic）辅助下完成，辅助内容包括：

- Architecture design and module planning
- Code implementation across all three layers
- Test case design and debugging
- Integration review and bug fixing (9 client-server protocol mismatches found and fixed)
- Documentation and defense material preparation
- Code review, identifying and fixing 17 potential issues

The entire development was completed within a single conversation session (~60 turns).

整个开发过程在一次对话（约 60 轮）中完成。

### Token Consumption Estimate / Token 消耗估算

| Category | Tokens | Note |
|----------|--------|------|
| **Input** | ~1,200,000 | System prompt, file reads, test output, conversation context |
| — System prompt (per turn) | ~180,000 | ~3,000 × 60 turns |
| — File reads | ~225,000 | ~15,000 lines read across session |
| — Test output | ~350,000 | 7 full runs of 214 tests (verbose) |
| — Context & history | ~250,000 | Conversation history before compression |
| — Sub-agents (code review) | ~130,000 | 2 exploration agents |
| **Output** | ~180,000 | Code, docs, conversation responses |
| — Source code generated | ~111,000 | 7,436 lines |
| — Documentation | ~21,000 | 1,070 lines |
| — Conversation text | ~48,000 | Explanations, plans, tables |
| **Total** | **~1,400,000** | **~140 万 tokens** |

> Rough estimate with ±30% margin. Input tokens dominate (~87%) due to per-turn system prompt loading, file reads, and verbose test output.
>
> 粗略估算，误差 ±30%。输入 token 占绝大部分（~87%），主要来自每轮系统提示加载、文件读取和测试输出。

---

*Built for learning. Not a production system.*

*课程作业项目，非生产级系统。*
