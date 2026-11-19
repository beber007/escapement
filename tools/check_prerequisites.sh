#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# Each image the examples build, in the variants listed below, made alone, builds every
# object it links. A rule whose prerequisites name a variable before it is defined has
# none: make expands them when it reads the rule. ReadmeExample.elf on the Pico was so
# linked before its objects under make -j8, and failed, while make alone built them
# first for the other images (#98). A link that reads its prerequisites ($^) cannot so
# miss an object without failing each time; one that repeats a variable can. Only make
# -n runs: it compiles nothing, though the Makefiles still create their build
# directories. A few seconds, without a toolchain.
set -u
cd "$(dirname "$0")/.." || exit 1
M=Escapement/CORTEX-Mx
# Every option a Makefile reads, so that one exported for something else changes
# nothing.
# shellcheck disable=SC2046
unset $(sed -n '/^ALL_OPTIONS/,/[^\\]$/p' $M/Options.mk |
        sed 's/ALL_OPTIONS *=//; s/\\//')
unset MAKEFLAGS MAKEOVERRIDES GNUMAKEFLAGS
failed=0

# check DIR GOALS [VARIABLES...]: each .elf that make -n GOALS builds there, made alone.
check() {
    dir=$1 goals=$2
    shift 2
    # shellcheck disable=SC2086
    if ! all=$(make -n -B -C "$dir" "$@" $goals 2>&1); then
        echo "$dir $* $goals: make -n fails: $(printf '%s\n' "$all" | tail -1)"
        failed=1; return
    fi
    images=$(printf '%s\n' "$all" | grep -o -- '-o [^ ]*\.elf' | sed 's/^-o //' |
             sort -u)
    if [ -z "$images" ]; then
        echo "$dir $* $goals: no image found"; failed=1; return
    fi
    for image in $images; do
        if ! alone=$(make -n -B -C "$dir" "$@" "$image" 2>&1); then
            echo "$dir $*: $image alone: $(printf '%s\n' "$alone" | tail -1)"
            failed=1; continue
        fi
        # A recipe line continued with a backslash is joined first.
        alone=$(printf '%s\n' "$alone" | sed -e ':a' -e '/\\$/N' -e 's/\\\n/ /' -e 'ta')
        # The command whose word after -o is the image, then the objects among its
        # words.
        link=$(printf '%s\n' "$alone" | awk -v image="$image" '{
                   for (i = 1; i < NF; i++)
                       if ($i == "-o" && $(i + 1) == image) print
               }')
        objects=$(printf '%s\n' "$link" | tr ' ' '\n' | grep '\.o$')
        if [ -z "$objects" ]; then
            echo "$dir $*: $image alone: no link, or no object in it"
            failed=1; continue
        fi
        built=$(printf '%s\n' "$alone" | tr ' ' '\n' | grep -A1 -x -- '-o' |
                grep '\.o$')
        for object in $objects; do
            if ! printf '%s\n' "$built" | grep -qxF "$object"; then
                echo "$dir $*: $image links $object without building it"; failed=1
            fi
        done
    done
}

P=$M/RP2040/Examples/pico
P2=$M/RP2350/Examples/pico2
U3=$M/STM32U3/Examples/nucleo-u385
U5=$M/STM32U5/Examples/nucleo-u575
Q=$M/STM32U5/Examples/uno-q
for v in "" KERNEL=SOFT KERNEL=PA FLASH=1 LIVE=4 SLEEP_GATE=1; do
    # shellcheck disable=SC2086
    check "$P" "" $v
done
check "$P" bench KERNEL=PA
check "$P" sleep KERNEL=PA
check "$P" bench-admission KERNEL=SOFT
for v in "" KERNEL=SOFT ARCH=riscv "ARCH=riscv KERNEL=SOFT" FLASH=1 LIVE=4 \
         SLEEP_GATE=1; do
    # shellcheck disable=SC2086
    check "$P2" "" $v
done
for v in "" KERNEL=SOFT KERNEL=PA FLASH=1; do
    # shellcheck disable=SC2086
    check "$U3" "" $v
done
for v in "" KERNEL=SOFT FLASH=1; do
    # shellcheck disable=SC2086
    check "$U5" "" $v
done
for v in "" KERNEL=SOFT; do
    # shellcheck disable=SC2086
    check "$Q" "" $v
done

[ "$failed" = 0 ] && echo "each image, made alone, builds the objects it links"
exit "$failed"
