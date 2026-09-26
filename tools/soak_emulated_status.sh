#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Sums up the logs of tools/soak_emulated.sh: for each instance, its last reading and
# whether it failed, and for all of them the virtual time run; with a token, posts it as
# the commit status "emulation/soak" of SHA.
#
#   tools/soak_emulated_status.sh OUT [SHA]     BOARD_CI_TOKEN, BOARD_CI_REPO as soak.py
set -eu
OUT=${1:?output directory}
SHA=${2:-}
REPO=${BOARD_CI_REPO:-beber007/escapement}
TOKEN=${BOARD_CI_TOKEN:-$HOME/.config/escapement-board-ci/token}
total=0 running=0 failed=0 passed=0
for log in "$OUT"/*.log; do
    name=$(basename "$log" .log)
    last=$(sed 's/\x1b\[[0-9;]*m//g' "$log" | grep -E '^reading ' | tail -1)
    seconds=$(echo "$last" | sed -n 's/^reading [0-9]*: \([0-9]*\) s.*/\1/p')
    total=$((total + ${seconds:-0}))
    if grep -q '^exit 0' "$log"; then state=passed; passed=$((passed + 1))
    elif grep -q '^exit' "$log"; then state=FAILED; failed=$((failed + 1))
    else state=running; running=$((running + 1)); fi
    echo "$name: $state, ${last:-no reading yet}"
done
summary="$passed passed, $running running, $failed failed; $((total / 3600)) h of virtual time"
echo "$summary"
if [ -n "$SHA" ] && [ -r "$TOKEN" ]; then
    if [ $failed -gt 0 ]; then state=failure
    elif [ $running -gt 0 ]; then state=pending
    else state=success; fi
    printf '{"state":"%s","context":"emulation/soak","description":"%s"}' "$state" "$summary" |
    curl --silent --show-error --fail --output /dev/null -X POST \
        -H "Authorization: Bearer $(cat "$TOKEN")" -H "Accept: application/vnd.github+json" \
        --data @- "https://api.github.com/repos/$REPO/statuses/$SHA" || true
fi
