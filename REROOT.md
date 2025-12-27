# Repository Re-root (2025-12-27)

This repository was re-rooted on 2025-12-27 so that the previous `GameEngine/` subtree is now the repository root. This was done to simplify the layout and make the project structure more direct for builds, CI, and contributors.

## Backups & recovery
- Branch backups (pushed): `pre-reroot-backup`, `pre-reroot-backup-2`
- Repository bundle snapshot (local): `pre-reroot-backup.bundle` and `pre-re-root-backup-20251227.bundle` (kept in the repo for convenience during the transition)
- Tag snapshot: `pre-reroot-snapshot-20251227`

If you need to restore the pre-re-root state, you can either check out a backup branch:

```bash
# example: restore local branch from remote
git fetch origin pre-reroot-backup
git checkout -b restore-pre-reroot origin/pre-reroot-backup
```

Or use the local bundle (if available):

```bash
git clone /path/to/pre-reroot-backup.bundle restored-repo
```

## Important developer notes (required on your machine)
- A forced update changed `main`'s history; you should re-sync your local clones to avoid confusion:

```bash
git fetch origin
git switch main
git reset --hard origin/main
```

- If you have topic branches, please rebase them onto the new `main` (or re-create them):

```bash
git checkout -b myfix origin/main
# cherry-pick or rebase your work onto the new main
```

## CI and next steps
- The `.github/workflows/ci.yml` workflow was restored and a CI run was triggered on the most recent commit.
- If CI fails, please attach the run logs to an issue or notify the maintainers so I can triage and fix any path or workflow issues introduced by the re-root.

If you want, I can update documentation, CMake presets, or break out further follow-up PRs to tidy any paths or workflow entries discovered by CI. Ask me to proceed and I'll take care of the next items.