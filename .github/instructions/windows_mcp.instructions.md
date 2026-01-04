---
name: windows-mcp-usage
description: Windows MCP Server usage (Copilot Agent Mode)
applyTo: "**"
---

# Windows MCP Server usage guidelines

When working on Windows and a task requires interacting with **desktop GUI applications**, prefer using **Windows MCP Server** tools (UI Automation / accessibility-tree based) over screenshot-guessing workflows.

## Preferred approach

1. **Target the correct window/app** (by title/process/app name) and ensure focus.
2. **Use semantic UIA operations** (find/click/type by element name/type) whenever possible.
3. Use **discovery/annotated snapshots** only if you don’t know the UI structure.
4. Prefer **atomic stateful operations** ("enable X" checks state first).
5. Fall back to screenshots/mouse/keyboard only for canvas apps/games/custom controls with poor accessibility metadata.

## Quality & safety

- Confirm before destructive UI actions (closing without saving, deleting, overwriting, submitting forms).
- Avoid interacting with elevated/UAC prompts unless explicitly requested.

## Troubleshooting hints

- If tools are missing, ensure the VS Code extension `sbroenne.windows-mcp` is installed and VS Code has been reloaded.
- Verify .NET 10 runtime is available (`dotnet --list-runtimes`).
