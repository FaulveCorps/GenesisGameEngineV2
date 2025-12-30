from minidump.minidumpfile import MinidumpFile
import struct, sys
import os

if len(sys.argv) < 2:
    print('usage: parse_ctx_for_dump.py <dump>')
    sys.exit(1)

path = sys.argv[1]
md = MinidumpFile.parse(path)
rec = md.exception.exception_records[0]
# try multiple ways to get exception address
exaddr = None
try:
    if hasattr(rec, 'ExceptionAddress'):
        exaddr = int(rec.ExceptionAddress)
    elif hasattr(rec, 'ExceptionRecord') and hasattr(rec.ExceptionRecord, 'ExceptionAddress'):
        exaddr = int(rec.ExceptionRecord.ExceptionAddress)
    else:
        print('Cannot find exception address in record')
except Exception as e:
    print('Error extracting exception address:', e)

ctx_rva = int(rec.ThreadContext.Rva)
ctx_size = int(rec.ThreadContext.DataSize)
print('ThreadContext rva', ctx_rva, 'size', ctx_size)
with open(path, 'rb') as f:
    f.seek(ctx_rva)
    ctxbytes = f.read(ctx_size)
print('Read ctx bytes len', len(ctxbytes))

if exaddr is not None:
    print('ExceptionAddress:', hex(exaddr))
    pattern = struct.pack('<Q', exaddr)
    idx = ctxbytes.find(pattern)
    print('index of RIP pattern:', idx)
    if idx >= 0:
        # collect nearby qwords
        start = max(0, idx - 16*8)
        end = min(len(ctxbytes), idx + 16*8)
        words = []
        for i in range(start, end, 8):
            if i+8 <= len(ctxbytes):
                val = struct.unpack_from('<Q', ctxbytes, i)[0]
                words.append((i, val))
        for off, val in words:
            print(hex(off), hex(val))
        # find rip index
        rip_index = None
        for wi, (off, val) in enumerate(words):
            if val == exaddr:
                rip_index = wi
                break
        if rip_index is not None:
            base_index = rip_index - 16  # Rax..R15, then Rip
            regs = ['Rax','Rcx','Rdx','Rbx','Rsp','Rbp','Rsi','Rdi','R8','R9','R10','R11','R12','R13','R14','R15','Rip']
            print('\nAttempt to map registers:')
            for i, name in enumerate(regs):
                j = base_index + i
                if 0 <= j < len(words):
                    off, val = words[j]
                    print(name, hex(val))
                else:
                    print(name, 'N/A')
    else:
        print('RIP pattern not found in context bytes')
else:
    print('No exception address available')

# scan for values that fall into module ranges
mods = []
for m in md.modules.modules:
    try:
        base = int(m.baseaddress)
        size = int(m.size)
        mods.append((base, base+size, getattr(m,'name',None) or getattr(m,'module_name',None)))
    except Exception:
        pass

print('\nLooking for words in context that fall into module ranges:')
for i in range(0, min(len(ctxbytes), 1024), 8):
    v = struct.unpack_from('<Q', ctxbytes, i)[0]
    for base, top, name in mods:
        if base <= v < top:
            print('ctx offset', hex(i), 'word', hex(v), 'module', name)
            break
