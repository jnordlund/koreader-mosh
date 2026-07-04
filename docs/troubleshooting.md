# Troubleshooting

## `dbclient` Missing

Install the archive into the KOReader root, not inside the data directory. The
plugin expects the bundled client at `dbclient` relative to the KOReader
installation.

Use `MOSH_SSH=/path/to/dbclient mosh user@example.com` only for diagnostics.

## Locale Errors

Mosh requires a real UTF-8 locale. The launcher sets `LANG=C.UTF-8` and
`LC_CTYPE=C.UTF-8` when the terminal environment is not already UTF-8. The
target binary must still be linked against a libc or bundled locale data that
implements those semantics.

Do not bypass Mosh's `is_utf8_locale()` check.

## Terminfo Errors

The launcher sets `TERMINFO` to the packaged terminfo directory and preserves
`TERM=vt52`. If `mosh-client -c` fails, check that
`plugins/mosh.koplugin/share/terminfo/v/vt52` exists.

## Remote Startup Errors

The remote host must have `mosh-server` installed. Use `--server PATH` if it is
not on the remote login path.

## UDP Timeout

SSH success does not prove UDP reachability. Open the selected server-side UDP
port or range. For example:

```sh
mosh -p 60000:60010 user@example.com
```

Then allow UDP 60000-60010 on the remote firewall.
