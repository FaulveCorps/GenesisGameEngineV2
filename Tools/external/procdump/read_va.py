"""read_va.py

Read bytes at a given virtual address from a minidump file and print a hex preview.
Usage:
    python read_va.py <dump> <vaddr_hex> <length>
"""
import sys
from minidump.minidumpfile import MinidumpFile

if len(sys.argv) < 4:
    print('usage: read_va.py <dump> <vaddr_hex> <length>')
    sys.exit(1)

path = sys.argv[1]
vaddr = int(sys.argv[2], 16)
length = int(sys.argv[3], 0)

md = MinidumpFile.parse(path)
ms = md.memory_segments_64
segs = None
if hasattr(ms, 'memory_segments'):
    segs = ms.memory_segments
elif hasattr(ms, 'segments'):
    segs = ms.segments
else:
    # try .to_table fallback
    try:
        tbl = ms.to_table()
        class Seg:
            def __init__(self, d):
                self.start_virtual_address = d.get('VA Start') or d.get('Start')
                self.size = d.get('Size')
                self.start_file_address = d.get('RVA')
                self._raw = d
            def read(self):
                # fallback not supported
                raise RuntimeError('no raw read')
        segs = [Seg(r) for r in tbl]
    except Exception as e:
        print('No iterable memory segments found:', e)
        sys.exit(2)

for s in segs:
    try:
        start = int(getattr(s, 'start_virtual_address', getattr(s, 'StartOfMemoryRange', getattr(s, 'start', 0))))
        size = int(getattr(s, 'size', getattr(s, 'Size', getattr(s, 'data_size', 0))))
    except Exception:
        continue
    if start <= vaddr < start + size:
        # Attempt to read the entire segment via s.read() if available
        # Prefer reading directly from file using the segment's file RVA (more robust)
        seg_rva = int(getattr(s, 'start_file_address', getattr(s, 'startFileAddress', getattr(s, 'start', 0))))
        seg_va = int(getattr(s, 'start_virtual_address', getattr(s, 'StartOfMemoryRange', getattr(s, 'start', 0))))
        seg_size = int(getattr(s, 'size', getattr(s, 'Size', getattr(s, 'data_size', 0))))
        if seg_rva and seg_size:
            file_offset = seg_rva + (vaddr - seg_va)
            try:
                with open(path, 'rb') as f:
                    f.seek(file_offset)
                    data = f.read(length)
                hexs = ' '.join(f"{b:02X}" for b in data)
                print(f'Found in segment start={hex(seg_va)} size={hex(seg_size)} file_offset={hex(file_offset)}')
                print(hexs)
                sys.exit(0)
            except Exception as e:
                print('Failed to read dump file at file_offset:', e)
                sys.exit(3)
        # Fallback: try to use segment read if available
        try:
            try:
                seg_size = int(getattr(s, 'size', getattr(s, 'Size', 0)))
                data = s.read(seg_size)
            except TypeError:
                data = s.read()
            off = vaddr - start
            chunk = data[off:off+length]
            hexs = ' '.join(f"{b:02X}" for b in chunk)
            print(f'Found in segment start={hex(start)} size={hex(size)}')
            print(hexs)
            sys.exit(0)
        except Exception as e:
            print('Segment found but read() not available or failed:', e)
            sys.exit(3)

print('Address not found in any dump memory segment')
sys.exit(4)
