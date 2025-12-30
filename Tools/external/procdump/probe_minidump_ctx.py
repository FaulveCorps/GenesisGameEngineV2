from minidump.minidumpfile import MinidumpFile
import sys
md = MinidumpFile.parse(sys.argv[1])
print('md attrs:', [n for n in dir(md) if not n.startswith('_')])
recs = md.exception.exception_records
print('exception_records count:', len(recs))
rec = recs[0]
print('rec attrs:', [n for n in dir(rec) if not n.startswith('_')])
if hasattr(rec, 'ThreadContext'):
    tc = rec.ThreadContext
    print('ThreadContext attrs:', [n for n in dir(tc) if not n.startswith('_')])
    try:
        print('ThreadContext.Rva:', getattr(tc, 'Rva', None))
        print('ThreadContext.DataSize:', getattr(tc, 'DataSize', None))
    except Exception as e:
        print('error getting ThreadContext values:', e)

threads = md.threads.threads
print('threads count:', len(threads))
for t in threads:
    print('ThreadId attr:', getattr(t, 'ThreadId', None), 'has attrs:', [n for n in dir(t) if not n.startswith('_')])
    break

# print memory segments summary
print('\nMemory segments (first 20):')
for i, seg in enumerate(md.memory_segments_64[:20]):
    print(i, 'base', hex(seg.base), 'size', seg.size)

print('\nFinished')
