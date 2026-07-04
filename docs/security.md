# Security

## Session Keys

The native launcher reads the Mosh session key from `dbclient` stdout and does
not print, log, or persist it. The key is placed in `MOSH_KEY` only immediately
before `exec` of `mosh-client`; if the exec fails, the launcher unsets it before
reporting the failure.

## SSH Trust

Authentication, host-key prompts, and key handling are delegated to KOReader's
bundled Dropbear `dbclient`. The launcher passes arguments directly to
`dbclient`; it does not use `eval` or split a shell command from `MOSH_SSH`.

`MOSH_SSH` and `--ssh` accept a path to the SSH client. They intentionally do
not accept inline arguments.

## Remote Command Quoting

The remote command is constructed from a fixed command template with centralized
single-quote escaping for user-controlled values. The local host argument is
passed as a `dbclient` argv element and rejects shell metacharacters and
whitespace.

## UDP Exposure

Mosh uses UDP after startup. Users should restrict `mosh-server` ports with
`-p PORT[:PORT2]` and match that range in server firewall rules.

## Supply Chain

Mosh source is pinned to tag `mosh-1.4.0` and verified by SHA-256 before
extraction. Release archives include a manifest with file checksums.

No downloaded source trees, compiler toolchains, caches, or generated release
binaries are committed.
