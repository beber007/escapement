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
# rev. 3, "10 kcycles endurance on all flash memory"). The STM32U575's lets any 32 pages a
# bank go to 100,000, which ST meant for data but does not keep from code (RM0456 rev. 7,
# 7.3.8); the budget below is taken from the 10,000 all the same, and an image takes
# only the pages it covers, two of 8 KB for SoakU5. The flash is first read against the
# image, which
# erases nothing: an image already there is only started. Each writing is counted, one line
# in $NUCLEO_FLASH_LEDGER (by default ~/.local/state/escapement/flash-<serial>), and the
# script refuses to write past $NUCLEO_FLASH_BUDGET writings in all (1,000 by default, a
# tenth of the endurance), or past $NUCLEO_FLASH_PER_DAY in the last 24 hours when it is
# set, for the board CI; it then exits with 3, the board reset on what its flash held.
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
written=$(($(wc -l <"$LEDGER")))
if [ "$written" -ge "${NUCLEO_FLASH_BUDGET:-1000}" ]; then
    echo "Error: $written writings of this flash counted in $LEDGER, its budget spent" >&2
    exit 3
fi
if [ -n "${NUCLEO_FLASH_PER_DAY:-}" ]; then
    since=$(($(date +%s) - 86400))
    today=$(awk -v s="$since" '$1 >= s { n++ } END { print n + 0 }' "$LEDGER")
    # Only a writing is held back: an image already in the flash is started whatever the
    # count; the read against it tells.
    PER_DAY=$((today >= NUCLEO_FLASH_PER_DAY))
else
    PER_DAY=0
fi

# Connected with the reset held, as tools/nucleo_load.sh. verify_image reads the flash
# and fails at the first difference; only then is the image written ("program", which
# erases the pages it covers and no other, and reads them back).
SERIAL=${NUCLEO_SERIAL:+adapter serial $NUCLEO_SERIAL;}
out=$(${OPENOCD:-openocd} -f interface/stlink-dap.cfg \
        -c "transport select dapdirect_swd; $SERIAL gdb_port disabled; telnet_port disabled; tcl_port disabled" \
        -f target/$TARGET.cfg \
        -c "reset_config srst_only srst_nogate connect_assert_srst; init; reset halt" \
        -c "if {[catch {verify_image $image}]} { if {$PER_DAY} { echo ESCAPEMENT-HELD } else { echo ESCAPEMENT-WRITE; program $image verify } } else { echo ESCAPEMENT-SAME }" \
        -c "reset run; echo ESCAPEMENT-DONE; shutdown" 2>&1) || true
# The differences verify_image lists come before the mark and are no error; any after it is.
after=$(echo "$out" | awk '/^ESCAPEMENT-(SAME|WRITE|HELD)/ { f = 1 } f')
echo "${after:-$out}" | grep -E "^(Error|Warn)|^ESCAPEMENT-(SAME|WRITE|HELD)|Verified|wrote" || true
case $after in
    ESCAPEMENT-HELD*) echo "Error: $today writings of this flash in the last 24 hours, held back," \
                          "the board reset on what it held" >&2
                      exit 3 ;;
    ESCAPEMENT-WRITE*) echo "$(date +%s) $(basename "$ELF")" >>"$LEDGER" ;;
esac
! echo "$after" | grep -q "^Error" && echo "$after" | grep -q "^ESCAPEMENT-DONE"
