import sys
try:
    import minidump
    print('minidump module found')
except Exception as e:
    print('minidump import failed:', e)
    print('Trying to install python-minidump')
    import subprocess
    subprocess.check_call([sys.executable, '-m', 'pip', 'install', 'minidump'])
    import minidump
    print('minidump installed')

from minidump.minidumpfile import MinidumpFile


def print_dump_info(path):
    print('Reading dump:', path)
    md = MinidumpFile.parse(path)
    print('\nDump attributes:')
    print('\n'.join([n for n in dir(md) if not n.startswith('_')]))
    try:
        print('\nException attr dir: ' + ','.join([n for n in dir(md.exception) if not n.startswith('_')]))
        if hasattr(md.exception, 'exception_records'):
            recs = md.exception.exception_records
            print('Found exception_records count:', len(recs))
            for rec in recs:
                print('Exception record dir:', '\n'.join([n for n in dir(rec) if not n.startswith('_')]))
                if hasattr(rec, 'ThreadId'):
                    print('Exception ThreadId:', rec.ThreadId)
                if hasattr(rec, 'ExceptionRecord'):
                    er = rec.ExceptionRecord
                    print('ExceptionRecord dir:', '\n'.join([n for n in dir(er) if not n.startswith('_')]))
                    try:
                        print('ExceptionCode raw repr:', repr(er.ExceptionCode))
                    except Exception as e:
                        print('ExceptionCode repr failed:', e)
                    try:
                        addr = int(er.ExceptionAddress)
                        print('ExceptionAddress (hex):', hex(addr))
                    except Exception as e:
                        print('ExceptionAddress repr failed:', e)
                    try:
                        print('ExceptionInformation raw repr:', repr(er.ExceptionInformation))
                    except Exception as e:
                        print('ExceptionInformation repr failed:', e)
                    try:
                        print('\nException ThreadContext dir:')
                        print('\n'.join([n for n in dir(rec.ThreadContext) if not n.startswith('_')]))
                        # Try to print common registers
                        for reg in ['Rip','Rbp','Rsp','Eip','Ebp','Esp','Rdi','Rsi']:
                            if hasattr(rec.ThreadContext, reg):
                                try:
                                    print(reg, hex(int(getattr(rec.ThreadContext, reg))))
                                except Exception as e:
                                    print('Failed to print reg', reg, e)
                    except Exception as e:
                        print('Failed to inspect ThreadContext:', e)
                else:
                    if hasattr(rec, 'ExceptionCode'):
                        print('ExceptionCode:', hex(rec.ExceptionCode))
                    if hasattr(rec, 'ExceptionAddress'):
                        print('ExceptionAddress:', hex(rec.ExceptionAddress))
                    if hasattr(rec, 'ExceptionInformation'):
                        print('ExceptionInformation:', rec.ExceptionInformation)
    except Exception as e:
        print('No exception record found or error inspecting exception:', e)
    try:
        print('\nThreads attr dir: ' + ','.join([n for n in dir(md.threads) if not n.startswith('_')]))
    except Exception as e:
        print('Error inspecting threads:', e)
    try:
        thrlist = md.threads.threads
        print('\nThreads:')
        ex_tid = int(md.exception.exception_records[0].ThreadId)
        print('\nLooking for thread with id', ex_tid)
        for thr in thrlist:
            tid = None
            if hasattr(thr, 'ThreadId'):
                tid = int(getattr(thr, 'ThreadId'))
            if tid == ex_tid:
                print('Found exception thread')
                print('\nThread object dir:')
                print('\n'.join([n for n in dir(thr) if not n.startswith('_')]))
                # Inspect stack
                try:
                    st = thr.Stack
                    print('\nStack attrs: ' + ','.join([n for n in dir(st) if not n.startswith('_')]))
                    if hasattr(st, 'StartOfMemoryRange') and hasattr(st, 'Memory'):
                        start = int(st.StartOfMemoryRange)
                        data = st.Memory
                        print('Stack start:', hex(start), 'len:', len(data))
                        # read addresses from stack to find likely return addresses
                        import struct
                        words = []
                        nwords = min(256, len(data) // 8)
                        for i in range(nwords):
                            val = struct.unpack_from('<Q', data, i*8)[0]
                            words.append(val)
                        # show first 32 potential return addresses within process modules
                        print('Scanning stack for addresses inside unit exe:')
                        exaddr = int(md.exception.exception_records[0].ExceptionRecord.ExceptionAddress)
                        for w in words[:256]:
                            if w >= 0x10000 and w < 0xffffffffffff:
                                # check if in any module
                                for m in mods:
                                    base = int(m.baseaddress)
                                    size = int(m.size)
                                    if base <= w < base+size:
                                        print('stack addr', hex(w), 'found in module', getattr(m,'name',None))
                                        break
                except Exception as e:
                    print('Error inspecting stack:', e)
                break
        else:
            print('Exception thread not found in thread list')
    except Exception as e:
        print('Error extracting thread list:', e)
    try:
        print('\nModules list dir:')
        print('\n'.join([n for n in dir(md.modules) if not n.startswith('_')]))
        # Try different properties to list modules
        if hasattr(md.modules, 'modules'):
            mods = md.modules.modules
        elif hasattr(md.modules, 'list'):
            mods = md.modules.list
        else:
            mods = []
        if not mods:
            print('No iterable modules list found')
        else:
            print('Found modules count:', len(mods))
            first = mods[0]
            print('First module dir:', '\n'.join([n for n in dir(first) if not n.startswith('_')]))
            print('First module name attr candidates:', [getattr(first,attr) for attr in ['module_name','name','name_str'] if hasattr(first,attr)])
            # If exception address exists, try to find the module that contains it
            try:
                exaddr = int(md.exception.exception_records[0].ExceptionRecord.ExceptionAddress)
                print('Looking for module containing exception addr', hex(exaddr))
                for m in mods:
                    base = int(m.baseaddress)
                    size = int(m.size)
                    if base <= exaddr < base + size:
                        print('Exception occurred in module:', getattr(m, 'name', '<unknown>'), 'base:', hex(base), 'size:', size)
                        break
            except Exception as e:
                print('Error while locating module for exception address:', e)
    except Exception as e:
        print('Error enumerating modules:', e)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('Usage: inspect_dump.py <dumpfile>')
        sys.exit(1)
    print_dump_info(sys.argv[1])
