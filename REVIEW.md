# Review instructions

The reviews of this repository follow the plugin
[`review-method`](https://github.com/beber007/review-method), at the version
`.claude/settings.json` pins by its commit: how many runs of `/code-review`, how each
finding is checked and tagged, when the reviews stop. `CLAUDE.md` brings this file in,
so that `/code-review` reads it. The first four sections are for each review. The last
three are for the plugin.

## What Important means here

Important means a wrong behaviour, or a false statement. Here, that is above all:

- A comment, or an option off by default, changes the firmware image it should leave
  the same, byte for byte.
- A build drops what it is asked for, an option, a kernel or a scheduler, and builds
  another image without a word (#84, #86, #92).
- The compiler moves or merges a store that an interrupt, or the other core of a
  dual-core chip, relies on (`CLAUDE.md`, "Pitfalls met here").
- A fix is made to one kernel or one port, and not to those that share its code.
- A text lies: a figure not checked against the code, a measurement without its date,
  a claim the code does not keep.
- A tool or a CI job now refuses, or skips without a word, what it ran before.

Style, naming and rewording are Nit at most.

## Cap the nits

Report at most five Nits per review, except for a change of documentation alone:
there, report every one, since its plain style is part of what it is reviewed for.

## Do not report

- What `tools/check_encoding.sh` already checks: each tracked file is valid UTF-8.

## Always check

- Each option that one example's Makefile passes to the compiler: do the other
  examples that build the same kernel pass it too, or refuse it (#84)?
- Each new value an option takes: is a value the code does not know refused (#92)?
- Each change to one kernel or one port: do the others that share this code need it?
- Each figure in a text: is it in the code, or in a measurement with its date?
- A change of documentation alone: is the meaning kept, each number and each name
  kept, and the style plain?

## How deep, by risk

- **Deep**, three runs: the kernels (`Escapement/*.c`, `Escapement/*.h`); the ports and
  their drivers (`Escapement/CORTEX-Mx/` and `Escapement/RISC-V/`, but not the sources
  of their `Examples/`); the tools that check the order of their stores
  (`tools/check_order.py`, `tools/check_order_mutants.sh`); their models and litmus
  tests (`test/model/`, `test/litmus/`); the files of the examples that choose what a
  build holds, `Escapement/CORTEX-Mx/*/Examples/*/Makefile` and `Escapement_Config.h`
  beside it, or the one its Makefile's `-I` path brings in (`nucleo-u575` takes that of
  `uno-q`), and `tools/check_options.sh`, which checks the Makefiles (#84, #86). The
  mutants of the kernel lines changed too (`CLAUDE.md`, "Rules that do not change").
- **Standard**, one run: anything else, such as the other tools, the CI, the sources
  of the examples, the tests on the host and the tests under the Renode emulator.
- **Documentation alone**, a change of Markdown files only: one run, then one run on
  its fixes. `CLAUDE.md`, "Rules that do not change", lists what else it needs.

## Commands a review may run

- `"Bash(make:*)"`: build the examples, and run the tests on the host.
- `"Bash(mktemp -d /tmp/escapement-main-*)"`,
  `"Bash(git worktree add --detach /tmp/escapement-main-*)"`,
  `"Bash(git worktree remove /tmp/escapement-main-*)"`: check `main` out to build its
  images, then remove it. First `mktemp -d /tmp/escapement-main-XXXXXX`, which gives
  the run a directory no other run has. Then `git worktree add --detach DIR
  origin/main`, with the path it printed written out. Call each command alone, not
  joined to another nor through a variable, and with no `..` in a path. A run removes
  only the worktree it added. Once every run has ended, the author removes the
  directories `/tmp/escapement-main-*` left, then runs `git worktree prune`.
- `"Bash(arm-none-eabi-objcopy:*)"`, `"Bash(cmp:*)"`: compare two images of the Arm
  cores. `"Bash(riscv64-elf-objcopy:*)"`, `"Bash(riscv64-unknown-elf-objcopy:*)"`: the
  same for the Hazard3 cores, under Homebrew's name on the Mac, or Ubuntu's, as in the
  CI's image.
- `"Bash(sh tools/check_options.sh)"`, `"Bash(tools/check_order.py:*)"`,
  `"Bash(python3 tools/check_order.py:*)"`, `"Bash(tools/check_order_mutants.sh:*)"`,
  `"Bash(sh tools/check_order_mutants.sh:*)"`: check the options of the Makefiles, and
  the order of the stores, called as `CLAUDE.md` or the CI writes them.

A review runs no tool that reaches the bench: a run of endurance may hold its probes.

## Checks before the reviews

- `git fetch origin`, so that `origin/main` is the `main` of now.
- Every check of `CLAUDE.md`, "Before a commit", and `sh tools/check_options.sh`. For
  a change meant to leave an image alone, that list includes the comparison of its
  images with those of `main`.
