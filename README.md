# koreader-mosh

`koreader-mosh` packages a Mosh client for Kobo devices running KOReader. The
primary workflow is:

```sh
mosh user@example.com
mosh -p 60000 user@example.com
mosh --ssh-port 2222 user@example.com
mosh --identity /mnt/onboard/ssh/id_dropbear user@example.com
mosh --predict never user@example.com
```

The terminal command is a native launcher, not Mosh's Perl wrapper. It uses
KOReader's bundled Dropbear `dbclient` for authentication and remote startup,
then runs the packaged `mosh-client` for the UDP session.

## Why Mosh

Mosh, the mobile shell, is a remote terminal protocol designed for unstable
network connections. It starts over SSH, then keeps an encrypted UDP session
alive while the client changes networks, loses connectivity, or goes to sleep.
Unlike a normal SSH terminal, a Mosh session can recover without
re-authenticating after the connection comes back.

This is useful on e-ink readers because Wi-Fi is often intermittent, devices
sleep aggressively to save battery, and terminal input is slower than on a
laptop. With Mosh, a KOReader terminal session can be hidden, the Kobo can sleep
and wake, and the user can continue the same remote shell after Wi-Fi returns,
without retyping credentials or restarting the workflow.

## Build

From this repository root:

```sh
make fetch
make build TARGET=kobo
make test
make package TARGET=kobo
```

The build scripts inspect a pinned `koreader-base` `Makefile.defs` for Kobo
target metadata and refuse to build if the target compiler is missing. If
`KOREADER_BASE=/path/to/koreader-base` points to a local checkout, that checkout
is used; otherwise the pinned metadata file is cached under
`koreader-mosh/.cache`. Source downloads are pinned and verified with SHA-256
before extraction.

Generated files are kept under `.cache`, `build`, and `dist`.

## Install

Extract the generated archive into the KOReader installation directory, for
example:

```sh
unzip dist/koreader-mosh-0.1.0-kobo.zip -d /mnt/onboard/.adds/koreader
```

In KOReader, open `Tools` > `Mosh client` > `Install or update terminal command`.
Then open KOReader's terminal and run `mosh user@example.com`.

## Sleep/Wake Resume

Mosh resume after Kobo sleep/wake depends on the local `mosh-client` process
staying alive. The native launcher hands off with `exec`, so after SSH startup
the terminal's foreground process is the actual `mosh-client`, not a wrapper.

To keep the session resumable:

- Use KOReader terminal's `Close` action to hide the terminal while leaving the
  shell and `mosh-client` running.
- Do not use terminal `Quit`, exit the remote shell, close KOReader, or reboot
  the Kobo if you expect the same Mosh session to resume.
- Use `tmux` or `screen` on the remote host for continuity after a KOReader
  restart or device reboot.

Physical Kobo sleep/wake behavior still must be verified with
`docs/device-test-checklist.md`.

## Troubleshooting

Use `Tools` > `Mosh client` > `Status` to check the packaged launcher,
`mosh-client`, `vt52` terminfo, terminal shim, and `dbclient` path.

The remote host must have `mosh-server` installed and must allow the selected
UDP port or range through its firewall.

The local client must run in a real UTF-8 locale. The launcher sets
`LANG`/`LC_CTYPE` to `C.UTF-8` when the terminal environment is not already
UTF-8, but the target runtime still must provide working UTF-8 locale semantics.
Do not patch Mosh to pretend an ASCII locale is UTF-8.
