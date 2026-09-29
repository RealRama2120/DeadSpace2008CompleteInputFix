# DEAD SPACE COMPLETE INPUT FIX

Standalone Dead Space (2008) input-modernization project by Rama2120.

This tree currently contains a hardened EA-build release candidate, not a
Nexus-ready public package. Its scope is deliberately limited to:

- a complete x86 `version.dll` forwarding/bootstrap test;
- a uniquely named payload;
- validated, chain-preserving `XInputGetState` observation;
- bounded controller diagnostics;
- one measured radial right-stick release candidate; and
- one fail-closed raw-mouse camera implementation for the canonical EA build.

The mouse implementation has passed local build, signature, transform, launch,
raw-input, standard-camera-call, subjective standard-camera, and mixed-input
coexistence checks. Explicit zero-G feel remains deferred to future
compatibility feedback.

The first controlled gameplay comparison supports keeping the controller fix at
the XInput boundary: the measured radial transform reduced the perceived
right-stick dead zone without reported drift, onset jump, aiming or diagonal
regression, or loss of full-stick speed. Clean-stack EA operation is now
verified. The accepted controller candidate uses a `0.11` physical cutoff and
the exact inverse of Dead Space's verified 26.52% radial response, while
preserving the full XInput square outer range. The first mouse experiment keeps
menus and DirectInput polling intact, substitutes raw relative deltas only at
the three validated camera calls, and leaves controller-only camera calls
vanilla. Broader controller coverage, Steam testing, and explicit zero-G mouse
feel remain compatibility coverage rather than v1.0 blockers; standard mouse
behavior is now the Phase 4 release candidate.

Phase 5 mixed-input coexistence is accepted. Periodic diagnostics are disabled
for normal users, including when the INI is absent; concise initialization and
failure logging remains available for support. The un-reproduced input-prompt
edge case is deferred rather than expanded into a fragile UI hook.

Build with:

```powershell
.\build.ps1
```

Artifacts are written to `build\Release`. See `docs\ARCHITECTURE.md` and
`docs\VERIFICATION.md` for the current evidence and remaining runtime tests.
The exact controller comparison profiles and procedure are in
`build\ControllerComparison` and `docs\CONTROLLER_COMPARISON.md`.
The mouse reverse-engineering and live-test evidence is in
`docs\MOUSE_INVESTIGATION.md`.
The exact vanilla/raw mouse A/B pair is in `build\MouseComparison` with its
procedure in `docs\MOUSE_COMPARISON.md`.
The final mixed-input gameplay pass is in `docs\COEXISTENCE_TEST.md`.

Create the audited local release-candidate archive with:

```powershell
.\package.ps1
```

The packager accepts only the two runtime DLLs, the public-default INI, and the
end-user README. It verifies staged hashes, ZIP entries, non-empty contents, and
writes a SHA-256 sidecar under `dist`. Public upload remains a separate step.
