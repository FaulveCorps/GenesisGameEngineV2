from minidump.minidumpfile import MinidumpFile
import sys, struct

md = MinidumpFile.parse(sys.argv[1])
rec = md.exception.exception_records[0]
ctx_rva = int(rec.ThreadContext.Rva)
ctx_size = int(rec.ThreadContext.DataSize)
print('ThreadContext rva', ctx_rva, 'size', ctx_size)
# read from dump file at rva offset (Rva is a file offset inside the minidump)
with open(sys.argv[1], 'rb') as f:
    f.seek(ctx_rva)
    ctxbytes = f.read(ctx_size)
print('ctx bytes len', len(ctxbytes))
# find exception address
# Exception Address handling
exaddr = None
try:
    # Try nested ExceptionRecord structure
    if hasattr(rec.ExceptionRecord, 'ExceptionRecord'):
        exaddr = int(rec.ExceptionRecord.ExceptionRecord.ExceptionAddress)
    elif hasattr(rec.ExceptionRecord, 'ExceptionAddress'):
        exaddr = int(rec.ExceptionRecord.ExceptionAddress)
    elif isinstance(rec.ExceptionRecord, int):
        # Sometimes ExceptionRecord is an int or different shape
        print('ExceptionRecord seems to be int:', rec.ExceptionRecord)
    else:
        print('Unknown ExceptionRecord structure:', type(rec.ExceptionRecord))
except Exception as e:
    print('Error extracting exception address from record:', e)

# Locate RIP pattern explicitly using the observed exception address
target_val = 0x7ff7f7f27c3f
pattern = struct.pack('<Q', target_val)
idx = ctxbytes.find(pattern)
print('index of rip pattern in context (target_val):', idx)
if idx >= 0:
    # show nearby 8-byte words
    start = max(0, idx - 160)
    end = min(len(ctxbytes), idx + 160)
    dump = []
    for i in range(start, end, 8):
        if i+8 <= len(ctxbytes):
            val = struct.unpack_from('<Q', ctxbytes, i)[0]
            dump.append((i, val))
    print('Nearby words (offset, value):')
    for off, val in dump:
        print(hex(off), hex(val))
    # find rip index
    rip_word_index = None
    for idx_word, (off, val) in enumerate(dump):
        if val == target_val:
            rip_word_index = idx_word
            break
    if rip_word_index is not None:
        # assume registers layout: Rax..R15 then Rip
        base_index = rip_word_index - 16  # Rip is after Rax..R15 (16 regs)
        names = ['Rax','Rcx','Rdx','Rbx','Rsp','Rbp','Rsi','Rdi','R8','R9','R10','R11','R12','R13','R14','R15','Rip']
        print('\nAttempted register mapping:')
        for i, name in enumerate(names):
            j = base_index + i
            if j >=0 and j < len(dump):
                off, v = dump[j]
                print(name, 'offset', hex(off), hex(v))
    else:
        print('rip index not found in dump words')
else:
    print('pattern not found in context')

# print first 64 bytes as hex
print('\nfirst 192 bytes of context:')
print(' '.join(f"{b:02X}" for b in ctxbytes[:192]))

# try to scan for probable registers inside module ranges
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

