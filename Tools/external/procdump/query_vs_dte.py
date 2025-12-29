import sys
try:
    import win32com.client
except Exception as e:
    print('pywin32 not installed or import failed:', e)
    sys.exit(2)

found = False
for vsver in ("VisualStudio.DTE.17.0","VisualStudio.DTE.16.0","VisualStudio.DTE.15.0"):
    try:
        dte = win32com.client.GetActiveObject(vsver)
        print('Got DTE', vsver)
        found = True
        break
    except Exception as e:
        print('No DTE for', vsver, '-', e)

if not found:
    print('No active Visual Studio DTE found via GetActiveObject')
    sys.exit(1)

try:
    dbg = dte.Debugger
    print('Debugger object type:', type(dbg))
    try:
        # Try DebuggedProcesses (for dump there may be 1)
        procs = dbg.DebuggedProcesses
        print('DebuggedProcesses.Count =', procs.Count)
        for i in range(1, procs.Count+1):
            p = procs.Item(i)
            try:
                print('Proc', i, 'Name:', p.Name, 'ProcessID:', p.ProcessID, 'IsDump:', getattr(p,'IsDump', None))
            except Exception as e:
                print('Error reading process properties:', e)
    except Exception as e:
        print('No DebuggedProcesses or error:', e)
        # If requested, attempt to start debugging (e.g., user opened a dump file and wants to start session)
        if len(sys.argv) > 1 and sys.argv[1] == 'start':
            print('Attempting to start debugging via DTE.ExecuteCommand("Debug.Start")')
            try:
                dte.ExecuteCommand('Debug.Start')
            except Exception as e2:
                print('ExecuteCommand Debug.Start failed:', e2)
            import time
            time.sleep(1)
            try:
                procs = dbg.DebuggedProcesses
                print('After Start: DebuggedProcesses.Count =', procs.Count)
                for i in range(1, procs.Count+1):
                    p = procs.Item(i)
                    try:
                        print('Proc', i, 'Name:', p.Name, 'ProcessID:', p.ProcessID, 'IsDump:', getattr(p,'IsDump', None))
                    except Exception as e:
                        print('Error reading process properties after start:', e)
            except Exception as e:
                print('Still no DebuggedProcesses or error after start:', e)
    try:
        # Try LocalProcesses
        lprocs = dbg.LocalProcesses
        print('LocalProcesses.Count =', lprocs.Count)
        for i in range(1, lprocs.Count+1):
            p = lprocs.Item(i)
            try:
                print('Local Proc', i, 'Name:', p.Name, 'ProcessID:', p.ProcessID)
            except Exception as e:
                print('Error reading local process props:', e)
    except Exception as e:
        print('No LocalProcesses or error:', e)

    # Try to show stack frames if any debugged process
    try:
        if procs.Count >= 1:
            p = procs.Item(1)
            for t in range(1, p.Threads.Count+1):
                thr = p.Threads.Item(t)
                print('Thread', t, 'Id', thr.ID, 'Name', getattr(thr,'Name',None))
                for f in range(1, thr.StackFrames.Count+1):
                    sf = thr.StackFrames.Item(f)
                    print('Frame', f, 'Function:', sf.FunctionName, 'InstructionOffset:', getattr(sf, 'InstructionOffset', None))
    except Exception as e:
        print('Error enumerating threads/frames:', e)

except Exception as e:
    print('Error accessing DTE.Debugger:', e)
    sys.exit(1)

print('done')