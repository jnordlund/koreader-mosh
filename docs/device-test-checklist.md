# Device Test Checklist

Record exact values. Do not mark an item passed unless it was tested on a
physical Kobo.

- Device model:
- KOReader version:
- KOReader install path:
- Package artifact name and SHA-256:
- Target used (`kobo`, `kobov4`, `kobov5`):
- Plugin loads in KOReader:
- Status screen shows launcher, `mosh-client`, terminfo and `dbclient` as ok:
- Terminal shim installation:
- `mosh user@example.com` password authentication:
- `mosh --identity /mnt/onboard/ssh/id_dropbear user@example.com` key authentication:
- Basic shell input and output:
- Swedish UTF-8 characters `å`, `ä`, `ö`:
- `tmux new-session -A -s kobo`:
- tmux window switching:
- `vim` or `nano`:
- Ctrl-L repaint:
- Terminal resize behavior:
- Configured Mosh escape sequence:
- Hide terminal with Close, reopen it, and confirm the same Mosh session is still active:
- Wi-Fi interruption and reconnection without re-authentication:
- Device sleep and wake resumes the same Mosh session without re-authentication:
- Device sleep and wake while terminal view is closed but session is running:
- Terminal Quit ends the local client and does not resume the same Mosh session:
- Server UDP firewall failure:
- Clean shutdown:
- Battery and CPU observations:

Automated tests cover launcher parsing, key redaction, shim ownership by static
plugin inspection, target metadata extraction, and package layout validation.
They do not verify KOReader's actual terminal drawing behavior or Kobo Wi-Fi
sleep/wake behavior.
