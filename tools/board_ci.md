# Checks on the board

A timer runs `board_ci.sh` every half hour. The script fetches `main`, and when it has
moved since the last run, builds and runs on a Pico wired to the machine the checks
listed in [`docs/rp2040.md`](../docs/rp2040.md#checks-on-the-board), then posts the
outcome as the commit status `board/pico`, with a fine-grained token for this
repository alone ("Commit statuses: read and write"). A run holds a lock; the board
stays free between runs for work by hand, which must not overlap one. The logs stay on
the machine, under `~/escapement-rp2040/board-ci/logs`.

The machine needs `git`, `jq` and `curl`; the probe side, OpenOCD and `python3`; the
build side, the ARM toolchain. The variables `BOARD_CI_*` at the head of the script
change the working directory, the containers, the repository and the token file.

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
