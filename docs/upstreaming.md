# Upstreaming Notes

Preferred `koreader/contrib` shape:

- Keep `koreader-mosh` in its own public repository with this directory as the
  repository root.
- Add it to `koreader/contrib` as a submodule at `koreader-mosh`.
- Keep CI, release artifacts, package validation, and device-test evidence in
  the application repository.
- The `koreader/contrib` pull request should only add the `.gitmodules` entry
  and gitlink to a tested application commit.

Potential upstreamable pieces:

- A small C/C++ Mosh launcher that preserves interactive SSH prompts while
  avoiding Perl on constrained clients.
- Additional upstream tests around malformed duplicate `MOSH CONNECT` messages
  and key redaction.
- KOReader terminal documentation clarifying the current `vt52` profile,
  partial ANSI behavior, and `TERMINAL_DATA/scripts` command path.
- A `koreader-base` package recipe for a client-only Mosh build if the runtime
  dependency and locale strategy can be kept maintainable.

Kobo-specific packaging, terminal shim installation, and KOReader menu UI should
remain in this plugin or in `koreader/contrib`, not in upstream Mosh.
