# Dead Space (2008) Complete Input Fix

![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-Windows-0078D6)
![Game](https://img.shields.io/badge/game-Dead%20Space%20(2008)-c41e1f)

**A standalone input-modernization mod for Dead Space (2008) on PC.**
It fixes the game's aging input stack where it matters: a measured right-stick
response curve that removes the oversized dead zone without introducing drift,
and a raw-mouse camera implementation for precise aiming. Everything happens at
the XInput boundary — no external config menus, no unrelated dependencies, and
no interference with other mods.

## What it does

- **Controller:** validated `XInputGetState` observation with a measured radial
  right-stick transform. Reduces the perceived dead zone with no reported
  drift, onset jump, aiming or diagonal regression, and no loss of full-stick
  speed. Preserves the full XInput square outer range.
- **Mouse:** fail-closed raw-mouse camera implementation. Substitutes raw
  relative deltas only at the validated camera calls — menus and DirectInput
  polling stay intact, and controller-only camera calls stay vanilla.
- **Safe by design:** complete x86 `version.dll` forwarding, uniquely named
  payload, chain-preserving hooks, bounded diagnostics (periodic diagnostics
  are off for normal users), and clean-stack operation verified on the EA
  build.

## Status and verification

This tree contains a hardened EA-build release candidate. The accepted
controller candidate uses a `0.11` physical cutoff and the exact inverse of
Dead Space's verified 26.52% radial response. The mouse implementation has
passed build, signature, transform, launch, raw-input, standard-camera-call,
subjective standard-camera, and mixed-input coexistence checks. Mixed-input
coexistence (Phase 5) is accepted. Explicit zero-G mouse feel, broader
controller coverage, and Steam testing remain future compatibility work, not
release blockers.

## Install

1. Copy the two runtime DLLs and the INI from the release archive beside
   `Dead Space.exe`.
2. Launch the game normally. No configuration is required.
3. To uninstall, remove the mod's DLLs and INI.

## Build from source

On Windows with Visual Studio C++ build tools and PowerShell:

```powershell
.\build.ps1
```

Artifacts are written to `build\Release`. Create the audited release archive
with:

```powershell
.\package.ps1
```

The packager accepts only the two runtime DLLs, the public-default INI, and
the end-user README. It verifies staged hashes and ZIP entries and writes a
SHA-256 sidecar under `dist`.

## Documentation

- `docs/ARCHITECTURE.md` — design and evidence
- `docs/VERIFICATION.md` — verification status and remaining runtime tests
- `docs/CONTROLLER_COMPARISON.md` — controller profiles and comparison procedure
- `docs/MOUSE_INVESTIGATION.md` — mouse reverse-engineering and live-test evidence
- `docs/MOUSE_COMPARISON.md` — vanilla/raw mouse A/B procedure
- `docs/COEXISTENCE_TEST.md` — mixed-input gameplay pass

## Credits and licensing

Created by Rama2120.

MIT licensed — see [LICENSE](LICENSE).
