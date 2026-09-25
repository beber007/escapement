#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Measure the per-activation cost of the kernel on a Raspberry Pi Pico.
#
# Reads back the counters that Escapement_Timer.c keeps under
# ESCAPEMENT_MEASURE_SCHEDULING_COST; the example must be built with that option.
#
#   tools/measure_cost.sh [seconds [elf]]
#
# Needs OpenOCD and a CMSIS-DAP probe (the Raspberry Pi Debug Probe).
#
# Two points make this trickier than it looks:
#
#  - the timer of the RP2040 stops while ANY core is halted by the debugger, and
#    OpenOCD halts both of them. Core 1 is never resumed here: a firmware in flash
#    may have armed the watchdog, which only pauses while a core is held and would
#    reboot the chip within a second (docs/rp2040.md). The timer would then stay
#    frozen and the kernel sleep forever in its idle task. The probe
#    example therefore builds with TIMER_DBGPAUSE cleared, which the kernel does
#    on its own under ESCAPEMENT_MEASURE_SCHEDULING_COST;
#  - each OpenOCD command needs its own -c, since the output of mdw is lost when
#    several commands are grouped into a single script.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
RUN_SECONDS=${1:-10}
ELF=${2:-$ROOT/Escapement/CORTEX-Mx/RP2040/Examples/pico/build/TaskLEDPico.elf}

[ -f "$ELF" ] || { echo "build the example first" >&2; exit 1; }

addr() { arm-none-eabi-nm "$ELF" | awk -v s="$1" '$3 == s { print "0x"$1 }'; }

[ -n "$(addr _OSCostCount)" ] || {
    echo "the example carries no instrumentation." >&2
    echo "Uncomment ESCAPEMENT_MEASURE_SCHEDULING_COST in its Escapement_Config.h" >&2
    echo "and rebuild; it is off by default." >&2
    exit 1
}

# 1. load, let the kernel initialise, free the timer from the debugger, let go
openocd -f interface/cmsis-dap.cfg -c 'adapter speed 5000' -f target/rp2040.cfg \
    -c init -c 'reset halt' -c "load_image $ELF" -c 'resume 0x20000000' \
    -c exit >/dev/null 2>&1

# 2. let it run on its own, with no debugger attached
sleep "$RUN_SECONDS"

# 3. come back and read the counters
# One mdw per symbol: the linker is free to lay the counters out in any order,
# and it does reorder them from one optimisation level to the next.
raw=$(openocd -f interface/cmsis-dap.cfg -c 'adapter speed 5000' -f target/rp2040.cfg \
    -c init -c halt \
    -c "mdw $(addr _OSCostMax)" -c "mdw $(addr _OSCostSum)" -c "mdw $(addr _OSCostCount)" \
    -c shutdown 2>&1 | sed -n 's/^0x[0-9a-f]*: //p')

set -- $raw
[ $# -ge 3 ] || { echo "no counters read back" >&2; exit 1; }
max=$((0x$1)); sum=$((0x$2)); count=$((0x$3))
[ "$count" -gt 0 ] || { echo "the kernel did not schedule anything" >&2; exit 1; }

echo "run time        : ${RUN_SECONDS} s"
echo "rounds          : $count"
echo "mean            : $(awk -v s=$sum -v c=$count 'BEGIN{printf "%.1f us", s/c}')"
echo "maximum         : $max us"
echo "processor share : $(awk -v s=$sum -v t=$RUN_SECONDS 'BEGIN{printf "%.2f %%", s/(t*1000000)*100}')"
