from pathlib import Path
p = Path(r"c:\Users\jpfau\Desktop\Project\GenesisGameEngine\Editor\src\main.cpp")
s = p.read_text()

lines = s.splitlines()
# Remove #if 0 blocks
out_lines = []
skip = 0
for i, ln in enumerate(lines):
    stripped = ln.strip()
    if stripped.startswith('#if') and '0' in stripped.split():
        skip += 1
        out_lines.append(f"// [REMOVED #if 0 at line {i+1}]")
        continue
    if stripped.startswith('#endif') and skip>0:
        out_lines.append(f"// [REMOVED #endif at line {i+1}]")
        skip -= 1
        continue
    if skip==0:
        out_lines.append(ln)

# Now compute brace balance per line
balance = 0
max_depth = 0
depth_lines = []
for idx, ln in enumerate(out_lines):
    for ch in ln:
        if ch == '{':
            balance += 1
            if balance > max_depth:
                max_depth = balance
                depth_lines.append((idx+1, balance, ln.strip()))
        elif ch == '}': balance -= 1

print('Final balance:', balance)
print('Max depth reached:', max_depth)
print('\nSample lines where depth increased:')
for ln_no, d, ln in depth_lines[:30]:
    print(f'{ln_no:5d}: depth={d:2d}: {ln}')

# show last 80 lines of file for context
print('\nLast 80 lines:')
for i in range(max(0, len(out_lines)-80), len(out_lines)):
    print(f'{i+1:5d}: {out_lines[i]}')
