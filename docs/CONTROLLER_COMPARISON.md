# Controller comparison

All four profiles use the same `version.dll` and
`DeadSpaceCompleteInputFix.dll`. Only `DeadSpaceCompleteInputFix.ini` changes,
and the game must be fully closed before changing it because the profile is read
once during startup.

Profiles are generated under `build\ControllerComparison`:

1. `1-Vanilla`: the Input Fix remains loaded for diagnostics, but right-stick
   values are passed through unchanged.
2. `2-Current-Experimental-09-14`: exact previously tested 0.09 / 0.14 behavior,
   including its original circular outer range.
3. `3-Release-Candidate-11-14`: superseded 0.11 / 0.14 candidate with full
   square outer range preserved.
4. `4-Verified-Game-Response`: 0.11 physical cutoff followed by the exact
   inverse of Dead Space's verified 8689-unit radial deadzone and response
   curve, with full square outer range preserved.

## Final comparison results

| Profile | Drift | Perceived deadzone | Onset | Aiming, diagonals, full speed |
|---|---|---|---|---|
| 1. Vanilla | None | Very noticeable | No jump | Normal |
| 2. 0.09 / 0.14 | None | Much smaller horizontally; larger vertically | No jump | Normal |
| 3. 0.11 / 0.14 | None | Horizontal good; vertical still larger | No jump | Normal |
| 4. Verified game response | None | Horizontal and vertical nearly equal; vertical ever so slightly larger | No jump; low-speed movement good | Normal |

The residual vertical perception is not a reason to introduce per-axis tuning.
Runtime disassembly verified that Dead Space applies one radial 8689-unit
cutoff and nonlinear scale to both axes after `XInputGetState`. Profile 4
composes the mathematical inverse of that processing with the safe physical
cutoff, allowing the game-side result to begin continuously at zero.

Profile 4 is the accepted controller release candidate. Its small remaining
axis difference is downstream of the centralized, identical right-X/right-Y
input processing and is small enough that correcting it at the API boundary
would create a real directional asymmetry, risk distorted diagonals, and reduce
controller-to-controller predictability.

For each profile:

1. Close Dead Space completely.
2. Copy that profile's `DeadSpaceCompleteInputFix.ini` over the active INI in
   `C:\Program Files\EA Games\Dead Space` without changing any DLL or executable.
3. Launch Dead Space normally through the EA App and load the same save and
   camera location.
4. Leave the right stick untouched for ten seconds and watch for drift.
5. Ease the stick horizontally and vertically from center. Compare physical
   travel before camera onset and whether onset jumps.
6. Make slow small circles near the onset point. Check that diagonals feel even
   and that no direction catches or accelerates differently.
7. Aim at the same small environmental detail and make horizontal, vertical,
   and diagonal micro-adjustments.
8. Perform full horizontal, vertical, and diagonal sweeps, then release the stick
   from several directions. Check full speed and return-to-center drift.
9. Exit through the game's normal Exit to Windows command before applying the
   next profile.

Record for each run: untouched drift, perceived deadzone size, onset smoothness,
aiming precision, diagonal consistency, full-stick speed, and any menu or device
switching anomaly. Profile 4 is the active candidate; leave it active unless a
failure requires returning to an earlier profile.
