#!/usr/bin/env bash
# Move the upstream/ submodule to a new OpenMPT release, then rebuild and test.
#
#   scripts/update-upstream.sh              # list the newest upstream releases
#   scripts/update-upstream.sh OpenMPT-1.32.13.00
#
# After a successful run, commit the submodule bump:
#   git commit -am "Update upstream to <tag>"
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

if [[ $# -eq 0 ]]; then
  echo "Current: $(git -C upstream describe --tags --exact-match 2>/dev/null || git -C upstream rev-parse --short HEAD)"
  echo "Newest upstream releases:"
  git ls-remote --tags --refs https://github.com/OpenMPT/openmpt.git 'OpenMPT-*' \
    | sed 's#.*refs/tags/##' | sort -V | tail -5
  exit 0
fi

tag="$1"
# Drop our applied patches before switching; CMake re-applies them.
git -C upstream checkout -q -- .
git -C upstream fetch --depth 1 origin tag "$tag"
git -C upstream checkout -q "$tag"

cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure

echo
echo "upstream is now at $tag. Review, then: git add upstream && git commit -m \"Update upstream to $tag\""
