# Copilot / Agent Policy (Agenda Reference)

Purpose: short, actionable rules for Copilot and other agents interacting with this repository.

- Agenda is authoritative: Always consult `Agenda/` for agent memory, ADRs, policies, meeting notes, and agent instructions.
- Single source-of-truth: Prefer `Agenda/Compatible/AGENT_INSTRUCTIONS.md` as the authoritative agent behavior and memory spec.
- Storing memory: If an agent needs to record or update memory, write/update files under `Agenda/` (e.g., `Agenda/Memory.md` or `Agenda/agents/*`). Confirm with human reviewer for irreversible changes.
- Security: Never commit secrets to the repo; prefer ephemeral, CI-protected stores when necessary. If a secret is present in repo artifacts, notify maintainers immediately.
- Preamble compliance: Follow the project's preamble/communication rules in `AGENT_INSTRUCTIONS.md` when posting milestone updates, test results, or making code changes.

If in doubt, ask a human reviewer before changing agent memory files or policies.