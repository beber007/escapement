#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The endurance test on the STM32U585 of an Arduino UNO Q, run by the board's own Linux:
# read the counts of SoakU5 at every interval, for as long as it runs. Each reading halts
# the core for a few milliseconds: while it sleeps in the idle task the debugger reads
# zeros, in SRAM as in the peripherals, and a first version of this script, reading the
# core as it ran, took a sleeping one for a board without SoakU5. The kernel's timers
# stand still while it is halted (tools/unoq_load.sh). The counterpart of
# tools/soak.sh, which drives a Pico
# through a probe; this one takes the board's OpenOCD, which drives the MCU's debug port
# from GPIOs (tools/unoq_load.sh).
#
#   tools/soak_unoq.sh ELF [INTERVAL]       ELF: the SoakU5 image running, or to load
#                                           INTERVAL: seconds, 300 by default
#
# Each reading appends a line to the log (SOAK_UNOQ_LOG, ~/soak-u5-<date>.log by
# default): the seconds run by the firmware, the wraps of the kernel clock crossed, the
# errors each part counted, the parts that did not move since the reading before, the
# worst lateness of the pulse and of the timer events, and the restarts. A reading whose
# marker is gone or whose seconds did not move has seen the board restart, into Arduino's
# firmware since the image runs from SRAM: it is logged, the image is loaded again, and
# the counts start over; the errors are added up across restarts. The image already
# running is read as it is, not loaded again, so that a test can be taken over.
set -eu

ELF=${1:?usage: tools/soak_unoq.sh ELF [INTERVAL]}
INTERVAL=${2:-300}
LOG=${SOAK_UNOQ_LOG:-$HOME/soak-u5-$(date +%Y%m%d-%H%M).log}
RESULTS=$(arm-none-eabi-nm "$ELF" | awk '$3 == "Results" { print "0x"$1 }')
[ -n "$RESULTS" ] || { echo "$ELF is not SoakU5" >&2; exit 1; }
MARKER=$((0x534f414b))    # "SOAK", SoakU5.c
PARTS="pulse queue buffer events buffer4 heartbeat interrupt memory"

ocd() {
    (cd /opt/openocd && ./bin/openocd -s /opt/openocd -f openocd_gpiod.cfg "$@" 2>&1)
}
# The words of Results, the core halted for the reading; nothing if the MCU does not
# answer. Reading only: no reset_config here, whose push-pull mode drives the reset line
# of the MCU.
reading() {
    ocd -c "init; halt" -c "echo \"R [read_memory $RESULTS 32 24]\"" -c "resume; shutdown" |
        sed -n 's/^R //p'
}
# As tools/unoq_load.sh: the timers and the watchdog frozen while the debugger halts.
load() {
    ocd -c "reset_config srst_only srst_push_pull; init; reset halt; mww 0xE0044008 0xb; \
load_image $ELF; resume 0x20000000; shutdown" >/dev/null
}

echo "SoakU5 on the UNO Q, $(date), read every $INTERVAL s, $ELF" >>"$LOG"
restarts=0 errors_before=0 stalls=0 previous=""
# Loaded only if the memory reads and holds no SoakU5; a reading that fails stops here,
# rather than restart a test that may be running.
set -- $(reading)
if [ $# -lt 24 ]; then
    echo "$(date '+%Y-%m-%d %H:%M:%S') no reading from the MCU: stopped" >>"$LOG"
    exit 1
elif [ $(($1)) -ne $MARKER ]; then
    echo "$(date '+%Y-%m-%d %H:%M:%S') no SoakU5 running: loaded" >>"$LOG"
    load
fi
while :; do
    sleep "$INTERVAL"
    now=$(date '+%Y-%m-%d %H:%M:%S')
    set -- $(reading)
    if [ $# -lt 24 ]; then
        echo "$now ERRORS: no reading from the MCU" >>"$LOG"
        stalls=$((stalls + 1))
        continue
    fi
    secs=$(($2)) wraps=$(($3))
    # A reset leaves the SRAM as it was, marker and counts included: seconds that no longer
    # move say it as well as seconds that went back.
    if [ $(($1)) -ne $MARKER ] || { [ -n "$previous" ] && [ "$secs" -le "$previous_secs" ]; }; then
        restarts=$((restarts + 1))
        errors_before=$((errors_before + ${run_errors:-0}))
        echo "$now RESTART $restarts: marker $1, $secs s after $previous_secs; loaded again" \
            >>"$LOG"
        load
        previous=""
        continue
    fi
    shift 3
    activity="" run_errors=0 stalled="" i=0
    for part in $PARTS; do
        i=$((i + 1))
        eval "a=\$$i e=\${$((i + 8))}"
        activity="$activity $(($a))"
        run_errors=$((run_errors + $e))
        if [ -n "$previous" ]; then
            # The counts go round at 2^32: moved means different, not greater.
            p=$(echo "$previous" | awk -v n="$i" '{ print $n }')
            [ "$p" -ne $(($a)) ] || stalled="$stalled $part"
        fi
    done
    [ -z "$stalled" ] || stalls=$((stalls + 1))
    late="$((${17})) / $((${18})) us"
    total=$((errors_before + run_errors + stalls))
    line="$now run $secs s, $wraps wraps, part errors $run_errors, restarts $restarts, late max $late"
    [ -z "$stalled" ] || line="$line, not moving:$stalled"
    if [ "$total" -ne 0 ] || [ "$restarts" -ne 0 ]; then
        echo "ERRORS $total: $line" >>"$LOG"
    else
        echo "$line" >>"$LOG"
    fi
    previous=$activity previous_secs=$secs
done
