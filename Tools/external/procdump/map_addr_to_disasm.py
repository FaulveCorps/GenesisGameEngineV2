from minidump.minidumpfile import MinidumpFile
import sys, os
if len(sys.argv) < 3:
    print('usage: map_addr_to_disasm.py <dump> <address_hex> [disasm_file]')
    sys.exit(1)
path = sys.argv[1]
addr = int(sys.argv[2], 16)
if len(sys.argv) >= 4:
    disasm = open(sys.argv[3], 'r', encoding='latin1').read()
else:
    disasm = None

md = MinidumpFile.parse(path)
modules = md.modules.modules
mod_found = None
for m in modules:
    base = int(m.baseaddress)
    size = int(m.size)
    if base <= addr < base + size:
        mod_found = (m, base, size)
        break
if not mod_found:
    print('No module contains address', hex(addr))
    sys.exit(1)
mod, base, size = mod_found
print('Address', hex(addr), 'found in module', getattr(mod,'name',None) or getattr(mod,'module_name',None), 'base', hex(base), 'size', hex(size))
off = addr - base
# Map to disasm address space which often uses base 0x140000000
disasm_va = 0x140000000 + off
print('Module-relative offset', hex(off), 'disasm VA (likely) ', hex(disasm_va))
if disasm:
    s = ' %016X:' % disasm_va
    idx = disasm.find(s)
    if idx >= 0:
        # find a likely function label by searching for the nearest blank line then the following non-empty line
        label_start = disasm.rfind('\n\n', 0, idx)
        if label_start < 0:
            label_start = max(0, idx - 2000)
        else:
            label_start += 2
        start = max(0, label_start)
        end = disasm.find('\n', idx + 1200)
        if end < 0: end = min(len(disasm), idx + 1200)
        print('\nContext around disasm matches (expanded):')
        print(disasm[start:end])
    else:
        print('No disasm match for', hex(disasm_va))
else:
    print('No disasm file provided')
