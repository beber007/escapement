#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Run the board checks on the newest commit of main, then on the head of each open pull
# request from a branch of this repository, and post the outcome to GitHub as commit
# statuses ("board/pico"). Meant for the machine the Pico is wired to, run by a timer;
# installing it: tools/board_ci.md, the checks: docs/rp2040.md.
#
#   tools/board_ci.sh [--force]      # --force: test main again even if already tested
#
# The machine pulls; GitHub never pushes code to it. It checks main, and a pull request
# only if its head is that of a branch of this repository, which only those who may push
# to it, and Dependabot, can make (tools/board_ci.md): a pull request from a fork runs
# nothing here, whatever it changes —
# which a self-hosted runner on a public repository could not promise (issue #21). Main
# comes first; then each run checks one pull request, the oldest whose head is not yet
# checked. The script itself always runs as main has it; the tools of the checks are
# those of the commit checked. On a pull request the long runs of the STM32 boards do not
# take its images: they go on with main's (resume() below).
#
# On Linux it builds in one container and drives the probe from another; on macOS,
# with the toolchain and OpenOCD from Homebrew, it runs both on the machine itself:
#   BOARD_CI_WORK    working directory (~/escapement-rp2040), which the containers mount
#   BOARD_CI_MOUNT   where they mount it (/work)
#   BOARD_CI_BUILD   container with the ARM toolchain (esc on Linux, none on macOS)
#   BOARD_CI_PROBE   container with OpenOCD and the probe (hw on Linux, none on macOS)
#   PROBE            the Debug Probe the checks use, probe1 by default (tools/probe.sh)
#   BOARD_CI_REPO    owner/name on GitHub (beber007/escapement)
#   BOARD_CI_TOKEN   file holding a token allowed to write commit statuses, nothing else
#                    (~/.config/escapement-board-ci/token); without it, nothing is posted
#   BOARD_CI_IMAGES  where the images come from: build (the default) builds them here,
#                    with the toolchain of the machine; ci takes those the CI built for
#                    the commit (tools/board_images.sh, artifact board-images), and needs
#                    no compiler: the token must then also read Actions. The compiled
#                    order, which the bench checked under a second compiler, is then the
#                    CI's to check (build.yml).
#   BOARD_CI_U5      1 on the Arduino UNO Q: beside the Pico, the STM32U5 of the board is
#                    checked too (tools/unoq_check.sh), on the CI's image, and posted as
#                    the status "board/u5"; the endurance run it holds goes on with the
#                    commit if it passes.
#   BOARD_CI_U3      1 when a NUCLEO-U385RG-Q is plugged into the UNO Q: the same checks
#                    on its STM32U385 (BOARD=u3 tools/unoq_check.sh), posted as the
#                    status "board/u3", through its ST-LINK, whose serial and port are
#                    BOARD_CI_U3_SERIAL and BOARD_CI_U3_TTY, and the OpenOCD
#                    BOARD_CI_OPENOCD_U3 (~/opt/openocd-upstream/bin/openocd), which
#                    knows the STM32U3; its long run, escapement-soak-u3, goes on likewise.
#                    The power-aware kernel's images are checked first, without it.
#   BOARD_CI_PICO2   1 when a Pico 2 is wired too: its six examples that count in memory
#                    (tools/pico2_check.py) and its two of the UART, through the probe's
#                    own (tools/pico2_uart.py), are run on the CI's images and posted as
#                    the status "board/pico2", through the probe BOARD_CI_PICO2_PROBE (probe3)
#                    and the OpenOCD BOARD_CI_OPENOCD_RP2350
#                    (~/opt/openocd-rpi/bin/openocd), which knows the RP2350.
set -eu
STARTED_AS=$(cksum <"$0")   # before the checkout below can replace this very file

if [ "$(uname)" = Darwin ]; then containers=""; else containers=yes; fi
WORK=${BOARD_CI_WORK:-$HOME/escapement-rp2040}
MOUNT=${BOARD_CI_MOUNT:-/work}
BUILD=${BOARD_CI_BUILD-${containers:+esc}}
PROBER=${BOARD_CI_PROBE-${containers:+hw}}
REPO=${BOARD_CI_REPO:-beber007/escapement}
TOKEN=${BOARD_CI_TOKEN:-$HOME/.config/escapement-board-ci/token}
IMAGES=${BOARD_CI_IMAGES:-build}
DIR=$WORK/board-ci
SRC=$DIR/src
PICO=Escapement/CORTEX-Mx/RP2040/Examples/pico

mkdir -p "$DIR/logs"
# take <lock>: makes the lock directory ours, our pid in it, and takes over one whose
# process is gone; returns 1 while a live one holds it. A directory, since mkdir is
# atomic everywhere; the taking over is not, and two processes finding one stale lock
# both held it, 6 times in 300 (a review, 2026-09-30): it runs under flock where there is
# one, Linux but not macOS, as tools/soak.py takes its probe's lock.
take() {
    (
        if command -v flock >/dev/null; then flock 8; fi
        if ! mkdir "$1" 2>/dev/null; then
            kill -0 "$(cat "$1/pid" 2>/dev/null)" 2>/dev/null && exit 1
            rm -rf "$1" && mkdir "$1" || exit 1
        fi
        echo $$ >"$1/pid"
    ) 8>"$DIR/lock-take"
}

# One run at a time.
LOCK=$DIR/lock
if [ "${BOARD_CI_RESTARTED:-}" = yes ]; then
    :   # started again by itself, below: the lock is already ours
elif ! take "$LOCK"; then
    echo "another run holds $LOCK"; exit 0
fi
echo $$ >"$LOCK/pid"
held="" PR="" MAIN=""
# After a pull request, main is checked out again: the timer starts this script from the
# checkout, and must find main's.
cleanup() {
    rm -rf "$LOCK" $held
    [ -z "$PR" ] || git -C "$SRC" checkout --quiet --force --detach "$MAIN"
}
trap cleanup EXIT

# hold <probe>: the lock of one Debug Probe, which tools/soak.py holds for as long as an
# endurance run lasts on the board wired to it; returns 1 while a live process holds it.
# A probe, not the bench: a run of two weeks on one board leaves the others checked.
hold() {
    probe_lock=$DIR/lock-$1
    take "$probe_lock" || return 1
    held="$held $probe_lock"
}

[ -d "$SRC/.git" ] || git clone --quiet "https://github.com/$REPO.git" "$SRC"
git -C "$SRC" fetch --quiet origin main
MAIN=$(git -C "$SRC" rev-parse origin/main)
main_due="" force=""
[ "${1:-}" != "--force" ] || force=yes
if [ -n "$force" ] || [ "$(cat "$DIR/last" 2>/dev/null)" != "$MAIN" ]; then
    main_due=yes
fi
# The open pull requests from a branch of this repository whose head is not checked yet,
# oldest first, as number:head:branch. GitHub lists them to anyone: no token. The answer
# kept before jq reads it, as in ci_images below; none read, none this run.
prs=""
if json=$(curl --silent --show-error --fail \
        "https://api.github.com/repos/$REPO/pulls?state=open&sort=created&direction=asc&per_page=100"); then
    for pr in $(printf '%s' "$json" | jq -r --arg r "$REPO" \
            '.[] | select(.head.repo.full_name == $r) | "\(.number):\(.head.sha):\(.head.ref)"'); do
        sha=${pr#*:}; sha=${sha%%:*}
        grep -qx "$sha" "$DIR/prs-checked" 2>/dev/null || prs="$prs $pr"
    done
fi
[ -n "$main_due$prs" ] || exit 0
git -C "$SRC" checkout --quiet --force --detach "$MAIN"
git -C "$SRC" clean --quiet -fdx
# The timer runs this script from the checkout it has just updated, and the shell goes on
# reading the file it opened: a change to the script took effect one run late, and the
# first run of a new check reported the old ones. Start the new script instead, once.
# Always main's: a pull request's tools are checked out only further down.
if [ "${BOARD_CI_RESTARTED:-}" != yes ] &&
   [ "$STARTED_AS" != "$(cksum <"$SRC/tools/board_ci.sh")" ]; then
    BOARD_CI_RESTARTED=yes exec sh "$SRC/tools/board_ci.sh" "$@"
fi
SHA=$MAIN

status() {   # state description [context, board/pico by default]
    # Kept for the comment on a pull request, the last of each context counting. A post
    # that fails sets $unposted, which only skip_docs reads.
    printf '%s\t%s\t%s\n' "${3:-board/pico}" "$1" "$2" >>"$DIR/posted"
    [ -r "$TOKEN" ] || return 0
    jq -n --arg s "$1" --arg d "$2" --arg c "${3:-board/pico}" \
        '{state: $s, context: $c, description: $d}' |
    curl --silent --show-error --fail --output /dev/null -X POST \
        -H "Authorization: Bearer $(cat "$TOKEN")" -H "Accept: application/vnd.github+json" \
        --data @- "https://api.github.com/repos/$REPO/statuses/$SHA" || unposted=yes
}

# in_build and in_probe run a command in the example's directory and at the root of the
# checkout, in the container when there is one; SEEN is $DIR as the command sees it.
if [ -n "$BUILD$PROBER" ]; then SEEN=$MOUNT/board-ci; else SEEN=$DIR; fi
in_build() {
    if [ -n "$BUILD" ]; then podman exec "$BUILD" sh -c "cd $SEEN/src/$PICO && $1"
    else (cd "$SRC/$PICO" && sh -c "$1"); fi
}
in_probe() {
    if [ -n "$PROBER" ]; then podman exec "$PROBER" sh -c "cd $SEEN/src && $1"
    else (cd "$SRC" && sh -c "$1"); fi
}

api() {   # path: a GET on the repository's API
    curl --silent --show-error --fail -H "Authorization: Bearer $(cat "$TOKEN")" \
        -H "Accept: application/vnd.github+json" "https://api.github.com/repos/$REPO/$1"
}

# ci_images <event>: the images the CI built for $SHA into $DIR/fw, in its run on that
# event (push for main, pull_request for a pull request, whose run builds the head merged
# into its base, as it would land). Returns 1 while its run is
# not over, which the next run of this script waits for; 2 if there are none; 3 if the
# run failed, its images then vouching for nothing (a review, 2026-09-30: the board posted
# its statuses on the images of a run whose checks had failed), with $attempt set; 4 if
# GitHub did not answer or the download broke, which the next run tries again.
ci_images() {
    event=$1
    # The answer kept before jq reads it: piped, the status was jq's, 0 on an empty input,
    # and a GitHub that did not answer passed for a run with no images (a review,
    # 2026-10-11).
    json=$(api "actions/workflows/build.yml/runs?head_sha=$SHA&event=$event") || return 4
    run=$(printf '%s' "$json" |
          jq -r '.workflow_runs[0] | "\(.status) \(.conclusion) \(.id) \(.run_attempt)"') ||
        return 4
    [ -n "$run" ] || return 4
    set -- $run
    [ "$1" = completed ] || return 1
    attempt="$3 $4"
    [ "$2" = success ] || return 3
    # Asked by name: the list comes 30 to a page, and the images fell on the second once
    # the run made 34 (2026-10-24, the jobs of the execution times).
    json=$(api "actions/runs/$3/artifacts?name=board-images") || return 4
    url=$(printf '%s' "$json" |
          jq -r '.artifacts[] | select(.name == "board-images") | .archive_download_url') ||
        return 4
    [ -n "$url" ] || return 2
    rm -rf "$DIR/fw" "$DIR/fw.zip" && mkdir -p "$DIR/fw"
    curl --silent --show-error --fail --location -H "Authorization: Bearer $(cat "$TOKEN")" \
        --output "$DIR/fw.zip" "$url" && unzip -q -o "$DIR/fw.zip" -d "$DIR/fw" || return 4
}

# build <name> <image> <make arguments>: one image into $DIR/fw/<name>.
build() {
    in_build "make clean >/dev/null && make $3 build/$2.elf" &&
    mkdir -p "$DIR/fw/$1" && cp "$SRC/$PICO/build/$2.elf" "$DIR/fw/$1/"
}

# fourslot <name>: no read of the 4-slot buffer torn or going backwards, and the plain
# array beside it torn at least once, or the check proved nothing.
fourslot() {
    out=$(in_probe "sh tools/fourslot_cores.sh 10 $SEEN/fw/$1/FourSlotCoresPico.elf")
    echo "$out"
    echo "$out" | awk '
        /^4-slot buffer/ { reads = $4; bad = $6 + $8 }
        /^plain array/   { torn = $6 }
        END { exit !(reads > 100000 && bad == 0 && torn > 0) }'
}

# order <name>: tools/check_order.py on the kernel this machine's compiler built, which is
# not the CI's: the order of the slot buffers and of the task-level stores holds under
# both, or one of them breaks it.
order() {
    in_build "arm-none-eabi-gcc --version | head -1 &&
              python3 $SEEN/src/tools/check_order.py \$(ls build/Escapement*.o | grep -v _)"
}

# cost <name>: the 1 ms round of TaskLEDPico, 10 s of it, and at most 5 us on average
# (3.2 measured for the hard kernel on 2026-09-23, docs/rp2040.md). The rounds count from
# the load to the reading, which takes the probe longer on some machines than on others:
# 10,067 on a Linux machine, 10,135 and 10,255 on a Mac mini, 10,116 on an Arduino UNO Q.
# The upper bound only catches a kernel that runs its round too often.
cost() {
    out=$(in_probe "sh tools/measure_cost.sh 10 $SEEN/fw/$1/TaskLEDPico.elf")
    echo "$out"
    echo "$out" | awk '
        /^rounds/ { rounds = $3 }
        /^mean/   { mean = $3 }
        END { exit !(rounds >= 9990 && rounds <= 11000 && mean <= 5.0) }'
}

# events <name>: TestTimerEventPico, five readings of its trace. GPIO 2 every 5 ms and
# GPIO 3 every 10 ms within 5 us, high for 1 and 2 ms plus the path from the alarm to the
# event-driven task (12 to 13 us on 2026-09-23, docs/rp2040.md) within 20 us, and every
# event delivered at most 2 us after its alarm (0 then).
events() {
    out=$(in_probe "python3 tools/timer_events.py $SEEN/fw/$1/TestTimerEventPico.elf")
    echo "$out"
    echo "$out" | awk '
        function within(value, low, high) { return value >= low && value <= high }
        $1 == "gpio" && $2 == 2 { pin2 = within($5, 4995, 5005) && within($6, 4995, 5005) &&
                                          within($10, 1000, 1020) && within($11, 1000, 1020) &&
                                          substr($8, 2) + 0 >= 20 }
        $1 == "gpio" && $2 == 3 { pin3 = within($5, 9995, 10005) && within($6, 9995, 10005) &&
                                          within($10, 2000, 2020) && within($11, 2000, 2020) &&
                                          substr($8, 2) + 0 >= 10 }
        /^events delivered/     { events = $4 + 0 > 0 && $7 <= 2 }
        END { exit !(pin2 && pin3 && events) }'
}

# dvfs <name>: BenchDVFSPico, each change of speed 10,000 times. Means of 2026-09-24 with
# two reads of the counter included (docs/rp2040.md): 8.7 us from 12 to 125 MHz at most,
# 6.0 by the wake-up path; allowed about a quarter more, rounded up to the microsecond.
# The wake-up path, which exists to be the shorter one, must stay shorter than the general
# one from the same speed.
dvfs() {
    out=$(in_probe "python3 tools/dvfs_bench.py $SEEN/fw/$1/BenchDVFSPico.elf")
    echo "$out"
    echo "$out" | awk -F': ' '
        { gsub(/ +$/, "", $1); us[$1] = $2 + 0 }
        END {
            ok = us["12 -> 125 MHz"] <= 11 && us["125 -> 12 MHz"] <= 6 &&
                 us["50 -> 125 MHz"] <= 7 && us["125 -> 50 MHz"] <= 7 &&
                 us["12 -> 50 MHz"] <= 10 && us["50 -> 12 MHz"] <= 6 &&
                 us["wake from 12 MHz"] <= 8 && us["wake from 50 MHz"] <= 6 &&
                 us["wake from 12 MHz"] < us["12 -> 125 MHz"] &&
                 us["wake from 50 MHz"] < us["50 -> 125 MHz"]
            for (row in us) if (row ~ /->|^wake/) { rows += 1; if (us[row] < 1) ok = 0 }
            exit !(ok && rows == 8) }'
}

# checked: $SHA checked, not to be again: main's in the file last, a pull request's added to
# prs-checked.
checked() {
    if [ -z "$PR" ]; then echo "$SHA" >"$DIR/last"; else echo "$SHA" >>"$DIR/prs-checked"; fi
}

# comment: the outcome on the pull request, one comment of the bench's, found by its
# marker and replaced at each check (issue #28): the statuses posted, the key figures and
# the whole log, the probes' serial numbers and this machine's paths taken out. Needs
# the token to write pull requests; without, the statuses alone are posted.
comment() {
    [ -r "$TOKEN" ] || return 0
    marker="<!-- board-ci -->"
    log=$(sed -e "s|$HOME|~|g" -e 's/[0-9A-F]\{16,\}/<serial>/g' "$LOG")
    body=$(
        echo "$marker"
        echo "### Board checks of $(echo "$SHA" | cut -c1-7)"
        echo
        echo "$(date -u '+%Y-%m-%d %H:%M UTC'), $((($(date +%s) - STARTED + 59) / 60)) min, images: $IMAGES"
        echo
        echo "| Board | State | Description |"
        echo "|---|---|---|"
        # The last status of each context, in the order first posted.
        awk -F'\t' '!($1 in last) { order[++n] = $1 } { last[$1] = $2 "\t" $3 }
            END { for (i = 1; i <= n; i++) { split(last[order[i]], f, "\t")
                  printf "| %s | %s | %s |\n", order[i], f[1], f[2] } }' "$DIR/posted"
        echo
        echo "<details><summary>Key figures</summary>"
        echo
        echo '```text'
        printf '%s\n' "$log" | grep -E '^(=== |ok|FAILED|mean|maximum|rounds|4-slot buffer|events delivered|[0-9]+ -> [0-9]+ MHz|SleepU|[0-9]+ reports over)' || true
        echo '```'
        echo "</details>"
        echo
        echo "<details><summary>The bench's log ($(printf '%s\n' "$log" | wc -l) lines)</summary>"
        echo
        echo '```text'
        printf '%s\n' "$log" | head -c 60000
        echo '```'
        echo "</details>"
    )
    auth="Authorization: Bearer $(cat "$TOKEN")"
    url="https://api.github.com/repos/$REPO/issues"
    # The answer kept before jq reads it, as in ci_images: unread, a second comment is
    # posted rather than none.
    id=""
    if json=$(curl --silent --show-error --fail -H "$auth" "$url/$PR/comments?per_page=100"); then
        id=$(printf '%s' "$json" |
             jq -r --arg m "$marker" '[.[] | select(.body | startswith($m))][0].id // empty') ||
            id=""
    fi
    if [ -n "$id" ]; then
        method=PATCH target=$url/comments/$id
    else
        method=POST target=$url/$PR/comments
    fi
    jq -n --arg b "$body" '{body: $b}' |
        curl --silent --show-error --fail --output /dev/null -X "$method" -H "$auth" \
            -H "Accept: application/vnd.github+json" --data @- "$target" ||
        echo "the comment on pull request #$PR was not posted"
}

# docs_only: whether $SHA changes Markdown files alone, none of which goes into an image
# the boards run, since a base whose four board statuses are all success: main since the
# last main checked, a pull request since its merge base with main. That last check may
# have failed, or posted nothing for a board whose probe was held, and a merge base may
# never have been checked: the statuses on GitHub, which the rule of main reads, decide,
# the latest of each context. Sets $base, $changes, as git lists them, and $docs, their
# number. Returns 1, and the commit gets the full check, when anything else changed,
# nothing did, the base has not passed, GitHub did not answer, or on --force.
# --no-renames shows a rename as a file deleted and one added: a file deleted, even a .md,
# never passes for documentation alone.
docs_only() {
    [ -z "$force" ] && [ -r "$TOKEN" ] || return 1
    if [ -n "$PR" ]; then
        # The head fetched as below, the same branch at the same commit.
        git -C "$SRC" fetch --quiet origin "refs/heads/$BRANCH" &&
            [ "$(git -C "$SRC" rev-parse FETCH_HEAD)" = "$SHA" ] &&
            base=$(git -C "$SRC" merge-base "$MAIN" "$SHA") || return 1
    else
        base=$(cat "$DIR/last" 2>/dev/null) && [ -n "$base" ] || return 1
    fi
    changes=$(git -C "$SRC" diff --no-renames --name-status "$base" "$SHA" 2>/dev/null) ||
        return 1
    docs=$(printf '%s\n' "$changes" | awk -F'\t' '
        NF == 0 { next }
        { n += 1 }
        $1 != "A" && $1 != "M" || $2 !~ /\.md$/ { other = 1 }
        END { if (other || n == 0) exit 1; print n }') || return 1
    # Kept before jq reads it, as in ci_images.
    json=$(api "commits/$base/status?per_page=100") || return 1
    printf '%s' "$json" | jq -e '[.statuses[] | {(.context): .state}] | add // {} |
        .["board/pico"] == "success" and .["board/pico2"] == "success" and
        .["board/u5"] == "success" and .["board/u3"] == "success"' >/dev/null
}

# skip_docs: the four statuses the rule of main requires, in success as on the base, the
# commit marked checked, and the comment on a pull request; no image, no probe's lock, no
# long run stopped. Such a pull request waited its turn on the bench for nothing. Marked
# checked only once the four are posted: else the next run tries again.
skip_docs() {
    LOG=$DIR/logs/$(date +%Y%m%d-%H%M%S)-$(echo "$SHA" | cut -c1-7).log
    STARTED=$(date +%s) IMAGES=none
    : >"$DIR/posted"
    what=main; [ -z "$PR" ] || what="pull request #$PR"
    short=$(echo "$base" | cut -c1-7)
    {
        echo "$what at $SHA, $(date), documentation only since $base, which passed:"
        printf '%s\n' "$changes" | cut -f2-
    } >"$LOG"
    files="files"; [ "$docs" != 1 ] || files="file"
    unposted=""
    for board in pico u5 u3 pico2; do
        status success "documentation only, as $short ($docs $files), nothing for the boards" \
            "board/$board"
    done
    if [ -n "$unposted" ]; then
        echo "the statuses of $SHA were not all posted: tried again at the next run"
        return 0
    fi
    checked
    cat "$LOG"
    [ -z "$PR" ] || comment
}

# The commit to check: main if due, else the pull requests in turn; with the CI's images,
# the first whose run is over, so that a run not over or failed, main's included, holds
# none of the others back. One commit a run.
[ "$IMAGES" != ci ] || [ -r "$TOKEN" ] || { echo "BOARD_CI_IMAGES=ci needs $TOKEN"; exit 1; }
picked=""
for candidate in ${main_due:+main} $prs; do
    if [ "$candidate" = main ]; then
        SHA=$MAIN PR="" event=push
    else
        PR=${candidate%%:*} rest=${candidate#*:}
        SHA=${rest%%:*} BRANCH=${rest#*:} event=pull_request
    fi
    if docs_only; then skip_docs; exit 0; fi
    [ "$IMAGES" = ci ] || { picked=yes; break; }
    got=0
    ci_images $event || got=$?
    if [ $got -eq 0 ]; then
        picked=yes; break
    elif [ $got -eq 1 ]; then
        echo "the CI has not finished with $SHA"
    elif [ $got -eq 3 ]; then
        # Posted once per attempt of the run: one run again and passing is then checked.
        if ! grep -qx "$SHA $attempt" "$DIR/ci-failed" 2>/dev/null; then
            echo "$SHA $attempt" >>"$DIR/ci-failed"
            status error "the CI failed on this commit: not checked on the board"
        fi
        echo "the CI failed on $SHA (run $attempt)"
    elif [ $got -eq 4 ]; then
        echo "no answer from GitHub for $SHA: tried again at the next run"
    else
        checked
        status error "no images from the CI for this commit"
    fi
done
[ -n "$picked" ] || { PR=""; exit 0; }
what=main
if [ -n "$PR" ]; then
    # Its head only if the branch of that name here is at it: GitHub's list is not trusted
    # to tell a fork from this repository.
    git -C "$SRC" fetch --quiet origin "refs/heads/$BRANCH" &&
        [ "$(git -C "$SRC" rev-parse FETCH_HEAD)" = "$SHA" ] ||
        { echo "pull request #$PR: $BRANCH is not at $SHA here, checked at a next run"; exit 0; }
    git -C "$SRC" checkout --quiet --force --detach "$SHA"
    git -C "$SRC" clean --quiet -fdx
    what="pull request #$PR"
fi
LOG=$DIR/logs/$(date +%Y%m%d-%H%M%S)-$(echo "$SHA" | cut -c1-7).log
STARTED=$(date +%s)
: >"$DIR/posted"

checks="fourslot_hard fourslot_pa cost_hard events_hard events_soft events_pa dvfs_pa"
[ "$IMAGES" = ci ] || checks="order_hard order_soft order_pa $checks"

# The Pico, unless an endurance run holds its probe: then no check and no status.
pico=yes
hold "${PROBE:-probe1}" || pico=held
{
    echo "$what at $SHA, $(date), images: $IMAGES"
    [ "$IMAGES" = build ] || cat "$DIR/fw/compiler" 2>/dev/null || true
} >"$LOG" 2>&1
board_pico() {
    if [ "$pico" = held ]; then
        echo "the Pico is not checked: its probe is held ($DIR/lock-${PROBE:-probe1})"
        return 0
    fi
    failed=""
    for check in $checks; do
        # shellcheck disable=SC2046  # its words are the fields wanted
        set -- $(echo "$check" | tr _ ' ')
        case $2 in
            hard) args="" ;;
            soft) args="KERNEL=SOFT" ;;
            pa)   args="KERNEL=PA" ;;
        esac
        echo "=== $1 $2"
        case $1 in
            order)    image=TaskLEDPico ;;
            fourslot) image=FourSlotCoresPico ;;
            cost)     image=TaskLEDPico ;;
            events)   image=TestTimerEventPico; args="$args TRACE=1" ;;
            dvfs)     image=BenchDVFSPico ;;
        esac
        if [ "$1" = cost ] || [ "$1" = events ]; then
            # The counters are off in the example as shipped; this checkout is ours. (No
            # sed -i: GNU and BSD disagree on it.)
            config=$SRC/$PICO/Escapement_Config.h
            sed 's|^//#define ESCAPEMENT_MEASURE_SCHEDULING_COST|#define ESCAPEMENT_MEASURE_SCHEDULING_COST|' \
                "$config" >"$config.new" && mv "$config.new" "$config"
        fi
        name=$1_$2
        if { [ "$IMAGES" = ci ] || build "$name" "$image" "$args" >/dev/null 2>&1; } &&
           "$1" "$name"; then
            echo "ok"
        else
            echo "FAILED"
            failed="$failed $1/$2"
        fi
        git -C "$SRC" checkout --quiet -- "$PICO/Escapement_Config.h"
    done
    if [ -n "$failed" ]; then
        status failure "failed:$failed"
        return 1
    elif [ "$IMAGES" = ci ]; then
        status success "4-slot across cores, round cost, timer events, DVFS, on the CI's images"
    else
        status success "compiled order, 4-slot across cores, round cost, timer events, DVFS"
    fi
}

# The long runs of the STM32 boards go on with main's commit, never a pull request's: on
# one, each board is checked without its long run (BOARD_SOAK_SERVICE=none, which every
# version of tools/unoq_check.sh since 0.1 knows: the long run stopped, neither carried
# on nor its flash written), which resume() then starts again. soak.py finds the board
# running the checked image, whose seconds fall short of those the long run counted plus
# the time it was stopped, counts it interrupted and loads its own image again.
service="" long="the long run goes on"
if [ -n "$PR" ]; then service=none long="the long run kept on main's image"; fi
resume() {   # resume <board>: its long run started again after a pull request's check
    [ -z "$PR" ] || [ ! -f "$HOME/.config/systemd/user/escapement-soak-$1.service" ] ||
        systemctl --user start "escapement-soak-$1"
}

# The STM32U5 of the UNO Q, on the CI's image of its endurance test.
u5=""
if [ "${BOARD_CI_U5:-}" = 1 ] && [ -f "$DIR/fw/soak_u5/SoakU5.elf" ]; then u5=yes; fi
board_u5() {
    verdict=0
    if (cd "$SRC" && BOARD_SOAK_SERVICE=$service \
            sh tools/unoq_check.sh "$DIR/fw/soak_u5/SoakU5.elf" "$SHA"); then
        status success "Stop 2 for 1 min with the HSE and 1 without, endurance test for 2, clock within 300 ppm; $long" \
            board/u5
    else
        status failure "Stop 2, the endurance test or the clock failed; see the bench's log" board/u5
        verdict=1
    fi
    resume u5
    return $verdict
}

# The STM32U385 of the NUCLEO-U385RG-Q, on the CI's images, unless an endurance run holds
# it (lock-u3, tools/endurance.sh): then no check and no status, as for a probe.
u3=""
if [ "${BOARD_CI_U3:-}" = 1 ] && [ -f "$DIR/fw/soak_u3/SoakU3.elf" ]; then
    if hold u3; then u3=yes; else u3=held; fi
fi
board_u3() {
    if [ "$u3" = held ]; then
        echo "the NUCLEO-U385RG-Q is not checked: an endurance run holds it ($DIR/lock-u3)"
        return 0
    fi
    # u3check ELF [none]: the checks of one kernel's images, with none for a check
    # without the long run (tools/unoq_check.sh); empty, the service is the default one,
    # or none on a pull request.
    u3check() {
        (cd "$SRC" && BOARD=u3 NUCLEO_SERIAL=${BOARD_CI_U3_SERIAL:-} \
            NUCLEO_TTY=${BOARD_CI_U3_TTY:-} BOARD_SOAK_SERVICE=${2:-$service} \
            OPENOCD=${BOARD_CI_OPENOCD_U3:-$HOME/opt/openocd-upstream/bin/openocd} \
            sh tools/unoq_check.sh "$1" "$SHA")
    }
    # The power-aware kernel first, if the images have it, then the hard kernel, whose
    # check carries the long run on, on main.
    pa=yes verdict=0
    if [ -f "$DIR/fw/soak_u3_pa/SoakU3.elf" ]; then
        u3check "$DIR/fw/soak_u3_pa/SoakU3.elf" none || pa=""
    fi
    if u3check "$DIR/fw/soak_u3/SoakU3.elf" && [ -n "$pa" ]; then
        status success "Stop 2 for 1 min, endurance test for 2, clock within 300 ppm, hard and power-aware kernels; $long" \
            board/u3
    else
        status failure "Stop 2, the endurance test or the clock failed; see the bench's log" board/u3
        verdict=1
    fi
    resume u3
    return $verdict
}

# The Pico 2, on the CI's images.
pico2=""
PICO2_PROBE=${BOARD_CI_PICO2_PROBE:-probe3}
if [ "${BOARD_CI_PICO2:-}" = 1 ] && [ -f "$DIR/fw/pico2/SoakPico2.elf" ]; then
    if hold "$PICO2_PROBE"; then pico2=yes; else pico2=held; fi
fi
board_pico2() {
    if [ "$pico2" = held ]; then
        echo "the Pico 2 is not checked: its probe is held ($DIR/lock-$PICO2_PROBE)"
        return 0
    fi
    OPENOCD_RP2350=${BOARD_CI_OPENOCD_RP2350:-$HOME/opt/openocd-rpi/bin/openocd}
    # The UART's images come with the commits from 2026-09-30 on.
    uart=""
    if [ -f "$DIR/fw/pico2/UARTEchoPico2.elf" ]; then uart=", UART echo and senders"; fi
    if ! (cd "$SRC" && PROBE=$PICO2_PROBE OPENOCD=$OPENOCD_RP2350 \
            python3 tools/pico2_check.py "$DIR/fw/pico2"); then
        status failure "an example that counts in memory failed; see the bench's log" board/pico2
        return 1
    elif [ -n "$uart" ] && ! (cd "$SRC" && PROBE=$PICO2_PROBE OPENOCD=$OPENOCD_RP2350 \
            python3 tools/pico2_uart.py "$DIR/fw/pico2"); then
        status failure "the UART echo or senders failed; see the bench's log" board/pico2
        return 1
    fi
    status success "slot buffers and queue across cores, IPC, endurance test, litmus tests$uart, on the CI's images" \
        board/pico2
}

# The boards side by side, each through its own debugger and with its own log, put
# together in this order once all are done (issue #32): in turn a check took 28 minutes,
# side by side 16 on 2026-10-18, the STM32U3's two kernels, still in turn, the longest.
# The clocks of the STM32 boards against Linux's read the same either way, to 0.4 ppm,
# their jitter too. Each job leaves the parent's trap alone, which frees the locks once
# every job has ended. The pending statuses are posted first, in this order, which the
# table of the comment keeps.
[ "$pico" != yes ] || status pending "running on the Pico"
[ -z "$u5" ] || status pending "running on the UNO Q's STM32U5" board/u5
[ "$u3" != yes ] || status pending "running on the NUCLEO-U385RG-Q" board/u3
[ "$pico2" != yes ] || status pending "running on the Pico 2" board/pico2
# Started in the background, the jobs and what they run ignore SIGINT (POSIX): a Ctrl-C
# on a run by hand ended it alone, its checks left loading images over the next run's (a
# review, 2026-10-18). Interrupted, the run now stops each job and what it runs, each
# process frozen before its children are listed, so that none starts another unseen
# (listed first, one in 40 went on past the locks, a review, 2026-10-18); waits for the
# jobs' shells, then frees the locks, and ends by the signal it got, as a run in turn
# did. Not its process group: a script that ran it would go too.
stop() {   # pid: it, then its descendants
    kill -STOP "$1" 2>/dev/null || true
    # shellcheck disable=SC2046  # one pid a word
    set -- "$1" $(ps -A -o pid= -o ppid= | awk -v p="$1" '$2 == p { print $1 }')
    kill -TERM "$1" 2>/dev/null || true
    kill -CONT "$1" 2>/dev/null || true
    shift
    for pid; do stop "$pid"; done
}
interrupted() {   # signal
    trap "" INT TERM
    for job in $jobs; do stop "${job#*:}"; done
    wait
    cleanup || true
    trap - EXIT "$1"
    kill -"$1" $$
}
jobs=""
trap 'interrupted INT' INT
trap 'interrupted TERM' TERM
# Each job's log beside the logs, not among them: tools/bench_status.sh shows the newest
# there, and one of a run stopped before the end would stay. The next run overwrites it.
for board in pico u5 u3 pico2; do
    eval "due=\$$board"
    [ -n "$due" ] || continue
    (trap - EXIT INT TERM; "board_$board") >"$DIR/job-$board.log" 2>&1 &
    jobs="$jobs $board:$!"
done
failed=""
for job in $jobs; do
    wait "${job#*:}" || failed="$failed ${job%%:*}"
done
# From here on, their pids, free, are never stopped by interrupted().
boards=$jobs jobs=""
# A job stopped by an error, under set -e, before its last status left its board pending
# for good, the commit marked checked all the same (a review, 2026-10-18); in turn, such
# an error had stopped the whole run before that mark.
for board in $failed; do
    last=$(awk -F'\t' -v c="board/$board" '$1 == c { s = $2 } END { print s }' "$DIR/posted")
    [ "$last" != pending ] ||
        status failure "the check stopped before its end; see the bench's log" "board/$board"
done
for job in $boards; do
    cat "$DIR/job-${job%%:*}.log" >>"$LOG"
done

checked
cat "$LOG"
[ -z "$PR" ] || comment
[ -z "$failed" ]
