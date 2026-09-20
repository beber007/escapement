# Verifying AI-assisted development

This project is written with an AI assistant. That is worth stating plainly,
because it changes what the repository has to prove: not that code was
produced, but that what was produced is correct.

The working rule is one line: **the AI proposes, the instrument decides.**
Nothing here is considered established because it was asserted confidently — by
the assistant or by anyone else. It is established when something outside the
claim confirms it.

## Four independent levels

| Level | Means | Catches |
|---|---|---|
| Compilation | GitHub Actions, six examples, every push | code that does not build, configurations inconsistent with their linker script |
| Replayable execution | Renode, replayed by `renode-test` in CI | a kernel that builds but does not schedule |
| Internal state on hardware | OpenOCD and SWD | an emulator that models the hardware wrongly |
| Independent instrument | frequency counter of a Bus Pirate v4 | everything above at once — it trusts no software from this repository |

The levels are ordered by how much they cost and by how little they assume. The
fourth exists because the third still runs through a debugger, which turned out
to matter: see the `TIMER_DBGPAUSE` investigation in `rp2040.md`.

## Hypotheses that were wrong

Kept on purpose. A record that shows only the conclusions teaches nothing about
how they were reached, and an assistant that is confidently wrong is a fact one
has to design around.

| Stated | What refuted it | What was actually true |
|---|---|---|
| The Renode timer model is conformant — then, once suspected, that it was the culprit, with no evidence either way | A reproducer of two register writes, outside of any kernel | Two real defects in the model: the `UG` callback ignored the value written, and `CC1G`–`CC4G` were unimplemented |
| `TIMER_DBGPAUSE` is not the cause of the kernel starting late | The binary under test was stale: no `Makefile` tracked header dependencies | It was the cause. The build system was fixed as a result |
| The narrow pulses of the 20 and 60 ms tasks defeat the frequency counter | Eight consecutive readings, stable to one part in a hundred thousand | The instrument was fine; the earlier failure came from the firmware |
| The per-activation cost of the kernel is what trips the overload guard | Measurement: 7.0 µs per round, 0.7 % of the processor | The core was still running on the crystal; the guard was right |
| Rewriting history would shrink `.git` from 12 MB to 7 MB | Measuring the repository after the attempt | No gain at all; the operation was reverted |
| Including the dependency files at the top of a `Makefile` is harmless | `make` stopped building anything but one object file | The first rule read becomes the default goal, and a `.d` file provides one |

One more, of a different kind: an early rebranding pass deleted a comment
terminator in 32 files, and the first attempt to repair them corrupted 57
healthy ones. It was undone with `git checkout` and redone from an exact list.
Working in a repository where every step is committed is what made that cheap.

## What this changes in the repository

- Every example is built in CI, and the kernel is **run** there, not merely
  compiled.
- Hardware measurements are recorded with their deviation and their
  interpretation, not as bare numbers.
- The documentation states what has *not* been verified as clearly as what has.
- Commit messages carry the reasoning, including the reasoning that turned out
  to be mistaken.

## What it does not prove

The new code — the RP2040 port and the Cortex-M layer — has been audited line by
line, which turned up four defects: a race between `OSEnqueueUART` and its
interrupt, a zero-sized transmission that emptied 64 KB onto the port, missing
header dependencies, and stale comments. The **kernel inherited from 2016 has
not been through that audit yet.**

The execution tests exercise three or four tasks. Nothing here establishes how
the scheduler behaves with thirty, nor across the 2³⁰ wrap of its clock, which
takes about eighteen minutes to reach on hardware. Testing the scheduler on the
host is the next item in `roadmap.md` for exactly that reason.
