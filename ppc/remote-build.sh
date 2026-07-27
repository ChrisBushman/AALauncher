#!/usr/bin/env bash
# Sync this repo to a PowerPC Mac OS X Tiger box over SSH and build it there.
#
# Usage:
#   AA_PPC_HOST=user@tiger-box ./ppc/remote-build.sh [make-target...]
#
# Env vars:
#   AA_PPC_HOST   (required) ssh destination, e.g. "aa@10.0.0.42" or a
#                 ~/.ssh/config Host alias.
#   AA_PPC_PATH   remote directory to sync into
#                 (default: ~/aa-ppc-build/AALauncher)
#   AA_PPC_PORT   ssh port (default: 22)
#   AA_PPC_MAKE   make binary to invoke remotely (default: auto-detect
#                 Tigerbrew's gmake, falling back to the system make).

set -euo pipefail

if [ -z "${AA_PPC_HOST:-}" ]; then
    echo "error: set AA_PPC_HOST to the ssh destination of the Tiger box (user@host)" >&2
    exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REMOTE_PATH="${AA_PPC_PATH:-~/aa-ppc-build/AALauncher}"
SSH_PORT="${AA_PPC_PORT:-22}"
MAKE_TARGETS=("$@")
if [ "${#MAKE_TARGETS[@]}" -eq 0 ]; then
    MAKE_TARGETS=(all)
fi

echo "==> Syncing $REPO_ROOT to $AA_PPC_HOST:$REMOTE_PATH"
ssh -p "$SSH_PORT" "$AA_PPC_HOST" "mkdir -p $REMOTE_PATH"
rsync -avz --delete \
    -e "ssh -p $SSH_PORT" \
    --exclude='.git/' \
    --exclude='build/' \
    --exclude='out/' \
    --exclude='ppc/build/' \
    --exclude='irix/build/' \
    --exclude='dist/' \
    "$REPO_ROOT/" "$AA_PPC_HOST:$REMOTE_PATH/"

if [ -z "${AA_PPC_MAKE:-}" ]; then
    AA_PPC_MAKE=$(ssh -p "$SSH_PORT" "$AA_PPC_HOST" \
        'PATH="/usr/local/bin:$PATH"; command -v gmake || command -v make')
fi

echo "==> Building on $AA_PPC_HOST ($AA_PPC_MAKE ${MAKE_TARGETS[*]})"
ssh -p "$SSH_PORT" "$AA_PPC_HOST" \
    "export PATH=/usr/local/bin:\$PATH; cd $REMOTE_PATH/ppc && $AA_PPC_MAKE ${MAKE_TARGETS[*]}"

echo "==> Done. Binary at $REMOTE_PATH/ppc/build/AALauncher on $AA_PPC_HOST"
