#!/bin/sh
# mfc-qt/ is a copy of the mfc-qt repository (github.com/codicusmaximus/
# mfc-qt), which is where it is developed. This replaces the copy with that
# repository's main branch (or the branch, tag or commit given) and stages
# the change for a commit.
#   scripts/update-mfc-qt.sh [ref]          (MFCQT_REPO: another clone URL)
# It stops if the copy here has changes the mfc-qt repository does not
# have; move those there first (see docs/BUILDING.md), or pass --force to
# drop them.
set -e
force=
if [ "$1" = --force ]; then force=1; shift; fi
ref=${1:-main}
repo=${MFCQT_REPO:-https://github.com/codicusmaximus/mfc-qt}
cd "$(git rev-parse --show-toplevel)"
if ! git diff --quiet HEAD -- mfc-qt || ! git diff --cached --quiet -- mfc-qt; then
	echo "mfc-qt/ has uncommitted changes; commit or drop them first." >&2
	exit 1
fi
git fetch --quiet "$repo" "$ref"
new=$(git rev-parse FETCH_HEAD)
current=$(git rev-parse HEAD:mfc-qt)
# The copy is unchanged if some commit of the repository has exactly its files.
if [ -z "$force" ] && ! git log --format=%T FETCH_HEAD | grep -qx "$current"; then
	echo "mfc-qt/ has changes that $repo ($ref) does not have." >&2
	echo "Move them there first: git format-patch --relative=mfc-qt <commits> -- mfc-qt" >&2
	echo "makes patches that git am applies in an mfc-qt clone. Or pass --force." >&2
	exit 1
fi
if [ "$(git rev-parse FETCH_HEAD^{tree})" = "$current" ]; then
	echo "mfc-qt/ is already the same as ${new%"${new#???????}"}."
	exit 0
fi
git rm -rq mfc-qt
git read-tree --prefix=mfc-qt/ -u FETCH_HEAD
echo "mfc-qt/ is now mfc-qt ${new%"${new#???????}"}. Commit it, for example:"
echo "  git commit -m 'Update mfc-qt to ${new%"${new#???????}"}'"
