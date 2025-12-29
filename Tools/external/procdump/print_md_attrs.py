from minidump.minidumpfile import MinidumpFile
import sys
if len(sys.argv) < 2:
    print('usage: print_md_attrs.py <dump>')
    sys.exit(1)
md = MinidumpFile.parse(sys.argv[1])
print('top-level attrs:')
print(sorted([n for n in dir(md) if not n.startswith('_')]))
try:
    print('\nmodules attr present:', hasattr(md, 'modules'))
    if hasattr(md, 'memory_info') and md.memory_info:
        print('memory_info attrs:', sorted([n for n in dir(md.memory_info) if not n.startswith('_')]))
    else:
        print('memory_info missing or None')
    if hasattr(md, 'streams'):
        print('streams attr present')
        print('streams members:', [n for n in dir(md.streams) if not n.startswith('_')])
except Exception as e:
    print('error inspecting md:', e)
