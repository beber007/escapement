#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The state of the bench on one screen: the machine, each Debug Probe and who holds it,
# the board CI, each endurance run with its last reading, and the commit statuses of the
# head of main and of the commits under endurance (tools/board_ci.md, "The bench").
#
#   tools/bench_status.sh             on the bench's machine
#   tools/bench_status.sh HOST        from another, over SSH: HOST is $BENCH_HOST if set,
#                                     else arduino@192.168.1.220, the UNO Q
#
# It reads, and changes nothing: no probe is driven, no lock taken.
set -u

if [ ! -d "${BOARD_CI_WORK:-$HOME/escapement-rp2040}/board-ci" ]; then
    exec ssh "${1:-${BENCH_HOST:-arduino@192.168.1.220}}" sh -s <"$0"
fi

WORK=${BOARD_CI_WORK:-$HOME/escapement-rp2040}/board-ci
PROBES=${PROBE_TABLE:-$HOME/.config/escapement-probes}
TOKEN=${BOARD_CI_TOKEN:-$HOME/.config/escapement-board-ci/token}
REPO=${BOARD_CI_REPO:-beber007/escapement}

# holder <lock>: the process holding a lock directory, or "free"
holder() {
    pid=$(cat "$1/pid" 2>/dev/null) || { echo free; return; }
    if kill -0 "$pid" 2>/dev/null; then
        echo "held by $pid: $(ps -o args= -p "$pid" | cut -c1-70)"
    else
        echo "free (stale lock of $pid)"
    fi
}

echo "== $(hostname), $(date -u '+%Y-%m-%d %H:%M UTC'), up since $(uptime -s)"
echo
echo "== Debug Probes"
grep -v '^#' "$PROBES" 2>/dev/null | while read -r name serial; do
    [ -n "$name" ] || continue
    if ls /dev/serial/by-id/*"$serial"* >/dev/null 2>&1; then seen=present; else seen=ABSENT; fi
    printf '%-7s %-17s %-8s %s\n' "$name" "$serial" "$seen" "$(holder "$WORK/lock-$name")"
done

echo
echo "== Board CI"
echo "run lock: $(holder "$WORK/lock")"
echo "timer:    $(systemctl --user show -p ActiveState --value escapement-board-ci.timer)," \
     "last $(systemctl --user show -p LastTriggerUSec --value escapement-board-ci.timer)"
last=$(ls -t "$WORK/logs" 2>/dev/null | head -1)
[ -n "$last" ] && echo "last log: $last, $(tail -1 "$WORK/logs/$last" | cut -c1-90)"

echo
echo "== Endurance runs"
shas=""
for unit in $(systemctl --user list-unit-files 'escapement-soak*' --no-legend | awk '{print $1}'); do
    state=$(systemctl --user show -p ActiveState --value "$unit")
    env=$(systemctl --user show -p Environment --value "$unit")
    log=$(echo "$env" | tr ' ' '\n' | sed -n 's/^BOARD_SOAK_LOG=//p')
    sha=$(echo "$env" | tr ' ' '\n' | sed -n 's/^BOARD_SOAK_SHA=//p')
    echo "$unit: $state, commit ${sha:-?}" | cut -c1-100
    [ -n "$sha" ] && [ "$state" = active ] && shas="$shas $sha"
    [ -f "${log:-/nonexistent}" ] || { echo "  no log"; continue; }
    # the lines of the current run, from its header on: the last line not a reading
    run=$(awk '/^[A-Z][A-Za-z0-9]* / && !/^20[0-9][0-9]-/ {n = NR} END {print n + 0}' "$log")
    tail -n +"$run" "$log" | head -1 | cut -c1-110 | sed 's/^/  started: /'
    marks=' (RESTART|INTERRUPTED|ERRORS|STOPPED|SECONDS BEHIND|LOAD FAILED|TAKEN OVER|no reading)'
    n=$(tail -n +"$run" "$log" | grep -cE "$marks")
    if [ "$n" -eq 0 ]; then
        echo "  marked:  no restart, interruption, take-over or error"
    else
        echo "  marked:  $n lines of restart, interruption, take-over or error, the last:"
    fi
    tail -n +"$run" "$log" | grep -E "$marks" | tail -3 | cut -c1-110 | sed 's/^/    /'
    # the whole reading, folded between its fields at 110 columns
    tail -1 "$log" | awk '{
        n = split($0, f, ", "); line = "  last:    " f[1]
        for (i = 2; i <= n; i++)
            if (length(line) + length(f[i]) + 2 > 110) { print line ","; line = "           " f[i] }
            else line = line ", " f[i]
        print line }'
done

echo
echo "== Commit statuses"
if [ -r "$TOKEN" ]; then
    head=$(curl -fsS -H "Authorization: Bearer $(cat "$TOKEN")" \
        "https://api.github.com/repos/$REPO/commits/main" | jq -r .sha 2>/dev/null)
    for sha in ${head:-} $(echo "$shas" | tr ' ' '\n' | sort -u | grep -vx "${head:-none}"); do
        [ "$sha" = "${head:-}" ] && what="head of main" || what="under endurance"
        echo "$(echo "$sha" | cut -c1-7) ($what)"
        curl -fsS -H "Authorization: Bearer $(cat "$TOKEN")" \
            "https://api.github.com/repos/$REPO/commits/$sha/status" |
            jq -r '.statuses[] | "  \(.context): \(.state), \(.description)"' | cut -c1-110
    done
else
    echo "no token ($TOKEN)"
fi
