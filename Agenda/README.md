# Agenda — project governance artifacts

This folder contains Agenda+ compatible project artifacts used for governance, agent manifests, Architecture Decision Records (ADRs), policies, and meeting notes.

Layout:
- `Agenda/agents/`: agent manifests (e.g., `GitHub_Copilot.md`)
- `Agenda/ADRs/`: decision records documenting important architectural choices
- `Agenda/policies/`: project-level policies (git, code of conduct, security)
- `Agenda/meetings/`: meeting notes and retrospectives

Guidelines:
- Keep agent manifests and governance materials in this folder.
- Avoid accidentally committing large build artifacts into the repository; use `.gitignore` to exclude them.
- Follow instructions in `Agenda/policies/GitPolicies.md` for project-only Git policies.
