#!/bin/sh
# Checks out psxrecomp (a submodule) at the pinned commit and applies the local runtime hooks.
set -e
cd "$(dirname "$0")"
git submodule update --init --recursive
want=$(cat patches/psxrecomp-commit.txt)
have=$(git -C psxrecomp rev-parse HEAD)
[ "$want" = "$have" ] || { echo "psxrecomp is at $have, expected $want"; exit 1; }
if git -C psxrecomp apply --check ../patches/psxrecomp-local.patch 2>/dev/null; then
    git -C psxrecomp apply ../patches/psxrecomp-local.patch && echo "applied patches/psxrecomp-local.patch"
elif git -C psxrecomp apply --reverse --check ../patches/psxrecomp-local.patch 2>/dev/null; then
    echo "patches/psxrecomp-local.patch is already applied"
else
    echo "patches/psxrecomp-local.patch does not apply cleanly"; exit 1
fi
echo "setup done. Put your .cue and .bin in disc/ (see INSTALL.md)."
