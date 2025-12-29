from minidump.minidumpfile import MinidumpFile
md=MinidumpFile.parse(r'C:\\Users\\jpfau\\Desktop\\Project\\GenesisGameEngine\\Tools\\external\\procdump\\dumps\\token_crash_20251229_142440.dmp')
out_lines = []
for m in md.modules.modules:
    name = getattr(m, 'name', None) or getattr(m, 'module_name', None)
    if name and 'UnitTests.exe' in name:
        out_lines.append('module %s' % name)
        out_lines.append('base %s' % hex(int(m.baseaddress)))
        out_lines.append('size %s' % int(m.size))
        out_lines.append('timestamp %s' % int(getattr(m, 'timestamp', 0)))
        out_lines.append('timestamp raw %s' % (getattr(m, 'timestamp', None)))
        if hasattr(m, 'cv_record'):
            out_lines.append('cv_record: %s' % (m.cv_record))
        if hasattr(m, 'versioninfo'):
            out_lines.append('versioninfo: %s' % (m.versioninfo))
        break
else:
    out_lines.append('UnitTests.exe module not found')

with open(r'C:\Users\jpfau\Desktop\Project\GenesisGameEngine\Tools\external\procdump\print_module_info_out.txt','w') as f:
    f.write('\n'.join(out_lines))