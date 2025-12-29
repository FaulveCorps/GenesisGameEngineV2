import sys, struct
from pathlib import Path

if len(sys.argv) < 3:
    print('Usage: pe_offset_to_va.py <pefile> <file_offset_decimal>')
    sys.exit(1)

p = Path(sys.argv[1])
off = int(sys.argv[2])

b = p.read_bytes()
# DOS header e_lfanew at offset 0x3C
e_lfanew = struct.unpack_from('<I', b, 0x3C)[0]
pe_sig = b[e_lfanew:e_lfanew+4]
if pe_sig != b'PE\x00\x00':
    print('Not a PE file')
    sys.exit(1)

# File header
file_header_off = e_lfanew + 4
machine, num_sections, time_date_stamp, ptr_sym_table, num_symbols, size_opt_header, characteristics = struct.unpack_from('<HHIIIHH', b, file_header_off)
opt_header_off = file_header_off + 20
# Determine PE32 or PE32+
magic = struct.unpack_from('<H', b, opt_header_off)[0]
if magic == 0x20b:
    # PE32+ (x64)
    image_base = struct.unpack_from('<Q', b, opt_header_off + 24)[0]
    size_of_opt_header = size_opt_header
    section_table_off = opt_header_off + size_opt_header
elif magic == 0x10b:
    # PE32 (x86)
    image_base = struct.unpack_from('<I', b, opt_header_off + 28)[0]
    size_of_opt_header = size_opt_header
    section_table_off = opt_header_off + size_opt_header
else:
    print('Unknown PE magic', hex(magic))
    sys.exit(1)

rva = None
sec_name = None
for i in range(num_sections):
    sec_off = section_table_off + i * 40
    name = b[sec_off:sec_off+8].split(b'\x00',1)[0].decode(errors='replace')
    virtual_size, virtual_address, size_of_raw_data, pointer_to_raw_data = struct.unpack_from('<IIII', b, sec_off+8)
    # print(name, 'VA', hex(virtual_address), 'VS', virtual_size, 'RawOff', pointer_to_raw_data, 'RawSz', size_of_raw_data)
    if pointer_to_raw_data <= off < pointer_to_raw_data + size_of_raw_data:
        rva = virtual_address + (off - pointer_to_raw_data)
        sec_name = name
        break

if rva is None:
    print('Offset not in any section')
    sys.exit(1)

va = image_base + rva
print('file offset', off, 'section', sec_name, 'rva', hex(rva), 'va', hex(va), 'image_base', hex(image_base))
