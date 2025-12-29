import struct
from pathlib import Path
p = Path(r"C:\Users\jpfau\Desktop\Project\GenesisGameEngine\build-vs\tests\Debug\UnitTests.exe")
b = p.read_bytes()
e_lfanew = struct.unpack_from('<I', b, 0x3C)[0]
file_header_off = e_lfanew + 4
magic = struct.unpack_from('<H', b, file_header_off + 20)[0]
opt_header_off = file_header_off + 20
if magic == 0x20b:
    # PE32+
    image_base = struct.unpack_from('<Q', b, opt_header_off + 24)[0]
    size_opt_header = struct.unpack_from('<H', b, file_header_off+16)[0]
else:
    image_base = struct.unpack_from('<I', b, opt_header_off + 28)[0]
    size_opt_header = struct.unpack_from('<H', b, file_header_off+16)[0]
section_table_off = opt_header_off + size_opt_header
num_sections = struct.unpack_from('<H', b, file_header_off+2)[0]
RVA = 0x2C6B2F
for i in range(num_sections):
    sec_off = section_table_off + i*40
    name = b[sec_off:sec_off+8].split(b'\x00',1)[0].decode(errors='replace')
    virtual_size, virtual_address, size_of_raw_data, pointer_to_raw_data = struct.unpack_from('<IIII', b, sec_off+8)
    if virtual_address <= RVA < virtual_address + virtual_size:
        file_offset = pointer_to_raw_data + (RVA - virtual_address)
        print('RVA', hex(RVA), 'is in section', name, 'file offset', file_offset, 'section VA', hex(virtual_address))
        break
else:
    print('RVA not found')
