#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The endurance runs of a release candidate on one platform, one kernel after the other
# (CLAUDE.md, "Before a release", rule 3; docs/roadmap.md, release 0.1, task 14): each image
# of DIR named <platform>-<kernel>.elf run for DURATION by tools/soak.py, its status posted
# as "endurance/<platform>-<kernel>" on the commit SHA, its log in DIR. A run that fails
# does not stop the next: each kernel has its own verdict.
#
#   tools/endurance.sh PLATFORM SHA DIR [DURATION]     8h by default
#
# PLATFORM is pico, pico2, u5 or u3, its kernels hard, soft and pa as DIR holds them; the
# probe, the ST-LINK and the OpenOCD come from the environment as tools/soak.py takes them
# (PROBE, OPENOCD, NUCLEO_SERIAL, NUCLEO_TTY). The run of a board the board CI also checks
# must not overlap its checks: the Pico 2's probe lock keeps them apart, the STM32U3's long
# run of the board CI, escapement-soak-u3, is to be stopped first, and no commit pushed
# meanwhile.
set -u
PLATFORM=$1 SHA=$2 DIR=$3 DURATION=${4:-8h}
HERE=$(cd "$(dirname "$0")" && pwd)
case $PLATFORM in
    pico) KIND=pico ;; pico2) KIND=pico2 ;; u5) KIND=nucleo ;; u3) KIND=nucleo-u3 ;;
    *) echo "PLATFORM is pico, pico2, u5 or u3" >&2; exit 2 ;;
esac
failed=""
for kernel in hard soft pa; do
    elf=$DIR/$PLATFORM-$kernel.elf
    [ -f "$elf" ] || continue
    echo "$(date -u '+%Y-%m-%d %H:%M:%S') $PLATFORM $kernel: $elf"
    BOARD_SOAK_SHA=$SHA BOARD_SOAK_CONTEXT=endurance/$PLATFORM-$kernel \
    BOARD_SOAK_LOG=$DIR/$PLATFORM-$kernel.log \
        python3 "$HERE/soak.py" "$KIND" "$DURATION" 1m "$elf" || failed="$failed $kernel"
done
echo "$(date -u '+%Y-%m-%d %H:%M:%S') $PLATFORM done${failed:+, failed:$failed}"
[ -z "$failed" ]
