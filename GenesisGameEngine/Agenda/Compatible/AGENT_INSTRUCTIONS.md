# Genesis Game Engine — Agent Instructions

## Overview
This document defines how I (the agent) will develop the Genesis Game Engine during the 1-month sprint exercise. It contains roles, workflows, acceptance criteria, coding standards, phases, communication rules, and how I will interpret your responses ("continue" or feedback).

**Agent identity:** GitHub Copilot

**Model:** Raptor mini (Preview)

---

## Goals ✅
- Deliver a functional, buildable, and testable C++ game engine skeleton that meets the functional requirements in the provided Game Engine Document.
- Prioritize a minimal, correct implementation, then expand features in clearly versioned phases.
- Keep changes incremental, well-tested, and documented.

---

## High-level Phased Plan (one-month sprint)
1. **Phase 0 — Setup & Agent Doc**: create project instructions, TODOs, and initial repo structure (this file).
2. **Phase 1 — Core & Renderer MVP** (Week 1–2): CMake + VS solution, Engine Core (SDL3), Rendering abstraction layer + OpenGL backend, basic renderer, sample game that draws a model.
3. **Phase 2 — Assets, ECS & Tools** (Week 2–3): integrate Assimp, simple asset packer, EnTT-based ECS, sample scene, ImGui overlay.
4. **Phase 3 — Additional Backends & Modding** (Week 3–4): plugin API skeleton (C interface), DirectX backend (Windows-only) stubs, hot-reload prototype, packaging scripts, CI workflow.
5. **Phase 4 — Polish & Tests**: cross-platform build verification, unit tests, simple automated sample run, docs and final packaging.

---

## Acceptance Criteria (what "done" means) 🎯
- The repository builds (Debug and Release) on Windows (Visual Studio) and at least one POSIX platform (Linux or WSL) via CMake without errors.
- A sample game executable can run and display a rendered triangle or simple Assimp model using the OpenGL backend.
- Basic ECS (EnTT) used to instantiate an entity in the sample scene.
- ImGui overlay showing FPS and basic engine stats.
- Basic asset packing tool that can pack/unpack a shader and texture.
- Clear README and developer instructions to reproduce builds for reviewers.
- No unresolved compiler or runtime errors for the implemented features.

---

## Coding & Project Conventions 🔧
- Primary build system: **CMake** (+ Visual Studio generator on Windows). Projects use libraries (.lib / .a) and a final executable.
- Language: **C++17/C++20** (prefer C++17 for wider compatibility unless a feature requires C++20).
- Style: consistent formatting, short headers, RAII usage, use smart pointers where ownership is needed.
- Tests: unit-tests for deterministic logic (C++ tests with Catch2 or GoogleTest as needed).
- Platform abstractions: minimize #ifdef scattering by using well-defined platform adapters and interfaces.
- Use vendor-based submodules or fetch scripts for third-party dependencies where possible (SDL3, Assimp, Bullet, EnTT, ImGui, OpenAL).

---

## Repository Layout
(Planned top-level layout)

```
/root
  /Engine
    /Core
    /Renderers
      /OpenGLRenderer
      /VulkanRenderer
      /DirectXRenderer
    /ThirdParty
    /Utils
  /GameProjects
    /SampleGame
  /Launcher
  /Editor (optional)
  /Tools
    asset_packer
    shader_compiler
  CMakeLists.txt
  README.md
  LICENSE
```

---

## Development Workflow
- Work progressively through the todo list. Each implemented feature is committed with a focused message and a short summary in the wrap-up preamble.
- On each major change or milestone, I will run the build and basic runtime verification locally and report results.
- For major architectural changes or dependency installs, I will ask for permission before proceeding.

---

## Communication & Control
- You will give feedback as textual suggestions or one-word commands: **"continue"** to proceed to the next todo, or targeted feedback to request changes. I will then re-evaluate the codebase and implement the next set of changes.
- After each implementation step, I'll send a short Wrap-Up preamble describing what was done, what I tested, and the next step (2 sentences max).
- If I need clarification, I'll ask one concise question.

---

## Testing & CI
- Set up CI for Windows (GitHub Actions or Azure Pipelines). We'll start with a Windows build target and later add Linux/macOS.
- Unit tests for small subsystems (parser, asset packer, serialization) and integration smoke tests for the sample game run (headless where possible).

---

## How I interpret your responses
- **"continue"** → I will mark the next todo in the list as **in-progress**, implement it, run builds/tests, and return a Wrap-Up preamble + commit summary.
- **Any other feedback** → I will apply requested changes or ask a single concise clarifying question if needed.

---

## Rules / Constraints
- Keep code safe: no secrets, no system changes outside repo without permission, no pushing to remote without consent.
- Aim for correctness and reproducible builds.
- Prefer minimal working implementations and iterate—avoid scope creep.

---

## Initial TODOs (managed by the agent)
The TODOs are tracked programmatically and updated as we progress.

1. Create Agent Instructions & project scaffold (in-progress)
2. Initialize repo: LICENSE, README, CMakeLists, initial folders (not-started)
3. Add Core window + input (SDL3) minimal app (not-started)
4. Implement rendering abstraction + OpenGL backend (not-started)
5. Integrate Assimp & sample model (not-started)
6. Add EnTT ECS & sample scene (not-started)
7. Hook ImGui overlay & profiling markers (not-started)
8. Create asset_packer & shader_compiler tools (not-started)
9. Add DirectX backend skeleton (Windows-only) and plugin API (not-started)
10. CI workflows & cross-platform checks (not-started)
11. Docs, packaging, and final polishing (not-started)
12. Testing and cross-platform verification (not-started)

---

## Final notes
If at any point you want a deeper analysis, a performance priority, or a change to milestones, respond with the suggestion and I will adapt. When you say **"continue"**, I'll proceed to the next implementation step.

---

*End of Agent Instructions*
