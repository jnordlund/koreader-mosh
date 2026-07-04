# License Notices

This project-specific wrapper and plugin are licensed as GPL-3.0-or-later to
remain compatible with Mosh.

Packaged release artifacts must preserve notices for:

- Mosh 1.4.0: GPL-3.0-or-later with the OpenSSL linking exception stated in
  upstream source files.
- KOReader integration snippets: AGPL-3.0-or-later compatibility applies when
  copied into KOReader itself; this standalone plugin is distributed separately.
- ncurses terminfo data: MIT-style ncurses license.
- Target runtime libraries, if packaged statically or dynamically: include the
  corresponding libc, libstdc++, libgcc, protobuf, OpenSSL, zlib and ncurses
  notices according to the actual linked artifacts.

The package manifest records file checksums. Add exact binary dependency license
files during release packaging when the target binary is produced.
