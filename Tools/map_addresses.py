import sys
from minidump.minidumpfile import MinidumpFile
import re

if len(sys.argv) < 3:
    print('Usage: map_addresses.py <dumpfile> <logfile>')
    sys.exit(1)

dump_path = sys.argv[1]
log_path = sys.argv[2]
md = MinidumpFile.parse(dump_path)
mlist = md.modules.to_table()
modules = []
for m in mlist:
    if not m or m[0] == 'Module name':
        continue
    name = m[0]
    base = int(m[1], 16)
    end = int(m[3], 16)
    modules.append((base, end, name))

# Read addresses from log file
addrs = []
with open(log_path, 'r', encoding='utf-8') as f:
    lines = f.readlines()
for i, line in enumerate(lines):
    if 'Backtrace (frames=' in line:
        # subsequent lines contain addresses
        j = i+1
        while j < len(lines) and lines[j].strip():
            m = re.search(r'([0-9A-Fa-fx]+)', lines[j])
            if m:
                addrs.append(int(m.group(1), 16))
            j += 1
        break

print('Found {} addresses'.format(len(addrs)))
for a in addrs:
    found = False
    for base, end, name in modules:
        if base <= a < end:
            print(hex(a), '->', name, hex(a-base))
            found = True
            break
    if not found:
        print(hex(a), '-> <unknown>')
