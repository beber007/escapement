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
# HOST is where to ssh to, arduino@MyUno.local by default (UNOQ_HOST); the user there must
# be in the group gpiod, which it is on the board as shipped.
set -eu

if [ "${1:-}" = --reset ]; then
    COMMANDS="init; reset; shutdown"
    ELF=""
else
    ELF=${1:?usage: tools/unoq_load.sh ELF [HOST] | --reset [HOST]}
    [ -f "$ELF" ] || { echo "no image $ELF" >&2; exit 1; }
    # Halted at reset, clocks as reset leaves them, then started at the entry code at the
    # head of the image (Escapement_RamEntry.S). TIM2, the kernel's clock, TIM3 and TIM5,
    # those of the examples, and the independent watchdog stop while the debugger halts
    # the core (DBGMCU_APB1FZR1, RM0456): otherwise reading the board by halting it makes
    # the tasks late, and the kernel stops on its overload check, or the watchdog
    # restarts the board.
    COMMANDS="init; reset halt; mww 0xE0044008 0x100b; load_image /tmp/escapement.elf; \
resume 0x20000000; shutdown"
fi
HOST=${2:-${UNOQ_HOST:-arduino@MyUno.local}}

[ -z "$ELF" ] || scp -q "$ELF" "$HOST:/tmp/escapement.elf"
# The board's OpenOCD, with the configuration Arduino's own scripts use (arduino-flash.sh).
ssh "$HOST" "cd /opt/openocd && ./bin/openocd -s /opt/openocd -f openocd_gpiod.cfg \
    -c 'reset_config srst_only srst_push_pull; $COMMANDS'" 2>&1 |
    grep -E "^(Error|Warn)|downloaded|bytes" || true
