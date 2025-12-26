# Git & Repository Policies

This file describes Git policies and checks enforced for projects using the Agenda folder:

- Use a single project-level Git repository; avoid nested Git repositories inside the project.
- Avoid committing build outputs and generated files. Use `.gitignore` to exclude these.
- Use `git rm --cached <file>` to untrack previously committed build artifacts before adding them to `.gitignore`.

Quick checks (run in project root):

- Find nested `.git` directories:
  - bash: `find . -type d -name .git`
  - PowerShell: `Get-ChildItem -Directory -Force -Recurse -Filter .git`

- Ensure no nested Git repos exist. If present, consider removing them or converting them to submodules intentionally.

- Preferred ignored directories:
  - `build*/`
  - `.vs/`, `*.suo`, `*.user` (IDE files)
  - `vcpkg/installed/*` (avoid committing large installed packages)
  - `**/Debug/**`, `**/Release/**` (build artifacts)
  - `*.exe`, `*.dll`, `*.pdb`, `*.lib` (binaries)

If you want, run the included `scripts/check_nested_git.*` helper to validate the repo before pushing.
