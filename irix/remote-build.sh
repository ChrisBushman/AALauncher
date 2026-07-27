#!/usr/bin/env bash
# Sync this repo to an SGI O2 (IRIX 6.5) box over SSH and build it there.
#
# Usage:
#   AA_O2_HOST=aa-o2 ./irix/remote-build.sh [make-target...]
#
# Env vars:
#   AA_O2_HOST   (required) ssh destination, e.g. "bushmac@10.0.0.42" or a
#                ~/.ssh/config Host alias.
#   AA_O2_PATH   remote directory to sync into (default: ~/aa-o2-build/AALauncher)
#   AA_O2_PORT   ssh port (default: 22)
#   AA_O2_MAKE   make binary to invoke remotely.
#
# No rsync on this box -- plain tar-over-ssh instead (see AmuletsArmor's
# Build/IRIX-O2/remote-build.sh for why).

set -euo pipefail

if [ -z "${AA_O2_HOST:-}" ]; then
    echo "error: set AA_O2_HOST to the ssh destination of the O2 (user@host, or a ~/.ssh/config alias)" >&2
    exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REMOTE_PATH="${AA_O2_PATH:-~/aa-o2-build/AALauncher}"
SSH_PORT="${AA_O2_PORT:-22}"
AA_O2_MAKE="${AA_O2_MAKE:-/usr/people/bushmac/sh-SOA960904-hms/bin/make}"

MAKE_TARGETS=("$@")
if [ "${#MAKE_TARGETS[@]}" -eq 0 ]; then
    MAKE_TARGETS=(all)
fi

TAR_EXCLUDES=(
    --exclude='.git'
    --exclude='build'
    --exclude='out'
    --exclude='ppc/build'
    --exclude='irix/build'
    --exclude='dist'
)

echo "==> Syncing $REPO_ROOT to $AA_O2_HOST:$REMOTE_PATH"
ssh -p "$SSH_PORT" "$AA_O2_HOST" "mkdir -p $REMOTE_PATH"
tar cf - -C "$REPO_ROOT" "${TAR_EXCLUDES[@]}" . | \
    ssh -p "$SSH_PORT" "$AA_O2_HOST" "cd $REMOTE_PATH && tar xf -"

echo "==> Building on $AA_O2_HOST ($AA_O2_MAKE ${MAKE_TARGETS[*]})"
ssh -p "$SSH_PORT" "$AA_O2_HOST" \
    "cd $REMOTE_PATH/irix && $AA_O2_MAKE ${MAKE_TARGETS[*]}"

echo "==> Done. Binary at $REMOTE_PATH/irix/build/AALauncher on $AA_O2_HOST"
