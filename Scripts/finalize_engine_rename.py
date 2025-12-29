import os
import sys

repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
tmp = os.path.join(repo_root, 'Engine', 'Core', 'include', 'ENGINE_tmp')
dst = os.path.join(repo_root, 'Engine', 'Core', 'include', 'Engine')
print(f"Tmp: {tmp}")
print(f"Dst: {dst}")
if not os.path.exists(tmp):
    print("Tmp directory not found; nothing to do")
    sys.exit(0)
if os.path.exists(dst):
    print("Dst already exists; nothing to do")
    sys.exit(0)
try:
    os.rename(tmp, dst)
    print("Rename successful: tmp -> dst")
except Exception as e:
    print("Rename failed:", e)
    sys.exit(1)

# Print resulting listing
print("Post-rename listing:")
for entry in os.listdir(os.path.join(repo_root, 'Engine', 'Core', 'include')):
    print(entry)
