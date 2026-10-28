# State of the project and direction

Escapement continues ZottaOS, a real-time kernel by Claude Evéquoz and Bertrand Hurst.
Its code last changed in 2014 and was published in 2016. `NOTICE` gives the lineage and
the third-party components.

This page gives the direction and what is done. Since 2026-10-12, the open work is in
the repository's issues ("Open work" below). Each done item says where its result is
measured or checked. A result counts as measured only where a page gives its date and
conditions.

## Direction

Development goes to **ARM Cortex-M**. The power-aware variant is the point of the
project, and its target is the **RP2040**. `power-aware.md` argues that dynamic voltage
and frequency scaling has a niche there, because the chip sleeps poorly. Its board, the
Pico, is also at hand. Measured on 2026-10-10, the niche is small: up to some 5 % with
the idle task in WFI, nothing once it sleeps in SLEEP. The STM32U3 is where DVFS has
paid (`power-aware.md`).

New work goes to the **Pico**, the **Pico 2**, the **STM32U5** and the **STM32U3**. The
RP2350 of the Pico 2 has a switching core regulator, and two cores with exclusive
accesses. Its port was begun on 2026-09-24, for the cores. A DVFS driver is not planned
for it. Measured on 2026-10-03, it would save some 10 % at best, less than sleeping
deeper does (items 3 and 5, issues #23 and #6).

Among the STM32, the **STM32U5** is the one kept. Its port was begun on 2026-09-25, for
the STM32U585 of the Arduino UNO Q (`stm32u5.md`). It was written anew in the manner of
the RP2350's. The older STM32 ports were removed on 2026-09-26 (9783ab4), the F4 last,
once the U5 ran on its board with a check of each commit. The U5 sleeps too well for
DVFS to gain (`power-aware.md`). Measured on 2026-10-03, a cycle costs least at 160 MHz.
Racing to Stop 2 beats every slower speed, on the U5's SMPS and LDO alike (item 4). The
**STM32L4** is set aside.

The **STM32U3** port is for a NUCLEO-U385RG-Q, ordered on 2026-10-06. It was written
from RM0487, and the board came on 2026-10-09. Its endurance test first ran some minutes
on it without an error, hard and soft (`stm32u3.md`, "On the board"). On the candidate
of release 0.1, each of the STM32U3's three kernels ran 8 hours without an error (task
14).

## Release 0.1

Written on 2026-10-07 from every note of the project, after the user set the rules of
`CLAUDE.md`: one task at a time, carried to its end; a release when it is coherent, with
no date. Coherent means two things. Each claim the release makes is checked on the code
it ships. Each endurance run and measurement it cites was made on that code.

**Scope.** The hard, soft and power-aware kernels, each under EDF and DM, on the boards
that check every commit:

- the RP2040, with all three kernels;
- the RP2350;
- the STM32U5;
- the STM32U3, added by the user on 2026-10-09, with all three kernels. The power-aware
  one came in once task 11 found DVFS worth it there.

The API (`api.md`) is not yet stable: 0.x, as semantic versioning has it.

**Tasks, in order, each finished before the next begins.** Since 2026-10-09, a task left
with nothing but a wait may be set aside for the next, two at most, as `CLAUDE.md` has
it. The wait may be an endurance run, the CI or the user's work at the bench. Task 2
waited so, and task 3 does. Task 4 began the same day.

1. **What the soft kernel's test of an optional instance costs on the Pico — done on
   2026-10-07** (`method.md`, after "It went into the kernel"). `DemandFits` took up to
   420 µs, and the board missed deadlines. The kernel went back to the count, as the
   user decided. The count took 13.8 µs in the mean and 31.0 at most over 20,000 tests,
   and kept every deadline (`make KERNEL=SOFT bench-admission`,
   `tools/admission_cost.py`).
2. **An endurance run that exercises the soft kernel's optional instances — done on
   2026-10-10.** `SoakPico` and `SoakU5`, built with `KERNEL=SOFT`, give every task m =
   k = 1. No optional instance runs, and no test of one. The task: a variant with
   (m,k)-firm tasks, under a load that forces drops, checking that no mandatory instance
   misses its deadline.

   Written on 2026-10-08: each port's soft build adds `SoakFirm` to its endurance test
   (`SoakFirmPico`, `SoakFirmPico2`, `SoakFirmU5`, `SoakFirmU3`, `SoakU5.c`). It is a
   (2,5)-firm task of 5 ms, working 1.5 ms of each. A long task declares 7 ms of its 10.
   The kernel's test refuses the optional instances whenever the long task arrives
   before their deadline. Each instance checks from the clock the instances dropped
   before it. It counts an error for a mandatory one among them, a number other than
   `OSGetTaskInstance`'s, or an end past the deadline. Under Renode, EDF and DM, 3.5 s
   of `SoakFirmU5` ran 280 mandatory instances and some 200 optional ones, and dropped
   some 215, without error. A kernel given m = 3 where the task counts 2 made 4 errors.
   `make -C .../nucleo-u575 KERNEL=SOFT` builds it for the NUCLEO-U575ZI-Q, and
   `tools/soak.py` reads its counts.

   On the board from 2026-10-08 05:28 UTC, at 98d8051. After 23 h 46 min: 6.85 million
   mandatory instances, 6.34 million optional ones run and 3.93 million dropped. Every
   count of the kernel was at 0, and the latest end 4,016 µs into the 5 ms. The link
   lost a byte. The fault was the UART driver's, from the STM32U5 port on (`stm32u5.md`,
   "The endurance test").

   Run again on the fix, d9bad06, from 2026-10-09 05:53 UTC. At 16:55, the reader
   mistook one report with some 20 bytes missing from its middle for one from an older
   image. Its numbers were shifted: 67,879,873 errors for one line. The board's counts,
   kept from the start of the run, were at 0 the minute after. The bytes were lost by
   `tools/soak.py`. It read the port once a minute: 18 kB of reports, against Linux's 4
   kB. It now drains the port as they come, and takes only the image's own length
   (9597205, d1a6c86). The run went on under it from 19:51 UTC, continued without a
   reload, the link's count at 0 errors.

   At 24 h, 2026-10-10 05:53 UTC: 6.91 million mandatory instances, 6.40 million
   optional ones run and 3.98 million dropped. Every count of the board was at 0, and
   the latest end 4,016 µs into the 5 ms. The link had 0 errors and 0 overruns. The
   reader had rejected no line since 19:51. The `errors` total of `tools/soak.py` for
   this run still holds the 67,879,873 of the line misread.
3. **The STM32U3 on its board — done on 2026-10-10, but for D0, D1 and JP4.** Added by
   the user on 2026-10-09, the day the NUCLEO-U385RG-Q came. `SoakU3` ran 329 s and
   `SoakFirmU3` 182 s without an error (`stm32u3.md`, "On the board"). `SleepU3` ran
   60 s through Stop 2, within every bound of `tools/unoq_sleep.py`.

   Written the same day (2e7f0b6): the board check of each commit, `board/u3`, as
   `board/u5`. It passed by hand. Its images are built by the CI, and its long run is
   the service `escapement-soak-u3`. The points of "What only the board can decide" that
   it answered are in `stm32u3.md`, "On the board". Left then:

   - 24 h of `SoakFirmU3` without an error, then `BOARD_CI_U3=1` on the bench;
   - the pins D0, D1, D7, D8, D12 and D13, and the current on JP4, which need the user
     at the bench;
   - the LSE's start, R1RDY and BOOSTRDY timed, or recorded as open.

   The run from 07:14 UTC was stopped at 12 h 32 min. The reader of 9597205 had
   continued that run without a reload. It took the link's count from a report left a
   minute in the buffers, behind the board's, and the board counted an error of the link
   (task 2 says why). The run was loaded again under d1a6c86 at 19:51 UTC, its 24 h to
   end on 2026-10-10 at 19:51 UTC.

   Done then: 24 h 2 min and 80 wraps. Every count of the board was at 0. The run had
   6.92 million mandatory instances, 6.53 million optional ones run and 3.85 million
   dropped. The latest end was 4,029 µs into the 5 ms, and the pulse at most 92 µs late.
   The link carried 55.3 MB with no error and no overrun, and the reader rejected no
   line. The MSI locked again 3 times (`stm32u3.md`, "On the board"). The same evening,
   `ClockU3` timed the set-up, and the PPK2's logic inputs found D7, D8, D12 and D13 on
   their pins. Left for the user to do at the bench: D0 and D1, LPUART1, which need the
   FTDI cable, and the current on JP4, with task 11's measure. `BOARD_CI_U3=1` was
   turned on in task 4.
4. **The examples from flash, on every board that allows it — done on 2026-10-10.** They
   ran from SRAM, loaded over SWD. The task: run them from flash on the Pico, the Pico
   2, the NUCLEO-U575ZI-Q (`SleepU5Flash` already does there) and the NUCLEO-U385RG-Q.
   The task also covers what flash brings in: the RP2040's and RP2350's execute-in-place
   cache, the STM32's wait states as the clock changes, DVFS with code read through XIP.
   Not on the UNO Q's STM32U585: its flash holds Arduino's firmware, which stays. The
   STM32U5's flash takes some 10,000 erasures. A board check may not program it at each
   commit, and how often it may is part of the task.

   Begun on 2026-10-09 with the Pico 2, the one board free of an endurance run: `make
   FLASH=1`, built by the CI, and `tools/pico2_flash.sh`. From its flash, the examples
   that count in memory and those of the UART pass. The exception is the overlapping
   outcome of LB, which `LitmusPico2` meets from SRAM only (`rp2040.md`, "From the
   flash"). That criterion is checked from SRAM only, as the user decided on 2026-10-09.

   The Pico the same day (`rp2040.md`, "The Pico from its flash"): a second stage of our
   own, `tools/pico_flash.sh`. The timer events and DVFS on one core pass from the
   flash. The plain array of `FourSlotCoresPico` never tears there, and its criterion is
   checked from SRAM only, as the user decided. Under the power-aware kernel, the first
   rounds took hundreds of microseconds, the XIP cache cold, and the reader overran its
   first period. The kernel and the port now run from SRAM in a flash image of the
   RP2040 and the RP2350, as the user decided the same day.

   The two NUCLEO boards take `make FLASH=1` too. It was not yet run then: their boards
   held endurance runs. How often their flash may be written was set on 2026-10-09. Both
   take 10,000 erasures a page (RM0456 rev. 7, 7.3.8; RM0487 rev. 3). Some 21 commits a
   day since 2026-09-19, 63 at most, would wear out a flash written at each commit in
   about a year. `tools/nucleo_flash.sh` reads the flash against the image first, and
   writes only what differs. It counts each write on the bench, and stops at 1,000 in
   all, a tenth of the endurance. It also takes a cap a day, `NUCLEO_FLASH_PER_DAY`. The
   board CI was to set it to 1, as the user decided on 2026-10-09, its other checks
   staying in SRAM: some 365 writes a year. The script's paths were checked on
   2026-10-09 against a stand-in for OpenOCD, and its Tcl in OpenOCD.

   On the NUCLEO-U575ZI-Q on 2026-10-10, once its run of task 2 ended: both paths on the
   board, one write, and one image found identical. `tools/soak.py` starts an image
   linked into the flash through `tools/nucleo_flash.sh`. `SoakFirmU5` and `SoakU5` ran
   10 min each from the flash, every count at 0 (`stm32u5.md`, "The endurance test from
   the flash").

   The board CI, as the user decided on 2026-10-10:

   - no check of each commit on the NUCLEO-U575ZI-Q, as before;
   - on the NUCLEO-U385RG-Q, the check stays in SRAM. The board's long run runs from the
     flash, written only when the commit's image differs, once in 24 hours at most
     (`tools/unoq_check.sh`, `board_ci.md`). Its three outcomes, written, held back and
     failed, were checked with stand-ins for the boards.

   The user lifted both caps the same day, the day's and the 1,000 in all, on every
   board. Each commit that changes the long run's image writes it, each write still
   counted.

   Done on 2026-10-10, once task 3's run ended: `SoakFirmU3` and `SoakU3` ran 10 min
   each from the NUCLEO-U385RG-Q's flash, written by `tools/soak.py` through
   `tools/nucleo_flash.sh`. Every count was at 0, and the link without an error
   (`stm32u3.md`, "The endurance test from the flash"). Its long run since is the
   service `escapement-soak-u3`, `SoakU3` of c7c8afa from the flash. `BOARD_CI_U3=1` is
   on the bench, which checks the board at each commit.
5. **Every mutant read — done on 2026-10-10.** The task: every survivor of each kernel
   at 300 task sets killed by a test, marked as equivalent with its reason
   (`test/host/equivalent-mutants.jsonl`), or recorded in `method.md` as a behaviour no
   test reads, and why. The hard kernel's unread ones first (0, 127, 130, 206, 225, 226,
   236, 459), then those of the soft and power-aware kernels never read. The scores
   measured again on the code as it then is.

   Begun on 2026-10-10, before tasks 3 and 4 ended, as the user allowed:

   - the hard kernel's eight read: six killed, one marked as equivalent, one recorded
     (`method.md`); 87.6 %, and 97.0 % of those not marked as equivalent;
   - the soft kernel's 142 read the same day: 84.0 %, and 95.4 % of those not marked as
     equivalent;
   - the power-aware kernel's 119 too: 82.6 %, and 95.5 % of those not marked as
     equivalent.

   Every survivor is killed, marked as equivalent, caught by `tools/check_order.py` on
   the ARM builds, or recorded in `method.md`, "The mutant campaigns, by date".
6. **The mutants further — done on 2026-10-10.** Added by the user on 2026-10-10, the
   day task 5 ended, from what its survivors left. The task: teach
   `tools/check_order.py` the loads and the event FIFO's posting. The soft and
   power-aware kernels' barriers order them, and the tool does not follow them (soft
   111, 122, 124, 508, 540, 542, 544; power-aware 631, 633, 635). Then write scenarios
   for the narrow ones: the share reserved for events under EDF (soft 199, 203, 204,
   206), EDF*'s ties after the wraparound (power-aware 129, 130, 140), and a slack
   pending at the wraparound (143, 144). Task 8 was set aside meanwhile, its first part
   done (6a4f138).

   Done on 2026-10-10. The loads need no rule, since `ScheduleNextTask` runs in a
   context not to be saved. The FIFO's barriers keep its order on every build, checked
   from the disassembly. Three tests kill six (`method.md`). Soft: 84.1 %, and 96.7 % of
   those not marked as equivalent. Power-aware: 83.0 %, and 96.4 %.
7. **The power-aware kernel against its reference under DRA — done on 2026-10-10.**
   Added by the user on 2026-10-10. Task 6's search at 20,000 task sets found set 19989
   of `pa_dra` (seed 1). In it, a task with one tick of work left resumes at 4674 at
   speed 1, where `tools/speed_reference.py` gives the fastest. No deadline is missed.
   The task: find which of the two is wrong, in the code and in the ZottaOS manual. If
   it is the kernel, correct it with the user's agreement. Keep the set in
   `tools/differential.py` once both agree.

   The reference was wrong. It served a periodic release and a signal of one instant in
   one pass of the handler, where the host makes two. It lost the server's excess that
   the first pass counts. It is corrected, and the set kept (`method.md`).
8. **The documentation against the code — done on 2026-10-10.** The task:

   - each claim of `README.md` checked again against what checks it (`method.md`, "What
     the README claims"), on the code as it then is;
   - `api.md` stating that the API is not yet stable;
   - the known limits stated where a reader meets them: an optional instance tested
     once, DM keeping the count, DVFS worth nothing on the STM32U5 and some 10 % on the
     RP2350, and the energy verdict on the RP2040 as task 9 finds it.

   Checked again on 2026-10-10 (`method.md`, "What the README claims"). Every claim of
   behaviour holds, and the stale rows are corrected (6a4f138). The boards checked at
   each commit become four when task 4 turns on the NUCLEO-U385RG-Q's check. The
   RP2040's verdict is task 9's to write.
9. **The energy verdict on the RP2040 — done on 2026-10-10.** Added by the user on
   2026-10-10, who had put it off until after this release on 2026-10-07. It is item 1
   below, on its bench: a plain Pico powered and measured by the PPK2. The task: whether
   DVFS beats racing to sleep, and what the same bench settles with it. That is the idle
   task at 12 or 125 MHz, DRA, DR_OTE and DM_SLACK against OTE, the regulator's
   settling, and the core undervolted. The verdict written where `README.md`,
   `power-aware.md` and `rp2040.md` list it as open. It needed the user at the bench: a
   plain Pico, probe2's, whose endurance run the user stopped on 2026-10-10 for it, and
   the PPK2 wired to it.

   Prepared on 2026-10-10: `SleepPico` (`rp2040.md`, "For the PPK2: SleepPico") was
   checked on probe1's Pico W without the PPK2. That covered its phases, its operating
   points and the lock of PLL_SYS, 55 µs. Measured the same day on probe2's Pico
   (`power-aware.md`, "Measuring the RP2040"):

   - a cycle costs least at 125 MHz;
   - DVFS saves up to some 5 % with the idle task in WFI, nothing in SLEEP;
   - the sleep is the lever, 73 %, and a SLEEP with PLL_SYS stopped would save more:
     task 10.
10. **The RP2040's idle task in SLEEP with PLL_SYS stopped — done on 2026-10-10.** Added
    by the user on 2026-10-10, from task 9's verdict: 1.22 mA against 5.07 in SLEEP with
    PLL_SYS running. The task, as the RP2350's idle task does it
    (`Escapement_SleepGate.c` there):

    - PLL_SYS stopped for each sleep long enough;
    - the sleep ended early by an alarm of its own, so that the lock, some 55 µs, is
      over by the kernel's next alarm;
    - clk_sys's auxiliary source moved off PLL_SYS first, without which the chip faulted
      waking from SLEEP (`rp2040.md`, "For the PPK2: SleepPico");
    - under the power-aware kernel too, at whichever operating point.

    Checked on the board: the timer events and the deadlines on time, and the current
    with the PPK2. Done on probe2's Pico (`rp2040.md`, "SLEEP with PLL_SYS stopped").
    The timer events' figures are unchanged under the hard and power-aware kernels,
    every event 0 µs late. `TaskLEDPico` draws 2.18 mA against 5.49 with PLL_SYS running
    (−60 %), and 2.22 and 2.41 under OTE and DR_OTE. Alarm 3 is now kept out of the
    timer events once the sleep uses it. The same holds on the RP2350, whose port lacked
    the guard.
11. **DVFS on the STM32U3: measured, then the power-aware kernel if it pays.** Added by
    the user on 2026-10-10. First the measure (`stm32u3.md`, "Measuring DVFS"):
    `DVFSU3Flash` (0b17ba9) on the NUCLEO-U385RG-Q, the PPK2 in place of JP4, after the
    pins of task 3. DVFS pays if a CRC at 48 MHz in range 2 costs less charge than at
    96 MHz in range 1, by more than the Stop 2 that the time saved at 96 MHz would
    sleep. The verdict is written in `power-aware.md` either way.

    If it pays, the power-aware kernel is added to the port:

    - `OSSetProcessorSpeed` on the sequences `DVFSU3.c` checks;
    - TIM2 and TIM4 rescaled at each change, so that the kernel's time does not slip
      (plan §5.3);
    - USART1 on HSI16, so that its rate does not follow the clock;
    - the wake-up from Stop 2 at the point in effect;
    - the Renode platform and the board check with it, and the kernel's run of task 14
      on the board.

    If it does not, the port stays hard and soft. It needed the user at the bench for
    the PPK2.

    Measured on 2026-10-10 (`stm32u3.md`, "Measuring DVFS"). On the SMPS, a CRC costs
    14.2 % less at 48 MHz in range 2 than at 96 MHz in range 1. That is far more than
    the Stop 2 given up: DVFS pays.

    **Done on 2026-10-11** (`stm32u3.md`, "The power-aware kernel"; the verdict in
    `power-aware.md`). There are two operating points, 48 MHz in range 2 and 96 in
    range 1. The speeds of 24 and 12 MHz cost more a cycle than 48. The plan had TIM2
    and TIM4 rescaled. Instead, they count the MSIK, 12 MHz whatever the core runs at:
    rewriting a prescaler loses the fraction of a microsecond under way at each change.
    On the board, `BenchDVFSU3` lost no microsecond over 5.8 million changes in a
    minute. A raise took 15.2 µs, a fall 4.3. The board check on the power-aware images
    passed by hand. `board/u3` checks them at each commit from this one on, before the
    hard kernel's. Renode runs the whole suite on a `KERNEL=PA` build. The kernel's run
    of task 14 was to come with the others.
12. **A review of the whole, code and documentation.** Added by the user on 2026-10-10,
    before the freeze, so that what it found would be fixed before the candidate and its
    runs. The review was made by agents on Claude Fable 5.1. Each was given one part,
    and none the conclusions of another:

    - the three kernels;
    - the RP2040 and RP2350 ports;
    - the STM32U5 and STM32U3 ports;
    - the flash images and their tools (second stages, linker scripts, `pico_flash.sh`,
      `nucleo_flash.sh`, `unoq_check.sh`);
    - the documentation against the code: each claim of `README.md` and of the pages it
      links to, against the code and the measurement it rests on.

    First came what no audit had read: the STM32U3 port, the flash images, the RP2040's
    sleep with PLL_SYS stopped, and the kernels' changes since the review of 2026-09-30
    (`method.md`, "Audits, and what they leave unproved"). Each finding was checked
    against the code before it counted. It was then fixed, judged not a fault, with its
    reason, or left open in the notes. What the review found, and how it was checked, is
    recorded in `method.md`.

    **Done on 2026-10-11** (`method.md`, "The review before release 0.1"): five agents
    at fef9d5e, each finding read against the code or the data.

    - The documentation had nineteen things out of date, all corrected.
    - The ports and tools had, among smaller ones: a timer written while still in reset
      by the RP2040's and RP2350's sleep; a power-aware check of the STM32U3 that left
      the board's long run reading it; and Makefiles that linked one kernel's objects
      into another's images.
    - The kernels had an overflow of the power-aware kernel's times after two wraps
      idle, reproduced by `TestLongIdle` and fixed.

    Each finding was fixed, or left open with its reason.
12b. **The entry pages made plain and inviting.** Added by the user on 2026-10-11, after
     the review. The task: `README.md` and the pages a visitor reads first
     (`architecture.md`, `build.md`, `api.md`) in short sentences, one idea each and the
     conclusion first, the detail left to the technical pages. The README's opening and
     its figures laid out to catch the eye, with simple diagrams where they help. No
     fact added, removed or changed: each figure and date checked unchanged against the
     diff.

    **Done on 2026-10-11.** The README opens on three points and the Pico's DVFS figure.
    A diagram shows what each push goes through, and the energy measured on each chip is
    a table. `architecture.md` opens its targets with a table too. The numbers of the
    README are the same before and after.
13. **The freeze.** A tag `v0.1.0-rc1` on a commit whose CI and board checks pass. The
    tools that task 14 needs went in first, as the user agreed on 2026-10-11, so that
    the candidate carries what tests it. `tools/soak.py` runs a Pico from its flash and
    drives a Pico 2 (`pico2`), each checked on its board for 3 minutes.
    `tools/endurance.sh` runs one platform's kernels in turn, each under a status of its
    own.

    **Done on 2026-10-11**: `v0.1.0-rc1` on c0ff963, its CI and its four board checks
    passed. From it on, fixes only. Until the runs of task 14 had run every kernel
    again, `EscapementHard.c` was to stay as it was at 041af76, the commit of its
    week-long run.
14. **Endurance on the candidate — done on 2026-10-12.** 8 hours for each kernel on each
    platform, as the user decided on 2026-10-10. It was a week until then, cut to 24
    hours, then to this (rule 3 of `CLAUDE.md`, "Before a release"). Ten runs, one board
    a platform: in turn on each board, and in parallel across them, from flash once task
    4 was done. The soft kernel ran the (m,k)-firm variant of task 2 (`SoakFirm*`), the
    others `Soak*`:

    - RP2040, probe2's Pico: hard, soft, power-aware, 24 hours in all. The PPK2 of task
      9 was unwired first, and the board powered by its USB again.
    - RP2350, probe3's Pico 2: hard, soft, 16 hours. The board CI skips the Pico 2 while
      a run holds `lock-probe3`, as it skipped probe2's Pico (`board_ci.md`).
    - STM32U5, the NUCLEO-U575ZI-Q: hard, soft, 16 hours.
    - STM32U3, the NUCLEO-U385RG-Q: hard, soft, power-aware, 24 hours in all.

    The runs before the candidate are cited beside, not in their place:

    - the hard kernel's week on the Pico, at 041af76;
    - the power-aware kernel's 4 d 18 h 47 min at 80cc6dc, stopped on 2026-10-10 for
      task 9's PPK2: no restart, no error, 384 wraps;
    - the soft kernel's day on each NUCLEO board (tasks 2 and 3).

    A fix to a kernel was to make an `rc2`, and start that kernel's runs again on every
    platform.

    Begun on 2026-10-11 at 14:27 UTC: the ten images built from the tag,
    `tools/endurance.sh` on each platform. The two NUCLEO boards kept running the image
    they had instead of the tag's: `soak.py` loads a NUCLEO only if it is silent.
    Stopped two minutes in, both started again at 14:29 with `BOARD_SOAK_RELOAD`, which
    `endurance.sh` sets since. That was a fix to the tools alone, its commit held back
    so as not to start the board CI meanwhile.

    All ten passed, each status `endurance/<platform>-<kernel>` posted on c0ff963 in
    success. Each ran 8 hours. None had an error, a restart or a reading REJECTED. Each
    counted 26 wraps of the kernel clock. The hard kernels ended on 2026-10-11 near
    22:30 UTC. The Pico 2's and the STM32U5's soft kernels ended on 2026-10-12 near
    06:30, and the Pico's and the STM32U3's power-aware kernels at 14:30. The logs stay
    on the UNO Q, in `~/endurance-rc1/images`. `escapement-soak-u3` started again after,
    and the commits held back were pushed.
14b. **What the STM32U3's power-aware kernel saves — item 7, brought into 0.1 — done on
     2026-10-12.** Added by the user on 2026-10-11, during the runs of task 14. It was
     measured on the candidate's own images, so that the verdict is on the code
     released. `SleepU3` was built from the tag with `FLASH=1 PHASES=30`. It ran under
     the hard kernel racing to Stop 2, against the power-aware kernel under each policy
     (`KERNEL=PA`, `POWER=` OTE, DRA, DR_OTE, DM_SLACK), as on the RP2040
     (`power-aware.md`, "Measuring the RP2040"): five images. `TaskLEDU3` was set aside:
     it drives LD2 on D13, whose current the PPK2 in place of JP4 would count. The
     measure was to come after the U3's runs ended, the board free, the PPK2 put in
     place of JP4 by the user. The verdict written in `power-aware.md` and `stm32u3.md`,
     and in the release's notes. A fault the measure found in the code would make an
     `rc2`, and start the U3's runs again.

    Measured from 15:24 to 15:51 UTC, then again from 15:56 to 16:23, 300 s an image
    (`tools/pa_u3.py`):

    - 3.3 to 4.0 % less charge than the hard kernel with the idle task in Stop 2;
    - 6.6 to 6.8 % less in Sleep;
    - the two runs within 0.15 % of each other;
    - DRA some 2 µA above the other policies in both;
    - no fault in any image, and no `rc2` (`stm32u3.md`, "The power-aware kernel").

    Each image was written through the PPK2, CN1 left in, then powered down 6 s. The
    ST-LINK's virtual COM port hands over what it held while no one read it, which the
    tool drops.
15. **The release — done on 2026-10-12.** Its notes, from `git log` since the fork and
    from this page; the tag `v0.1.0`; a GitHub release. Then, as the user decided on
    2026-10-11, `main` takes changes through pull requests only. A GitHub ruleset on it
    requires a pull request with the CI's `build` passed, and a linear history. It
    allows neither a force push nor a deletion on `main`, and no push straight to it,
    the user's included, unless the rule is turned off on purpose. The rule is written
    in `CLAUDE.md`, "Rules that do not change". The board checks still ran once a change
    was merged: `tools/board_ci.sh` built `main` alone, and a pull request ran nothing
    on the bench (its header). Pull requests let work go on while an endurance run holds
    `main` still, as the eight commits waiting through task 14's runs did not.

    The tag `v0.1.0` is on 8f3f794. Its CI and its four board checks passed. Its code is
    the one the candidate's runs and measures were made on, but for the number of the
    release in `Escapement.h` (`ESCAPEMENT_RELEASE`). Its 31 board images are byte for
    byte those before. The release was published the same day. The rule on `main`, a
    GitHub ruleset, requires the job `build` that the workflow gained for it, which
    passes only if every other job did.

**Not in 0.1**, open after it:

- DORMANT on the RP2350 with the external 32.768 kHz oscillator (item 5, step 3);
- the PPK2 as a check of each commit on the STM32U5 (item 4);
- how often a wake-up byte of LPUART1 comes out wrong (item 6);
- testing a dropped optional instance again;
- an exhaustive enumeration of small task sets, and fuzzing;
- a test of optional instances by processor demand that counts its own cost, bounded and
  measured on each target, and costs less (`DemandFits`, taken out on 2026-10-07,
  `method.md`).

## Open work

Since 2026-10-12, the open work is in the repository's issues, in order on its project
board (github.com/beber007/escapement/issues; github.com/users/beber007/projects/1).
This page keeps the direction and what is done. The open items of the list kept here
until then became these issues:

| Item | Issue |
|---|---|
| 0. Two weeks of endurance on a board | #4 |
| 4. The STM32U5's energy (now the STM32U3's) | #5 |
| 5. A deeper sleep on the RP2350 | #6 |
| 6. How often a wake-up byte of LPUART1 comes out wrong (what was left of it) | #7 |
| 7. Why DRA draws some 2 µA more on the STM32U3 (what was left of it) | #8, closed as not planned |
| 8. A reset of the NUCLEO-U385RG-Q that nothing asked for | #9, closed as not planned |
| 10. A tool to measure each task's WCET | #10, done |
| 11. What a visitor of the repository sees | #11 (done), #12, #13 |
| 12. Lock-free mechanisms beyond the slot buffers and the queues | #14, merged into #15 |
| 13. The drivers, and the description of the boards | #15, #16 |
| 14. Noticing an overrun as it happens | #17 |
| 2 and 3. The power-aware kernel and DVFS on the RP2350 | #23, closed as not planned |

and, from "Not in 0.1", #18, #19 and #20. The items done are kept below, under Done.

## Done

### Issues closed

- **Tasks created and deleted while the hard kernel runs (#45), 2026-10-24.** Built with
  `make LIVE=N`, the hard kernel under EDF keeps a pool of N periodic tasks.
  `OSCreateTaskLive` releases one at once. `OSDeleteTaskLive` stops its releases: at
  once between two instances, otherwise after the one under way. A task asks. The
  timer's handler serves the request at the time it reads, once it has shifted the
  others at the wraparound. The kernel tests no schedulability: that is the caller's to
  decide. DM, the soft and the power-aware kernels refuse the option. The Forth console
  of #40 wanted it. It lets the console's slots come and go.

  The host test's eight modes caught each of twenty-one faults made by hand in the
  kernel's new lines. The reviews found three defects the modes had let pass, each now
  caught by a mode of its own:

  - a block given back while still asked for (a deletion from the overrun hook);
  - a creation served between the counter's wrap and the shift;
  - the caller's masked interrupts unmasked.

  The reviews also found a check of the example that never ran. The Robot Framework
  suites now require that check to run. On the Pico W, LiveTasksPico created and deleted
  37,564 workers in two minutes, none late nor released after its deletion. Under
  Renode, LiveTasksPico2 created and deleted 626 in 2 s. Without the option, every image
  is the same, byte for byte.

- **The endurance test's heartbeat as short as it declares, on the Picos (#41),
  2026-10-24.** It scanned the whole of core 0's painted stack each second. That took
  4.4 ms on the Pico W and 6.5 ms on the Pico 2, against the 50 µs it declared. It now
  scans a slice of 128 words a second, from where it stopped. It also checks the 128
  words at the bottom each time, where an overflow shows first. The free stack it
  reports is that of a whole pass: some eight minutes on the Pico W, seventeen on the
  Pico 2. It is that of two passes when the stack grew below the scan during a pass. At
  the start, it is that of a scan made in `main` before the kernel starts.

  Measured with `make WCET=1` over three minutes: 47 µs on the Pico W and 33 on the Pico
  2 under the hard kernel. Under the soft kernel, 65 and 39: that kernel counts its
  admission test in the instance that ends. Each Soak declares 80 µs. SoakU5 and SoakU3
  have the same code, not measured: their boards run their endurance tests, which a
  measure would stop. Their heartbeat's first instance waits for the IWDG to take its
  settings, most likely past the 80 µs.

- **Each task's execution times measured (#10), 2026-10-23.** Built with `make WCET=1`,
  the hard and soft kernels count the ticks each instance holds the processor. They
  count from one context switch to the next, the soft timer interrupt left out, and keep
  each task's largest. `tools/wcet.py` reads them over SWD without stopping a core. A
  task reads its own with `OSGetTaskMaxTime()`. Each figure is one word, which the probe
  reads whole, so the sequence count the issue planned for 64-bit times is not needed.
  On the Pico W, SoakFirmPico's (2,5)-firm task, which works 1.5 ms, measured 1499 to
  1503 µs. SoakPico's heartbeat measured 4.4 ms, against the 50 µs it then declared
  (#41). The power-aware kernel refuses the option: its ticks would need scaling by the
  speed. Without the option, every image is the same, byte for byte.

- **The Pico's two figures for a dark page too (#38), 2026-10-23.**
  `tools/schedule_figure.py` and `tools/dvfs_figure.py` write a light and a dark
  version, from the palettes of `tools/figure_svg.py`. The README and `rp2040.md` show
  the one the page's theme asks for, as the banner and the figures of how the kernels
  schedule do. The light versions are the former images byte for byte.

- **No descriptor of the queue used once its operation returned (#26), 2026-10-23.** A
  descriptor lives on its operation's stack. `test/model/fifo.py` checks that each read
  and write of a descriptor is for an operation still running, or preempted by the one
  touching it. A variant that returns with `PendingOp` still set is caught.
  Linearizability alone let it pass. CodeQL's alerts 25 to 27
  (`cpp/stack-address-escape`) were dismissed on reasoning that is now checked.

- **The figure scripts share one module (#31), 2026-10-23.** `tools/figure_svg.py` holds
  what `tools/host_figures.py`, `tools/schedule_figure.py` and `tools/dvfs_figure.py`
  wrote three times: the font, the light and dark palettes, the page, the time axis, and
  a lane's label and its bars. `tools/differential.py` takes the span of a run as a
  parameter, `duration`. The figures used to set that constant on a copy of
  `tools/differential.py`. Each trace of the figures is read once. The six SVG files,
  the README's four and the Pico's two, come out byte for byte as before.

- **The boards checked side by side (#32), 2026-10-18.** `tools/board_ci.sh` runs the
  Pico, the Pico 2, the STM32U5 and the STM32U3 at once, each through its own debugger.
  It puts their logs together afterwards. A check takes 16 minutes, against 28 in turn.
  The STM32U3's two kernels take the longest. The clocks against Linux's read the same
  as in turn: +36.2, -80.7 and -91.9 ppm, against +36.2, -81 and -92.3.

- **How it schedules, drawn by the kernel itself (#11), 2026-10-18.** The README has two
  figures, from the host build's trace:

  - (50, 25) and (80, 35) at 94 % of the processor: every deadline kept under EDF, and
    the second task unfinished at 80 under DM;
  - the same periods at 110 %, declared (m,k)-firm: every deadline kept by the soft
    kernel, and two optional instances dropped in the 400 ticks shown.

  `tools/host_figures.py` checks those statements on each run before drawing. It refuses
  traces made wrong (`--self-test`). The CI fails if the figures differ from what the
  kernels draw.

- **The bench's results on the pull request (#28), 2026-10-13.** `tools/board_ci.sh`
  leaves one comment on each pull request it checks, replaced at each check. The comment
  holds the statuses, the key figures and the whole log, serial numbers and local paths
  taken out.

- **Board checks on pull requests (#21), 2026-10-13.** `tools/board_ci.sh` checks the
  head of each open pull request from a branch of this repository, after `main`, one per
  run. It posts the same four statuses on it as on `main`. A fork's pull request is
  never checked: its head must be that of a branch here. The long runs of the STM32
  boards stay on `main`'s image, stopped for the check and started again. The ruleset on
  `main` requires the four statuses.

- **CodeQL and Dependabot (#22), 2026-10-13.** CodeQL reads the C code as
  `tools/clang_check.sh` compiles it: the three kernels under both algorithms, the
  Cortex-M layer and each port. It uses the `security-and-quality` queries
  (`codeql.yml`). It reads the whole code on every push to `main`. On a pull request, it
  reports only the alerts on the lines changed. Dependabot follows the actions of the
  workflows each week. The CI image is left out: Dependabot would read only its base
  image, and the image changes only when rebuilt by hand under a new tag
  (`dependabot.yml`).

Closed without work, on 2026-10-23:

- **Why DRA draws some 2 µA more than the other policies on the STM32U3 (#8).** It is
  0.45 %, with no fault, and nothing needs the cause. The measurement stays in
  `stm32u3.md`.
- **A reset of the NUCLEO-U385RG-Q that nothing asked for (#9).** It did not come back.
  Since 2026-10-11, none of the ten 8-hour runs of release 0.1 restarted, and neither
  did the STM32U3's long run on af88a34 (4 d 22 h). Reopen it if such a reset comes
  back. Then look at RCC_CSR and the bench's journal at that time.
- **Lock-free mechanisms beyond the slot buffers and the queues (#14).** Merged into
  #15. Its likeliest need was a driver's: LPUART1 received by DMA through Stop 2. #15
  carries that need now, with the table of gaps.
- **The kernels' mutants at night on GitHub (#34).** No need. The kernels change little,
  and the rule before a merge already runs the mutants of the kernel lines changed.
  Reopen it if the kernels start changing often, or if a survivor slips through that
  rule.

### Items of the open list, done when it moved to the issues (2026-10-12)

- **The energy verdict on the RP2040** — in release 0.1 as its task 9, done on
  2026-10-10 (`power-aware.md`). The idle task in SLEEP with PLL_SYS stopped was task 10
  of release 0.1, done the same day (`rp2040.md`, "SLEEP with PLL_SYS stopped").

   The item was to build the bench of `power-aware.md`: a plain Pico rather than a Pico
   W, powered and measured by a Power Profiler Kit II. The Power Profiler Kit II was
   chosen on 2026-09-24 over an INA226. Then the item was to answer whether DVFS beats
   race-to-sleep. The same bench was to settle what the documentation left open:

   - whether the idle task should sleep at 12 MHz rather than 125 (`rp2040.md`);
   - whether DRA, DR_OTE or DM_SLACK save anything over OTE;
   - how long the regulator really takes to settle;
   - whether the core, undervolted, still computes right, checked by a computation whose
     result is verified.

   For the idle task, the RP2040 datasheet (build of 2025-02-20) was read on 2026-09-27.
   SLEEP is entered when both cores wait in WFI. SLEEP gates the clocks that the
   SLEEP_EN registers name, while the oscillators and the PLLs run on and the state is
   kept (§2.11.2, p. 161). The kernel's timer and its tick, from clk_ref, can stay on
   (SLEEP_EN1, CLK_SYS_TIMER, p. 211-213). The kernel's time then stays exact, and the
   timer's alarm can wake the core. The datasheet's example of SLEEP draws 0.39 mA
   typical, against 9.0 mA idle in BOOTSEL (table 637, p. 623). In that example, every
   clock is on the 12 MHz crystal, the PLLs are stopped and the timer is gated off.
   DORMANT stops every clock, clk_ref and the timer with them (§2.11.3). A timed wake-up
   from DORMANT can come only from the RTC, to the second. That wake-up also works only
   on an external clock on a GPIN (§4.8). That is no use to an idle task that keeps time
   to the microsecond. No erratum touches SLEEP, DORMANT or the RTC.

   The idle task in SLEEP (`make SLEEP_GATE=1`) kept every deadline on the board on
   2026-09-28 (`rp2040.md`, "SLEEP rather than WFI alone"). What it drew was left for
   the bench to measure.

   Built with `SLEEP_SPEED=0`, the power-aware kernel's idle task sleeps at 12 MHz with
   the PLL still locked (`power-aware.md`), but the 0.39 mA of the datasheet had the PLL
   stopped. The datasheet gives no lock time (§2.18). The steps on the bench were then:

   1. the idle task as it is, at 125 and at 12 MHz;
   2. SLEEP, with the timer and the interrupts in use left on: the tick, from clk_ref,
      needs no more (`rp2040.md`, "SLEEP rather than WFI alone");
   3. the PLL's lock time, to decide whether a long sleep pays for stopping the PLL, as
      the U5 stops its clocks in Stop 2.

- **The Pico 2 on the board — done on 2026-09-30.** Every example has run on a Pico 2.
  The bench checks six of them and the UART at each commit (Done). Left: the power-aware
  kernel and DVFS (item 3).

- **DVFS on the RP2350 — measured on 2026-10-03, worth some 10 % at best.** The RP2350's
  regulator and power manager differ from the RP2040's, and a driver would be written
  from the pico-sdk headers. DVFS on the RP2350 is not planned: SLEEP comes first
  (item 5).

   The Pico 2 was measured on 2026-10-03, with `SleepPico2` built with `RUN=1`:

   - the core computed without a pause, 30 s at each point;
   - the PPK2 powered the board at 5 V on VSYS, its regulator included;
   - the speeds of 75 and 37.5 MHz are PLL_SYS's 150 divided in clk_sys, the VCO still
     at 1.5 GHz;
   - the speed of 12 MHz is the crystal, with PLL_SYS stopped.

   | core voltage | 150 MHz | 75 MHz | 37.5 MHz | 12 MHz |
   |---|---|---|---|---|
   | 1.10 V | 15.99 mA, 107 pC a cycle | 8.84 mA, 118 pC | 5.79 mA, 154 pC | 2.94 mA, 245 pC |
   | 1.00 V | | 7.60 mA, 101 pC | 5.11 mA, 136 pC | 2.69 mA, 225 pC |

   As on the U5, a cycle costs least at full speed at a given voltage, but what decides
   is the sleep that the time saved goes to. Racing at 150 MHz beats 75 MHz at 1.10 V
   only if that sleep draws under 1.7 mA. Racing beats 37.5 MHz only if that sleep draws
   under 2.4 mA: SLEEP, at 4.2 mA (item 5), does not, but DORMANT, at 0.36 mA, does.
   With a tenth of the processor busy and the idle task in SLEEP, 75 MHz saves some 5 %
   and 37.5 some 10 %, at 1.10 V. SLEEP itself took 61 % off WFI.

   At 1.00 V, 75 MHz costs less a cycle than 150 at 1.10 and beats racing to any sleep,
   but the datasheet guarantees 1.1 V only (6.3.2), and the chip's brown-out detector
   resets the chip under some 0.95 V. So a core at 1.00 V is a bench setting, as
   UNDERVOLT is on the RP2040.

   Within the specification, then, DVFS on the RP2350 is worth some 10 % if the idle
   task sleeps in SLEEP with PLL_SYS running, but next to nothing once the idle task
   stops PLL_SYS: that sleep draws 1.99 mA, just over the 1.7 that 75 MHz needs. DVFS is
   also worth next to nothing once DORMANT keeps the time (item 5, step 3). SLEEP comes
   first, PLL_SYS stopped.

- **LPUART1 at 115,200 baud through Stop 2 — done on 2026-10-02** (Done, "LPUART1 at
  115,200 baud through Stop 2"). Left: how often a wake-up byte comes out wrong. No
  check of a minute can bound that.

- **What the STM32U3's power-aware kernel saves — done on 2026-10-12, as task 14b of
  release 0.1.** The kernel saves some 4 % with the idle task in Stop 2 on `SleepU3`,
  and some 7 % in Sleep (`stm32u3.md`, "The power-aware kernel"). Left: why DRA draws
  some 2 µA more than the other policies there, in both runs. The gap is 0.45 %, with no
  fault: #8 was closed on 2026-10-23 as not needed.

- **An overrun stops the kernel.** This was found on 2026-10-11, while drawing on the
  host the curve of a demonstration. A mandatory instance unfinished at its next release
  freezes the system, interrupts disabled, as in ZottaOS (`api.md`, "When an instance
  overruns"). The choice to make, in all three kernels, was to keep that stop, or to
  drop the late instance, count the drop and let the application know.

   On the host on 2026-10-11, every task was slowed past its `wcet` by time stolen from
   it, as an interrupt load would steal it. Once the margin was gone, that froze the
   soft kernel within seconds: the soft kernel's test of optional instances reads the
   declared figures, and does not see that time. The soft kernel under DM did not stop
   for a mandatory instance that never started: the soft kernel dropped that instance in
   silence (`api.md`).

   **Decided and done on 2026-10-12**, after two reviews the user asked for. A first
   design skipped by default and dropped a late instance that never started. A second
   design moved the late instance's deadline a period on under EDF, with a budget to
   demote the instance. Each design was found wrong on the same ground: all tasks share
   one stack, so an instance that started can be neither dropped nor given a later
   deadline, its frames lying above those of the instances it preempted. Also, skipping
   by default protects no one under EDF, where the late instance keeps the earliest
   deadline.

   What was kept, in the three kernels:

   - the stop stays the default;
   - a hook of the application (`OSSetOverrunHook`), called at the overrun with the
     task's argument: the hook may set outputs safe, signal an event, and answer
     `OS_OVERRUN_STOP` or `OS_OVERRUN_CONTINUE`, the latter skipping the release, the
     late instance keeping its deadline;
   - counts, `OSGetOverruns` and `OSGetTaskOverruns`.

   The silent drop of the soft kernel under DM is now an overrun, told and counted. The
   reinsertion that the soft kernel's EDF build would have made without `DEBUG_MODE` is
   gone.

   The work was checked on the host, in nine builds, by:

   - `overrunskip` and `overrunstop` (the stop met in a child process, with and without
     a hook);
   - `differential.py` drawing a third as many sets that overrun: every release skipped
     was the one that found an instance unfinished, computed apart (5,257 over 900
     sets).

   This is not protection: an instance looping for ever still holds the processor
   (`api.md`, "When an instance overruns").

   A review the user asked for before merging found two faults the tests had let pass,
   each with the reason why (`method.md`, "Hypotheses that were wrong"):

   - an admitted optional instance, preempted before it started, was inserted twice into
     the ready queue under EDF;
   - test/host never setting STATE_RUNNING.

   After the review, a skipped release also leaves the deadline of the instance still
   under way as it was. Before, under DM, a skipped release rewrote that deadline. That
   was no fault: nothing read that deadline then, and so no test could show it. The
   review led to:

   - (m,k)-firm sets that overrun;
   - the timer taken at the kernel's barriers;
   - a check that the ready queue stays whole;
   - `overrunadmitted`.

   Then came the mutants of the lines changed: 232 at 300 task sets on home. Six
   survived the tests and showed four holes in them. Each hole was closed by a test seen
   to fail on its mutant first. The holes were these:

   - the admitted optional instance inserted twice (`overrunadmitted`, above);
   - the stop did not check that it masks interrupts (`overrunstop` does now);
   - no test had the timer's handler late by several periods, where a kernel that kept
     the task it had just put back removed that task from the arrival queue
     (`overrunlate`);
   - no set dropped an optional instance from behind another of an earlier deadline
     (`optionalbehind`): the deadlines of the (m,k)-firm sets were drawn equal to their
     periods.

   Every other survivor was marked as equivalent, with its reason. Four such marks, on
   the code that this work removed, were taken out. One mark was re-ranked: the
   identical statements added before its mutant had moved that mutant's rank. The tests
   covered 91.97 % of the branches.

### Before

- **No periodic tick, checked (2026-10-05).** The host test's trace prints each
  interrupt of the comparator. `tools/differential.py` fails a trace in which the timer
  interrupts where nothing is released, or twice at an instant. Every build passes, but
  a host port made to interrupt every 500 ticks fails its first task set.
- **A day of endurance under Renode (2026-10-04).** Four instances ran a day of virtual
  time each at 1b67f30, 80 wraps, with no error (`emulation.md`, status
  `emulation/soak`). The four instances were the hard, soft, deadline-monotonic and DRA
  kernels, each with its own build and seed.
- **The Pico 2 on the board (2026-09-28 to 30).** The Pico 2 was on probe3, driven by
  Raspberry Pi's OpenOCD. It showed (`rp2040.md`, "The Pico 2 on the board";
  `tools/pico2_check.py`, `tools/pico2_uart.py`, `tools/pico2_soak.py`):

  - its clocks right;
  - the six examples that count in memory passing the criteria of the Renode suite;
  - the litmus tests showing no reordering between the cores;
  - the periods right on the frequency counter;
  - the UART echo and the two senders through the probe's UART;
  - `SoakPico2` across two 2^30 wraps in 40 minutes without an error.

  At each commit, the bench checks all of it but the wrap, which takes 18 minutes each
  time (status `board/pico2`).

- **A stack that faults (2026-10-04 and 05).** The one stack grows down toward the
  globals, with nothing between them. ZottaOS's comment said an overflow corrupted
  nothing. Since 2026-10-04, the Cortex-M33 ports set MSPLIM at the end of the globals
  (`_OSResetHandler`). The Renode suites of the Pico 2 and of the STM32U5 still pass
  with that limit. Renode does not model MSPLIM, so showing that the limit holds was
  left to the board.

  On the Pico 2, the board showed it on 2026-10-04, with `StackGuardPico2` loaded on
  probe3 from the Mac over the gateway. With the limit, the core ended in its HardFault
  handler, with CFSR 0x00100000 (STKOF) and HFSR FORCED. The main stack pointer stopped
  at 0x20001600, above the end of the globals at 0x20001210, 513 levels deep. Built
  without the limit, the stack went three levels further, to 0x20000a24. The stack went
  through the globals and into the image's code, which runs from SRAM below them. The
  core was lost at 0x20000fce, with no fault recorded. `Sentinel` read 0xA5A5A5A5 both
  times, under Renode too: `Sentinel` is no witness, the stack pointer is.

  On the STM32U5 of the UNO Q, `StackGuardU5` showed it too, the same evening. With the
  limit, the core ended in the HardFault handler, with CFSR 0x00100000 (STKOF) and HFSR
  FORCED. The main stack pointer was at 0x20001b70, above the end of the globals at
  0x20001878, 759 levels deep. Without the limit, the stack went three levels further,
  to 0x20000f70. `Sentinel` was written over (0xFAFAFAFA, the low byte of level 762).
  The core faulted on an undefined instruction, because the stack's bytes were run as
  code. That fault says nothing of the cause.

  The Cortex-M0+ of the RP2040 has no stack limit: since 2026-10-05, the linker script
  keeps 1 KB between the globals and the stack. A region of the MPU closes that 1 KB to
  any access. On the Pico (probe1), `StackGuardPico` ended in the HardFault handler, 507
  levels of 512 bytes deep. The stack pointer was at 0x20001780, inside the region,
  above the end of the globals at 0x20001258. `Sentinel` was kept. Without the region,
  the stack went five levels further, to 0x20000f84, through the globals and into the
  code. `Sentinel` was written over (0xFFFFFFFF), and the core was still recursing. The
  core does not lock up, contrary to what was expected: HFNMIENA at 0 turns the MPU off
  at the HardFault's priority, and the HardFault's stacking lands in the region.

  A frame of 1 KB or more steps over the region: GCC first put two levels of the test in
  one frame of 1,536 bytes. That frame wrote over `Sentinel` in the deadline-monotonic
  build under Renode. The test now keeps its function from being inlined. Renode stops
  the overflow too, but in a MemManage handler, a fault that ARMv6-M does not have.
  `SoakPico` paints and reads its stack from the end of the region.

- **Sleep on the RP2350, measured and gated (2026-10-03 and 04).** These were steps 1
  and 2 of item 5. `SleepPico2` (Examples/pico2) measured WFI, SLEEP and DORMANT on
  2026-10-03, with:

  - no kernel;
  - some 100 µs of work every 100 ms on core 0, core 1 off;
  - the ring oscillator, PLL_USB and the USB, ADC and HSTX clocks stopped;
  - 30 s of each phase, told apart on the PPK2's D0 and D1.

  The PPK2 powered the board at 5 V on VSYS, its regulator included, with the LED off.
  There were three phases of each, in one run:

  | | mean | median, the level between wake-ups |
  |---|---|---|
  | WFI, every clock running | 13.15 mA | 12.6 mA |
  | SLEEP, every clock gated but the tick and TIMER0 | 5.10 mA | 4.2 mA |
  | DORMANT, the crystal and PLL_SYS stopped | 1.29 mA | 0.36 mA |

  SLEEP alone takes 61 % off WFI, and keeps the kernel's time exact: step 2 was worth
  doing. DORMANT's mean is mostly its wake-ups, every 100 ms: the crystal's start of
  6 ms, and PLL_SYS locked again. The start of 6 ms is the crystal's STARTUP delay, 6
  times the millisecond, as the port sets it.

  Two things turned up on the way. First, the always-on timer's alarm did not wake this
  DORMANT: the timer's count runs on LPOSC, but its alarm is compared on the power
  manager's clock, which follows clk_ref, here the stopped crystal. The alarm fired
  while the chip was awake, and never while it slept. The image is woken instead by a
  byte that the UNO Q sends every 100 ms on the probe's UART. The byte's falling edge on
  GP1 is a DORMANT wake-up (`tools/pico2_sleep_load.sh --wake`). A kernel would run
  clk_ref from LPOSC first, as the pico-extras do, or take step 3's external clock.

  The cheaper sleeps a kernel could take were measured the same night, with `make
  DEEP=1`. There were three phases of each, in one run:

  | | mean | median |
  |---|---|---|
  | SLEEP, PLL_SYS running (the first run read 5.10 mA) | 4.47 mA | 4.1 mA |
  | SLEEP, clk_sys on the crystal, PLL_SYS stopped, locked again on waking | 1.99 mA | 1.4 mA |
  | DORMANT, the crystal's start at 1 ms rather than 6 | 1.23 mA | 0.35 mA |

  Stopping PLL_SYS through the sleep takes 55 % more off SLEEP, 85 % off WFI in all,
  with TIMER0 still counting the crystal: that was the idle task to write, as the U5's
  idle task sleeps through its wake-up's margin on the MSIS. The crystal's start is a
  small part of DORMANT's mean: 1.29 to 1.23 mA. The rest of DORMANT's wake-ups was not
  told apart.

  The second thing: TIMER0 ran at half speed. The firmware in the Pico 2's flash leaves
  clk_ref divided by 2, and a debugger's reset keeps that divider. Neither port set the
  divider back. Since then, both ports set it back for clk_ref, clk_sys and, on the
  RP2350, clk_peri.

  Step 2 was written on 2026-10-04: `Escapement_SleepGate.c` on the RP2350, as on the
  RP2040. The idle task is called through `_OSIdleHook`. The idle task:

  - gates every clock but those the image keeps;
  - moves clk_sys onto the crystal and stops PLL_SYS;
  - sleeps until 150 µs before the first alarm of TIMER0 armed, waking on TIMER0's
    alarm 3;
  - locks the PLL again on waking.

  ALARMn cannot be read back: the port keeps what it wrote in `_OSAlarmTime`.
  `IdlePico2` (Examples/pico2) runs a task of some 100 µs every 100 ms, and a timer
  event 40 ms after each. `IdlePico2` was measured on the PPK2 as `SleepPico2` was, 30 s
  of each, in one run:

  | `IdlePico2` | mean | median | instances' jitter, events' offset |
  |---|---|---|---|
  | WFI (`make`) | 13.76 mA | 13.3 mA | 1 µs, 5 µs |
  | SLEEP, PLL_SYS stopped (`make SLEEP_GATE=1`) | 2.46 mA | 1.8 mA | 0 µs, 4 µs |

  The idle task's current fell by 82 %, and the kernel's time was kept: the jitter and
  offsets were read while the idle task ran, over 251 instances. Without the early
  wake-up, the PLL's lock made them 56 and 61 µs.

  Two defects turned up on the way, both fixed in the RP2040's port too:

  - an image loaded after one that gated its clocks slept with the bus gated, as
    SLEEP_EN outlived the debugger's reset: `OSInitializeSystemClocks` now sets SLEEP_EN
    back;
  - core 0, its gates and deep sleep already set, entered SLEEP while waiting for core
    1's bootrom on the inter-core FIFO, whose clock was gated: `OSInitSleepGate`
    launches core 1 first.

- **LPUART1 at 115,200 baud through Stop 2 (2026-10-02).** LPUART1 ran at 57,600 baud
  until then. The rate of 57,600 stays the default, but `SleepU5` and `SoakU5` run at
  115,200 (below). LPUART1's margin on waking was read on 2026-10-02 (`stm32u5.md`,
  "LPUART1 through Stop 2"):

  - With its FIFO on, an overrun comes when a byte is complete and the 8 places are full
    (RM0456, LPUART, "Overrun error"). From the byte that wakes the chip, that is 8
    frames: 1.39 ms at 57,600 baud, 694 µs at 115,200.
  - Interrupts stay masked meanwhile: leaving Stop 2 takes 20 to 60 µs (DS13086, table
    74). Then comes `_OSRaiseSystemClock`: 18 to 29 ticks of LPTIM1 measured over
    454,487 wake-ups, at most 916 µs. Then comes the timer's interrupt, of a higher
    priority. That makes some 1.0 ms: a margin of about 30 % at 57,600 baud, and none at
    115,200.
  - The HSE dominates, and has no maximum: 2 ms typical, "can vary significantly with
    the crystal manufacturer" (DS13086, table 80). On this board at room temperature,
    the HSE has always started in less, but nothing guarantees that elsewhere or in the
    cold.

  `SoakU5` never enters Stop 2, its pulse being due every millisecond, so the overrun of
  2026-10-01 does not come from this margin. The ways weighed that day:

  - Hardware flow control: PG6, the LPUART's RTS and the Linux side's CTS, left to the
    LPUART. The FIFO never overflows, but the first byte is still sampled while HSI16
    starts, so this way stays at 57,600 baud.
  - PG6 held at "stop" in Stop 2, with two frames of silence checked before, and at
    "ready" after the clock is raised. That gives 115,200 baud and some 20 µA, but the
    UART no longer wakes the chip, and Linux waits up to the length of a sleep, 1.83 s
    at most.
  - HSI16 kept on in Stop 2 (HSIKERON). That gives 115,200 baud, and the UART wakes the
    chip, at some 150 µA against 20.5. The FIFO still needs flow control or draining.
  - The LPDMA, autonomous in Stop 2, filling a ring in SRAM4. That way has no limit, and
    is a new receive path.
  - The FIFO drained in the waits of `_OSRaiseSystemClock`. The handler is then called
    with the kernel's clock stopped.
  - A wake-up byte, followed by an acknowledgement from the MCU before the message.

  The way chosen and built on 2026-10-02: **a wake-up byte without acknowledgement**.
  The client sends one byte, waits T, then sends its message. This way holds on three
  conditions:

  1. T is bounded by the code, not only measured: the wait for the HSE is capped at 64
     ticks of LPTIM1, 1.95 ms. Beyond the cap, that wake-up runs on PLL1 from the MSIS,
     as on a board without the HSE: 160.017 MHz, some 100 ppm off. The HSE is tried
     again at the next wake-up. Until then, `NoHSE` gave the HSE up for good after some
     20 ms, as it still does at reset. T is then made of leaving Stop 2, the capped HSE,
     and the maxima of the voltage range, booster and PLL1 lock, to read in DS13086. For
     the client, T is some 5 ms.
  2. The MCU stays awake W after each byte received, `_OSUARTIdle` refusing Stop 2
     meanwhile. Otherwise the MCU sleeps again as soon as the wake-up byte is read. W
     covers T, the jitter of a Linux process and the gaps in a message: some 20 ms.
  3. The wake-up byte can be dropped without doubt. Sampled while HSI16 starts, the
     wake-up byte's value is anything. Received awake, the wake-up byte looks like data.
     And "the first byte after a silence" does not tell the wake-up byte apart, since
     the message's first byte comes T after it. A framed protocol does tell it apart:
     COBS with a CRC. The client sends 0x00, waits T, then sends 0x00, the frame and
     0x00. Whatever the wake-up byte became ends in an empty or invalid frame, dropped
     as noise.

  This way keeps 115,200 baud, 20 µA in Stop 2, and the UART waking the chip in some
  5 ms. This way relies on no flow control. The work went in this order:

  - The cap on the HSE came first. The cap also keeps one slow start from leaving the
    chip on the MSIS for good, and is testable under Renode. The cap was done on
    2026-10-02 (`stm32u5.md`, "A bound on the HSE's start"), and seen on the board the
    same day.
  - Then the window W, done under Renode on 2026-10-02 (`stm32u5.md`, "The window after
    a byte").
  - Then the link of `SleepU5` and `tools/unoq_sleep.py` framed, at 115,200 baud, done
    under Renode on 2026-10-02 (`stm32u5.md`, "The wake-up byte").

  The window and the framed link were both seen on the board the same day, every byte
  received at 115,200 through Stop 2. `SoakU5`, which never enters Stop 2, moved to
  115,200 too on 2026-10-02, its link raw as before. Awake, `SoakU5` ran without an
  overrun up to 921,600 baud the same day (`stm32u5.md`, "Faster, awake"). A rate above
  115,200 is open to a link that needs it.

- **The order between the cores, as far as it can be verified (closed 2026-09-30).** The
  queue between the cores (`Escapement_CoreQueue.c`) had a DMB between any two of its
  accesses to different words, fifteen in all. A model of weakly ordered cores
  (`test/model/fifo_mp.py`, 2026-09-26) kept six of them (a6f8b6e, 2026-09-27). On
  2026-09-29, the model stopped assuming that a store is never performed before an SC
  ahead of it. Armv8-M does not promise that (DDI0553B.y, B7.2.3): two more barriers
  were needed, after E15 and D15, and the barrier before a dequeue returns was removed.
  The queue has seven. Each is shown to be needed, and the seven are shown to be enough
  at the full windows (`fifo_mp.py --wide`, some 40 minutes; the CI explores a part).

  The 3-slot buffer holds under the same model (`threeslot.py`): after its SC, each core
  reads Reading again. What the core stores next takes its slot from that read. Since
  2026-09-30, the CI holds the queue's compiled code to the same seven, as it did the
  slot buffers' code. Each barrier's removal is caught (`tools/check_order.py`,
  `tools/check_order_mutants.sh`).

  On the Pico 2, the queue ran with each core both producer and consumer
  (`FIFOCoresPico2`, four runs on 2026-09-28). Every record was taken once, whole and in
  order. The checks of each commit run `FIFOCoresPico2`. No instrument can show more:
  the litmus tests found no reordering between the RP2350's cores in 69.4 million rounds
  each (`architecture.md`), and Renode's cores keep program order. The barriers stay
  because the architecture allows what the chip did not show. The models are what show
  that each barrier is needed.

- **Response times against their analysis, on the Pico (2026-09-27, 42db832).** The
  bound of ZottaOS's manual (eq. 2.3) held for every response seen under DM and EDF. The
  kernel's costs in the bound were taken from the trace: 7 to 16 µs from an alarm to the
  task it releases. The responses were 58, 140 and 526 µs under DM, and 58, 141 and 527
  under EDF. The bounds were 123, 220 and 620 µs. The last figures, 526, 527 and 620 µs,
  were for the 60 ms task's longest instances, traced four minutes after start. A figure
  sets the schedule computed from the task set beside the trace. The schedule and the
  trace agree within 2 µs (`rp2040.md`, "Response times against their analysis";
  `tools/response_times.py`, `tools/schedule_figure.py`).

- **The STM32U5 port, under Renode (2026-09-25).** `Escapement/CORTEX-Mx/STM32U5` runs
  the hard and the soft kernel under both algorithms. The port has its examples with the
  endurance test. The port also has `SleepWrapU5` and `Stop2EventWrapU5`, scaled to
  cross the 2^30 wrap under Renode. The port's Renode platform is our own
  (`escapement_u5.repl`). The suite passes under each of the four builds, in the CI
  (`stm32u5.md`).

- **The STM32U5 on the board (2026-09-26).** It ran on the Arduino UNO Q, from SRAM. On
  the first day, the clock set-up ran, and `SoakU5` ran 22 s without error. The clock
  has come from the board's 16 MHz crystal since 2026-09-26, within some 25 ppm of NTP
  (`stm32u5.md`). A board check of each commit has run on the UNO Q's own Linux since
  2026-09-26 (`tools/unoq_check.sh`, status `board/u5`). Since 2026-09-27, the check
  runs the idle task in Stop 2 for a minute (`SleepU5`). Since 2026-10-02, the check
  runs a minute more without the HSE (`SleepNoHSEU5`). Then the check runs the endurance
  test for two minutes, and checks the clock to within 300 ppm over five minutes. The
  long endurance run then goes on with the commit's image.

  - Step 1 of the Stop 2 plan, the LPTIM1 driver on the LSE (`Escapement_LPTimer.c`) and
    its example `TestLPTimerU5`, was done on 2026-09-27. On the board, the LSE ran 15
    ppm slow against the HSE over 30 s, with no compare missed. Under Renode, the model
    STM32L0_LpTimer is used.
  - Step 2, the idle task in Stop 2 (`Escapement_Stop2.c`, example `SleepU5`), was done
    the same day. On the board, there were 388 entries in 43 s. Every start came one
    period after the last, to the microsecond. The longest wake-up took 885 µs. TIM2 ran
    2.1 ppm ahead of LPTIM1 (`stm32u5.md`). Since that day, TIM5 is carried through Stop
    2, and LPUART1 receives through Stop 2 (item 4).

- **The kernel runs, and is run in CI.** The Makefiles were repaired. Every example is
  built on each push to `main`, and executed under Renode as a regression test
  (`emulation.md`, `emulation/renode/RP2040.md`):

  - the STM32F4 on Renode's platform, with two fixes to its timer model, until the
    STM32F4's removal on 2026-09-26;
  - the RP2040 on the models of matgla/Renode_RP2040 with a fixed timer, since
    2026-09-21;
  - the RP2350, the STM32U5 and the STM32U3 on platforms of our own.
- **Every kernel and algorithm.** The algorithms are EDF and deadline-monotonic. The
  examples claimed EDF, and none ran it until the host test showed it. The hard and the
  soft kernel run on every target. The power-aware kernel runs on the RP2040, the
  STM32U3 and the host, under its four policies (`architecture.md`, `method.md`).
- **The scheduler on the host.** Every kernel runs under AddressSanitizer, crossing the
  2^30 wrap three times. The host test also covers what no example runs
  (`test/host/README.md`):

  - event-driven tasks;
  - the queue;
  - the slot buffers;
  - tasks that take time.
- **Every interleaving of the lock-free mechanisms**, in exhaustive models. The models
  found six defects, all fixed. Two of the defects were in the queue between the cores
  (`method.md`).
- **The RP2040 port on the board**:

  - all three kernels;
  - the DVFS driver on the silicon;
  - the timer events;
  - the cost of a scheduling round;
  - the periods on a frequency counter;
  - the regulator's response;
  - the 4-slot buffer between the cores.

  All of it was watched through a trace rather than a halted core (`rp2040.md`).
- **Checks on the board on every change of `main`**, on the images the CI builds. The
  bench pulls the images rather than being pushed to a self-hosted runner. Since
  2026-09-26, the bench is an Arduino UNO Q. The UNO Q checks (`tools/board_ci.md`):

  - the Pico (status `board/pico`);
  - the UNO Q's own STM32U5 (status `board/u5`);
  - since 2026-09-28, the Pico 2 (status `board/pico2`);
  - since 2026-10-10, a NUCLEO-U385RG-Q (status `board/u3`).
- **The RP2350 port (from 2026-09-24).** The generic layer was ported to ARMv8-M. Both
  cores run under Renode, with `ACTLR.EXTEXCLALL` set and memory barriers between the
  cores (`architecture.md`). The CI checks the barriers' compiled order against the
  models (`tools/check_order.py`). The 3-slot buffer runs across the cores under Renode,
  with the RP2350's exclusive monitor emulated in place of Renode's (`emulation.md`). A
  FIFO queue between the cores adapts Figure 3 of Evéquoz's paper to the RP2350's LL/SC
  (`Escapement_CoreQueue.c`, `test/model/fifo_mp.py`).
- **`-O2` (2026-09-20)**, once the barrier that makes a pended exception take effect was
  added (`method.md`). A round takes 3.2 µs instead of 7.0.
- **The queue sentinels made whole task control blocks (2026-09-22).** Two defects came
  from reading a task field through the idle task, past a block allocated to the few
  fields the idle task used. The head and the tail are now whole TCBs in `.bss`, for
  about sixty bytes of RAM per kernel. GCC's `-fanalyzer` was tried first. `-fanalyzer`
  reported nothing on the code before or after the change, because the blocks came from
  `OSMalloc` and were reached through casts.
- **A defined start for the STM32 timer (2026-09-20).** `_OSStartTimer` clears the
  counter. Started under Renode at 0x3FFFF000, the kernel produced no output at all.
  Since the fix, the kernel produces the 164 pulses in 80 ms of a normal start. The U5
  port's timer does the same.
- **A user guide**, `api.md`, written from the headers. Where the headers and the code
  disagree, the guide was checked against the code. For instance, on Cortex-M the kernel
  does not mask the source of an interrupt, as the headers say of the original port.
- **Reported upstream (2026-10-02): the two defects of Renode's STM32 timer**, as
  renode/renode#1023, with the reproducer of `emulation.md`. Reporting them had been
  dropped on 2026-09-24, and was taken up again once the defects were found still in
  Renode's `master`. The fixed copy lives in `emulation/renode`, where the CI loads it,
  now for the U5 and U3 platforms, so nothing waits on the issue. The older STM32 port
  was kept at the time because, until the RP2350 port, that port was the only one to run
  `LDREX`, `STREX` and `CLREX`.

## The MSP430 port was removed

Escapement began life on the TI MSP430. The port was removed on 2026-09-20 with its
three examples (8276933): 19,406 lines across 58 files. The port had not been built once
since the takeover. The original IAR and Code Composer projects are not in the
repository. Neither is the configurator that produced the port's per-derivative headers.
No test could reach the port.

What was lost is the FRAM of the MSP430FRxx parts. FRAM is non-volatile,
byte-addressable and nearly free to write. FRAM still has no equivalent for intermittent
computing under energy harvesting. FRAM was the one reason to keep the port. TI adds no
new MSP430 families. TI steers new designs towards MSPM0, a Cortex-M0+ that the generic
layer already covers. The history keeps it all, and so does the archived
`beber007/zottaos`.
