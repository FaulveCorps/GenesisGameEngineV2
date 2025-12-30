import shutil
from pathlib import Path
p = Path(__file__).resolve().parent.parent / 'build.ps1'
if not p.exists():
    print('build.ps1 not found at', p)
    raise SystemExit(1)
backup = p.with_suffix('.ps1.bak')
shutil.copy2(p, backup)
print('Backup written to', backup)
lines = p.read_text(encoding='utf-8').splitlines()
new_lines = []
for line in lines:
    if line.strip() in ('```', '```powershell'):
        print('Removing fence line:', line)
        continue
    new_lines.append(line)
p.write_text('\n'.join(new_lines) + '\n', encoding='utf-8')
print('Updated build.ps1')
