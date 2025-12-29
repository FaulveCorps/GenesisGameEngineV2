import os
import sys

repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
src = os.path.join(repo_root, 'Engine', 'Core', 'include', 'ENGINE')
tmp = os.path.join(repo_root, 'Engine', 'Core', 'include', 'ENGINE_tmp')
dst = os.path.join(repo_root, 'Engine', 'Core', 'include', 'Engine')

print(f"Repo root: {repo_root}")
print(f"Source: {src}")
print(f"Tmp: {tmp}")
print(f"Dst: {dst}")

def safe_rename(s, d):
    if not os.path.exists(s):
        print(f"Source does not exist: {s}")
        return False
    if os.path.exists(d):
        print(f"Destination already exists: {d}")
    print(f"Renaming {s} -> {d}")
    os.rename(s, d)
    return True

# Step 1: src -> tmp
if safe_rename(src, tmp):
    print("Step1 ok")
    # Step2: tmp -> dst
    if safe_rename(tmp, dst):
        print("Step2 ok")
    else:
        print("Step2 failed")
else:
    print("Step1 failed")

print("Done")