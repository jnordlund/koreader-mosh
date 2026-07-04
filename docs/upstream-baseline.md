# Upstream Baseline

Inspected on 2026-06-21.

| Project | Revision | Notes |
| --- | --- | --- |
| `koreader/koreader` | `bb3514bdf120b4635e4bd4a68585fa2f17147ac0` | Current checkout on branch `feature/mosh` |
| `koreader/koreader-base` | `e26612880d468c45e6bea6805ae422840032dd66` | Submodule revision from current KOReader checkout |
| `mobile-shell/mosh` | tag `mosh-1.4.0`, commit `bc73a26316ede2a79259d859f8ee309b32412420` | Selected stable tag; source archive SHA-256 `ae581fbddf038730af9eee4d319a483288395a0722d0c94c7efb7fdbdbb0dbac` |
| `koreader/contrib` | `8bbaf5e22c2366e1a47ee686b4fcbc8b70b766c6` | Inspected plugin layout and OWNER expectations |

Relevant findings:

- KOReader terminal sets `TERM=vt52`, exports `TERMINAL_DATA`, and appends
  `TERMINAL_DATA/scripts` to `PATH`.
- `koreader-base` currently defines separate Kobo targets `kobo`, `kobov4`, and
  `kobov5`, all ARM hard-float but with different CPU tuning and GLIBC maximums.
- `koreader-base` builds Dropbear `dbclient` for KOReader and installs it at the
  KOReader root for Kobo packages.
- Mosh 1.4.0 still ships `scripts/mosh.pl`, requires protobuf, and enforces a
  real UTF-8 locale in `mosh-client`.
- `koreader/contrib` expects each application to live in its own folder with a
  README and OWNER file and to document compatibility.
