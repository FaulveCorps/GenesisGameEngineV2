import minidump
from minidump.minidumpfile import MinidumpFile
print('minidump members:', [n for n in dir(minidump) if 'context' in n.lower() or 'CONTEXT' in n])
print('minidumpfile members:', [n for n in dir(MinidumpFile) if 'context' in n.lower() or 'CONTEXT' in n])
try:
    import minidump.minidumpcommon as c
    print('minidumpcommon members with context:', [n for n in dir(c) if 'context' in n.lower() or 'CONTEXT' in n])
except Exception as e:
    print('minidump.minidumpcommon import error:', e)

md = MinidumpFile.parse('C:/Users/jpfau/Desktop/Project/GenesisGameEngine/Tools/external/procdump/dumps/UnitTests.exe_251229_133427.dmp')
rec = md.exception.exception_records[0]
print('rec.ThreadContext has attrs:', [n for n in dir(rec.ThreadContext) if not n.startswith('_')])
try:
    tcbytes = rec.ThreadContext.to_bytes()
    print('ThreadContext bytes len:', len(tcbytes))
except Exception as e:
    print('to_bytes failed:', e)
print('ThreadContext.Rva:', rec.ThreadContext.Rva, 'DataSize:', rec.ThreadContext.DataSize)
