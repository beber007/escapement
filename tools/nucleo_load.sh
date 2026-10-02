#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Load an image into the SRAM of the STM32U575 of a NUCLEO-U575ZI-Q and start it, through
# the board's ST-LINK V3 and the OpenOCD of the machine it is plugged into (Debian's
# openocd package on the UNO Q). The flash is left untouched, and the next reset brings the
# board back to what it holds; --reset does that at once.
#
#   tools/nucleo_load.sh ELF        an image of Examples/nucleo-u575 (docs/stm32u5.md)
#   tools/nucleo_load.sh --reset    back to the firmware in the flash
#
# $NUCLEO_SERIAL selects the ST-LINK by its serial when more than one is plugged in.
# tools/soak.py calls it to load the endurance test again.
set -eu

if [ "${1:-}" = --reset ]; then
    COMMANDS="init; reset; echo ESCAPEMENT-DONE; shutdown"
else
    ELF=${1:?usage: tools/nucleo_load.sh ELF | --reset}
    [ -f "$ELF" ] || { echo "no image $ELF" >&2; exit 1; }
    image=$(cd "$(dirname "$ELF")" && pwd)/$(basename "$ELF")
    # As tools/unoq_load.sh: halted at reset, the timers of the kernel and the examples
    # stopped while the core is halted (DBGMCU_APB1FZR1, RM0456), then started at the
    # entry code at the head of the image (Escapement_RamEntry.S). A reset that did not
    # reach the MCU, JP2 off on 2026-10-02 (UM2861), left the core in its HardFault
    # handler, where the image was started and locked up, the load reported done: the
    # core must be in no exception after it (ICSR.VECTACTIVE).
    COMMANDS="init; reset halt; if {[mrw 0xE000ED04] & 0x1FF} { error \"Error: the core is in an exception after the reset, which did not reach it\" }; mww 0xE0044008 0xb; \
load_image $image; resume 0x20000000; echo ESCAPEMENT-DONE; shutdown"
fi
# Connected with the reset held, as on the UNO Q: an image idle in Stop 2 leaves the
# debug port unpowered most of the time. The ST-LINK drives NRST (UM2861). An error from
# OpenOCD fails the load, and so does the end of the commands not reached, as in
# tools/unoq_load.sh. Its ports for gdb, telnet and Tcl are closed, as in
# tools/probe.sh: another OpenOCD may be running on the bench.
SERIAL=${NUCLEO_SERIAL:+adapter serial $NUCLEO_SERIAL;}
openocd -f interface/stlink-dap.cfg \
        -c "transport select dapdirect_swd; $SERIAL gdb_port disabled; telnet_port disabled; tcl_port disabled" \
        -f target/stm32u5x.cfg \
        -c "reset_config srst_only srst_nogate connect_assert_srst; $COMMANDS" 2>&1 |
    { out=$(cat)
      echo "$out" | grep -E "^(Error|Warn)|downloaded|bytes" || true
      ! echo "$out" | grep -q "^Error" && echo "$out" | grep -q "^ESCAPEMENT-DONE"; }
