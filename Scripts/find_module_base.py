import sys
from minidump.minidumpfile import MinidumpFile
if len(sys.argv) < 2:
    print('usage: find_module_base.py <dump>')
    sys.exit(1)
md = MinidumpFile.parse(sys.argv[1])
rec = md.exception.exception_records[0]
try:
    exaddr = int(rec.ExceptionRecord.ExceptionAddress)
except Exception:
    exaddr = int(rec.ExceptionAddress)
for m in md.modules.modules:
    base = int(m.baseaddress)
    size = int(m.size)
    if base <= exaddr < base + size:
        print('base:', hex(base), 'name:', getattr(m,'name',None))
        break
else:
    print('module not found')
