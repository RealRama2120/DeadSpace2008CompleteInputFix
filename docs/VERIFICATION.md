# Verification record

## Independently rechecked before implementation

- Local EA App executable: x86, file version `1.0.0.222`.
- Local game root had no `version.dll` or `winmm.dll`.
- Occupied root proxies: `dinput8.dll`, `xinput1_3.dll`, `dsound.dll`, and the
  locally installed ReShade `d3d9.dll`.
- Vortex deployment manifests identify the first three as separate Rama2120 mod
  files; none deploys `version.dll`.
- The 32-bit system `version.dll` exposes 17 named functions at ordinals 1-17.
- Installed ReShade imports `GetFileVersionInfoSizeW`, `GetFileVersionInfoW`,
  `VerQueryValueA`, and `VerQueryValueW` from VERSION.
- Ishimura Stability Patch's `XInputGetState` implementation directly calls the
  function resolved from the real system `xinput1_3.dll`; no stick-value
  transformation exists in that path.
- Seamless Texture Compatibility marks its real child with
  `DSTL_TEXMOD_CHILD=1` and bypasses its own bootstrap in that child.

## Automated tests

`build.ps1` builds x86 release artifacts and runs:

1. Complete VERSION name/ordinal and real-call forwarding.
2. The same proxy test with the payload intentionally absent.
3. A controlled XInput fixture proving unique IAT discovery, chaining, error-code
   preservation, and bit-transparent controller state while the experiment is
   disabled by a dedicated test-only configuration.
4. The same validated chain against the real system `xinput1_3.dll`, representing
   a game installation without the Stability Patch proxy.
5. Radial-transform invariants, including worst-recorded idle suppression,
   exact composition with the recovered Dead Space response, no camera-side
   onset jump, horizontal/vertical equivalence, an exhaustive 11-by-11 input
   grid, monotonicity, symmetry, legacy-profile reproduction, and full XInput
   square-boundary preservation for cardinal, diagonal, and mixed directions.
6. A process named `Dead Space.exe` proving the standalone bootstrap selects at
   most eight processors from the process's existing allowed affinity mask.

## EA App full-stack runtime result — 2026-08-29

The development POC was deployed beside the installed EA App executable without
changing any existing mod file. A normal `Dead Space.exe` launch produced:

- Initial Texture Compatibility parent: PID 41392, `texture_child=0`, payload
  loaded successfully.
- Real Texture Compatibility child: PID 51964, `texture_child=1`, payload loaded
  successfully.
- Dead Space reached its rendered main menu and remained responsive.
- The unique XInput triplet resolved at the expected current-build IAT RVA
  `0x007C9418`.
- The saved call target belonged to the installed root `XINPUT1_3.dll`, proving
  the new mod chained the Ishimura Stability Patch proxy rather than replacing
  it.
- The hook recorded thousands of real game calls. With no controller recognized,
  all calls correctly returned non-success and no state was transformed.
- Ishimura Stability Patch logged successful CPU, D3D9, subtitle, borderless,
  native-backbuffer, and 60 FPS pacing initialization in the same child process.
- ReShade 6.8.0 initialized, created its D3D9 path, compiled effects, and observed
  the later system DirectInput load. Existing unrelated shader compile failures
  remained present but did not prevent the game from reaching its menu.
- The active `Dead Space.exe` SHA-256 remained
  `31DBF16A3F7CA7ED35AFC11E009A0CEDF1375C15D036CBC5CB2E9A3B0D89152C`, matching
  the 4GB mod manifest's patched hash.

This verifies the preferred loader on the accessible EA build and complete
installed stack. It does not establish Steam compatibility.

An attempted clean-profile launch from a separate local test root did not remain
isolated: EA's activation layer redirected execution to the canonical installed
`C:\Program Files\EA Games\Dead Space\Dead Space.exe`, which then followed the
normal Texture Compatibility path. A copied EA executable is therefore not a
valid way to claim a vanilla-stack runtime result. The automated system-XInput
test remains valid, but a real clean launch required temporarily disabling the
deployed root proxies in the canonical EA installation.

## EA App clean-stack runtime result — 2026-08-29

With the user's permission, the canonical installation's existing
`xinput1_3.dll`, `dinput8.dll`, `dsound.dll`, and `d3d9.dll` were temporarily
held outside the active root under their original names. Exact pre-test sizes,
SHA-256 hashes, creation times, and last-write times were recorded first.

The initial isolated launch exposed an original-game high-core-count startup
fault at `Dead Space.exe+0x002CC6AE`; the same fault occurred with the Input Fix
payload intentionally absent. The Input Fix bootstrap now independently limits
only `Dead Space.exe` to at most eight processors from its existing allowed
mask. This removes an accidental runtime dependency on the Stability Patch
without changing the executable.

The corrected payload-active run (PID `38732`) then:

- launched normally through the EA App and reached the rendered main menu;
- loaded the root Input Fix `version.dll` and uniquely named payload;
- preserved system VERSION forwarding;
- initialized the validated XInput hook at RVA `0x007C9418`;
- resolved `dinput8.dll`, `dsound.dll`, `d3d9.dll`, and `xinput1_3.dll` from the
  Windows system directory;
- chained directly to system `XINPUT1_3.dll` with no Stability Patch present;
- received successful controller state on slot 0 while the `0.09` / `0.14`
  right-stick transform was active; and
- exited normally through the game's own Exit to Windows command.

The payload-unavailable run (PID `37836`) also reached a rendered menu and
exited normally. Its bootstrap entry recorded
`status=payload_missing_or_load_failed_safe`, proving safe fallback in the real
clean-stack environment.

All four original proxies were restored to their exact names and locations.
Their post-restore sizes, hashes, creation times, and last-write times matched
the pre-test record byte for byte; `Dead Space.exe` retained SHA-256
`31DBF16A3F7CA7ED35AFC11E009A0CEDF1375C15D036CBC5CB2E9A3B0D89152C`.
The temporary hold and obsolete held payload were deleted, and a recursive audit
found no disabled proxies, renamed clean-stack files, temporary loaders, or
other Input Fix test leftovers. The separate pre-existing LAA executable backup
was intentionally left untouched. Full evidence is preserved in
`test-artifacts/2026-08-29-clean-stack-snapshot.md`.

## Remaining compatibility coverage

- Steam executable and runtime testing when that installation becomes available.
- The narrow mixed-input stress pass in `COEXISTENCE_TEST.md`.
- Additional controller models through later compatibility feedback; the
  current zero-configuration default is intentionally conservative and already
  accepted on the measured controller.

## Controller baseline — 2026-08-29

The transparent diagnostic build captured a successful controller session with
the transform explicitly disabled (`experiment=0`). Therefore, any subjective
variation in that run was the game's unmodified response, not a fix effect.

- The first five-second center sample settled at raw right-stick
  `(2308, -1160)`, radial magnitude `2583`, or about `7.9%` of XInput range.
- Later steady low-motion plateaus appeared in roughly the `3200` to `4650`
  magnitude band (`9.8%` to `14.2%`). Exact correspondence to visible camera
  onset remains subjective because the diagnostic log has no access to camera
  state.
- The tester reported a noticeable dead zone, no obvious onset jump, normal
  aiming, and no noticeable diagonal difference.

This evidence supports a deliberately narrow API-layer experiment: discard the
measured physical center region at `0.09`, map the first surviving radial input
to `0.14`, and rescale continuously to full output. The test is intended to
answer whether preconditioning the XInput values can materially reduce the
game's dead zone without creating drift or a perceptible onset jump.

## Controlled right-stick experiment — 2026-08-29

The enabled build ran in the real Texture Compatibility child (PID `36940`) and
logged:

- validated IAT hook at RVA `0x007C9418`;
- `experiment=1`, radial inner cutoff `0.0900`, anti-deadzone `0.1400`;
- chained target in the installed root `XINPUT1_3.dll`, preserving the Stability
  Patch path; and
- five-second raw idle maximum `(2809, 254)`, radial magnitude `2820` (about
  `8.6%`).

The tester reported:

- no camera drift while untouched;
- a smaller perceived dead zone;
- no jump when camera movement began;
- normal aiming and diagonal response; and
- normal full-stick speed.

The active-profile log and subjective comparison agree: preconditioning the
right-stick state at the XInput API boundary can materially improve this build's
response without exposing the downstream discontinuity the experiment was
designed to detect. The final controller implementation should therefore remain
at this boundary unless later tests contradict the result.

The `2820` idle magnitude is only `129` raw units below the `0.09` cutoff
(approximately `2949`). That is adequate for this run but too little safety
margin to declare `0.09` a universal release default.

## Phase 3 controller refinement — 2026-08-30

Cross-run center analysis found long stable plateaus at 4.05% to 8.57% of
XInput range, with a worst five-second idle magnitude of 2820 (8.61%). The same
class of offset remained visible in the clean-stack run chained directly to
Windows `XINPUT1_3.dll`, excluding the Stability Patch and Input Fix as the
source. The plateau behavior and shifts after physical movement are more
consistent with hardware/firmware centering than random input noise. Steam was
running, but no evidence currently attributes the EA App game's direct system
XInput values to Steam Input.

The retained hardware profile uses a `0.11` physical cutoff. Its raw cutoff is
about 3604, leaving 784 raw units (2.39% of full range) above the worst captured
idle state while adding only two percentage points to the drift-free 0.09
experiment. Automatic calibration was rejected because the observed rest point
is not stable across movement and could be learned incorrectly.

The transform was also corrected to preserve XInput's square outer range. The
experimental unit-circle mapping could reduce both axes at the extreme diagonal
corner; the later profiles preserve 32767/32767 as well as mixed-direction
boundary values.

The rebuilt 0.11 / 0.14 candidate was deployed over only the three Input Fix files;
all four restored runtime proxies retained their recorded hashes. A normal EA
App full-stack launch produced Texture Compatibility child PID `56792`, reached
the title/menu render, and remained responsive. The payload logged the expected
validated IAT hook and chain to the installed Stability Patch proxy with
`experiment=1 inner=0.1100 anti=0.1400 square_outer=1`. The candidate
artifact hashes used for this launch were:

- `version.dll`:
  `915A788C2FD34C26270C9514B79AAFA049FBD1A99B0CCC5121B799A7C2A72453`
- `DeadSpaceCompleteInputFix.dll`:
  `EB261B7C43D0B176451AEEE47E84DF888683ED444C6C3937CA162EBFC63299FD`
- release-candidate INI:
  `18A31516E60AEEB4513D3DA2F5601764E6EFB054106EF1369662D480B44B8120`

The controlled gameplay results were:

| Profile | Drift | Perceived deadzone | Onset | Aiming, diagonals, full speed |
|---|---|---|---|---|
| Vanilla | None | Very noticeable | No jump | Normal |
| 0.09 / 0.14 | None | Much smaller horizontally; larger vertically | No jump | Normal |
| 0.11 / 0.14 | None | Horizontal good; vertical still larger | No jump | Normal |

The repeated axis symptom could not originate in the fix's radial transform, so
further blind threshold tuning was stopped.

## Recovered game response and profile 4 — 2026-08-30

A read-only process-image dumper reconstructed the unpacked canonical EA image
without modifying the running process or installed executable. The diagnostic
artifact is `test-artifacts/DeadSpace-runtime-unpacked.exe`, SHA-256
`6AB7EF2FF2B3E804FDEA628E0DE2276971A22B8903D95AB532367F768AF19E03`; it is not
a release file.

Static inspection found one `XInputGetState` call and the right-stick processing
block at `0x008F70A4`-`0x008F7158`. The verified routine zeros both right-stick
axes at or below radial magnitude 8689 (`0.265175`), then scales the original
components by `(clamp(magnitude, 32767) - 8689) / 24078`. This proves that the
earlier 0.14 game-facing minimum remained below the game's real cutoff.

Profile 4 retains the safe 0.11 physical cutoff and sends the exact inverse of
that verified game response. At onset, its API output reaches the game's
threshold while the composed game output remains zero; subsequent input begins
continuously and linearly. The full automated suite, including exact composition
over an 11-by-11 square grid, passes under `/W4 /WX /O2` for x86.

Only the three Input Fix files were deployed for a normal full-stack EA launch:

- `version.dll`:
  `AF0C71F87E314288F94966F2FC3BD079A04D6CC8F41D9A85FAF1A89F86357F0F`
- `DeadSpaceCompleteInputFix.dll`:
  `271B0AE4CA33B356249328E60041952B8145B28A8C67D4C4C320EC6783C84307`
- profile 4 INI:
  `F33E86E2D557871C15B28F3E74DCE24FAFD5F9A94412AA8D59E6CFC9E5BCE27D`

Texture Compatibility child PID `32544` reached the rendered title/menu and the
payload logged `experiment=1 inner=0.1100 game_comp=1
game_inner=0.265175 legacy_anti=0.1400 square_outer=1`, with the saved XInput
target still chained to the installed Stability Patch proxy. The game was left
running for the required subjective profile-4 comparison.

The profile-4 gameplay result was:

- no untouched-camera drift;
- horizontal and vertical onset felt much more equal, with only an
  ever-so-slightly larger perceived vertical deadzone;
- no onset jump and good low-speed camera movement;
- normal aiming and diagonal behavior; and
- normal full-stick speed.

The right-X and right-Y game code uses the same radial threshold, response
factor, and final integer conversion. An axis-specific corrective boost was
therefore rejected because it would manufacture asymmetric stick data and risk
diagonal distortion for a very small downstream difference. Profile 4 is now
the accepted controller release candidate; controller work is locked while the
mouse phase proceeds.

## Phase 4 first mouse runtime — 2026-08-31

The first raw-mouse experiment passed the strict build and transform-test suite,
and all seven EA-build signatures matched exactly once in the unpacked runtime
image. Only `version.dll`, `DeadSpaceCompleteInputFix.dll`, and
`DeadSpaceCompleteInputFix.ini` were deployed; no other runtime proxy was
modified.

A normal EA App launch completed the Texture Compatibility parent/child handoff,
loaded the six deployed texture packs, and reached the rendered title, main menu,
and current save. The payload installed both independent paths:

- the accepted controller transform chained to the existing Stability Patch;
- the fail-closed mouse capture and three validated camera calls.

The menu produced continuing DirectInput captures and real nonzero raw samples
without camera replacement. In live gameplay, the mouse summary first reached
`standard=200/200`, proving 200 nonzero standard camera calls were all replaced
by the raw transform. Continued play exercised all three signed call sites and
reached `standard=1815/1815`, `zero_primary=504/657`, and
`zero_vertical=463/657` without a crash. At that point, subjective
standard-camera and explicit zero-G feel had not yet been dispositioned.

The subsequent standard-camera report found no drift, immediate low-speed
response, consistent physical-distance turning at slow and fast mouse speeds,
similar horizontal and vertical behavior, normal aiming and diagonals, and
normal controller behavior after switching devices. The menu cursor retained
its faster/disconnected vanilla feel; because menu camera overrides remained
zero and the capture detour preserves the original poll data, this is not an
Input Fix regression. Standard mouse behavior is promoted to release candidate;
explicit zero-G feel was not readily accessible from the current Chapter 5
Medical save. The user accepted deferring that scenario to future personal
gameplay or Nexus compatibility reports, so it is a documented post-release
coverage item rather than a v1.0 blocker.

The live log independently confirmed controller coexistence after the mouse
test: slot 0 resumed successful XInput polling while the mouse hooks remained
installed. A malformed non-finite INI float is now rejected in favor of its
bounded fallback, and the mouse transform independently rejects non-finite
scales. The strict build and expanded transform suite pass with those guards.

The hardened candidate was deployed after a normal shutdown. Only the three
Input Fix files changed; the four existing runtime proxies retained their exact
preserved SHA-256 hashes. Candidate hashes are:

- `version.dll`:
  `CA94FB2DDC4F9C618A61C52B8DEC34B46DDB16F8453BF334716C78BB778335AD`;
- `DeadSpaceCompleteInputFix.dll`:
  `A2817C7900D0B2977A1C9EDE8DA37634B65DF6A2B62C887F0365C71475A32516`;
- `DeadSpaceCompleteInputFix.ini`:
  `84FF8CC056B5AF28A634861604C74D232B9484F237863CED73F22E27A4F4F361`.

Preserved runtime hashes after deployment:

- `xinput1_3.dll`:
  `F1B93E590327FBCEA7DEA238FB63EAFD8F86E33BA28C8EA8F3922CDFDE9BBFF2`;
- `dinput8.dll`:
  `6E3BEC60AC29D013783D086235CA98C3D2AAA37C5C7ABE978667A6048360786B`;
- `dsound.dll`:
  `5736124F3E46B124856DFF9C4FDE441346EEB8F23186BEF4EC1F81DB0BE1DFB2`;
- `d3d9.dll`:
  `DA430E0A9C6EECEFA0D1B27D05E16C426FB5D04E808B194D914EAAC4B31BC0F8`.

The hardened candidate then completed a normal EA App launch through the
installed full runtime stack. Texture Compatibility loaded all six deployed
packs, child PID `12220` reached the rendered title and main menu, the bootstrap
reported `payload_initialised`, and the payload installed the validated XInput
and mouse paths. XInput chained to the installed `XINPUT1_3.dll`; all three
mouse call-site signatures matched their expected RVAs. The game exited through
its own `Exit to Windows` menu at `09:15:25`, and the texture launcher observed
the game close and shut down its TexMod process normally.

Post-shutdown hashing reproduced every candidate and preserved-runtime hash
listed above exactly. No Input Fix or existing runtime proxy changed during the
launch. This closes the hardened build's objective launch, initialization,
render, normal-shutdown, and installed-stack integrity checks.

## Phase 5 coexistence status

The accepted mouse gameplay session already covered controller-to-mouse and
mouse-to-controller switching without restarting: the user reported normal
controller behavior after the raw-camera test, and the runtime log showed slot
0 returning to successful XInput polling while all mouse hooks remained
installed. Aiming, diagonal input, menus, standard gameplay, and normal
full-stick response were also exercised across the controller and mouse passes.

The user confirmed the mixed-input pass had already been completed and that
both input paths seemed normal. Phase 5 is therefore accepted: no right-stick
failure after mouse use, mouse corruption after controller use, or confused
menu state was reported. The detailed repeatable procedure remains in
`COEXISTENCE_TEST.md`. Interfaces that were not readily accessible remain later
compatibility coverage rather than blockers.

## Phase 6 prompt disposition

The occasional wrong prompt was not reproduced during the accepted switching
tests and is not tied to a validated, narrow patch point. Per scope, no
speculative UI-engine hook will be added. Prompt handling remains vanilla and
the rare reported edge case is documented as a possible future improvement if
a reproducible screen/action is supplied; it does not delay v1.0.

## Release logging defaults

The public candidate disables periodic diagnostics by default and retains only
concise startup, hook, failure, and safe-fallback logging. Missing-INI defaults
also disable diagnostics while enabling the accepted controller and mouse
paths. A dedicated missing-INI test verifies those zero-configuration defaults.
Development comparison profiles continue to enable periodic diagnostics.

The first run of that new test found that the configuration reader serialized
absent floating-point defaults with only four decimal places. Installed INI
values were unaffected, but a missing key rounded the verified game-deadzone
constant and the small zero-G scale. Fallback serialization now preserves nine
significant digits, and the test requires the fallback values to match the
accepted constants within floating-point tolerance.

The corrected quiet candidate passed the complete strict x86 build and test
suite. It was then deployed by replacing only the three Input Fix files and
launched normally through the EA App with the full installed runtime stack.
Texture Compatibility loaded all six packs; child PID `34324` reached the
rendered main menu. The payload installed the accepted controller transform,
chained to the existing `XINPUT1_3.dll`, installed every validated mouse call
site, and logged `Diagnostics disabled; validated input hooks remain active.`
The process produced 11 concise Input Fix lines and zero periodic summary lines
before exiting normally through the game's confirmation dialog at `14:08:26`.

Final quiet-candidate hashes after shutdown:

- `version.dll`:
  `F2A394F32D79CCCFFE8921DDD862E04F1D8262E1076E5DCE3115F52078E1B1AB`;
- `DeadSpaceCompleteInputFix.dll`:
  `FE709C1587BA164CE332216A26FC1AE230227D229B93B4BC49E8129F32BB3D75`;
- `DeadSpaceCompleteInputFix.ini`:
  `78E3592D2D67C04CF7CE805D020936A1E47BCC407932146FF593E8507659BAFD`.

Post-shutdown hashes for `xinput1_3.dll`, `dinput8.dll`, `dsound.dll`, and
`d3d9.dll` remained identical to the preserved hashes above. No other mod file
was modified.

## Local release-candidate package

`package.ps1` created the local four-file archive
`Dead_Space_Complete_Input_Fix_v1.0.0-rc1_Rama2120.zip`. An independent extract
audit confirmed that it contains exactly:

- `version.dll`;
- `DeadSpaceCompleteInputFix.dll`;
- `DeadSpaceCompleteInputFix.ini`; and
- `README.txt`.

All four extracted files are non-empty and match their tested source artifacts
byte for byte. The 142,048-byte archive has SHA-256
`A917F9DE0755E20A87C6E50413A304B630FF71A9FEBDFDAC42F734A9B602C015`,
which matches its generated `.sha256` sidecar. No game executable, game asset,
other mod, log, test binary, symbol, import library, or temporary staging file
is present. This is a local RC artifact only; it has not been uploaded.

The exact implementation, addresses, hashes, and rejected global interpolation
write are documented in `MOUSE_INVESTIGATION.md`.
