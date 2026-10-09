#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Load an image into the SRAM of the STM32U575 of a NUCLEO-U575ZI-Q, or of the STM32U385 of
# a NUCLEO-U385RG-Q, and start it, through the board's ST-LINK V3 and the OpenOCD of the
# machine it is plugged into (Debian's openocd package on the UNO Q). The flash is left
# untouched, and the next reset brings the board back to what it holds; --reset does that
# at once.
#
#   tools/nucleo_load.sh ELF        an image of Examples/nucleo-u575 (docs/stm32u5.md)
#   tools/nucleo_load.sh --reset    back to the firmware in the flash
#
# $NUCLEO_SERIAL selects the ST-LINK by its serial when more than one is plugged in.
# NUCLEO_MCU=u385 drives the U385 of Examples/nucleo-u385 (docs/stm32u3.md), which takes an
# OpenOCD newer than 0.12, named by $OPENOCD (tools/board_ci.md, "OpenOCD for the STM32U3").
# tools/soak.py calls it to load the endurance test again.
set -eu

# The target's script, and the timers of the kernel and the examples stopped while the
# core is halted: TIM2, TIM3 and TIM5 in DBGMCU_APB1FZR1 on the U575 (RM0456), TIM2 to
# TIM4 in DBGMCU_APB1LFZR on the U385 (RM0487 rev. 3, p. 2850), both at 0xE0044008.
case ${NUCLEO_MCU:-u575} in
    u575) TARGET=stm32u5x FREEZE=0xb ;;
    u385) TARGET=stm32u3x FREEZE=0x7 ;;
    *) echo "NUCLEO_MCU is u575 or u385" >&2; exit 1 ;;
esac

if [ "${1:-}" = --reset ]; then
    COMMANDS="init; reset; echo ESCAPEMENT-DONE; shutdown"
else
    ELF=${1:?usage: tools/nucleo_load.sh ELF | --reset}
    [ -f "$ELF" ] || { echo "no image $ELF" >&2; exit 1; }
    image=$(cd "$(dirname "$ELF")" && pwd)/$(basename "$ELF")
    # As tools/unoq_load.sh: halted at reset, the timers frozen, then started at the
    # entry code at the head of the image (Escapement_RamEntry.S). A reset that did not
    # reach the MCU, JP2 off on 2026-10-02 (UM2861), left the core in its HardFault
    # handler, where the image was started and locked up, the load reported done: the
    # core must be in no exception after it (ICSR.VECTACTIVE).
    COMMANDS="init; reset halt; if {[mrw 0xE000ED04] & 0x1FF} { error \"Error: the core is in an exception after the reset, which did not reach it\" }; mww 0xE0044008 $FREEZE; \
load_image $image; resume 0x20000000; echo ESCAPEMENT-DONE; shutdown"
fi
# Connected with the reset held, as on the UNO Q: an image idle in Stop 2 leaves the
# debug port unpowered most of the time. The ST-LINK drives NRST (UM2861). An error from
# OpenOCD fails the load, and so does the end of the commands not reached, as in
# tools/unoq_load.sh. Its ports for gdb, telnet and Tcl are closed, as in
# tools/probe.sh: another OpenOCD may be running on the bench.
SERIAL=${NUCLEO_SERIAL:+adapter serial $NUCLEO_SERIAL;}
${OPENOCD:-openocd} -f interface/stlink-dap.cfg \
        -c "transport select dapdirect_swd; $SERIAL gdb_port disabled; telnet_port disabled; tcl_port disabled" \
        -f target/$TARGET.cfg \
        -c "reset_config srst_only srst_nogate connect_assert_srst; $COMMANDS" 2>&1 |
    { out=$(cat)
      echo "$out" | grep -E "^(Error|Warn)|downloaded|bytes" || true
      ! echo "$out" | grep -q "^Error" && echo "$out" | grep -q "^ESCAPEMENT-DONE"; }
