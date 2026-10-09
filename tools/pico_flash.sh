#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Write an image linked into the flash (make FLASH=1, Examples/pico/build-flash) into the
# Pico's flash, and boot it through the bootrom and the second stage, on the UNO Q:
#
#   OPENOCD=~/opt/openocd-rpi/bin/openocd PROBE=probe1 tools/pico_flash.sh ELF
#
# Raspberry Pi's OpenOCD, which knows the rescue: Debian's 0.12.0 ignores RESCUE and stays
# up as a server.
#
# The chip is first reset whole through the rescue debug port (RESCUE=1 of target/
# rp2040.cfg, RP2040 datasheet, 2.3.4.2), which a reset of the cores is not: its clocks go
# back to the ring oscillator, and the image is written at the slow rate of the SSI that
# gives, whatever the image before had set. OpenOCD then halts the chip at the bootrom's
# entry, writes the image, and resets it (SYSRESETREQ, its reset run): the bootrom reads
# the second stage, checks its CRC and runs it, which configures the XIP and enters the
# image (Escapement_Boot2.S). The image then runs without the debugger.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
ELF=$1

OCD=${OPENOCD:-openocd}
timeout 20 "$OCD" -f interface/cmsis-dap.cfg -c "$(sh "$HERE/probe.sh")" -c "set RESCUE 1" \
    -f target/rp2040.cfg >/dev/null 2>&1 || true
"$OCD" -f interface/cmsis-dap.cfg -c "$(sh "$HERE/probe.sh")" \
    -c "adapter speed ${ADAPTER_KHZ:-5000}" \
    -c "set USE_CORE 0" -f target/rp2040.cfg -c init -c "reset halt" \
    -c "program $ELF verify" -c "reset run" -c exit 2>&1 |
    grep -E 'Programming|Verified|rror' || true
