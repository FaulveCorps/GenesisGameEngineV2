from pathlib import Path
s = Path('build.ps1').read_text(encoding='utf-8')
print('double quotes:', s.count('"'))
print("single quotes:", s.count("'"))
print('open braces {:', s.count('{'), 'close braces }:', s.count('}'))
lines = s.splitlines()
print('\n--- lines 70..180 ---')
for i in range(70, min(180, len(lines))):
    print(f"{i+1}: {lines[i]}")
