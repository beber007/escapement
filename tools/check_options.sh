#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Each example's Makefile refuses an option it does not take
# (Escapement/CORTEX-Mx/Options.mk, #86). It refuses a KERNEL and an ARCH it does not
# build, and a SCHEDULER or a POWER the code does not name (#92). It still takes the
# options the CI and the tools pass it. A Makefile that dropped an option built another
# image without a word: in #84, a mutant of the DM code built under EDF passed for
# missed. Only make -n runs, which compiles nothing: a few seconds, without a toolchain.
set -u
unset ARCH KERNEL SCHEDULER POWER

cd "$(dirname "$0")/.." || exit 1
M=Escapement/CORTEX-Mx
P=$M/RP2040/Examples/pico
P2=$M/RP2350/Examples/pico2
U3=$M/STM32U3/Examples/nucleo-u385
U5=$M/STM32U5/Examples/nucleo-u575
Q=$M/STM32U5/Examples/uno-q
DM=SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING
failed=0

# check taken|refused DIR [ARGS...]: a refusal counts only with a message that says
# why, Options.mk's or the Makefile's own, not any other failure of make.
check() {
    want=$1 dir=$2
    shift 2
    if err=$(make -n -C "$dir" "$@" 2>&1 >/dev/null); then
        got=taken
    elif printf '%s' "$err" |
         grep -qE '\*\*\* .*(is not|not ported|does not take)'; then
        got=refused
    else
        got="failed ($(printf '%s' "$err" | tail -1))"
    fi
    if [ "$got" != "$want" ]; then
        echo "${dir#"$M"/} $*: $got, should be $want"
        failed=1
    fi
}

for d in "$P" "$P2" "$U3" "$U5" "$Q"; do
    check taken "$d"
    check taken "$d" "$DM"
    check taken "$d" KERNEL=SOFT "$DM"
    check refused "$d" KERNEL=soft
    check refused "$d" KERNEL=HARD
    check refused "$d" "KERNEL=SOFT PA"
done
for d in "$P" "$U3"; do
    check taken "$d" KERNEL=PA POWER=DRA
    check taken "$d" KERNEL=PA SLEEP_SPEED=0
done
for d in "$P2" "$U5" "$Q"; do
    check refused "$d" KERNEL=PA
done
# SCHEDULER and POWER take the names of the code only. The compiler would take another
# word for 0, and build a kernel nobody asked for (#92).
for d in "$P" "$P2" "$U3" "$U5" "$Q"; do
    check taken "$d" SCHEDULER=EARLIEST_DEADLINE_FIRST
    check refused "$d" SCHEDULER=DM
    check refused "$d" SCHEDULER=2
    check refused "$d" SCHEDULER=EARLIEST_DEADLINE_FIRST_STAR
    check refused "$d" "SCHEDULER=EARLIEST_DEADLINE_FIRST DEADLINE_MONOTONIC_SCHEDULING"
done
for d in "$P" "$U3"; do
    check taken "$d" KERNEL=PA SCHEDULER=EARLIEST_DEADLINE_FIRST
    check taken "$d" KERNEL=PA SCHEDULER=DEADLINE_MONOTONIC_SCHEDULING
    check taken "$d" KERNEL=PA SCHEDULER=EARLIEST_DEADLINE_FIRST_STAR
    check refused "$d" KERNEL=PA SCHEDULER=DM
    check refused "$d" KERNEL=SOFT SCHEDULER=EARLIEST_DEADLINE_FIRST_STAR
    for v in NONE OTE DR_OTE DM_SLACK; do
        check taken "$d" KERNEL=PA POWER=$v
    done
    check refused "$d" KERNEL=PA POWER=DRAA
    check refused "$d" KERNEL=PA POWER=1
    check refused "$d" KERNEL=PA "POWER=DRA OTE"
    check refused "$d" POWER=DRA
    check refused "$d" KERNEL=SOFT POWER=DRA
done
for d in "$P" "$P2"; do
    check taken "$d" FLASH=1 LIVE=4 WCET=1
    check taken "$d" TRACE=1
    check taken "$d" SLEEP_GATE=1
    for o in MHZ=16 PHASES=30 SMPS=1 FAST=1 NOHSE=1 WAKE=1; do
        check refused "$d" "$o"
    done
done
check taken "$P" KERNEL=PA sleep RUN=1 UNDERVOLT=1 PHASE_PERIODS=20
check refused "$P" ARCH=riscv
check taken "$P2" ARCH=riscv B=build-riscv KERNEL=SOFT
# Under ARCH=riscv, the images are built in SRAM without options so far (#77). Only
# SleepPico2 reads DEEP, RUN, RUN_VSEL and PHASE_PERIODS, and ARCH=riscv does not build
# it (#90).
for o in FLASH=1 TRACE=1 WCET=1 LIVE=4 SLEEP_GATE=1 DEEP=1 RUN=1 RUN_VSEL=9 \
         PHASE_PERIODS=20; do
    check refused "$P2" ARCH=riscv B=build-riscv "$o"
done
check taken "$P2" ARCH=riscv B=build-riscv FLASH= TRACE= WCET= LIVE= SLEEP_GATE= DEEP= \
    RUN= RUN_VSEL= PHASE_PERIODS=
check refused "$P2" ARCH=arm
check refused "$P2" UNDERVOLT=1
check refused "$P2" POWER=DRA
check taken "$U3" MHZ=48 PHASES=30 FAST=1 build/SleepU3Flash.elf
check taken "$U3" KERNEL=PA FLASH=1 PHASES=30 TRACE=1
check taken "$U5" PHASES=30 SMPS=1 MHZ=16 FLASH=1
check taken "$Q" PHASES=30 TRACE=1 WCET=1 NOHSE=1
check refused "$U5" TRACE=1
check refused "$U5" WCET=1
check refused "$Q" FLASH=1
# Given empty, an option is not set. ifdef takes it for absent, and so does Options.mk.
check taken "$U5" TRACE=
check taken "$Q" FLASH=
for d in "$U3" "$U5" "$Q"; do
    for o in LIVE=4 SLEEP_GATE=1 UNDERVOLT=1 ARCH=riscv RUN_VSEL=1 DEEP=1; do
        check refused "$d" "$o"
    done
done
# Set in the environment for something else, ARCH is not taken for an option.
export ARCH=x86_64
check taken "$U3"
unset ARCH
# Nor is POWER. Under KERNEL=PA, EscapementHardPA.h refuses a name it does not define
# when it is compiled, which make -n does not do (test/host, make refused).
export POWER=performance
for d in "$P" "$P2" "$U3" "$U5" "$Q"; do
    check taken "$d"
done
check taken "$P" KERNEL=SOFT
unset POWER

[ "$failed" = 0 ] && echo "every Makefile takes its options and refuses the others"
exit "$failed"
