from minidump.minidumpfile import MinidumpFile
import sys
if len(sys.argv) < 2:
    print('usage: dump_stack_info.py <dump>')
    sys.exit(1)
md = MinidumpFile.parse(sys.argv[1])
rec = md.exception.exception_records[0]
ex_tid = int(rec.ThreadId)
print('ExceptionThreadId', ex_tid)
thr = None
for t in md.threads.threads:
    if int(t.ThreadId) == ex_tid:
        thr = t
        break
if not thr:
    print('thread not found')
    sys.exit(1)
st = thr.Stack
print('Stack StartOfMemoryRange', hex(int(st.StartOfMemoryRange)))
print('Stack Rva', int(st.Rva))
print('Stack DataSize', int(st.DataSize))
try:
    ctx = rec.ThreadContext
    print('ThreadContext size', int(ctx.DataSize), 'rva', int(ctx.Rva))
except Exception as e:
    print('ThreadContext not available', e)
