from minidump.minidumpfile import MinidumpFile
import sys, struct

if len(sys.argv) < 2:
    print('usage: scan_thread_stacks.py <dump>')
    sys.exit(1)

md = MinidumpFile.parse(sys.argv[1])
mods = []
if hasattr(md.modules, 'modules'):
    mods = md.modules.modules
elif hasattr(md.modules, 'list'):
    mods = md.modules.list

mod_ranges = []
for m in mods:
    try:
        base = int(m.baseaddress)
        size = int(m.size)
        name = getattr(m, 'name', None) or getattr(m, 'module_name', None)
        mod_ranges.append((base, base+size, name))
    except Exception:
        pass

print('Found modules:', len(mod_ranges))

threads = []
try:
    threads = md.threads.threads
except Exception as e:
    print('No threads list in dump or error:', e)
    sys.exit(1)

for t in threads:
    tid = getattr(t, 'ThreadId', None)
    print('\nThreadId:', tid)
    # print stack summary
    if hasattr(t, 'Stack'):
        st = t.Stack
        try:
            start = int(st.StartOfMemoryRange)
        except Exception:
            print('  Stack start not available')
            continue
        # Attempt to get stack bytes directly
        data = None
        if hasattr(st, 'Memory'):
            data = st.Memory
        else:
            # find a memory segment covering the start
            for seg in getattr(md, 'memory_segments_64', []).memory_segments:
                try:
                    if seg.start_virtual_address <= start < seg.start_virtual_address + seg.size:
                        offset = start - seg.start_virtual_address
                        # attempt to read remaining bytes from segment
                        try:
                            full = seg.read()
                            data = full[offset:]
                        except Exception:
                            # fallback: try aread/read methods
                            try:
                                full = seg.aread()
                                data = full[offset:]
                            except Exception:
                                data = None
                        break
                except Exception:
                    pass
        if not data:
            print('  No stack bytes available for thread')
            continue
        print('  Stack start:', hex(start), 'len:', len(data))
        words = []
        nwords = min(1024, len(data) // 8)
        for i in range(nwords):
            val = struct.unpack_from('<Q', data, i*8)[0]
            words.append(val)
        # find words that fall into modules
        found = []
        for i, w in enumerate(words):
            if w >= 0x10000 and w < 0xffffffffffffffff:
                for base, top, name in mod_ranges:
                    if base <= w < top:
                        found.append((i, w, name))
                        break
        print('  Words in module ranges (first 20):')
        for idx, w, name in found[:20]:
            print('   idx', idx, 'val', hex(w), 'module', name)
    else:
        print('  No stack info')

print('\nDone')
