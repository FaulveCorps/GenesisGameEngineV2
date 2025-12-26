#!/usr/bin/env bash
set -euo pipefail

# Check for nested .git directories excluding the top-level .git
found=0
while IFS= read -r -d '' d; do
  echo "Nested git repo found: ${d}"
  found=1
done < <(find . -type d -name .git -not -path './.git' -print0)

if [ "$found" -eq 1 ]; then
  echo "Error: nested git repositories detected. See Agenda/policies/GitPolicies.md for guidance." >&2
  exit 2
fi

echo "No nested .git directories found."
