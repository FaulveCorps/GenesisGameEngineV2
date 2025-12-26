# Agent Manifest: GitHub Copilot

- id: github_copilot
- name: GitHub Copilot
- model: Raptor mini (Preview)
- role: Specialist agent (development assistant)
- source: migrated from repository root `AGENT_INSTRUCTIONS.md`

## Summary
This document defines the agent's responsibilities and development conventions for the Genesis Game Engine. It contains the agent instructions, phased plan, acceptance criteria, and development workflow used during the sprint.

## Responsibilities
- Implement features in incremental, well-tested steps.
- Update TODOs and provide concise wrap-up preambles on milestone completion.
- Respect repository policies, do not add secrets, and ask for permission for major design or tooling changes.

## Operational notes
- The agent expects to be controlled via textual commands (e.g., "continue") or targeted feedback.
- The agent keeps a short preamble at milestones and records progress in the repository.

*Full agent instructions and the phased plan were migrated here from `AGENT_INSTRUCTIONS.md`.*
