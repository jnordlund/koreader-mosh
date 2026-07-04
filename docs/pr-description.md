# Client-only Mosh support for KOReader terminal on Kobo

## Summary

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

This adds a standalone `koreader-mosh` application for `koreader/contrib`. It
packages a Kobo-focused Mosh client workflow for KOReader's existing terminal:
a Lua plugin installs a `mosh` command into the terminal scripts directory, a
native launcher starts the remote `mosh-server` through KOReader's bundled
Dropbear `dbclient`, and the launcher then `exec`s into the packaged
`mosh-client` for the UDP session.

This does not add an SSH server, a Mosh server, host profiles, or automatic key
management to KOReader.

## Behavior

- The remote host must have `mosh-server` installed.
- The selected Mosh UDP port or range must be reachable through the remote
  firewall.
- SSH authentication, host-key prompts, and key handling stay delegated to
  KOReader's bundled `dbclient`.
- The launcher preserves interactive prompts by capturing only `dbclient`
  stdout; stdin and stderr remain attached to the KOReader terminal.
- After startup, the launcher is replaced by `mosh-client`, so a running Mosh
  session can survive KOReader terminal Close/reopen as long as KOReader and the
  local client process keep running.

## Compatibility

Initial scope is Kobo-only. The package includes a local `vt52` terminfo entry
for the Mosh client because KOReader's terminal advertises `TERM=vt52`.

The local Mosh client still requires a real UTF-8 locale from the target runtime
or packaged locale data. This PR does not patch Mosh's locale check or use an
`LD_PRELOAD` shim.

## Validation

- Unit tests cover launcher startup, prompt handling, key redaction, IPv4/IPv6,
  requested UDP ports, SSH port and identity options, prediction modes, remote
  command quoting, malformed and duplicate `MOSH CONNECT` messages, and signal
  forwarding.
- Package validation checks the plugin layout, binaries, `vt52` terminfo,
  README, COMPATIBILITY, OWNER, security notes, target metadata, and manifest.
- Physical sleep/wake validation should be recorded with exact device model,
  KOReader version, target, package artifact, and SHA-256 before claiming a Kobo
  model as tested.
