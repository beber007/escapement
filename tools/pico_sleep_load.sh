#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# SleepPico loaded on a Pico of the bench and left running, for the PPK2 (Examples/pico/
# SleepPico.c), on the UNO Q, as tools/pico2_sleep_load.sh does for the Pico 2:
#
#   PROBE=probe1 tools/pico_sleep_load.sh ELF            # loaded, the debug port powered down
#   PROBE=probe1 tools/pico_sleep_load.sh --read ELF     # its Results
#   PROBE=probe1 tools/pico_sleep_load.sh --lines SECONDS   # the lines it sends meanwhile
#
# Core 0 only (USE_CORE 0): SleepPico launches core 1 itself, into deep sleep. The debug
# port's power-up requests are then cleared (DP CTRL/STAT, CDBGPWRUPREQ and CSYSPWRUPREQ):
# left set, as OpenOCD leaves them, the debug domain stays powered, which the PPK2 would
# count in each phase. Reading the Results sets them again: read once the measure is done.
# Powered by the PPK2, the Pico 2's SWD read nothing at 5 MHz and did at 1: the default
# here is 1 MHz (ADAPTER_KHZ).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
OCD=${OPENOCD:-openocd}
run() {
    "$OCD" -f interface/cmsis-dap.cfg -c "$(sh "$HERE/probe.sh")" \
        -c "adapter speed ${ADAPTER_KHZ:-1000}" \
        -c "set USE_CORE 0" -f target/rp2040.cfg -c init "$@" -c exit 2>&1
}

if [ "$1" = --lines ]; then
    serial=$(sh "$HERE/probe.sh" | sed 's/.*serial \([0-9A-F]*\).*/\1/')
    for tty in /dev/serial/by-id/*Debug_Probe*"$serial"-if01; do :; done
    stty -F "$tty" 115200 raw -echo
    timeout "$2" cat "$tty" | grep -a --line-buffered SLEEP1 || true
    exit 0
fi
if [ "$1" = --read ]; then
    addr=$(arm-none-eabi-nm "$2" | awk '$3 == "Results" { print $1 }')
    run -c "halt" -c "mdw 0x$addr 9" -c "resume" -c "rp2040.dap0 dpreg 4 0" | grep -E "^0x"
    exit 0
fi
out=$(run -c "reset halt" -c "load_image $1" -c "resume 0x20000000" -c "rp2040.dap0 dpreg 4 0")
if ! echo "$out" | grep -q "bytes written\|downloaded"; then
    echo "$out" >&2
    exit 1
fi
echo "SleepPico loaded and running, the debug port powered down"
