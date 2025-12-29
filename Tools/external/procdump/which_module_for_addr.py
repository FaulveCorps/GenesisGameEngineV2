from minidump.minidumpfile import MinidumpFile
import sys
if len(sys.argv) < 3:
    print('usage: which_module_for_addr.py <dump> <addr_hex>')
    sys.exit(1)
md = MinidumpFile.parse(sys.argv[1])
addr = int(sys.argv[2], 16)
for m in md.modules.modules:
    base = int(m.baseaddress)
    size = int(m.size)
    if base <= addr < base + size:
        print('addr', hex(addr), 'in module', getattr(m,'name',None) or getattr(m,'module_name',None), 'base', hex(base), 'size', hex(size))
        sys.exit(0)
print('addr not in any module')