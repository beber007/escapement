#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Write an image linked into the flash (make FLASH=1, Examples/pico2/build-flash) into the
# Pico 2's flash, and boot it as a power-up would, through the bootrom, on the UNO Q:
#
#   OPENOCD=~/opt/openocd-rpi/bin/openocd PROBE=probe3 tools/pico2_flash.sh ELF
#
# The chip is first restarted by the RP-AP's rescue (rescue_reset of Raspberry Pi's
# target/rp2350.cfg), which stops both cores in the bootrom whatever the flash holds, as
# tools/pico2_check.py does. OpenOCD then writes the image through the bootrom's flash
# functions, and resets core 0 (SYSRESETREQ, its reset run): the bootrom, the rescue flag
# acknowledged since the rescue (RP2350 datasheet, 3.5.8), runs its whole boot path, finds
# the IMAGE_DEF and enters the image; core 1 waits in the bootrom, where the rescue left
# it, for the image to launch it. The image runs without the debugger.
#
# A reset by the watchdog instead, both cores and all but the oscillators selected in
# PSM_WDSEL as the pico-sdk's watchdog_reboot does, booted an image that the debugger could
# not examine afterwards: its access port to core 0 answered WAIT for good (2026-10-09).
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
ELF=$1
OCD=${OPENOCD:-openocd}

"$OCD" -f interface/cmsis-dap.cfg -c "$(sh "$HERE/probe.sh")" \
    -c "adapter speed ${ADAPTER_KHZ:-5000}" \
    -c "set USE_CORE 0" -f target/rp2350.cfg -c init -c rescue_reset \
    -c "program $ELF verify" -c "reset run" -c exit 2>&1 |
    grep -E 'Programming|Verified|rror' || true
