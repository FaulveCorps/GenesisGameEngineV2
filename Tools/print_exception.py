from minidump.minidumpfile import MinidumpFile
import sys

if len(sys.argv) < 2:
    print('Usage: print_exception.py <dumpfile>')
    sys.exit(1)

path = sys.argv[1]
md = MinidumpFile.parse(path)
print('Dump:', path)

if md.exception:
    recs = getattr(md.exception, 'exception_records', None)
    if recs and len(recs) > 0:
        er = recs[0].ExceptionRecord
        print('ExceptionCode:', hex(int(er.ExceptionCode)))
        print('ExceptionAddress:', hex(int(er.ExceptionAddress)))
        print('ThreadId:', recs[0].ThreadId)
        tc = getattr(recs[0],'ThreadContext',None)
        if tc:
            for r in ['Rip','Rsp','Rbp','Rdi','Rsi','Rcx','Rdx','Rax','Rbx']:
                if hasattr(tc,r):
                    print(r+':', hex(int(getattr(tc,r))))
    else:
        print('No exception records found')
else:
    print('No exception info')

print('\nModule list:')
try:
    modules = md.modules.to_table()
    print('modules:', len(modules))
    for m in modules:
        print(m)
except Exception as e:
    print('failed to list modules', e)

print('\nThreads:')
try:
    for t in md.threads.to_table():
        print(t)
except Exception as e:
    print('failed to list threads',e)

print('\nDone')