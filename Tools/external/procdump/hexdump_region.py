from pathlib import Path
import sys
if len(sys.argv) < 4:
    print('Usage: hexdump_region.py <file> <offset> <length>')
    sys.exit(1)
file = Path(sys.argv[1])
offset = int(sys.argv[2])
length = int(sys.argv[3])
buf = file.read_bytes()
start = max(0, offset)
end = min(len(buf), offset + length)
for i in range(start, end, 16):
    chunk = buf[i:i+16]
    hexs = ' '.join(f"{b:02X}" for b in chunk)
    print(f"{i:08X}: {hexs}")
