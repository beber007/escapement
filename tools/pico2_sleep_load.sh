#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# SleepPico2 loaded on the Pico 2 of the bench and left running, for the PPK2 (Examples/
# pico2/SleepPico2.c), on the UNO Q:
#
#   OPENOCD=~/opt/openocd-rpi/bin/openocd PROBE=probe3 tools/pico2_sleep_load.sh ELF
#   OPENOCD=... PROBE=probe3 tools/pico2_sleep_load.sh --read ELF   # its Results
#   PROBE=probe3 tools/pico2_sleep_load.sh --wake SECONDS   # wakes its DORMANT meanwhile
#
# --wake sends a byte 0x00 every 100 ms on the probe's UART, whose TX drives GP1: its
# falling edge is what wakes SleepPico2 from DORMANT, the always-on timer's alarm not
# doing it there (SleepPico2.c). Without it the image stays in DORMANT, its first, for
# good, until the Pico 2 is powered off. The lines SleepPico2 sends at each change of
# phase are printed meanwhile.
#
# Powered by the PPK2 the SWD reads nothing at 5 MHz, and does at 1: ADAPTER_KHZ=1000.
#
# Loaded as tools/pico2_check.py loads an image: the RCP seeded first, core 1 held off in
# the PSM, which also leaves the chip free to enter SLEEP and DORMANT, both cores then
# asleep. The debug port's power-up requests are then cleared (DP CTRL/STAT, CDBGPWRUPREQ
# and CSYSPWRUPREQ): left set, as OpenOCD leaves them, the debug domain stays powered and
# clocked, which the PPK2 would count in each phase, as the NUCLEO's ST-LINK did
# (docs/stm32u5.md). Reading the Results sets them again: read once the measure is done.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
READ=no
if [ "$1" = --read ]; then
    READ=yes
    shift
fi
ELF=$1
OCD=${OPENOCD:-openocd}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

run() {
    "$OCD" -f interface/cmsis-dap.cfg -c "$(sh "$HERE/probe.sh")" \
        -c "adapter speed ${ADAPTER_KHZ:-5000}" \
        -c "set USE_CORE 0" -f target/rp2350.cfg -c init "$@" -c exit 2>&1
}

if [ "$1" = --wake ]; then
    tty=/dev/serial/by-id/$(ls /dev/serial/by-id | grep "Debug_Probe.*$(sh "$HERE/probe.sh" |
        sed 's/.*serial \([0-9A-F]*\).*/\1/')-if01")
    stty -F "$tty" 115200 raw -echo
    timeout "$2" cat "$tty" | grep -a --line-buffered SLEEP2 &
    end=$(($(date +%s) + $2))
    while [ "$(date +%s)" -lt $end ]; do
        printf '\000' > "$tty"
        sleep 0.1
    done
    wait
    exit 0
fi
if [ $READ = yes ]; then
    addr=$(arm-none-eabi-nm "$ELF" | awk '$3 == "Results" { print $1 }')
    run -c "mdw 0x$addr 6" -c "rp2350.dap dpreg 4 0" | grep -E "^0x"
    exit 0
fi
arm-none-eabi-as -o "$TMP/seed.o" "$HERE/rp2350_rcp_seed.S"
arm-none-eabi-objcopy -O binary "$TMP/seed.o" "$TMP/seed.bin"
out=$(run -c "reset halt" -c "mww 0x4001A004 0x1000000" \
          -c "load_image $TMP/seed.bin 0x20000000 bin" -c "resume 0x20000000" \
          -c "wait_halt 500" -c "load_image $ELF" -c "resume 0x20000000" \
          -c "rp2350.dap dpreg 4 0")
if [ "$(echo "$out" | grep -c "bytes written")" -lt 2 ]; then
    echo "$out" >&2
    exit 1
fi
echo "SleepPico2 loaded and running, the debug port powered down"
