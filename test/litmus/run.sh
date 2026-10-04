#!/bin/sh
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The litmus bench: the litmus tests and the queue between the cores (litmus.c), then the
# kernel's 3-slot buffer (slots.c), each with its barriers, then once without each. A
# mutant the bench catches is a barrier this host shows needed; one it misses proves
# nothing (test/model/fifo_mp.py and threeslot.py are the proof).
#   sh run.sh [ITEMS] [PAIRS]       defaults 5000000 items each way, 2 pairs of threads
# Each pair keeps two cores busy, at a low priority (nice): the host stays usable.
set -u
cd "$(dirname "$0")"
QUEUE=../../Escapement/CORTEX-Mx/RP2350/Escapement_CoreQueue.c
ITEMS=${1:-5000000}
PAIRS=${2:-2}
make -s SKIP=0 || exit 2
nice -n 10 ./build/litmus-0 -p "$PAIRS" -n "$ITEMS" || { echo "the queue with its barriers failed"; exit 1; }
missed=0
for line in $(grep -n '_OSMemoryBarrier()' $QUEUE | cut -d: -f1); do
   # The step the barrier follows: the last one named in a comment before it.
   step=$(head -n $((line - 1)) $QUEUE | grep -o '// [ED][0-9]*' | tail -1 | cut -c4-)
   make -s SKIP="$line" || exit 2
   out=$(nice -n 10 ./build/litmus-"$line" -q -p "$PAIRS" -n "$ITEMS" 2>&1)
   if [ $? -eq 0 ]; then
      echo "barrier after $step (line $line): not caught"
      missed=$((missed + 1))
   else
      echo "barrier after $step (line $line): caught —" \
           "$(echo "$out" | grep -v '^queue' | head -1 | sed 's/^ *//')"
   fi
done
echo "queue: $missed of the barriers not caught on this host"

# The 3-slot buffer: the barriers of OSWriteBuffer's 3-slot branch, the one before the
# status common to both buffers, and those of GetReadyBuffer3Slot.
HARD=../../Escapement/EscapementHard.c
make -s slots SKIP=0 || exit 2
nice -n 10 ./build/slots-0 -p "$PAIRS" -n "$ITEMS" || { echo "the 3-slot buffer with its barriers failed"; exit 1; }
missed=0
for line in $(awk '/^UINT8 OSWriteBuffer/ { w = 1 } /BUFFER_3_SLOT \*buffer;/ { t = w }
                   /end of OSWriteBuffer/ { w = t = 0 }
                   /^BUFFER_DATA \*GetReadyBuffer3Slot/ { r = 1 } /end of GetReadyBuffer3Slot/ { r = 0 }
                   (t || r) && /_OSMemoryBarrier\(\)/ { print NR }' $HARD); do
   # The statement the barrier comes before.
   before=$(tail -n +$((line + 1)) $HARD | grep -v '^ *\(/\*\|\*\*\|$\)' | head -1 | sed 's/^ *//; s/ *\/\/.*//')
   make -s slots SKIP="$line" || exit 2
   out=$(nice -n 10 ./build/slots-"$line" -p "$PAIRS" -n "$ITEMS" 2>&1)
   if [ $? -eq 0 ]; then
      echo "barrier before \`$before\` (line $line): not caught"
      missed=$((missed + 1))
   else
      echo "barrier before \`$before\` (line $line): caught —" \
           "$(echo "$out" | head -1 | sed 's/^ *//')"
   fi
done
echo "3-slot buffer: $missed of the barriers not caught on this host"
