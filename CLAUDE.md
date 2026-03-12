# CLAUDE.md

## Project
This project is a course assignment implementation of a "Thunder Class"-style online teaching system based on the 2020 Tsinghua C++ capstone assignment.

The goal is not a production-grade product. The priorities are:
1. Build a runnable, demoable, defendable system
2. Keep the object-oriented design clear
3. Maintain complete documentation, architecture notes, and acceptance materials
4. Finish a functional closed loop within limited time

## Primary Goal
Prioritize an MVP for the course project with the following features:
- User login
- Role separation: admin, teacher, student
- Teacher can create, start, and close classes
- Student can join and leave classes
- Attendance check-in
- Random calling
- Question publishing and answer submission
- Online duration statistics
- Class record storage and export

The following features belong to phase two:
- Screen sharing
- Audio streaming
- Audio device switching
- More advanced attention tracking

## Non-Goals
Do not treat this as a production-grade video conferencing system.
Do not optimize early for performance, scalability, or cross-platform completeness.
Do not introduce large or complex dependencies without clear justification.
Do not perform large refactors without explanation.

## Engineering Priorities
Make decisions in this order:
1. Acceptance completeness
2. Clear design
3. Maintainability
4. Demo quality
5. Performance optimization

## Architecture Direction
Prefer a three-layer structure:
- `client/`: Qt GUI, page state, user interaction
- `server/`: class state, permission control, message routing, persistence
- `common/`: shared protocol definitions, DTOs, enums, utilities

Principles:
- Put business logic on the server side whenever possible
- Keep the client focused on presentation and request initiation
- Define shared data structures only once
- Wrap OS APIs in classes instead of scattering them through business code

## Coding Rules
- Default to C++17
- Organize functionality with classes and objects
- Avoid adding free functions outside `main` unless truly necessary
- Keep each class focused on a clear responsibility
- Separate networking, database modeling, and UI state cleanly
- Prefer readability and explainability over cleverness
- Add comments only where they provide real value
- Do not modify unrelated files
- Read related code before making assumptions

## Dependency Rules
- Keep dependencies lightweight
- Prefer libraries that are reasonable and explainable in a course setting
- If a third-party library is needed, explain its purpose, risk, and whether it fits the "source-level inclusion" expectation
- Do not introduce large multimedia frameworks unless clearly necessary

## Delivery Rules
Whenever a module is completed, update documentation:
- `docs/assignment-spec.md`
- `docs/architecture.md`
- `docs/acceptance-checklist.md`
- `docs/progress.md`

If these files do not exist, create minimal first versions before continuing.

## Required Workflow
Every new task must follow this order:
1. Read the relevant code and docs
2. Clarify the goal, scope, dependencies, and acceptance points
3. Propose the smallest viable implementation
4. Then modify code
5. Run the minimum necessary verification
6. Update docs and progress notes

## Response Format
Use this structure by default in each response:
- Current goal
- Implementation plan
- Files to change
- Verification method
- Risks and fallback plan
- Next step suggestion

If code has already been changed, also include:
- Actual changes made
- Verification result
- Remaining issues

## Module Order
Follow this order unless I explicitly change priorities:
1. Requirement clarification and acceptance checklist
2. Overall architecture design
3. Login and permission system
4. Class lifecycle
5. Question publishing and answering
6. Statistics and export
7. Screen sharing
8. Audio module
9. Integration and defense materials

## Scope Control
If a module is high-risk, prefer a downgraded implementation:
- Screen sharing can start as "periodic screenshot + image frame transfer"
- Audio can start as "one-way broadcast" or "called student goes on mic"
- Attention tracking can start with simple metrics instead of advanced analysis

Any fallback solution must:
1. Preserve the main workflow
2. Be usable for course demonstration
3. Be documented honestly

## Quality Bar
Before considering a module done, confirm:
- The feature works end to end
- The module responsibility is easy to explain
- The code structure is suitable for presentation in a defense
- There is a minimal verification path
- Documentation is updated

## First Task
If the repository is not prepared yet, start with:
1. A proposed directory structure
2. First version of `docs/architecture.md`
3. First version of `docs/acceptance-checklist.md`
4. First version of `docs/progress.md`

Do not start with multimedia features.

