from minidump.minidumpfile import MinidumpFile
import sys
md = MinidumpFile.parse(sys.argv[1])
ms = md.memory_segments_64
print('type:', type(ms))
print('attrs:', [n for n in dir(ms) if not n.startswith('_')])
# try to access .streams or similar
try:
    if hasattr(ms, 'memory_segments'):
        segs = ms.memory_segments
    elif hasattr(ms, 'segments'):
        segs = ms.segments
    else:
        # fallback to using parse or to_table
        print('members of ms:', dir(ms))
        # try to call to_table or parse
        try:
            print('to_table:', ms.to_table())
        except Exception as e:
            print('to_table failed:', e)
        segs = None
    if segs:
        print('first 10 segments info:')
        for s in segs[:10]:
            try:
                print('base', hex(s.base), 'size', s.size)
            except Exception:
                print('segment repr', s)
except Exception as e:
    print('error enumerating segments:', e)

# Print stack StartOfMemoryRange for exception thread
rec = md.exception.exception_records[0]
ex_tid = int(rec.ThreadId)
print('exception tid', ex_tid)
for t in md.threads.threads:
    if int(getattr(t, 'ThreadId')) == ex_tid:
        st = t.Stack
        print('stack StartOfMemoryRange', getattr(st, 'StartOfMemoryRange', None), 'Rva', getattr(st, 'Rva', None), 'DataSize', getattr(st,'DataSize', None))
        break
