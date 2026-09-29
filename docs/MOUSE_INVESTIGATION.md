# Phase 4 mouse investigation

## Scope

The first mouse experiment targets the canonical EA App executable
`Dead Space.exe` version 1.0.0.222. It does not replace DirectInput, alter menu
input, patch the executable on disk, or change another Rama2120 project.
Controller handling remains at the validated XInput boundary.

## Recovered input path

Read-only inspection of the unpacked runtime image found the mouse DirectInput
poll routine at `0x008F7A90`-`0x008F7E6D`. Its mouse branch calls
`IDirectInputDevice8::GetDeviceState` for a 20-byte `DIMOUSESTATE2` and copies
the relative X, Y, and wheel values directly. No acceleration or smoothing was
present in this poll path.

The standard camera call is at `0x004388E1`. Zero-G uses a primary call at
`0x005447A2` and a separate vertical call at `0x0054480C`. All three call the
same camera function at `0x00521260`.

The game settings used by the recovered path are:

- sensitivity: `0x00E88E34`;
- invert X: `0x00E88E1D`; and
- invert Y: `0x00E88E1E`.

The earlier public Dead Space Mouse Fix was treated only as a read-only behavior
reference. Its standard-camera scale is raw X divided by -1000 and raw Y
divided by 1000, multiplied by game sensitivity plus 0.01. Its zero-G divisors
are -1200 and 1200. It also disables a broad camera interpolation value; this
experiment deliberately does not apply that global write because it could
change controller and unrelated camera behavior.

## Experiment design

The payload requires seven unique executable-section signatures: mouse capture,
three camera calls, the shared camera function, sensitivity, and inversion.
It verifies that all camera calls target the recovered function and that the
settings addresses lie in writable, non-executable image memory with the
expected relationship. Any absent, duplicate, or inconsistent signature leaves
mouse behavior completely vanilla.

Installation is transactional. Other game threads are suspended only while the
four short in-memory patches are checked and written; installation is abandoned
if a thread is executing inside a patch range, and partial writes are rolled
back. No game file is patched on disk.

The capture detour records the raw relative X/Y values after a successful game
poll. At a camera call:

- nonzero raw input is converted with the configured scale, live game
  sensitivity, and live inversion settings;
- the standard path replaces horizontal and vertical camera deltas;
- zero-G primary replaces horizontal and observes vertical, while the separate
  zero-G call applies vertical;
- vertical movement is clamped to the game's `[-1, 1]` range; and
- a zero raw delta leaves every original camera argument unchanged.

That last rule is the controller-safety boundary: when the mouse is idle, this
hook does not reinterpret or replace controller-derived camera arguments.

## Automated verification

The strict x86 `/W4 /WX /O2` build passes mouse-transform tests covering:

- zero-input vanilla preservation;
- standard and zero-G reference scaling;
- X/Y inversion;
- the zero-G vertical-only pass;
- upper and lower vertical clamps; and
- safe rejection of an invalid sensitivity value.

All seven live signatures were independently scanned against the unpacked EA
image and each matched exactly once. All three recovered call targets matched
the shared camera function.

## First live EA App run — 2026-08-31

Only the three Dead Space Complete Input Fix files were deployed. The existing
Texture Compatibility, Stability Patch, 4GB proxy, and ReShade files were not
changed. The normal EA App launch completed its Texture Compatibility handoff,
loaded all six texture packs, and reached rendered gameplay.

The payload logged the validated mouse installation at these RVAs:

- capture: `0x004F7BD0`;
- standard camera call: `0x000388E1`;
- zero-G primary: `0x001447A2`;
- zero-G vertical: `0x0014480C`;
- shared camera function: `0x00121260`;
- sensitivity: `0x00A88E34`; and
- invert X/Y: `0x00A88E1D` / `0x00A88E1E`.

The title/menu run produced continuing DirectInput captures and real nonzero raw
samples without a camera override. After loading the current save, the standard
camera counter first reached `200/200`: 200 nonzero standard-camera calls and
200 successful raw-mouse substitutions. Continued live gameplay exercised all
three signed call sites without a crash; the later summary reached
`standard=1815/1815`, `zero_primary=504/657`, and `zero_vertical=463/657`.
The numerator is a successful raw override and the denominator is total calls.
The tester reported no untouched drift, immediate slow movement, materially less
acceleration/strangeness, consistent turn distance for slow and fast sweeps,
similar horizontal and vertical response, normal aiming and diagonals, and
unchanged controller behavior. This promotes the standard raw-camera path to a
release candidate. Explicit zero-G feel was not readily accessible in the
current save. The user accepted deferring it to post-release compatibility
feedback: revisit the zero-G path if personal gameplay or a Nexus report
identifies a problem.

The menu cursor still felt much faster and more disconnected than the in-game
camera. The tester clarified that this was already true in vanilla. That agrees
with the implementation and runtime evidence: the experiment observes but does
not transform DirectInput mouse samples in menus, and no camera call was
overridden at the title or main menu. No menu compensation will be added.

The deployed experimental hashes are:

- `version.dll`:
  `55DAC609676CF2BED68B8310D63F1051C416837A9F998D7E3B78601373BA6BEB`;
- `DeadSpaceCompleteInputFix.dll`:
  `CF18583A5B80F616D4F7F5A315D7CA5D41A3B8B567D721476FE4532F1F280CBD`;
- `DeadSpaceCompleteInputFix.ini`:
  `91EB11A31FCF77942C530B59EC44185445244D4C4E359E9CD8D2FE742CBFAE8B`.

## Current decision

The global camera-interpolation write used by the earlier fix remains disabled.
The successful standard-camera result shows that raw injection alone removes the
reported acceleration problem while preserving smooth presentation and
controller behavior. A broader interpolation change is rejected unless later
zero-G gameplay or a specific Nexus report demonstrates a narrowly attributable
problem.
