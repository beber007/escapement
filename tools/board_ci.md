# Checks on the board

`tools/board_ci.sh` runs the board checks on each new commit of `main` and posts the
outcome to GitHub as commit statuses: `board/pico` for the Pico, and `board/u5` for the
STM32U5 of the Arduino UNO Q that carries the bench. The checks themselves are listed in
[`docs/rp2040.md`](../docs/rp2040.md#checks-on-the-board). This page explains how to
install the script under systemd on Linux, under launchd on a Mac, and on the UNO Q.

## How it runs

A timer starts `board_ci.sh` every two minutes. The script fetches `main`. When `main`
has moved since the last run, it runs the checks on a Pico wired to the machine and
posts the outcome as the commit status `board/pico`. Posting takes a fine-grained token
for this repository alone ("Commit statuses: read and write"). Only `main` is ever
built, and only its owner pushes to it. Since the machine pulls, no pull request runs
there. A run with nothing new ends at once, in some 2 s, when the commit was already
checked or its CI has not finished. The timer ran every half hour until 2026-09-27, when
that wait had become the longest part of a check.

A run holds a lock. Between runs the board is free for work by hand, which must not
overlap a run. The logs stay on the machine, under `~/escapement-rp2040/board-ci/logs`.

The machine needs `git`, `jq` and `curl`. Driving the probe takes OpenOCD and `python3`,
and building the images takes the ARM toolchain. A machine can instead take the images
the CI builds for each commit (`BOARD_CI_IMAGES=ci`, below). It then needs only `unzip`
and `arm-none-eabi-nm`, and a token that can also read Actions, since GitHub serves
artifacts to a signed-in client only. In that mode the CI checks the compiled order,
under two compilers (`build.yml`). The `BOARD_CI_*` variables at the head of the script
set the working directory, the containers, the repository, the token file, where the
images come from, and whether the STM32U5 of the UNO Q is checked.

## On Linux

The toolchain runs in a container `esc`, set up as in
[`emulation/renode/RP2040.md`](../emulation/renode/RP2040.md), and OpenOCD with the
probe in another, `hw`:

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
once it wakes.

## On a Mac

The toolchain and OpenOCD run natively, without containers, and launchd takes the place
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
processor. The probe and the Pico sit behind a powered USB-C hub on its only port, which
it drives as a host. The board has no GCC 16, so it takes the CI's images. The script
runs under systemd as on Linux, without containers:

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
# a fine-grained token for this repository alone: "Commit statuses: read and write"
# and "Actions: read-only"; cat it in, then Ctrl-D
mkdir -p ~/.config/escapement-board-ci && (umask 077 && cat > ~/.config/escapement-board-ci/token)
```

Then install the service and the timer of the Linux section, with one more line under
`[Service]`: `Environment=BOARD_CI_IMAGES=ci BOARD_CI_BUILD= BOARD_CI_PROBE=`. The last
two are empty because the script otherwise expects containers on Linux. A run waits for
the CI of its commit to finish, and the next run tries again until it has. The first run
there, on 2026-09-26 against 3b80541, passed every check: 322,880 reads of the 4-slot
buffer across the cores with none torn, 3.5 µs a round, the timer events within 3 µs of
their period, and 8.8 µs from 12 to 125 MHz.

### The STM32U5 of the UNO Q

With `BOARD_CI_U5=1` in that same line, the script also checks the board's own STM32U5
(`tools/unoq_check.sh`) and posts the outcome as the status `board/u5`. The check runs
on the CI's images, so it needs `BOARD_CI_IMAGES=ci`. It has three steps:

1. `SleepU5` for a minute, the idle task in Stop 2 (`tools/unoq_sleep.py`): every start
   on its period, no late wake-up, at least 80 % of the instances in Stop 2, every
   timer event within 20 µs of its time, TIM2 within 20 ppm of LPTIM1, and every byte
   sent to it received;
2. the endurance test, `SoakU5`, for two minutes, every part without error;
3. its clock against Linux's raw clock, which NTP does not pull, over five minutes
   (`tools/unoq_drift.py`), within 300 ppm.

The long endurance run (`tools/soak.py uno-q`, the user service `escapement-soak-u5`)
is stopped for the check. If the check passed, the run starts again on the commit's
image; otherwise it goes back to the image it had.

### The Pico 2

With `BOARD_CI_PICO2=1` in that line too, the script then runs the six examples of the
Pico 2 that count in memory (`tools/pico2_check.py`) on the CI's images and posts the
outcome as the status `board/pico2`: both slot buffers and the queue between the cores,
`IPCPico2` and the endurance test, each held to the criteria of the Renode suite, and
the litmus tests of `LitmusPico2`, no weak outcome and the two cores seen within a cycle
of each other in each. The Pico 2 hangs on probe3 (`BOARD_CI_PICO2_PROBE`), driven by
Raspberry Pi's OpenOCD, built as below into `~/opt/openocd-rpi`
(`BOARD_CI_OPENOCD_RP2350`). The check takes about a minute.

### A long run on a NUCLEO-U575ZI-Q

The checks of each commit interrupt the long run on the UNO Q. A NUCLEO-U575ZI-Q plugged
into one of its USB ports can hold one that nothing interrupts: the same `SoakU5`, built
in `STM32U5/Examples/nucleo-u575` (`tools/board_images.sh` builds it too, into `soak_nucleo`), its reports and
link on USART1 to the virtual COM port of the board's ST-LINK at 115,200 baud. It is
loaded by `tools/nucleo_load.sh`, through the ST-LINK and Debian's OpenOCD, whose
package brings the udev rules that let the group plugdev drive the probe; the port is
read by the group dialout. `tools/soak.py nucleo` finds the port under
`/dev/serial/by-id`, or takes `NUCLEO_TTY`, and posts the status `board/soak-nucleo`.
The run is the user service `escapement-soak-nucleo`, its files in `~/soak-nucleo`, the
image and the two scripts copied there by hand when it is to run on another commit:

```sh
mkdir -p ~/soak-nucleo && cp build/SoakU5.elf tools/soak.py tools/nucleo_load.sh ~/soak-nucleo/
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

### Several probes

The bench has three Debug Probes, all on the hub since 2026-09-28: probe1 wired to the
Pico the checks use, probe2 to a second Pico, probe3 to a Pico 2.
`~/.config/escapement-probes` names them after their USB serials, one `name serial` per
line. Every tool that drives a probe takes `PROBE=name`, or a serial as is. The default
is probe1, the one wired to the Pico, since OpenOCD left to itself takes the first probe
it finds (`tools/probe.sh`). To read a probe's serial once it is plugged in:

```sh
for d in /sys/bus/usb/devices/*; do
    [ "$(cat $d/idProduct 2>/dev/null)" = 000c ] && echo "$(basename $d) $(cat $d/serial)"
done
```

### OpenOCD for the RP2350

Debian's OpenOCD, 0.12, knows the RP2040 but not the RP2350 of the Pico 2, and no
release does yet. Raspberry Pi's fork does. It was built on the board on 2026-09-26
(branch `rpi-common`, acff23f) and installed apart from the OpenOCD the checks use and
from Arduino's in `/opt/openocd`. Like Debian's, it reads the Pico's CHIP_ID through
probe1; on 2026-09-28 it read the Pico 2's, 0x20004927, and found its two Cortex-M33,
through probe3:

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
