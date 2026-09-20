#!/bin/sh
# Capture the GPIO transitions of the stm32f4 example under Renode, with virtual
# time, and write them as CSV on standard output.
#
# The kernel drives its outputs through the BSRR register of the STM32: writing a
# bit at offset 0x18 raises the pin, writing it at 0x1A lowers it. Two watchpoints
# report those writes along with the elapsed virtual time.
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
sysbus AddWatchpointHook 0x40020418 Word Write \"print 'EV %d,rise,%d' % ($TIME_US, value)\"
sysbus AddWatchpointHook 0x4002041A Word Write \"print 'EV %d,fall,%d' % ($TIME_US, value)\"
emulation RunFor \"$DURATION\"
quit" 2>/dev/null | sed -n 's/^EV //p'
