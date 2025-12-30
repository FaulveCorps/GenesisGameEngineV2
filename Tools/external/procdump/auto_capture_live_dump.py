r"""auto_capture_live_dump.py

Launch UnitTests with a single-test filter, attach procdump to the runner, and wait
for a minidump (full memory) to be written when a breakpoint/exception occurs.

Usage:
    python auto_capture_live_dump.py --unit "path\to\UnitTests.exe" --test "<test-filter>"

Options:
    --procdump <path>   Path to procdump executable (defaults to bundled procdump64.exe)
    --dumpdir <path>    Where to write dumps (defaults to Tools/external/procdump/dumps)
    --timeout <sec>     Seconds to wait for a dump (default 60)
    --sym-module <path> Optional full path to module (UnitTests.exe) for later symbolization

Outputs:
    Writes a minidump into the dumpdir and prints its filename. Optionally invokes
    parse scripts to extract register context and candidate addresses for symbolization.

"""
import argparse
import subprocess
import time
import os
import shutil
from pathlib import Path
import sys
import glob
import datetime

# Config defaults
ROOT = Path(__file__).resolve().parent
DEFAULT_PROC_DBG = ROOT / 'procdump64.exe'
DEFAULT_DUMPDIR = ROOT / 'dumps'


def find_procdump(candidates=None):
    candidates = list(candidates) if candidates else []
    # try provided, then common names in the folder
    cp = [Path(p) for p in candidates if p]
    cp.extend([DEFAULT_PROC_DBG, ROOT / 'procdump.exe', ROOT / 'procdump64a.exe'])
    for p in cp:
        if p.exists():
            return str(p)
    return None


def list_new_dumps(dumpdir, since_ts):
    files = sorted(Path(dumpdir).glob('*.dmp'))
    out = []
    for f in files:
        try:
            if f.stat().st_mtime >= since_ts.timestamp():
                out.append(f)
        except Exception:
            pass
    return out


def attach_procdump_to_pid(procdump_path, pid, dumpdir):
    # Try a few invocation forms (some procdump versions differ)
    cmds_to_try = []
    # Attach by pid with -p; create full memory dumps (-ma), capture on exception (-e), write to dumpdir (-x)
    cmds_to_try.append([procdump_path, '-accepteula', '-ma', '-e', '1', '-x', str(dumpdir), '-p', str(pid)])
    cmds_to_try.append([procdump_path, '-accepteula', '-ma', '-e', '1', '-x', str(dumpdir), str(pid)])
    cmds_to_try.append([procdump_path, '-accepteula', '-ma', '-x', str(dumpdir), '-p', str(pid)])
    cmds_to_try.append([procdump_path, '-accepteula', '-ma', '-x', str(dumpdir), str(pid)])

    for cmd in cmds_to_try:
        try:
            print('Running:', ' '.join(cmd))
            # Use Popen so procdump can stay attached and create dump when exception occurs
            p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            # give it a short moment to ensure it starts and attaches
            time.sleep(0.5)
            # If process already terminated quickly, read code and output
            if p.poll() is not None:
                out, err = p.communicate(timeout=1)
                print('procdump exited early: rc=', p.returncode)
                print('stdout:', out.decode(errors='replace'))
                print('stderr:', err.decode(errors='replace'))
                continue
            return p
        except FileNotFoundError as e:
            print('procdump not found at', procdump_path, 'error', e)
            return None
        except Exception as e:
            print('Failed to launch procdump with cmd', cmd, 'error', e)
    return None


def start_procdump_wait_for_name(procdump_path, proc_name, dumpdir):
    """Start procdump in wait-for-process-name mode (-w). This is more reliable
    for short-lived test processes because procdump will wait for the target
    process to start and attach automatically.
    """
    cmd = [str(procdump_path), '-accepteula', '-ma', '-e', '1', '-w', '-x', str(dumpdir), proc_name]
    try:
        print('Running (wait-for-name):', ' '.join(cmd))
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        time.sleep(0.2)
        if p.poll() is not None:
            out, err = p.communicate(timeout=1)
            print('procdump (wait) exited early: rc=', p.returncode)
            print('stdout:', out.decode(errors='replace'))
            print('stderr:', err.decode(errors='replace'))
            return None
        return p
    except Exception as e:
        print('Failed to start procdump (wait-for-name):', e)
        return None


def run_and_capture(unit_path, test_filter, procdump_path=None, dumpdir=None, timeout=60, sym_module=None):
    if not unit_path:
        raise SystemExit('UnitTests path is required')
    unit_path = Path(unit_path)
    if not unit_path.exists():
        raise SystemExit(f'UnitTests path not found: {unit_path}')

    procdump = find_procdump([procdump_path] if procdump_path else None)
    if not procdump:
        print('No procdump found; please install Sysinternals Procdump or specify --procdump')
        return 1
    print('Using procdump:', procdump)

    dumpdir = Path(dumpdir) if dumpdir else DEFAULT_DUMPDIR
    dumpdir.mkdir(parents=True, exist_ok=True)

    start_ts = datetime.datetime.now()

    # Start procdump in wait-for-name mode so it can attach reliably even for
    # short-lived test processes. Then launch UnitTests; procdump will attach
    # when it sees the new process with that name.
    p_procdump = start_procdump_wait_for_name(procdump, unit_path.name, dumpdir)
    if not p_procdump:
        print('Falling back to attach-by-pid method')
        # Launch UnitTests with filter argument
        args = [str(unit_path)]
        if test_filter:
            args.append(test_filter)
        print('Starting UnitTests:', args)
        proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        pid = proc.pid
        print('UnitTests PID:', pid)

        # Attach procdump - try multiple variants
        p_procdump = attach_procdump_to_pid(procdump, pid, dumpdir)
        if not p_procdump:
            print('Failed to start procdump; aborting')
            try:
                proc.kill()
            except Exception:
                pass
            return 2
    else:
        # procdump is waiting; now launch the UnitTests process which procdump will catch
        args = [str(unit_path)]
        if test_filter:
            args.append(test_filter)
        print('Starting UnitTests (procdump waiting):', args)
        proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        pid = proc.pid
        print('UnitTests PID:', pid)

    # Wait for a dump to appear
    deadline = time.time() + timeout
    found = None
    try:
        while time.time() < deadline:
            new = list_new_dumps(dumpdir, start_ts)
            if new:
                # pick the newest
                found = new[-1]
                print('Found dump:', found)
                break
            # Poll process output briefly to forward partial output for user's visibility
            try:
                # non-blocking read of stdout lines
                if proc.stdout:
                    line = proc.stdout.readline()
                    if line:
                        print(line.decode(errors='replace').rstrip())
            except Exception:
                pass
            time.sleep(0.5)
    finally:
        # if procdump still running, terminate it gracefully
        try:
            if p_procdump and p_procdump.poll() is None:
                print('Terminating procdump process')
                p_procdump.terminate()
                # give it a moment
                time.sleep(0.2)
                if p_procdump.poll() is None:
                    p_procdump.kill()
        except Exception as e:
            print('Error terminating procdump:', e)

    # Optionally kill UnitTests if still running
    try:
        if proc.poll() is None:
            print('UnitTests still running - terminating')
            proc.kill()
    except Exception:
        pass

    if not found:
        print('No dump created within timeout; check procdump output or increase --timeout')
        return 3

    # Do quick post-processing (extract context / attempt symbolization)
    print('Dump created at', found)
    print('Running quick post-inspection (parse_ctx_for_dump.py)')
    try:
        inspect_script = ROOT / 'inspect_dump.py'
        ctx_script = ROOT / 'parse_ctx_for_dump.py'
        if inspect_script.exists():
            subprocess.run([sys.executable, str(inspect_script), str(found)])
        if ctx_script.exists():
            subprocess.run([sys.executable, str(ctx_script), str(found)])
    except Exception as e:
        print('Post-inspection failed:', e)

    # If symbol module provided, attempt to map RIP to symbol using symbolize_addr_fix.py
    if sym_module:
        try:
            # Attempt robust post-processing: try parsing this dump first, then fall back to
            # other recent token_crash_*.dmp files and try extract->symbolize paths.
            import re
            ctx_script = ROOT / 'parse_ctx_for_dump.py'
            stack_script = ROOT / 'extract_stack_segment.py'
            symbolizer = ROOT / 'symbolize_addr_fix.py'
            # `Scripts` is at the repo root; walk up one more level from ROOT (which is Tools/external/procdump)
            find_mod = ROOT.parent.parent.parent / 'Scripts' / 'find_module_base.py'

            dumps = sorted(Path(dumpdir).glob('token_crash_*.dmp'), key=lambda p: p.stat().st_mtime, reverse=True)
            candidates = [Path(found)] + [p for p in dumps if p != Path(found)]
            parse_success = False
            analysis_dir = Path(dumpdir) / 'analysis'
            analysis_dir.mkdir(exist_ok=True)

            for d in candidates:
                print(f'Attempting parse & symbolization on dump: {d}')
                # 1) Try parse_ctx_for_dump.py which extracts ExceptionAddress and register context
                if ctx_script.exists():
                    p = subprocess.run([sys.executable, str(ctx_script), str(d)], capture_output=True, text=True)
                    print('parse_ctx_for_dump stdout:')
                    print(p.stdout)
                    print('parse_ctx_for_dump stderr:')
                    print(p.stderr)
                    if p.returncode == 0 and 'ExceptionAddress:' in p.stdout:
                        parse_success = True
                        # Save stdout for quick reference
                        outf = analysis_dir / (d.stem + '_parse_ctx.txt')
                        outf.write_text(p.stdout + '\n' + p.stderr, encoding='utf-8')

                        m = re.search(r'ExceptionAddress:\s*(0x[0-9a-fA-F]+)', p.stdout)
                        if m:
                            exaddr = m.group(1)
                            print('Found ExceptionAddress', exaddr, '— attempting symbolization')
                            # Try to find module base and run symbolizer if provided
                            base_addr = None
                            if find_mod.exists():
                                fm = subprocess.run([sys.executable, str(find_mod), str(d)], capture_output=True, text=True)
                                print('find_module_base stdout:')
                                print(fm.stdout)
                                print('find_module_base stderr:')
                                print(fm.stderr)
                                # Try to find base in either stdout or stderr with flexible separators
                                for s in (fm.stdout or '', fm.stderr or ''):
                                    m2 = re.search(r'base[:=]?\s*(0x[0-9a-fA-F]+)', s)
                                    if m2:
                                        base_addr = m2.group(1)
                                        break
                            if symbolizer.exists():
                                # If we have a module base, pass it; otherwise pass 0 and let symbolizer try
                                base_arg = base_addr if base_addr else '0x0'
                                subprocess.run([sys.executable, str(symbolizer), str(sym_module), base_arg, exaddr])
                            # Extract registers from the parse output and save them
                            regs = {}
                            for r in ['Rax','Rcx','Rdx','Rbx','Rsp','Rbp','Rsi','Rdi','Rip']:
                                mm = re.search(rf'{r}\s*(0x[0-9a-fA-F]+)', p.stdout)
                                if mm:
                                    regs[r] = mm.group(1)
                            print('Parsed registers:', regs)
                            regs_file = analysis_dir / (d.stem + '_regs.txt')
                            regs_file.write_text('\n'.join(f"{k} {v}" for k, v in regs.items()), encoding='utf-8')
                            # Disassemble around the exception address using disasm helper if available
                            disasm_script = ROOT / 'disasm_at_addr.py'
                            if disasm_script.exists():
                                print('Running disassembly around', exaddr)
                                ds = subprocess.run([sys.executable, str(disasm_script), str(d), exaddr, '128'], capture_output=True, text=True)
                                print('disasm stdout:\n', ds.stdout)
                                (analysis_dir / (d.stem + '_disasm.txt')).write_text(ds.stdout + '\n' + ds.stderr, encoding='utf-8')
                            # Attempt to read bytes at RAX and RAX+8 (if present) to inspect pointer/contents
                            if 'Rax' in regs:
                                try:
                                    rax_val = int(regs['Rax'], 16)
                                    for delta in (0, 8):
                                        addr = '0x%X' % (rax_val + delta)
                                        rv = subprocess.run([sys.executable, str(ROOT / 'read_va.py'), str(d), addr, '64'], capture_output=True, text=True)
                                        print(f'read_va {addr} stdout:\n', rv.stdout)
                                        (analysis_dir / (d.stem + f'_mem_{addr}.txt')).write_text(rv.stdout + '\n' + rv.stderr, encoding='utf-8')
                                except Exception as e:
                                    print('read_va failed:', e)
                            # Check for suspicious pointer patterns (e.g., small RAX)
                            if 'Rax' in regs:
                                try:
                                    rax_val = int(regs['Rax'], 16)
                                    if rax_val < 0x1000 or rax_val == 0:
                                        print('Suspicious RAX value (likely invalid pointer):', hex(rax_val))
                                        (analysis_dir / (d.stem + '_suspicious.txt')).write_text(f'RAX suspicious: {hex(rax_val)}\n', encoding='utf-8')
                                except Exception:
                                    pass
                        break
                # 2) If parse_ctx failed, try extract_stack_segment to find candidate return addresses
                if stack_script.exists():
                    q = subprocess.run([sys.executable, str(stack_script), str(d)], capture_output=True, text=True)
                    print('extract_stack_segment stdout:')
                    print(q.stdout)
                    outf = analysis_dir / (d.stem + '_stack.txt')
                    outf.write_text(q.stdout + '\n' + q.stderr, encoding='utf-8')
                    # Look for addresses in the output
                    addrs = re.findall(r'(0x[0-9a-fA-F]+)', q.stdout)
                    if addrs:
                        unique_addrs = list(dict.fromkeys(addrs))
                        print('Candidate addresses from stack:', unique_addrs[:8])
                        # Try symbolizing first few addresses
                        for a in unique_addrs[:8]:
                            if symbolizer.exists():
                                # try with base found earlier, or 0
                                base_arg = '0x0'
                                if find_mod.exists():
                                    fm = subprocess.run([sys.executable, str(find_mod), str(d)], capture_output=True, text=True)
                                    m2 = re.search(r'base:\s*(0x[0-9a-fA-F]+)', fm.stdout)
                                    if m2:
                                        base_arg = m2.group(1)
                                subprocess.run([sys.executable, str(symbolizer), str(sym_module), base_arg, a])
                        # If we got here, we consider the dump useful even without exception record
                        parse_success = True
                        break
            if not parse_success:
                print('No parseable dump with exception context found among recent dumps; consider capturing a "Minidump with heap" from Visual Studio while paused to see frame locals')
        except Exception as e:
            print('Symbolization helper failed:', e)

    return 0


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--unit', required=True, help='Path to UnitTests.exe')
    ap.add_argument('--test', required=False, default=None, help='Catch2 test filter (enclose in quotes)')
    ap.add_argument('--procdump', help='Path to procdump executable (optional)')
    ap.add_argument('--dumpdir', help='Directory to write dumps (optional)')
    ap.add_argument('--timeout', type=int, default=60, help='Timeout in seconds to wait for dump')
    ap.add_argument('--sym-module', help='Optional module path for symbolization step (UnitTests.exe)')

    args = ap.parse_args()
    rc = run_and_capture(args.unit, args.test, args.procdump, args.dumpdir, args.timeout, args.sym_module)
    sys.exit(rc)
