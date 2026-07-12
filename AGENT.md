# Agent Guidelines: Avoiding Misunderstandings and Errors
This project for Arduino IDE, do not try to run code, instead ask me to run.

## Core Principles
1. **Clarify Conflicts**: If a request conflicts with a prior constraint, point out the conflict and ask for clarification.
2. **Remember Constraints**: Respect all user-stated constraints throughout the conversation unless explicitly retracted.
3. **Avoid Assumptions**: Do not assume intent; ask when uncertain.
4. **Handle Errors Gracefully**: Inform users of tool errors, suggest alternatives.
5. **Propose Compliant Alternatives**: If a request violates constraints, offer ways to achieve the goal within constraints.
6. **Read First**: Consult relevant project files (AGENT.md, README, settings) before acting.
7. **Stay Focused**: Keep responses directly relevant to the user's query.
8. **Acknowledge Limits**: Clearly state limitations and what can be done instead.

## Specific Rules
- **No File Edits**: If user says "DO NOT EDIT FILES!" or "STOP", treat as absolute constraint. Propose alternatives like temporary outputs or explanations.
- **Edit Files**: Ask confirmation to edit files.
- **Implementation**: Implement Plan Only if user says to. If plan is existed, discussed and no unresolved questions.
- **Tool Use**: Verify commands/tools exist before use; handle failures by reporting error and suggesting fallbacks.
- **Updates**: When user changes intent, confirm: "To confirm, you want me to [new action] despite earlier constraint [X]?"
- **LLM Rate Limiting**: For each LLM call, add a request delay of 1.6 seconds minimum to avoid rate limits, use pi-rate-limit extention.

# Project specific Rules

## Constrains
- **No Git**: Do NOT initialize a git repository or use git commands. This project explicitly avoids version control.
- **No Hardcoded Variables**: All variables above the code
- **No Absolute Path**: Use Relative path only.

## Executon Rules
- No executions

## Architecture Rules
- just one file with ino extention for code
README.md: instructions app documentation, how to install, how to run, how to use.
PLAN.md: app implementation plan
AGENT.md: rules

## Development Workflow
- **Plan First**: Refer to `PLAN.md` before starting new features.
- **Read Before Write**: Always read existing files before attempting to modify them, read CSVs files partly (head & tail).

## Planning vs Implementation Separation
- **Planning Phase**: Only `read`, `bash` (grep/find), and edit `PLAN.md` / `README.md`. **NO code edits**.
- **Implementation Phase**: Only after user explicitly says "start" or "implement".
- **Transition**: Must get explicit user confirmation before switching from planning to implementation.

## 🛠 Data & API Safety
- **Do not break code**: you can mark existing code (lines, functions, etc.) as "DONT TOUCH", "TODO", "DONE", "EDIT", etc.

## Visual & Style Standards

