Auto capture helper

This folder includes an automation helper `auto_capture_live_dump.py` that:

- starts `UnitTests.exe` with an optional Catch2 test filter
- attaches Sysinternals Procdump to the running process
- waits for a full memory dump to be written when a breakpoint/exception occurs
- performs a quick post-inspection (runs `inspect_dump.py` and `parse_ctx_for_dump.py` if available)

Basic usage:

    python auto_capture_live_dump.py --unit "C:\...\UnitTests.exe" --test "WasmRuntime RAII: register and invoke host function" --timeout 60

Notes & tips:

- The script looks for `procdump64.exe` in this directory by default. You can override the path with `--procdump`.
- The script writes dumps to `Tools/external/procdump/dumps` by default.
- If the dump isn't written, try increasing `--timeout` or running procdump manually for interactive debugging:

    Tools\external\procdump\procdump64.exe -accepteula -ma -e 1 -x Tools\external\procdump\dumps -p <pid>

- If you still need interactive Visual Studio capture (save dump with locals from the paused session), attach VS to the UnitTests process after it hits `DebugBreak()` and use "Save Dump As..." → "Minidump with heap".

If you'd like, I can add a PowerShell helper to start the process and attach with procdump for you.