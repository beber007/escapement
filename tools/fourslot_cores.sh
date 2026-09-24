#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Run FourSlotCoresPico on a Raspberry Pi Pico and read back what its reader found:
# the 4-slot buffer of the kernel between the two cores, and a plain array beside it.
#
#   tools/fourslot_cores.sh [seconds [elf]]
#
# Needs OpenOCD and a CMSIS-DAP probe (the Raspberry Pi Debug Probe).
#
# OpenOCD is told to handle core 0 only (USE_CORE 0): by default it halts both cores and
# resumes core 0 alone, and a core 1 left halted by the debugger is no core 1 at all —
# the example could not start it, and the timer, which pauses while either core is held,
# would stop the kernel with it. Results is read with both cores running, since a
# Cortex-M lets the probe read memory without halting it.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
RUN_SECONDS=${1:-10}
ELF=${2:-$ROOT/Escapement/CORTEX-Mx/RP2040/Examples/pico/build/FourSlotCoresPico.elf}

[ -f "$ELF" ] || { echo "build the example first" >&2; exit 1; }
RESULTS=$(arm-none-eabi-nm "$ELF" | awk '$3 == "Results" { print "0x"$1 }')
[ -n "$RESULTS" ] || { echo "$ELF is not FourSlotCoresPico" >&2; exit 1; }

ocd() {
    openocd -f interface/cmsis-dap.cfg -c 'adapter speed 5000' -c 'set USE_CORE 0' \
        -f target/rp2040.cfg -c init "$@" -c exit 2>&1
}

ocd -c 'reset halt' -c "load_image $ELF" -c 'resume 0x20000000' >/dev/null
sleep "$RUN_SECONDS"
set -- $(ocd -c "mdw $RESULTS 10" | sed -n 's/^0x[0-9a-f]*: //p')
[ $# -ge 10 ] || { echo "Results not read back" >&2; exit 1; }

echo "run time          : ${RUN_SECONDS} s"
echo "records written   : $((0x$1))"
printf '%-18s: %d reads, %d torn, %d backwards\n' \
    "4-slot buffer" $((0x$2)) $((0x$3)) $((0x$4)) \
    "plain array" $((0x$6)) $((0x$7)) $((0x$8))
echo "longest reader run: $((0x${10})) us"
