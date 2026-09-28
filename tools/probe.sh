#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Print the OpenOCD command that picks the Debug Probe the tools talk to.
#
#   PROBE=probe2 tools/probe.sh      # a name from the table, or a USB serial as is
#
# The bench has several probes: OpenOCD, left to itself, takes the first it finds, which
# is not always the one wired to the board under test. The table names them by their USB
# serial, one "name serial" per line (~/.config/escapement-probes, or $PROBE_TABLE):
#   probe1 0123456789ABCDEF
# Without PROBE, probe1 if the table has it; without a table, any Debug Probe.
#
# The probe is picked by its USB ids too: OpenOCD otherwise asks every Raspberry Pi device
# for its strings, the Pico's own USB too, which a firmware in flash may leave unanswering:
# 3.3 s per connection, which failed the cost check's count of rounds (2026-09-25).
#
# The line also closes the ports OpenOCD opens for gdb, telnet and Tcl, which no tool
# uses: two OpenOCDs at once, each on a probe of its own, otherwise collide on port 3333,
# and the second fails, as a reload of an endurance run did when the UNO Q booted
# (2026-09-28).
set -eu

TABLE=${PROBE_TABLE:-$HOME/.config/escapement-probes}
NAME=${PROBE:-probe1}

SERIAL=""
if [ -f "$TABLE" ]; then
    SERIAL=$(awk -v n="$NAME" '$1 == n { print $2; exit }' "$TABLE")
fi
if [ -z "$SERIAL" ]; then
    case $NAME in
        probe[0-9]*)
            if [ -n "${PROBE:-}" ] || [ -f "$TABLE" ]; then
                echo "no $NAME in $TABLE" >&2; exit 1
            fi ;;
        *) SERIAL=$NAME ;;
    esac
fi

# One line, which OpenOCD takes as a single -c: Tcl runs the commands in turn.
echo "cmsis_dap_vid_pid 0x2e8a 0x000c${SERIAL:+; adapter serial $SERIAL}; gdb_port disabled; telnet_port disabled; tcl_port disabled"
