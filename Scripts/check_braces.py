import sys
from pathlib import Path

p = Path(r"c:\Users\jpfau\Desktop\Project\GenesisGameEngine\Editor\src\main.cpp")
s = p.read_text()

# Remove #if 0/#endif blocks (handle nested)
lines = s.splitlines()
out_lines = []
skip = 0
for i, ln in enumerate(lines):
    stripped = ln.strip()
    if stripped.startswith('#if') and '0' in stripped.split():
        skip += 1
        continue
    if stripped.startswith('#endif') and skip>0:
        skip -= 1
        continue
    if skip==0:
        out_lines.append(ln)

code = '\n'.join(out_lines)

# Now check brace balance
stack = []
for idx, ch in enumerate(code):
    if ch == '{': stack.append(idx)
    elif ch == '}':
        if not stack:
            print('Unmatched } at index', idx)
            break
        stack.pop()
else:
    if stack:
        print('Unmatched { count:', len(stack))
        # Print all unmatched positions with line numbers
        for i, pos in enumerate(stack):
            pre = code[:pos]
            ln_no = pre.count('\n')+1
            print(f' Unmatched {{ #{i+1} at index {pos} approx line {ln_no}')
    else:
        print('Braces balanced after removing #if 0 blocks')

# Show number of #if 0 and #endif in original file
ifs = sum(1 for ln in lines if ln.strip().startswith('#if') and '0' in ln)
endifs = sum(1 for ln in lines if ln.strip().startswith('#endif'))
print('#if 0 count =', ifs)
print('#endif count =', endifs)
