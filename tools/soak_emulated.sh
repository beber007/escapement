#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Several endurance tests under Renode side by side, each in a container of the CI's
# image with its own build of SoakPico and its own seed
# (emulation/renode/soak_emulated.robot). For a Linux machine with podman; every
# instance logs to OUT/<name>.log.
#
#   tools/soak_emulated.sh OUT INTERVAL READINGS NAME:MAKE_ARGUMENTS:SEED...
#
#   tools/soak_emulated.sh ~/soak 60 1440 hard::1 soft:KERNEL=SOFT:3 \
#       dm:SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING:2 pa-dra:KERNEL=PA,POWER=DRA:5
#
# INTERVAL is in seconds of virtual time; the models run close to real time, a day of
# readings every minute takes about a day. Commas in MAKE_ARGUMENTS stand for spaces.
# While any instance runs, the machine is kept from sleeping (systemd-inhibit, where
# there is one): the one this was written for suspends itself when idle, and its users
# count, not its load. tools/soak_emulated_status.sh sums the logs up.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:?output directory}; INTERVAL=${2:?interval}; READINGS=${3:?readings}; shift 3
IMAGE=${SOAK_IMAGE:-ghcr.io/beber007/escapement-ci:2}
mkdir -p "$OUT"
for instance in "$@"; do
    name=${instance%%:*}; rest=${instance#*:}
    make=$(echo "${rest%:*}" | tr , ' '); seed=${rest##*:}
    variables="--variable SEED:$seed --variable INTERVAL:$INTERVAL --variable READINGS:$READINGS"
    # z: under SELinux (Fedora and its kin) a container may not write a directory it was
    # not labelled for; elsewhere podman ignores it.
    # Unbuffered, so that each reading reaches the log as it is made, not by the 8 KiB.
    podman run -d --replace --rm --name "soak-$name" -e PYTHONUNBUFFERED=1 \
        -v "$ROOT":/src:ro,z -v "$OUT":/out:z \
        "$IMAGE" bash -c "cp -r /src /w && cd /w && ln -s /opt/rp2040 emulation/renode/rp2040 &&
            make -s -C Escapement/CORTEX-Mx/RP2040/Examples/pico $make build/SoakPico.elf &&
            /opt/renode-1.16.1/renode-test $variables emulation/renode/soak_emulated.robot \
                > /out/$name.log 2>&1; echo \"exit \$?\" >> /out/$name.log" >/dev/null
    echo "soak-$name: make $make, seed $seed -> $OUT/$name.log"
done
if command -v systemd-inhibit >/dev/null; then
    nohup systemd-inhibit --what=sleep:idle --who=escapement --why="emulated soak" \
        sh -c 'while podman ps -q --filter name=soak- | grep -q .; do sleep 60; done' \
        >/dev/null 2>&1 &
    echo "sleep inhibited while the instances run"
fi
