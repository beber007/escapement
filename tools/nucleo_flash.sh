#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Write an image linked into the flash (make FLASH=1, build-flash/) into the flash of the
# STM32U575 of a NUCLEO-U575ZI-Q, or of the STM32U385 of a NUCLEO-U385RG-Q, and start it
# from a reset, through the board's ST-LINK V3, as tools/nucleo_load.sh does into SRAM:
#
#   tools/nucleo_flash.sh ELF
#
# Each page of the flash takes 10,000 erasures at least (DS13737, table 87, NEND; RM0487
# rev. 3, "10 kcycles endurance on all flash memory"); an image takes only the pages it
# covers, two of 8 KB for SoakU5. The flash is first read against the image, which
# erases nothing: an image already there is only started. Each writing is counted, one
# line in $NUCLEO_FLASH_LEDGER (by default ~/.local/state/escapement/flash-<serial>), for
# the wear to be read; nothing caps them since the user lifted the caps on 2026-10-10
# (a total of 1,000 and one a day for the board CI until then).
#
# $NUCLEO_SERIAL, $NUCLEO_MCU and $OPENOCD as in tools/nucleo_load.sh.
set -eu

case ${NUCLEO_MCU:-u575} in
    u575) TARGET=stm32u5x ;;
    u385) TARGET=stm32u3x ;;
    *) echo "NUCLEO_MCU is u575 or u385" >&2; exit 1 ;;
esac

ELF=${1:?usage: tools/nucleo_flash.sh ELF}
[ -f "$ELF" ] || { echo "no image $ELF" >&2; exit 1; }
image=$(cd "$(dirname "$ELF")" && pwd)/$(basename "$ELF")

LEDGER=${NUCLEO_FLASH_LEDGER:-$HOME/.local/state/escapement/flash-${NUCLEO_SERIAL:-default}}
mkdir -p "$(dirname "$LEDGER")" && touch "$LEDGER"

# Connected with the reset held, as tools/nucleo_load.sh. verify_image reads the flash
# and fails at the first difference; only then is the image written ("program", which
# erases the pages it covers and no other, and reads them back).
SERIAL=${NUCLEO_SERIAL:+adapter serial $NUCLEO_SERIAL;}
out=$(${OPENOCD:-openocd} -f interface/stlink-dap.cfg \
        -c "transport select dapdirect_swd; $SERIAL gdb_port disabled; telnet_port disabled; tcl_port disabled" \
        -f target/$TARGET.cfg \
        -c "reset_config srst_only srst_nogate connect_assert_srst; init; reset halt" \
        -c "if {[catch {verify_image $image}]} { echo ESCAPEMENT-WRITE; program $image verify } else { echo ESCAPEMENT-SAME }" \
        -c "reset run; echo ESCAPEMENT-DONE; shutdown" 2>&1) || true
# The differences verify_image lists come before the mark and are no error; any after it is.
after=$(echo "$out" | awk '/^ESCAPEMENT-(SAME|WRITE)/ { f = 1 } f')
echo "${after:-$out}" | grep -E "^(Error|Warn)|^ESCAPEMENT-(SAME|WRITE)|Verified|wrote" || true
case $after in
    ESCAPEMENT-WRITE*) echo "$(date +%s) $(basename "$ELF")" >>"$LEDGER" ;;
esac
! echo "$after" | grep -q "^Error" && echo "$after" | grep -q "^ESCAPEMENT-DONE"
