# Checks on the board

A timer runs `board_ci.sh` every half hour. The script fetches `main`, and when it has
moved since the last run, builds and runs on a Pico wired to the machine the checks
listed in [`docs/rp2040.md`](../docs/rp2040.md#checks-on-the-board), then posts the
outcome as the commit status `board/pico`, with a fine-grained token for this
repository alone ("Commit statuses: read and write"). A run holds a lock; the board
stays free between runs for work by hand, which must not overlap one. The logs stay on
the machine, under `~/escapement-rp2040/board-ci/logs`.

The machine needs `git`, `jq` and `curl`; the probe side, OpenOCD and `python3`; the
build side, the ARM toolchain — unless it takes the images the CI builds for each commit
(`BOARD_CI_IMAGES=ci`, below), which then needs `unzip` and `arm-none-eabi-nm` only,
and a token that also reads Actions: GitHub serves artifacts to a signed-in client
only. The compiled order is then checked by the CI, under two compilers (`build.yml`).
The variables `BOARD_CI_*` at the head of the script change the working directory, the
containers, the repository, the token file and where the images come from.

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
Description=Escapement: board checks every half hour

[Timer]
OnCalendar=*:0/30
Persistent=true

[Install]
WantedBy=timers.target
EOF
systemctl --user daemon-reload && systemctl --user enable --now escapement-board-ci.timer
loginctl enable-linger   # runs without a session open
```

A machine asleep runs nothing; `Persistent=true` catches up once it wakes.

## On a Mac

The toolchain and OpenOCD run natively, without containers, and launchd takes the place
of systemd (replace `YOU` with the user's name, and the label with one of your own):

```sh
brew install open-ocd arm-none-eabi-gcc arm-none-eabi-binutils
git clone https://github.com/beber007/escapement.git ~/escapement-rp2040/board-ci/src
# the token: macOS's install cannot read /dev/stdin, so cat it in, then Ctrl-D
mkdir -p ~/.config/escapement-board-ci
(umask 077 && cat > ~/.config/escapement-board-ci/token)
# an agent that runs the script every half hour:
cat > ~/Library/LaunchAgents/li.hurst.escapement-board-ci.plist <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>                <string>li.hurst.escapement-board-ci</string>
  <key>ProgramArguments</key>     <array><string>/bin/sh</string>
    <string>/Users/YOU/escapement-rp2040/board-ci/src/tools/board_ci.sh</string></array>
  <key>EnvironmentVariables</key> <dict><key>PATH</key>
    <string>/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin</string></dict>
  <key>StartInterval</key>        <integer>1800</integer>
</dict>
</plist>
EOF
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/li.hurst.escapement-board-ci.plist
```

A launch agent runs while its user is logged in, as on a Mac left on for the purpose.

## On an Arduino UNO Q

The bench since 2026-09-26: Debian on the board's Qualcomm processor, the probe and the
Pico behind a powered USB-C hub on its only port, which it drives as a host. It has no
GCC 16, so it takes the CI's images, and runs under systemd as on Linux, without
containers:

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

Then the service and the timer of the Linux section, with one line more in the
service, under `[Service]`: `Environment=BOARD_CI_IMAGES=ci`. A run waits for the CI of
the commit to finish, and the next one tries again until it has.
