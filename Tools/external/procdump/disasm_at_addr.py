import sys
try:
    from minidump.minidumpfile import MinidumpFile
except Exception as e:
    print('minidump import failed:', e)
    sys.exit(1)
try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
except Exception as e:
    print('capstone import failed:', e)
    sys.exit(1)

if len(sys.argv) < 3:
    print('Usage: disasm_at_addr.py <dumpfile> <addr_hex> [bytes_to_show]')
    sys.exit(1)

dumpfile = sys.argv[1]
addr = int(sys.argv[2], 16)
bytes_to_show = int(sys.argv[3]) if len(sys.argv) > 3 else 64

md = MinidumpFile.parse(dumpfile)
# find memory segment containing addr
seg = None
# different minidump versions expose memory segments under different attributes
mem_segs = None
for attr in ('memory_segments','memory_segments_64','memory','memory_list'):
    if hasattr(md, attr) and getattr(md, attr) is not None:
        mem_segs = getattr(md, attr)
        break
if mem_segs is None:
    print('No memory segments attribute found on minidump object; available:', [n for n in dir(md) if 'memory' in n.lower()])
    sys.exit(1)
# mem_segs may be an object containing a list; try common properties
seg_list = None
if hasattr(mem_segs, '__iter__'):
    seg_list = list(mem_segs)
elif hasattr(mem_segs, 'memory_segments'):
    seg_list = mem_segs.memory_segments
elif hasattr(mem_segs, 'segments'):
    seg_list = mem_segs.segments
elif hasattr(mem_segs, 'list'):
    seg_list = mem_segs.list
elif hasattr(mem_segs, 'data'):
    seg_list = mem_segs.data
else:
    print('Could not iterate memory segments; dir:', dir(mem_segs))
    sys.exit(1)

for s in seg_list:
    # start/size attr names may vary
    start = int(getattr(s, 'StartOfMemoryRange', getattr(s, 'start', getattr(s, 'start_address', 0))))
    size = int(getattr(s, 'size', getattr(s, 'Size', getattr(s, 'data_size', 0))))
    if start <= addr < start + size:
        seg = s
        break
if not seg:
    # fallback: try to search memory by reading all segments and checking ranges
    print('Address not found in dump memory segments')
    sys.exit(1)

offset = addr - int(seg.start)
data = seg.read(int(seg.size))
# ensure enough bytes
start_off = max(0, offset - 16)
end_off = min(len(data), offset + bytes_to_show)
code = data[start_off:end_off]

cs = Cs(CS_ARCH_X86, CS_MODE_64)
cs.detail = True
print('Disassembling around', hex(addr), 'from segment', hex(int(seg.start)), 'length', len(code))
for i in cs.disasm(code, int(seg.start) + start_off):
    mark = '>' if i.address == addr else ' '
    print(f"{mark} 0x{i.address:x}: {i.mnemonic} {i.op_str}")

# print raw bytes
print('\nRaw bytes:', code.hex())
