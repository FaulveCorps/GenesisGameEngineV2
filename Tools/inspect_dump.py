import sys
from minidump.minidumpfile import MinidumpFile

if len(sys.argv) < 2:
    print('Usage: inspect_dump.py <dumpfile.dmp>')
    sys.exit(1)

path = sys.argv[1]
print('Reading dump:', path)
md = MinidumpFile.parse(path)

# Exception info
if md.exception:
    # minidump library may expose exception_records
    recs = getattr(md.exception, 'exception_records', None)
    if recs and len(recs) > 0:
        print('md.exception object attrs:', dir(md.exception))
        print('recs[0] attrs:', dir(recs[0]))
        # try to access fields available on this object
        try:
            er = recs[0].ExceptionRecord
            # Some fields are custom types; safely print their reprs
            print('ExceptionCode:', er.ExceptionCode)
            print('ExceptionAddress:', er.ExceptionAddress)
            print('ExceptionInformation:', getattr(er, 'ExceptionInformation', None))
            print('Exception thread id:', recs[0].ThreadId)

            # Try to inspect thread context associated with this exception
            tc = getattr(recs[0], 'ThreadContext', None)
            if tc:
                print('ThreadContext attrs:', dir(tc))
                if hasattr(tc, 'Rip'):
                    print('RIP:', hex(int(tc.Rip)))
                if hasattr(tc, 'Rsp'):
                    print('RSP:', hex(int(tc.Rsp)))
                if hasattr(tc, 'Rbp'):
                    print('RBP:', hex(int(tc.Rbp)))
                if hasattr(tc, 'Eip'):
                    print('EIP:', hex(int(tc.Eip)))
            else:
                print('No thread context available in exception record')
        except Exception as e:
            print('Failed to extract fields from exception record:', e)
    else:
        print('No exception record found in md.exception')
else:
    print('No exception info in dump')

# Modules
print('\nModules list object attrs:', dir(md.modules))
try:
    mlist = md.modules.to_table()
    print('Number of modules:', len(mlist))
    for m in mlist[:10]:
        print(m)
except Exception as e:
    print('Failed to list modules:', e)

# Threads and stack region (report thread that crashed)
print('\nThreads table (sample):')
try:
    tlist = md.threads.to_table()
    for row in tlist[:20]:
        print(row)
except Exception as e:
    print('Failed to list threads:', e)

# Try to print instruction pointer for crashed thread
if md.exception:
    recs = getattr(md.exception, 'exception_records', None)
    if recs and len(recs) > 0:
        try:
            er = recs[0].ExceptionRecord
            addr = int(er.ExceptionAddress)
            exc_addr_hex = hex(addr)
            print('\nException address hex:', exc_addr_hex)
            # find module that contains this address
            try:
                mlist = md.modules.to_table()
                containing = None
                for m in mlist:
                    # Skip header row
                    if not m or m[0] == 'Module name':
                        continue
                    base = int(m[1], 16)
                    end = int(m[3], 16)
                    if base <= addr < end:
                        containing = m
                        break
                if containing:
                    print('Exception occurred in module row:', containing)
                else:
                    print('Exception module not found')
            except Exception as e:
                print('Failed to search modules for address:', e)
        except Exception as e:
            print('Failed to extract exception address:', e)
else:
    print('No exception info available for stack extraction')
print('\nDone')
