import sys
import traceback
try:
    import pythoncom
    import win32com.client
except Exception as e:
    print('pywin32 not available:', e)
    sys.exit(2)

EX_TID = None
# Accept optional thread id to look for
if len(sys.argv) > 1:
    try:
        EX_TID = int(sys.argv[1])
    except Exception:
        EX_TID = None

rot = pythoncom.GetRunningObjectTable()
enum = rot.EnumRunning()
bc = pythoncom.CreateBindCtx(0)

found = []
print('Enumerating ROT for VisualStudio.DTE instances...')
while True:
    try:
        pair = enum.Next(1)
    except Exception:
        break
    if not pair:
        break
    moniker = pair[0]
    try:
        name = moniker.GetDisplayName(bc, None)
    except Exception:
        name = '<unknown>'
    if 'VisualStudio.DTE' in name:
        try:
            comobj = rot.GetObject(moniker)
            dte = win32com.client.Dispatch(comobj)
            found.append((name, dte))
        except Exception:
            print('Failed to GetObject for', name, traceback.format_exc())

print('Found DTE instances:', len(found))
for name, dte in found:
    print('\n--- DTE instance:', name)
    try:
        print('MainWindow Caption:', getattr(dte.MainWindow, 'Caption', None))
    except Exception as e:
        print('MainWindow access failed:', e)
    try:
        dbg = dte.Debugger
        print('Debugger object type:', type(dbg))
    except Exception as e:
        print('No Debugger on this DTE:', e)
        continue

    procs = None
    try:
        procs = dbg.DebuggedProcesses
        print('DebuggedProcesses.Count =', procs.Count)
    except Exception as e:
        print('DebuggedProcesses not available or error:', e)

    if procs is None:
        # try starting a debug session (best-effort)
        try:
            print('Attempting to start debugging via DTE.ExecuteCommand("Debug.Start")')
            dte.ExecuteCommand('Debug.Start')
        except Exception as e:
            print('ExecuteCommand Debug.Start failed:', e)
        import time
        time.sleep(1)
        try:
            procs = dbg.DebuggedProcesses
            print('After Start: DebuggedProcesses.Count =', procs.Count)
        except Exception as e:
            print('Still no DebuggedProcesses or error after start:', e)

    if procs is None:
        continue

    for i in range(1, procs.Count+1):
        try:
            p = procs.Item(i)
        except Exception as e:
            print('Failed to access process item', i, e)
            continue
        try:
            print('Proc', i, 'Name:', p.Name, 'ProcessID:', getattr(p, 'ProcessID', None), 'IsDump:', getattr(p, 'IsDump', None))
        except Exception as e:
            print('Error reading process properties:', e)

        # find the exception thread if TID given
        try:
            threads = p.Threads
            print('Threads.Count =', threads.Count)
            for tindex in range(1, threads.Count+1):
                thr = threads.Item(tindex)
                try:
                    tid = getattr(thr, 'ID', None)
                except Exception:
                    tid = None
                print('Thread', tindex, 'Id', tid, 'Name', getattr(thr, 'Name', None))
                if EX_TID is not None and tid == EX_TID:
                    print('\nFound exception thread', EX_TID, '-- enumerating frames and locals')
                    for f in range(1, thr.StackFrames.Count+1):
                        try:
                            sf = thr.StackFrames.Item(f)
                            print('\nFrame', f, 'Function:', getattr(sf, 'FunctionName', None), 'InstructionOffset:', getattr(sf, 'InstructionOffset', None))
                            # try to access locals collection
                            locals_coll = getattr(sf, 'Locals', None)
                            if locals_coll is not None:
                                try:
                                    print(' Locals.Count =', locals_coll.Count)
                                    for j in range(1, locals_coll.Count+1):
                                        try:
                                            L = locals_coll.Item(j)
                                            print('  ', getattr(L, 'Name', None), getattr(L, 'Type', None), getattr(L, 'Value', None))
                                        except Exception as e:
                                            print('   Error reading local', j, e)
                                except Exception as e:
                                    print('  Error iterating Locals:', e)
                            else:
                                print('  No Locals property on StackFrame; attempting expressions...')
                                # Try reading common register names or 'this'
                                regs = ['rax','rbx','rcx','rdx','rsi','rdi','rsp','rbp']
                                for reg in regs:
                                    try:
                                        expr = dbg.GetExpression(reg)
                                        print('   Expr', reg, '->', getattr(expr, 'Value', None))
                                    except Exception as e:
                                        print('   GetExpression failed for', reg, e)
                        except Exception as e:
                            print('Failed to read stack frame', f, e)
        except Exception as e:
            print('Error enumerating threads of process', e)

print('\nDone')
