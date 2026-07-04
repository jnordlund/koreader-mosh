# Compatibility

## Automated Status

| Target | CPU flags from koreader-base | GLIBC max from koreader-base | Binary sharing status |
| --- | --- | --- | --- |
| `kobo` | `ARMV7_A8_ARCH`, hard-float | 2.15 | Build separately until device tests prove sharing |
| `kobov4` | `ARMV7_A9_ARCH`, hard-float | 2.19 | Build separately until device tests prove sharing |
| `kobov5` | `ARMV7_A53_ARCH`, hard-float | 2.35 | Build separately until device tests prove sharing |

The Kobo Clara family is expected to use the `kobo` target in current
`koreader-base`, but this must be verified on each device with the checklist in
`docs/device-test-checklist.md`.

## Device Test Status

No physical Kobo device validation has been performed in this checkout. Do not
claim a target as tested until the device checklist has been completed with the
exact KOReader version, device model, and package artifact.

Sleep/wake resume is a release blocker for supported devices. A target should
not move from expected to tested until the same local `mosh-client` session has
survived Kobo sleep/wake and Wi-Fi restoration without SSH re-authentication.

## Terminal Limits

KOReader's terminal advertises `TERM=vt52` and implements VT52 plus partial ANSI
and VT100 behavior. The package includes a local compiled `vt52` terminfo entry
only for the local Mosh client. Applications requiring full xterm, full VT100,
mouse reporting, or rich color behavior are not considered verified.
