# DEAD SPACE COMPLETE INPUT FIX — Proof-of-concept architecture

## Project boundary

This is a new, standalone Rama2120 project. It does not modify or require the
Ishimura Stability Patch, Seamless Texture Compatibility, the 4GB patch, or the
Dead Space Vortex extension. Those projects are read-only compatibility inputs.

## Loader split

- `version.dll` is a minimal x86 bootstrap and transparent system proxy.
- `DeadSpaceCompleteInputFix.dll` owns executable validation, diagnostics, and
  input interception.
- A missing or rejected payload does not prevent the 17 system VERSION exports
  from continuing to work.
- The proxy resolves the real DLL from the 32-bit system directory by absolute
  path. This avoids recursive application-directory resolution.
- The payload is initialized from a worker thread after loader-lock work ends.

## Standalone high-core-count startup safety

The canonical EA executable crashes in its own CPU enumeration when more than
eight logical processors are visible. This occurred at `Dead Space.exe` fault
offset `0x002CC6AE` both with the controller payload active and with the payload
intentionally missing, while the same installation launches when the separate
Stability Patch applies its early process-local limit.

To preserve the requirement that this project work by itself, the bootstrap
restricts only a process named exactly `Dead Space.exe`, and only when its
current allowed affinity contains more than eight processors. It selects up to
eight processors from that already-allowed mask; it never broadens affinity,
changes the executable, or applies to test hosts and unrelated programs. This
small prerequisite runs before game code and remains active if the optional
payload cannot load. Co-installation is compatible because another mod can only
observe the already narrowed process allowance.

The system VERSION surface observed on the development Windows installation has
17 named exports at ordinals 1 through 17. All 17 are forwarded. This includes
the four W/A functions imported by the installed ReShade `d3d9.dll`.

## Texture parent/child lifecycle

Seamless Texture Compatibility starts a launcher from its `dinput8.dll` worker,
then terminates the first `Dead Space.exe`. The real child inherits
`DSTL_TEXMOD_CHILD=1`. The payload records this state and waits briefly before
installing hooks. An initial parent may load the payload, but it exits before
long-lived diagnostics begin; the child performs the lasting initialization.

No Texture Compatibility code or file is changed.

## XInput interception

The proof of concept does not install `xinput1_3.dll`. It waits for the module
already selected by Windows, resolves its three relevant functions, and searches
only the main executable's readable, non-executable sections for one unique
contiguous triplet:

1. `XInputGetState`
2. `XInputGetCapabilities`
3. `XInputSetState`

This triplet matches the recovered Dead Space import-address-table ordering. The
slot is changed only when exactly one validated match exists. The hook saves and
calls the current target, so it naturally chains either the system XInput DLL or
the separately installed Ishimura Stability Patch proxy.

The diagnostic path calls the original function first and preserves the return
code, packet number, controller index, buttons, triggers, left stick, and all
other state. The experimental transform can change only `sThumbRX` and
`sThumbRY`.

The first measured development profile used a radial `0.09` physical cutoff and
a radial `0.14` game-facing minimum. A second profile raised the physical cutoff
to `0.11` and preserved the square outer range. Both produced no drift or onset
jump and preserved aiming, diagonals, and full-stick speed, but gameplay still
reported more apparent deadzone vertically than horizontally. Because both axes
pass through the same radial preconditioner, that asymmetry could not be
explained or safely corrected by lowering one API-layer threshold.

A read-only dump of the unpacked EA process exposed one centralized
`XInputGetState` call and the game's right-stick routine at
`0x008F70A4`-`0x008F7158`. The routine computes radial magnitude, zeros both axes
at or below `8689` raw units (`0.265175` of the XInput axis range), and otherwise
multiplies both components by:

`(clamp(magnitude, 32767) - 8689) / (32767 - 8689)`

This verified why a `0.14` API-facing minimum could reduce the perceived
deadzone without removing it: it remained below Dead Space's own 26.52% cutoff.

The current Phase 3 candidate keeps the deterministic `0.11` hardware cutoff,
but replaces the guessed anti-deadzone with the mathematical inverse of the
verified game response. For a desired normalized post-game magnitude `t` and
game cutoff `d`, the API-facing magnitude is:

`r = (d + sqrt(d*d + 4*(1-d)*t)) / 2`

At physical onset, `t` is zero and `r` is exactly the game's threshold, which
the game maps back to zero. Immediately above onset, the composed output follows
`t` continuously. The implementation remains radial, direction-preserving, and
monotonic, then preserves XInput's square outer range once the target radial
magnitude reaches one. Full cardinal, diagonal, and mixed-direction deflection
therefore remain available.

The recovered code also shows that all game consumers receive controller state
through the same downstream processing path. A deeper camera hook is therefore
not needed to correct the known deadzone and would add build-specific risk
without a demonstrated benefit. It should be reconsidered only if gameplay
finds behavior that the verified composition does not explain.

Automatic center calibration remains rejected for this candidate because the
observed controller returned to different long-lived center plateaus after
movement; learning or chasing those plateaus could create asymmetric response
or a delayed camera shift. The exact vanilla, earlier experiments, and verified
inverse remain separately reproducible through the comparison profiles. Full
analysis is in `CONTROLLER_TUNING.md`.

The development INI writes one bounded summary every two seconds. It records
five-second idle noise, precise last raw right-stick axes, and per-window
minimum/maximum magnitude without doing file I/O from the input hook. This lets a
tester hold the stick at the first visible camera response for a few seconds and
gives the experiment a numerical threshold without per-frame log spam.

## Mouse interception

The Phase 4 experiment retains the game's DirectInput device, acquisition, and
menu paths. It captures the game's own raw relative X/Y sample after a successful
mouse poll and changes camera arguments only at three uniquely validated direct
calls: standard camera, zero-G primary, and zero-G vertical. All three must still
target one uniquely identified camera function.

Sensitivity and X/Y inversion are read from uniquely signed game-setting
references. Those extracted pointers must lie in writable, non-executable game
image memory and retain their verified relative layout. Patch installation is
transactional across suspended game threads and rolls back partial writes.

Nonzero mouse samples use the recovered standard and zero-G scales. Zero input
leaves the original camera arguments untouched, so a controller camera call does
not pass through mouse scaling. The older fix's global camera-interpolation
write is intentionally excluded until gameplay establishes that raw injection
alone is insufficient. Full evidence is in `MOUSE_INVESTIGATION.md`.

## Safe failure

- Missing payload: VERSION forwarding remains active; no input hook exists.
- Wrong process: payload rejects initialization.
- XInput module absent: retry for a bounded startup period, then stop.
- XInput triplet missing or non-unique: no write is performed.
- IAT slot changes during installation: compare/exchange fails without replacing
  the new target.
- Mouse signature absent, duplicate, or inconsistent: no mouse patch is written.
- Mouse patch transaction cannot suspend/check all threads: no mouse patch is
  written.
- Configuration absent or invalid: conservative built-in defaults are used.

The unavoidable bootstrap prerequisite is a functional Windows system
`version.dll`. If that operating-system component cannot be loaded or lacks one
of its normal exports, the proxy itself refuses to load rather than exposing
null jump targets.
