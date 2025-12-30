from minidump.minidumpfile import MinidumpFile
import sys, struct

if len(sys.argv) < 3:
    print('Usage: stack_scan_from_dump.py <dumpfile> <disasm_file>')
    sys.exit(1)

md = MinidumpFile.parse(sys.argv[1])
ms = md.memory_segments_64
# get exception ThreadId
try:
    ex_tid = int(md.exception.exception_records[0].ThreadId)
except Exception as e:
    print('Failed to get exception thread id:', e)
    sys.exit(1)

print('Exception thread id:', ex_tid)

thr = None
for t in md.threads.threads:
    if int(getattr(t, 'ThreadId')) == ex_tid:
        thr = t
        break
if not thr:
    print('Could not find thread')
    sys.exit(1)

st = thr.Stack
print('Stack Rva/DataSize:', getattr(st, 'Rva', None), getattr(st, 'DataSize', None), 'StartOfMemoryRange', getattr(st, 'StartOfMemoryRange', None))

# Determine the file offset for the stack (find the memory segment that contains StartOfMemoryRange)
start_va = int(getattr(st, 'StartOfMemoryRange'))
stack_size = int(getattr(st, 'DataSize'))
max_read = 64 * 1024
read_len = min(stack_size, max_read)
segment_found = None
print('Enumerating segments to find stack:')
for seg in ms.memory_segments:
    try:
        # inspect segment object to find available attributes
        print('SEG DIR:', [n for n in dir(seg) if not n.startswith('_')])
        # many versions expose attributes differently; attempt to read common ones
        base = None
        size_s = None
        rva_seg = None
        if hasattr(seg, 'baseaddress'):
            base = int(seg.baseaddress)
        if hasattr(seg, 'base'):
            try: base = int(seg.base)
            except: pass
        if hasattr(seg, 'start'):
            try: base = int(seg.start)
            except: pass
        if hasattr(seg, 'size'):
            size_s = int(seg.size)
        if hasattr(seg, 'region_size'):
            size_s = int(seg.region_size)
        if hasattr(seg, 'rva'):
            rva_seg = int(seg.rva)
        if hasattr(seg, 'rva_offset'):
            rva_seg = int(seg.rva_offset)
        if base is not None and size_s is not None and rva_seg is not None:
            print('seg base', hex(base), 'size', hex(size_s), 'rva', hex(rva_seg))
            if base <= start_va < base + size_s:
                segment_found = (base, size_s, rva_seg)
                break
    except Exception as e:
        print('segment parse error', e)
        continue

if not segment_found:
    print('Could not locate memory segment containing stack start', hex(start_va))
    sys.exit(1)

base, size_s, rva_seg = segment_found
print('stack segment base', hex(base), 'size', size_s, 'rva_seg', hex(rva_seg))
# file offset to read = rva_seg + (start_va - base)
file_offset = rva_seg + (start_va - base)
with open(sys.argv[1], 'rb') as f:
    f.seek(file_offset)
    stack_bytes = f.read(read_len)

modules = []
mods = md.modules
if hasattr(md.modules, 'modules'):
    modules = md.modules.modules
else:
    modules = md.modules.list

mod_ranges = []
for m in modules:
    try:
        base = int(m.baseaddress)
        size_m = int(m.size)
        name = getattr(m, 'name', None) or getattr(m, 'module_name', None) or '<unknown>'
        mod_ranges.append((base, base+size_m, name))
    except Exception:
        continue

found = []
for i in range(0, len(stack_bytes) - 8, 8):
    val = struct.unpack_from('<Q', stack_bytes, i)[0]
    for base, top, name in mod_ranges:
        if base <= val < top:
            found.append((i, val, name))
            break

print('Found', len(found), 'module addresses on stack (first 100 shown):')
for i, val, name in found[:100]:
    print('stack+{0:#x} -> {1:#x} in {2}'.format(i, val, name))

# try to map addresses to disasm by textual search
disasm = open(sys.argv[2], 'r', encoding='latin1').read()
for i, val, name in found[:50]:
    s = ' %016X:' % val
    idx = disasm.find(s)
    print('\nAddress', hex(val), 'found in module', name)
    if idx >= 0:
        start = disasm.rfind('\n', 0, idx - 200)
        if start < 0: start = 0
        end = disasm.find('\n', idx + 400)
        if end < 0: end = min(len(disasm), idx + 400)
        print(disasm[start:end])
    else:
        print('No disasm context found for', hex(val))

print('\nDone')
