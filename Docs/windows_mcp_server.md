# Windows MCP Server (VS Code) – Setup & Best Practices

This repository is developed primarily on Windows, and we often need to interact with GUI tools (e.g., the editor, build dialogs, settings panes).

For robust, fast, and token-efficient Windows GUI automation with GitHub Copilot **Agent Mode**, we recommend using **Windows MCP Server**.

## Install / Verify

### 1) Install the VS Code extension

Install **Windows MCP Server** (`sbroenne.windows-mcp`) from the VS Code Marketplace:
- https://marketplace.visualstudio.com/items?itemName=sbroenne.windows-mcp

The extension is designed to auto-configure itself for GitHub Copilot.

### 2) Ensure the required runtime exists

Windows MCP Server requires **.NET 10 Runtime**.

If you already have `dotnet` installed, you can verify with:
- `dotnet --list-runtimes`

You should see entries like `Microsoft.NETCore.App 10.x` and/or `Microsoft.WindowsDesktop.App 10.x`.

## How to use it effectively ("optimize utilization")

### Prefer accessibility/UI Automation over screenshots

Use named UI actions whenever possible:
- Click controls **by name** (e.g., “Click `Save`”, “Open the `File` menu”, “Toggle `Dark mode`”).

This is typically faster and more stable than coordinate-based clicking:
- resilient to DPI scaling
- resilient to theme changes
- resilient to window movement

### Use discovery only when you don’t know what’s available

If the UI structure isn’t obvious:
- request an **annotated/structured** UI snapshot first (rather than repeated trial clicks)
- then perform the minimal set of actions needed

### Use atomic state-based operations when available

Prefer commands that check state before changing it:
- “Enable X” should *verify* X is disabled before toggling
- helps avoid double-toggle mistakes

### Multi-window / multi-monitor hygiene

Before clicking:
- ensure the correct window is focused/foreground
- if multiple monitors exist, prefer operations that target windows by title/app instead of absolute coordinates

### Security / safety notes

Windows MCP Server can control your desktop.
- Be cautious around elevated/UAC prompts and sensitive windows.
- Don’t automate destructive operations without an explicit confirmation step.

## Troubleshooting

### Copilot can’t see the tools

Common causes:
- The extension is not installed in the *same* VS Code instance (Stable vs Insiders).
- GitHub Copilot Agent Mode is not enabled.
- VS Code needs a reload after installation.

### The server feels “slow”

Common fixes:
- avoid screenshot-heavy workflows; use UIA/name-based actions
- reduce repeated discovery; snapshot once, act many

## References

- Windows MCP Server homepage: https://windowsmcpserver.dev/
- Extension listing: https://marketplace.visualstudio.com/items?itemName=sbroenne.windows-mcp
- Project repo: https://github.com/sbroenne/mcp-windows
