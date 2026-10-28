# Checks on the board

`tools/board_ci.sh` runs the board checks on each new commit of `main`. It posts the
outcome to GitHub as commit statuses:

- `board/pico` for the Pico;
- `board/pico2` for the Pico 2;
- `board/u5` for the STM32U5 of the Arduino UNO Q that carries the bench;
- `board/u3` for the STM32U3 of a NUCLEO-U385RG-Q plugged into the UNO Q.

The checks themselves are listed in
[`docs/rp2040.md`](../docs/rp2040.md#checks-on-the-board). This page explains how to
install the script under systemd on Linux, under launchd on a Mac, and on the UNO Q.

## How it runs

A timer starts `board_ci.sh` every two minutes. The script fetches `main`. When `main`
has moved since the last run, the script runs the checks on a Pico wired to the machine.
It posts the outcome as the commit status `board/pico`. Posting takes a fine-grained
token for this repository alone ("Commit statuses: read and write").

Once `main` is checked, the script takes the open pull requests, oldest first, one per
run. Each pull request whose branch is in this repository gets the same checks and
statuses, if its head is not yet checked. The ruleset on `main` requires these statuses
before a merge (issue #21, 2026-10-13). A pull request from a fork is never checked: its
head must be the head of the branch of the same name here. Only those who may push to
the repository can make such a branch, and so can Dependabot. Dependabot's pull requests
change only the versions of the actions (`.github/dependabot.yml`): the tools these pull
requests run on the bench are then `main`'s. An action that went bad could at worst
change the images the CI builds. Those images are loaded into the boards, not into this
machine. Since the machine pulls, nothing a fork sends runs there.

A pull request's images are those its run builds, with its head merged into its base.
Its tools are those of its head: a branch behind `main` may fail on a change made to the
tools since, until the branch takes `main` in. The list of pull requests is read without
the token, because GitHub gives it to anyone for a public repository. The script always
runs as `main` has it. So a change to `board_ci.sh` itself is checked on the bench only
once merged. The tools of the checks run as the commit checked has them. On a pull
request, the STM32 boards are checked without their long run (`BOARD_SOAK_SERVICE=none`,
`unoq_check.sh`), and nothing is written to a flash. The long runs then start again:
`soak.py` counts them interrupted and loads `main`'s image again.

On a pull request, the bench also leaves one comment of its own (issue #28, 2026-10-13).
The bench finds this comment by a marker and replaces it at each check. The comment
holds the statuses the bench posted, the key figures of the log and the whole log. The
probes' serial numbers and this machine's paths are removed from it. Posting the comment
takes "Pull requests: read and write" on the token. GitHub accepts this permission for a
comment on a pull request, as it accepts "Issues: write". Without it, only the statuses
are posted. A commit of `main` has no pull request, so it gets only the statuses.

A run with nothing new ends at once, in about 2 s. This happens when every commit was
already checked or its CI has not finished. The timer ran every half hour until
2026-09-27: by then the wait for the timer had become the longest part of a check.

A commit that changes only `.md` files does not run the boards. None of these files goes
into an image the boards run. The script first finds a base commit. For a pull request,
the base is the point where the pull request left `main`. For `main`, the base is the
last commit of `main` the bench checked. The base must show `success` on GitHub for all
four board statuses. A failed check, a board left unchecked or a base never checked does
not count. If every file changed since the base ends in `.md`, the script posts the four
statuses as a success: "documentation only", with the base named. The script marks the
commit as checked only once the four statuses are posted. On a pull request, the script
also leaves its comment. It takes no image and no probe's lock, and it stops no long
run. Any other case runs the full check:

- a file that is not `.md`;
- a file deleted or renamed;
- an empty change;
- a base that did not pass;
- no answer from GitHub;
- no token.

`--force` always runs the full check. Before this skip, a pull request of documentation
alone waited its turn on the bench like any other.

A run holds a lock. Between runs, the board is free for work by hand. That work must not
overlap a run. Within one run, the boards are checked side by side. Each board goes
through its own debugger (a Debug Probe, an ST-LINK, the UNO Q's own SWD) and has its
own log. The logs are put together in a fixed order once all boards are done. A check
took 16 minutes this way on 2026-10-18, against 28 with the boards in turn. The boards'
clocks, measured against Linux's, read the same either way. Under the containers of
Linux, the probe container must see the table of probes (`tools/probe.sh`): without it,
the Pico's OpenOCD takes the first Debug Probe it finds, which may be the Pico 2's.

Each Debug Probe also has a lock of its own, `lock-probe1` and so on. An endurance run
(`tools/soak.py pico`) holds that lock for as long as the run lasts. The board CI then
skips the board on that probe, without posting its status, and checks the others. The
NUCLEO-U385RG-Q is on none of our probes. It has `lock-u3`, which `tools/endurance.sh`
holds for a candidate's runs on the board: a pull request no longer leaves `main` alone,
and its checks would load their images over such a run. A pull request checked this way
lacks the status of the board it skipped, which its merge requires. A new push to its
branch has it checked again. Removing its head from `board-ci/prs-checked` does the
same. The logs stay on the machine, under `~/escapement-rp2040/board-ci/logs`. A lock
whose process is gone is taken over under `flock` where there is one, as on the UNO Q:
two processes taking over one stale lock at once could both hold it (a review,
2026-09-30).

The checks run on the CI's images only when the CI's run succeeded. A CI run that failed
gets the status `error` once per attempt of that run, and nothing is checked on the
board: its images vouch for nothing. When that CI run is started again and passes, the
commit is then checked. Until 2026-09-30, the board posted its statuses on the images of
a failed run. When GitHub does not answer or a download breaks, the next run tries
again.

The machine needs `git`, `jq` and `curl`. Driving the probe takes OpenOCD and `python3`.
Building the images takes the ARM toolchain. A machine can instead take the images the
CI builds for each commit (`BOARD_CI_IMAGES=ci`, below). It then needs only `unzip`,
`arm-none-eabi-nm` and a token that can also read Actions, because GitHub serves
artifacts to a signed-in client only. In that mode, the CI checks the compiled order,
under two compilers (`build.yml`). The `BOARD_CI_*` variables at the head of the script
set:

- the working directory;
- the containers;
- the repository;
- the token file;
- where the images come from;
- whether the STM32U5 of the UNO Q is checked.

## The bench

Since 2026-09-28, everything hangs on the Arduino UNO Q, through its powered hub. The
UNO Q is reached by `ssh arduino@192.168.1.220` (`unoq.local` does not resolve from the
Mac). This table is the reference. `~/.config/escapement-probes` and the units under
`~/.config/systemd/user` must agree with it.

| Probe or port | Board | Role | Held by |
|---|---|---|---|
| probe1 | Pico W | the checks of each commit, `board/pico` | `board_ci.sh`, a minute of a check's 16, side by side with the others (2026-10-18) |
| probe2 | Pico | the endurance test, a week per kernel, `board/soak`; the PPK2 since 2026-10-10 (roadmap, task 9) | `escapement-soak-pico`, stopped on 2026-10-10 |
| probe3 | Pico 2, the probe's UART on GP0 and GP1 | the checks of each commit, `board/pico2` | `board_ci.sh` |
| STLINK-V3 | NUCLEO-U575ZI-Q | a run until stopped, `board/soak-nucleo` | `escapement-soak-nucleo` |
| the UNO Q's own SWD | STM32U585 | the checks of each commit, `board/u5`, then a run until the next | `board_ci.sh`, then `escapement-soak-u5` |
| Bus Pirate v4 | — | the frequency counter, read by hand | — |
| FTDI TTL-232R-3V3 | NUCLEO-U385RG-Q, LPUART1 on D0 and D1 | `SleepU3`'s link (`LINK_TTY` of `tools/unoq_sleep.py`) | by hand |
| PPK2 | NUCLEO-U575ZI-Q, or a Pico 2 it powers | current, measured by hand (`tools/ppk2_nucleo.py`) | — |

Which board is a Pico W was recorded the wrong way round until 2026-10-10. On that day
the user read the boards: probe1's board is the Pico W, and probe2's board is the plain
Pico. Every check and run since 2026-09-28 was on the board its probe names. Only the
word was swapped.

`tools/bench_status.sh` shows on one screen:

- who holds each probe;
- the board CI;
- the last reading of each endurance run;
- the statuses of the commits concerned.

The script works from the Mac over SSH as well as on the board. It changes nothing.

Rules, each learnt on this bench:

- **No board is kept for work by hand.** Work on probe1 goes between two checks. That
  work holds the probe's lock (`mkdir lock-probe1`, a pid in it) the whole time. A check
  that finds the lock held skips the Pico without posting `board/pico`, and the commit
  stays without that status. Afterwards, put the Pico back as the checks expect it.
- **Never touch probe2 while its run lasts,** nor the image, the log or the state of a
  run.
- **Rebooting the UNO Q, or unbinding its drivers, interrupts every run.** On
  2026-09-28, two reboots from sessions working on its serial port started the Nucleo's
  run over. Before that, unbinding `geni_se_qup` had oopsed the kernel. Look at
  `tools/bench_status.sh` first. `tools/soak.py` keeps the state of a run. So a run
  whose image ran on through the reboot goes on. A run whose board lost power counts an
  interruption.

## On Linux

The toolchain runs in a container `esc`, set up as in
[`emulation/renode/RP2040.md`](../emulation/renode/RP2040.md). OpenOCD runs with the
probe in another container, `hw`:

```sh
git clone https://github.com/beber007/escapement.git ~/escapement-rp2040/board-ci/src
mkdir -p ~/.config/escapement-board-ci ~/.config/systemd/user
# a fine-grained token for this repository alone, "Commit statuses: read and write"
install -m 600 /dev/stdin ~/.config/escapement-board-ci/token <<<"$TOKEN"
cat > ~/.config/systemd/user/escapement-board-ci.service <<'EOF'
[Unit]
Description=Escapement: board checks on the newest main

[Service]
Type=oneshot
# A check took 28 minutes on 2026-10-11, the boards in turn, and 16 on 2026-10-18, side
# by side (issue #32), the STM32U3's two kernels the longest; one stuck on a probe's
# USB is stopped at the hour, its locks then taken over by the next run, and the
# commit, never marked as checked, checked again whole.
TimeoutStartSec=1h
ExecStart=/bin/sh %h/escapement-rp2040/board-ci/src/tools/board_ci.sh
EOF
cat > ~/.config/systemd/user/escapement-board-ci.timer <<'EOF'
[Unit]
Description=Escapement: board checks every two minutes

[Timer]
OnCalendar=*:0/2
Persistent=true

[Install]
WantedBy=timers.target
EOF
systemctl --user daemon-reload && systemctl --user enable --now escapement-board-ci.timer
loginctl enable-linger   # runs without a session open
```

A machine that is asleep runs nothing. With `Persistent=true`, the missed run happens
once the machine wakes.

## On a Mac

The toolchain and OpenOCD run natively, without containers. Here launchd takes the place
of systemd. Replace `YOU` with the user's name, and the label with one of your own:

```sh
brew install open-ocd arm-none-eabi-gcc arm-none-eabi-binutils
git clone https://github.com/beber007/escapement.git ~/escapement-rp2040/board-ci/src
# the token: macOS's install cannot read /dev/stdin, so cat it in, then Ctrl-D
mkdir -p ~/.config/escapement-board-ci
(umask 077 && cat > ~/.config/escapement-board-ci/token)
# an agent that runs the script every two minutes:
cat > ~/Library/LaunchAgents/escapement.board-ci.plist <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>                <string>escapement.board-ci</string>
  <key>ProgramArguments</key>     <array><string>/bin/sh</string>
    <string>/Users/YOU/escapement-rp2040/board-ci/src/tools/board_ci.sh</string></array>
  <key>EnvironmentVariables</key> <dict><key>PATH</key>
    <string>/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin</string></dict>
  <key>StartInterval</key>        <integer>120</integer>
</dict>
</plist>
EOF
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/escapement.board-ci.plist
```

A launch agent runs only while its user is logged in, so the Mac must be left on and
logged in for the purpose.

## On an Arduino UNO Q

The UNO Q has been the bench since 2026-09-26. Debian runs on the board's Qualcomm
processor. The probe and the Pico sit behind a powered USB-C hub on the board's only
port. The board drives that port as a host. The board has no GCC 16, so it takes the
CI's images. The script runs under systemd as on Linux, without containers:

```sh
sudo apt install openocd binutils-arm-none-eabi jq unzip
# the probe and the RP2040/RP2350 in BOOTSEL for the group plugdev, OpenOCD without sudo
sudo tee /etc/udev/rules.d/60-escapement-probes.rules <<'EOF'
ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000c", MODE="0660", GROUP="plugdev"
ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0003", MODE="0660", GROUP="plugdev"
ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="000f", MODE="0660", GROUP="plugdev"
EOF
sudo usermod -aG plugdev "$USER" && sudo udevadm control --reload
git clone https://github.com/beber007/escapement.git ~/escapement-rp2040/board-ci/src
# a fine-grained token for this repository alone: "Commit statuses: read and write",
# "Actions: read-only" and "Pull requests: read and write" (the comment on each pull
# request, below); cat it in, then Ctrl-D
mkdir -p ~/.config/escapement-board-ci && (umask 077 && cat > ~/.config/escapement-board-ci/token)
```

Then install the service and the timer of the Linux section. Add one more line under
`[Service]`: `Environment=BOARD_CI_IMAGES=ci BOARD_CI_BUILD= BOARD_CI_PROBE=`. The last
two variables are empty because the script otherwise expects containers on Linux. A run
waits for the CI of its commit to finish. The next run tries again until the CI has
finished. The first run there, on 2026-09-26 against 3b80541, passed every check:

- 322,880 reads of the 4-slot buffer across the cores, with none torn;
- 3.5 µs a round;
- the timer events within 3 µs of their period;
- 8.8 µs from 12 to 125 MHz.

### The STM32U5 of the UNO Q

With `BOARD_CI_U5=1` in that same line, the script also checks the board's own STM32U5
(`tools/unoq_check.sh`). The script posts the outcome as the status `board/u5`. The
check runs on the CI's images, so it needs `BOARD_CI_IMAGES=ci`. The check has four
steps:

1. `SleepU5` runs for a minute, with the idle task in Stop 2 (`tools/unoq_sleep.py`).
   The step requires every start on its period, no late wake-up, and at least 80 % of
   the instances in Stop 2. It also requires every timer event within 20 µs of its time,
   TIM2 within 20 ppm of LPTIM1, and every byte sent to the board received.
2. `SleepNoHSEU5` runs for a minute. It is the same test with the HSE never started and
   PLL1 on the MSIS of range 2 at every wake-up. It covers the path of a board without
   the HSE, which neither U5 board of the bench takes otherwise. It is checked like
   `SleepU5`. TIM2 must be within 20 ppm of LPTIM1 there too, since TIM2 is set from
   LPTIM1 at every wake-up and runs on PLL1, 106.7 ppm fast on the MSIS, only while
   awake.
3. The endurance test, `SoakU5`, runs for two minutes, with every part free of errors.
4. The board's clock is compared with Linux's raw clock, which NTP does not adjust, over
   five minutes (`tools/unoq_drift.py`). The clock must stay within 300 ppm.

The long endurance run (`tools/soak.py uno-q`, the user service `escapement-soak-u5`) is
stopped for the check. If the check passed, the run starts again on the commit's image.
Otherwise the run goes back to the image it had. The run of the commit before then ends.
Its status `board/soak-u5`, if still pending, is posted again from the state `soak.py`
kept until the run was stopped. That status is a failure if the run restarted or found
errors, and a success otherwise. The last status of a run is posted at most once an
hour, so it could be up to an hour behind. It could also stay pending after a failure
whose post was lost.

### The Pico 2

With `BOARD_CI_PICO2=1` in that line too, the script then runs the six examples of the
Pico 2 that count in memory (`tools/pico2_check.py`) on the CI's images. The script
posts the outcome as the status `board/pico2`. The six examples are:

- both slot buffers;
- the queue between the cores;
- `IPCPico2`;
- the endurance test;
- the litmus tests of `LitmusPico2`.

Each of these, except the litmus tests, is held to the criteria of the Renode suite. In
each litmus test, no weak outcome may appear, and the two cores must be seen within a
cycle of each other. Then, through the probe's UART wired to GP0 and GP1, the script
runs the UART echo and two tasks sending at once (`tools/pico2_uart.py`). This part runs
for images from 2026-09-30 on. The Pico 2 hangs on probe3 (`BOARD_CI_PICO2_PROBE`).
Raspberry Pi's OpenOCD drives the board. That OpenOCD is built as below into
`~/opt/openocd-rpi` (`BOARD_CI_OPENOCD_RP2350`). The check takes about a minute and a
half.

### A long run on a NUCLEO-U575ZI-Q

The checks of each commit interrupt the long run on the UNO Q. A NUCLEO-U575ZI-Q plugged
into the UNO Q's hub can hold a long run that nothing interrupts. The run is either the
same `SoakU5`, built in `STM32U5/Examples/nucleo-u575`, or `SoakFirmU5`, its build with
`KERNEL=SOFT`. `tools/board_images.sh` builds `SoakU5` too, into `soak_nucleo`. The
image's reports and link go over USART1 to the virtual COM port of the board's ST-LINK
at 115,200 baud. `tools/nucleo_load.sh` loads the image, through the ST-LINK and
Debian's OpenOCD. The OpenOCD package brings the udev rules that let the group plugdev
drive the probe. The group dialout reads the port. `tools/soak.py nucleo` finds the port
under `/dev/serial/by-id`, or takes `NUCLEO_TTY`. It posts the status
`board/soak-nucleo`.

The run is the user service `escapement-soak-nucleo`. Its files are in `~/soak-nucleo`.
The image and the two scripts are copied there by hand when the run is to move to
another commit. `tools/soak.py` reads the image already running if that image reports.
It loads its own image only if no image reports: a new image must be loaded first, or
the run goes on reading the one before (as on 2026-10-08, from `SoakU5` to
`SoakFirmU5`). With another ST-LINK on the same machine, the first port under
`/dev/serial/by-id` may be the other board's. The probe OpenOCD finds first may be the
other board's too. So give the service `NUCLEO_TTY` and `NUCLEO_SERIAL`.

```sh
mkdir -p ~/soak-nucleo && cp build/SoakU5.elf tools/soak.py tools/nucleo_load.sh ~/soak-nucleo/
sh ~/soak-nucleo/nucleo_load.sh ~/soak-nucleo/SoakU5.elf
cat > ~/.config/systemd/user/escapement-soak-nucleo.service <<'EOF'
[Unit]
Description=Escapement: the endurance test of the STM32U575, read on USART1 every minute

[Service]
Environment=BOARD_SOAK_LOG=%h/soak-nucleo/soak-nucleo.log BOARD_SOAK_SHA=<commit> PYTHONUNBUFFERED=1
ExecStart=/usr/bin/python3 %h/soak-nucleo/soak.py nucleo 0 60 %h/soak-nucleo/SoakU5.elf
Restart=on-failure
RestartSec=60

[Install]
WantedBy=default.target
EOF
systemctl --user daemon-reload && systemctl --user enable --now escapement-soak-nucleo
```

### The NUCLEO-U385RG-Q

The NUCLEO-U385RG-Q was plugged into the UNO Q on 2026-10-09, beside the
NUCLEO-U575ZI-Q. The UNO Q now has two ST-LINKs. Each ST-LINK is named by its serial.
The port of each is named by the same serial under `/dev/serial/by-id`. With
`BOARD_CI_U3=1` in the line of the board CI's service, the script checks the board's
STM32U385 after the STM32U5 (`BOARD=u3 tools/unoq_check.sh`). The check runs:

- `SleepU3` for a minute through Stop 2;
- `SoakU3` for two minutes;
- the board's clock against Linux's, within 300 ppm.

The script posts the outcome as `board/u3`, and it carries the board's long run on.
Since 2026-10-11, the same three steps run first under the power-aware kernel, from the
images' `soak_u3_pa`, without a long run (`BOARD_SOAK_SERVICE=none`). `board/u3` passes
only if both kernels pass. Then the long run goes on. The long run is the user service
`escapement-soak-u3`, in `~/soak-u3`, and it posts `board/soak-u3`. `BOARD_CI_U3_SERIAL`
and `BOARD_CI_U3_TTY` give the ST-LINK. `BOARD_CI_OPENOCD_U3` gives the OpenOCD (below).
On 2026-10-09, the check passed by hand. The board's clock was 121 ppm fast against
NTP's (`stm32u3.md`). `BOARD_CI_U3=1` has been in the line of the service since
2026-10-10, 20:46 UTC, with the board's serial and port.

The board's checks run from SRAM and its long run from the flash (decided by the user on 2026-10-10). The CI's images hold `soak_u3/flash/SoakU3.elf` beside the others.
`tools/unoq_check.sh` writes that image through `tools/nucleo_flash.sh` once the check
has passed, and only if the image differs from what the flash holds. The writes are
counted in `~/.local/state/escapement/flash-<serial>`, so that the wear can be read. The
user lifted two caps on 2026-10-10: one write in 24 hours, and 1,000 in all. The
NUCLEO-U575ZI-Q has no check of each commit: its long runs are started by hand, from its
flash too.

```sh
mkdir -p ~/soak-u3 && cp build-flash/SoakU3.elf tools/soak.py tools/nucleo_load.sh \
    tools/nucleo_flash.sh ~/soak-u3/
NUCLEO_SERIAL=<serial> OPENOCD=~/opt/openocd-upstream/bin/openocd NUCLEO_MCU=u385 \
    sh ~/soak-u3/nucleo_flash.sh ~/soak-u3/SoakU3.elf
cat > ~/.config/systemd/user/escapement-soak-u3.service <<'EOF'
[Unit]
Description=Escapement: the endurance test of the STM32U385, read on USART1 every minute

[Service]
Environment=BOARD_SOAK_LOG=%h/soak-u3/soak-u3.log BOARD_SOAK_SHA=<commit> PYTHONUNBUFFERED=1
Environment=NUCLEO_SERIAL=<serial> NUCLEO_TTY=/dev/serial/by-id/usb-STMicroelectronics_STLINK-V3_<serial>-if01
Environment=OPENOCD=%h/opt/openocd-upstream/bin/openocd
ExecStart=/usr/bin/python3 %h/soak-u3/soak.py nucleo-u3 0 60 %h/soak-u3/SoakU3.elf
Restart=on-failure
RestartSec=60

[Install]
WantedBy=default.target
EOF
```

### OpenOCD for the STM32U3

Debian's OpenOCD, 0.12, has no `stm32u3x.cfg`. OpenOCD's own sources have it. These
sources were built on the board on 2026-10-07 (8da0578d7) and installed apart, like
Raspberry Pi's fork below. Their `stlink-dap.cfg` and `dapdirect_swd` are deprecated but
still accepted. `tools/nucleo_load.sh` uses them for both OpenOCDs:

```sh
git clone https://github.com/openocd-org/openocd.git ~/src/openocd-upstream
cd ~/src/openocd-upstream && ./bootstrap && ./configure --prefix=$HOME/opt/openocd-upstream \
    --enable-stlink --enable-cmsis-dap --enable-cmsis-dap-v2 --disable-werror &&
    make -j2 && make install
```

### Several probes

The bench has three Debug Probes. All three have been on the hub since 2026-09-28, wired
as "The bench" says. `~/.config/escapement-probes` names them after their USB serials,
one `name serial` per line. Every tool that drives a probe takes `PROBE=name`, or a
serial as is. The default is probe1, the probe wired to the Pico, since OpenOCD left to
itself takes the first probe it finds (`tools/probe.sh`). To read a probe's serial once
it is plugged in:

```sh
for d in /sys/bus/usb/devices/*; do
    [ "$(cat $d/idProduct 2>/dev/null)" = 000c ] && echo "$(basename $d) $(cat $d/serial)"
done
```

### OpenOCD for the RP2350

Debian's OpenOCD, 0.12, knows the RP2040 but not the RP2350 of the Pico 2, and no
release does yet. Raspberry Pi's fork does. The fork was built on the board on
2026-09-26 (branch `rpi-common`, acff23f). It was installed apart from the OpenOCD the
checks use and from Arduino's in `/opt/openocd`. Like Debian's, the fork reads the
Pico's CHIP_ID through probe1. On 2026-09-28, it read the Pico 2's, 0x20004927, through
probe3, and found its two Cortex-M33:

```sh
sudo apt install build-essential git autoconf automake libtool texinfo pkg-config \
    libusb-1.0-0-dev libhidapi-dev libjim-dev
git clone --depth 1 --branch rpi-common --recurse-submodules --shallow-submodules \
    https://github.com/raspberrypi/openocd.git ~/src/openocd-rpi
cd ~/src/openocd-rpi && ./bootstrap && ./configure --prefix=$HOME/opt/openocd-rpi \
    --enable-cmsis-dap-v2 --enable-cmsis-dap --disable-werror && make -j2 && make install
~/opt/openocd-rpi/bin/openocd -f interface/cmsis-dap.cfg \
    -c "$(PROBE=probe3 sh ~/escapement-rp2040/board-ci/src/tools/probe.sh)" \
    -f target/rp2350.cfg -c init -c exit
```
