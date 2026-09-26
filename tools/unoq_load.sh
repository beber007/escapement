#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Load an image into the SRAM of the STM32U585 of an Arduino UNO Q and start it, through
# the OpenOCD the board ships in /opt/openocd, which drives the SWD of the MCU from GPIOs
# of its Linux processor: the pins of the MCU are not brought out, and no probe reaches
# them. The flash, which holds Arduino's bootloader and firmware, is left untouched, and
# the next reset brings the board back to them; --reset does that at once.
#
#   tools/unoq_load.sh ELF [HOST]       an image of Examples/uno-q (docs/stm32u5.md)
#   tools/unoq_load.sh --reset [HOST]   back to Arduino's firmware
#
# Run on the board itself, where /opt/openocd is, it loads at once; elsewhere it sends the
# image to HOST over SSH and loads it there, HOST being arduino@MyUno.local by default
# (UNOQ_HOST). The user on the board must be in the group gpiod, which it is as shipped.
# The one place the U5 is loaded: tools/soak.py calls it to load the endurance test again.
set -eu

if [ "${1:-}" = --reset ]; then
    COMMANDS="init; reset; shutdown"
    ELF=""
else
    ELF=${1:?usage: tools/unoq_load.sh ELF [HOST] | --reset [HOST]}
    [ -f "$ELF" ] || { echo "no image $ELF" >&2; exit 1; }
    # Halted at reset, clocks as reset leaves them, then started at the entry code at the
    # head of the image (Escapement_RamEntry.S). TIM2, the kernel's clock, TIM3 and TIM5,
    # those of the examples, stop while the debugger halts the core (DBGMCU_APB1FZR1,
    # RM0456): otherwise reading the board by halting it makes the tasks late, and the
    # kernel stops on its overload check. The bit of the independent watchdog in that
    # register reads back 0 on the UNO Q: the watchdog runs on, and a halt must stay well
    # under its period, 3 s in SoakU5.
    COMMANDS="init; reset halt; mww 0xE0044008 0xb; load_image IMAGE; \
resume 0x20000000; shutdown"
fi
# The board's OpenOCD, with the configuration Arduino's own scripts use (arduino-flash.sh).
OCD="cd /opt/openocd && ./bin/openocd -s /opt/openocd -f openocd_gpiod.cfg -c"
report() { grep -E "^(Error|Warn)|downloaded|bytes" || true; }

if [ -x /opt/openocd/bin/openocd ] && [ -z "${2:-}" ]; then
    image=$(cd "$(dirname "${ELF:-.}")" && pwd)/$(basename "${ELF:-.}")
    sh -c "$OCD 'reset_config srst_only srst_push_pull; $(echo "$COMMANDS" |
        sed "s|IMAGE|$image|")'" 2>&1 | report
else
    HOST=${2:-${UNOQ_HOST:-arduino@MyUno.local}}
    [ -z "$ELF" ] || scp -q "$ELF" "$HOST:/tmp/escapement.elf"
    ssh "$HOST" "$OCD 'reset_config srst_only srst_push_pull; $(echo "$COMMANDS" |
        sed "s|IMAGE|/tmp/escapement.elf|")'" 2>&1 | report
fi
