# Device Test Checklist

Record exact values. Do not mark an item passed unless it was tested on a
physical Kobo.

- Device model: Kobo Clara Colour
- KOReader version: not recorded
- KOReader install path: `/mnt/onboard/.adds/koreader`
- Package artifact name and SHA-256:
  `koreader-mosh-0.1.0-kobo.zip`; SHA-256 is published by CI as
  `koreader-mosh-0.1.0-kobo.zip.sha256`
- Target used (`kobo`, `kobov4`, `kobov5`): `kobo`
- Plugin loads in KOReader: pass
- Status screen shows launcher, `mosh-client`, terminfo and `dbclient` as ok: not recorded
- Terminal shim installation: pass
- `mosh user@example.com` password authentication: pass
- `mosh --identity /mnt/onboard/ssh/id_dropbear user@example.com` key authentication: pass
- Basic shell input and output: pass
- Swedish UTF-8 characters `å`, `ä`, `ö`: not recorded
- `tmux new-session -A -s kobo`:
- tmux window switching:
- `vim` or `nano`:
- Ctrl-L repaint:
- Terminal resize behavior:
- Configured Mosh escape sequence:
- Hide terminal with Close, reopen it, and confirm the same Mosh session is still active: pass
- Wi-Fi interruption and reconnection without re-authentication: pass
- Device sleep and wake resumes the same Mosh session without re-authentication: pass
- Device sleep and wake while terminal view is closed but session is running: pass
- Terminal Quit ends the local client and does not resume the same Mosh session:
- Server UDP firewall failure:
- Clean shutdown:
- Battery and CPU observations:

Automated tests cover launcher parsing, key redaction, shim ownership by static
plugin inspection, target metadata extraction, and package layout validation.
They do not verify KOReader's actual terminal drawing behavior or Kobo Wi-Fi
sleep/wake behavior.
