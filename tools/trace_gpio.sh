#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Capture the GPIO transitions of the stm32f4 example under Renode, with virtual
# time, and write them as CSV on standard output.
#
# The example drives its outputs through the BSRR register of the STM32 (offset 0x18),
# one 32-bit write per edge: the pin in the low half raises it, in the high half lowers
# it. A watchpoint on that register reports each write with the elapsed virtual time,
# and the edge is read from the half the pin is in.
#
#   tools/trace_gpio.sh > docs/data/f4-gpio-trace.csv
#
# RENODE may point at another Renode binary; DURATION is in emulated seconds.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
RENODE=${RENODE:-$HOME/Applications/Renode.app/Contents/MacOS/renode}
DURATION=${DURATION:-0.025}
EXAMPLE=$ROOT/Escapement/CORTEX-Mx/STM32/Examples/stm32f4-discovery/build/TaskLEDF4

[ -f "$EXAMPLE.elf" ] || { echo "build the example first: make -C $(dirname "$EXAMPLE") bin" >&2; exit 1; }

TIME_US="self.Machine.ElapsedVirtualTime.TimeElapsed.TotalMicroseconds"

echo "time_us,edge,value"
"$RENODE" --disable-xwt --console --plain -e "
path add @$ROOT/emulation/renode
include @Escapement_STM32_Timer.cs
mach create \"f4\"
machine LoadPlatformDescription @$ROOT/emulation/renode/escapement_f4.repl
sysbus LoadELF @$EXAMPLE.elf
sysbus LoadBinary @$EXAMPLE.bin 0x08000000
sysbus AddWatchpointHook 0x40020418 DoubleWord Write \"print 'EV %d,%s,%d' % ($TIME_US, 'rise' if value & 0xFFFF else 'fall', (value & 0xFFFF) or (value >> 16))\"
emulation RunFor \"$DURATION\"
quit" 2>/dev/null | sed -n 's/^EV //p'
