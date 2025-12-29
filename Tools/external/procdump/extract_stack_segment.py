from minidump.minidumpfile import MinidumpFile
import sys, struct
if len(sys.argv) < 2:
    print('usage: extract_stack_segment.py <dump>')
    sys.exit(1)
md = MinidumpFile.parse(sys.argv[1])
rec = md.exception.exception_records[0]
ex_tid = int(rec.ThreadId)
thr = None
for t in md.threads.threads:
    if int(t.ThreadId) == ex_tid:
        thr = t
        break
if not thr:
    print('thread not found'); sys.exit(1)
st = thr.Stack
start = int(st.StartOfMemoryRange)
size = int(st.DataSize)
print('Stack start', hex(start), 'size', size)
# find segment containing start
seg_found = None
# memory segments may be exposed directly on md as memory_segments or memory_segments_64
segments = None
# choose whichever memory segments struct is available
if hasattr(md, 'memory_segments') and md.memory_segments:
    segments = md.memory_segments
elif hasattr(md, 'memory_segments_64') and md.memory_segments_64:
    ms = md.memory_segments_64
    # try to get an iterable list of segment objects
    if hasattr(ms, 'memory_segments'):
        segments = ms.memory_segments
    elif hasattr(ms, 'segments'):
        segments = ms.segments
    else:
        try:
            tbl = ms.to_table()
            # table entries are dict-like with 'Start' and 'RVA' etc - wrap into simple objects
            class Seg:
                def __init__(self, d):
                    self.start_virtual_address = d.get('VA Start') or d.get('Start')
                    self.size = d.get('Size')
                    self.start_file_address = d.get('RVA')
                def __repr__(self):
                    return f"Seg({hex(self.start_virtual_address)}, size={self.size}, rva={hex(self.start_file_address)})"
            segments = [Seg(r) for r in tbl]
        except Exception as e:
            print('to_table failed on memory_segments_64:', e)
            segments = None
else:
    print('No memory segments available on dump')
    sys.exit(1)

if not segments:
    print('No memory segment list found')
    sys.exit(1)

for seg in segments:
    try:
        seg_start = int(getattr(seg, 'start_virtual_address'))
        seg_size = int(getattr(seg, 'size'))
        if seg_start <= start < seg_start + seg_size:
            seg_found = seg
            break
    except Exception:
        continue
if not seg_found:
    print('No segment contains stack start')
    sys.exit(1)
seg_rva = int(seg_found.start_file_address)
seg_va = int(seg_found.start_virtual_address)
seg_size = int(seg_found.size)
print('Segment VA', hex(seg_va), 'RVA', hex(seg_rva), 'size', hex(seg_size))
# compute file offset where stack data begins
file_offset = seg_rva + (start - seg_va)
read_len = min(64*1024, seg_size - (start - seg_va))
print('Reading', read_len, 'bytes at file offset', hex(file_offset))
with open(sys.argv[1], 'rb') as f:
    f.seek(file_offset)
    data = f.read(read_len)
# print first 256 bytes as qwords
for i in range(0, min(len(data), 256), 8):
    val = struct.unpack_from('<Q', data, i)[0]
    print(hex(start + i), hex(val))

# Attempt to extract RSP from thread context bytes and print nearby stack words
ctx_rva = int(rec.ThreadContext.Rva)
ctx_size = int(rec.ThreadContext.DataSize)
with open(sys.argv[1], 'rb') as f:
    f.seek(ctx_rva)
    ctxbytes = f.read(ctx_size)
# find exception address in context
try:
    if hasattr(rec, 'ExceptionAddress'):
        exaddr = int(rec.ExceptionAddress)
    elif hasattr(rec, 'ExceptionRecord') and hasattr(rec.ExceptionRecord, 'ExceptionAddress'):
        exaddr = int(rec.ExceptionRecord.ExceptionAddress)
    else:
        exaddr = None
except Exception:
    exaddr = None

if exaddr is not None:
    patt = struct.pack('<Q', exaddr)
    idx = ctxbytes.find(patt)
    if idx >= 0:
        # collect nearby qwords
        start_ctx = max(0, idx - 16*8)
        words = []
        for i in range(start_ctx, min(len(ctxbytes), idx + 16*8), 8):
            if i+8 <= len(ctxbytes):
                val = struct.unpack_from('<Q', ctxbytes, i)[0]
                words.append((i, val))
        # find rip index
        rip_index = None
        for wi, (off, val) in enumerate(words):
            if val == exaddr:
                rip_index = wi
                break
        if rip_index is not None:
            base_index = rip_index - 16  # Rax..R15, then Rip
            names = ['Rax','Rcx','Rdx','Rbx','Rsp','Rbp','Rsi','Rdi','R8','R9','R10','R11','R12','R13','R14','R15','Rip']
            regmap = {}
            for i, name in enumerate(names):
                j = base_index + i
                if j >=0 and j < len(words):
                    off, v = words[j]
                    regmap[name] = v
            if 'Rsp' in regmap:
                rsp = regmap['Rsp']
                print('\nComputed RSP from context:', hex(rsp))
                # compute offset into data
                if start <= rsp < start + len(data):
                    off = rsp - start
                    # print a larger window from RSP including offsets used by the crash site
                    print('Printing 512 bytes at RSP (for offsets used by crash):')
                    for i in range(off, min(off+512, len(data)), 8):
                        val = struct.unpack_from('<Q', data, i)[0]
                        print(hex(start + i), hex(val))
                    # print the specific words at RSP+0xE0 and RSP+0xF8
                    for delta in (0xE0, 0xF8):
                        idx2 = off + delta
                        if 0 <= idx2 < len(data):
                            v = struct.unpack_from('<Q', data, idx2)[0]
                            print(f'Value at RSP+{hex(delta)} ({hex(rsp+delta)}): {hex(v)}')
                            # attempt to dereference that pointer (print first 64 bytes) if within dump segment
                            if start <= v < start + len(data):
                                inner_off = v - start
                                print(f'First 4 qwords at pointer {hex(v)}:')
                                for j in range(inner_off, min(inner_off+32, len(data)), 8):
                                    vv = struct.unpack_from('<Q', data, j)[0]
                                    print(hex(start + j), hex(vv))
                            else:
                                print(f'Pointer {hex(v)} not within the extracted stack segment; not dereferencing')
                else:
                    print('RSP not within extracted stack region')


# Scan the printed stack region for addresses that fall into module ranges and map them to disasm
# Build module ranges
mods = []
for m in md.modules.modules:
    try:
        base = int(m.baseaddress)
        size = int(m.size)
        name = getattr(m, 'name', None) or getattr(m, 'module_name', None)
        mods.append((base, base+size, name))
    except Exception:
        pass

# search for candidate return addresses in the data (8-byte aligned)
cands = set()
# prefer scanning around the RSP frame if available
if 'off' in locals():
    start_scan = max(0, off - 512)
    end_scan = min(len(data), off + 1024)
else:
    start_scan = 0
    end_scan = min(len(data), 4096)
for i in range(start_scan, end_scan, 8):
    val = struct.unpack_from('<Q', data, i)[0]
    for base, top, name in mods:
        if base <= val < top:
            cands.add(val)
            break

if cands:
    print('\nCandidate return addresses found on stack (unique):')
    for v in sorted(cands):
        print(hex(v), 'module:', next((n for (b,t,n) in mods if b <= v < t), None))
    # attempt to map them to disasm using the local disasm file if available
    disasm_path = os.path.join(os.path.dirname(__file__), 'unit_disasm.txt')
    if os.path.exists(disasm_path):
        disasm = open(disasm_path, 'r', encoding='latin1').read()
        for v in sorted(cands):
            off = v - list(filter(lambda mm: mm[0] <= v < mm[1], mods))[0][0]
            disasm_va = 0x140000000 + off
            s = ' %016X:' % disasm_va
            idx = disasm.find(s)
            if idx >= 0:
                start = disasm.rfind('\n\n', 0, idx)
                if start < 0: start = max(0, idx - 400)
                else: start += 2
                end = disasm.find('\n', idx + 400)
                if end < 0: end = min(len(disasm), idx + 400)
                print('\nDisasm for', hex(v), ':')
                print(disasm[start:end])
            else:
                print('\nNo disasm match for', hex(v))
    else:
        print('\nDisasm file not found; skipping map-to-disasm')
else:
    print('\nNo module addresses found on stack (within first region)')

print('done')
