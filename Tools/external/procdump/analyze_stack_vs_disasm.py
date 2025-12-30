import sys
from minidump.minidumpfile import MinidumpFile

if len(sys.argv) < 3:
    print('Usage: analyze_stack_vs_disasm.py <dumpfile> <disasm_file>')
    sys.exit(1)

md = MinidumpFile.parse(sys.argv[1])
modules = []
if hasattr(md.modules, 'modules'):
    modules = md.modules.modules
elif hasattr(md.modules, 'list'):
    modules = md.modules.list
else:
    print('No modules list in dump')
    sys.exit(1)

# build module ranges
mod_ranges = []
for m in modules:
    try:
        base = int(m.baseaddress)
        size = int(m.size)
        name = getattr(m, 'name', None) or getattr(m, 'module_name', None) or '<unknown>'
        mod_ranges.append((base, base+size, name))
    except Exception:
        continue

# find exception thread
ex_tid = None
try:
    recs = md.exception.exception_records
    if len(recs) > 0:
        rec = recs[0]
        if hasattr(rec, 'ThreadId'):
            ex_tid = int(rec.ThreadId)
        elif hasattr(rec, 'ThreadId'):
            ex_tid = int(rec.ThreadId)
except Exception:
    pass

if not ex_tid:
    print('No exception thread found')
    sys.exit(1)

print('Exception thread id:', ex_tid)

# find the thread in md.threads
thrlist = md.threads.threads
target_thr = None
for thr in thrlist:
    tid = int(getattr(thr, 'ThreadId'))
    if tid == ex_tid:
        target_thr = thr
        break

if not target_thr:
    print('Exception thread not found in thread list')
    sys.exit(1)

st = target_thr.Stack
print('Stack attrs:', ','.join([n for n in dir(st) if not n.startswith('_')]))
# Different minidump versions expose stack bytes differently; try multiple accessors
stack_bytes = None
start = int(st.StartOfMemoryRange)
if hasattr(st, 'Memory'):
    stack_bytes = st.Memory
elif hasattr(st, 'to_bytes'):
    try:
        stack_bytes = st.to_bytes()
    except Exception as e:
        print('st.to_bytes() failed:', e)
else:
    # last resort: try reading via Rva + DataSize from the dump buffer
    try:
        rva = int(st.Rva)
        size = int(st.DataSize)
        # limit the read to first 64 KiB to avoid huge allocations
        max_read = 64 * 1024
        stack_bytes = md.parse_bytes(rva, min(size, max_read))
    except Exception as e:
        print('Failed to extract stack bytes via Rva/DataSize:', e)

if not stack_bytes:
    print('No stack bytes could be extracted')
    sys.exit(1)

import struct
words = []
for i in range(0, min(len(stack_bytes), 4096), 8):
    val = struct.unpack_from('<Q', stack_bytes, i)[0]
    words.append((start + i, val))

hits = []
for addr, val in words:
    for base, top, name in mod_ranges:
        if base <= val < top:
            hits.append((addr, val, base, top, name))
            break

if not hits:
    print('No module addresses found on stack (within first 4096 bytes)')
else:
    print('Found', len(hits), 'addresses on stack that fall inside modules (showing first 40):')
    for h in hits[:40]:
        print('stack@', hex(h[0]), 'val', hex(h[1]), 'module', h[4])

# open disasm file and map addresses to regions
import os
disasm = open(sys.argv[2], 'r', encoding='latin1').read()

def find_address_in_disasm(addr):
    # addr is 64-bit VA
    s = ' %016X:' % addr
    idx = disasm.find(s)
    if idx >= 0:
        # show some context lines
        start = disasm.rfind('\n', 0, idx - 200)
        if start < 0: start = 0
        end = disasm.find('\n', idx + 400)
        if end < 0: end = min(len(disasm), idx + 400)
        return disasm[start:end]
    return None

for h in hits[:40]:
    addr = h[1]
    ctx = find_address_in_disasm(addr)
    print('\n---\naddress', hex(addr), 'module', h[4])
    if ctx:
        print(ctx)
    else:
        print('No disasm match for address', hex(addr))

print('\nDone')
