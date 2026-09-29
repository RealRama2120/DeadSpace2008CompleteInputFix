# Controller tuning decision

## Captured center behavior

Three real EA App sessions exposed the right stick through two independent
XInput chains:

| Runtime path | Longest stable raw center | Radial magnitude | XInput range |
|---|---:|---:|---:|
| Stability Patch chain, transform disabled | `(2308, -1160)` | 2583 | 7.88% |
| Stability Patch chain, 0.09 / 0.14 experiment | `(2809, 3)` | 2809 | 8.57% |
| Direct Windows system XInput, clean stack | `(1797, -931)` | 2024 | 6.18% |

The experimental run's five-second idle window reached `(2809, 254)`, radial
magnitude 2820, or 8.61%. Other long plateaus after stick movement included
`(1260, -414)` at 4.05%, `(2121, -676)` at 6.79%, and `(1797, 218)` at 5.52%.

These are not shaped like ordinary sample-to-sample software noise. Each major
state remained exactly or nearly constant for many consecutive two-second log
windows, but the resting vector changed between launches and after physical
movement. That pattern is most consistent with controller sensor/firmware
centering plus mechanical return position. It is observable through the real
Windows `XINPUT1_3.dll`, so it is not created by the Stability Patch or the
Input Fix. Steam was running during development, but the clean game launched
through the EA App and chained directly to system XInput; current evidence does
not identify Steam Input as the source. A driver-level or firmware calibration
layer cannot be separated from the physical device using XInput values alone.

## Hardware cutoff

The proven 0.09 physical cutoff corresponds to about 2949 raw units. It cleared
the worst captured idle state by only 129 units, or 0.39% of full XInput range.
That is too little margin for a default intended for other controllers.

The retained 0.11 cutoff corresponds to about 3604 raw units and leaves 784 raw
units, or 2.39% of full range, above the worst measured idle state. It adds only
two percentage points of physical cutoff compared with the drift-free 0.09
test. This cutoff handles hardware rest noise only; it is intentionally separate
from compensation for Dead Space's much larger internal deadzone.

Automatic center calibration was considered but rejected for this release
candidate. The logs show that the controller can return to different stable
centers after being moved. A startup or online estimator could therefore learn
a held stick or chase a changing mechanical rest point, creating asymmetric
response or a delayed camera shift. A fixed radial cutoff is deterministic,
slot-independent, and fails conservatively.

## Verified Dead Space response

A read-only dump of the unpacked canonical EA process made the runtime code
available for static inspection. It found one centralized `XInputGetState` call
and the right-stick processing block at `0x008F70A4`-`0x008F7158`. The game:

- reads right X and Y from the returned XInput state;
- computes their radial magnitude;
- zeros both axes when magnitude is at or below 8689 raw units; and
- above that threshold, scales both raw components by
  `(clamp(magnitude, 32767) - 8689) / 24078`.

The normalized internal threshold is `d = 8689 / 32767 = 0.265175`. For an
API-facing radial magnitude `r < 1`, the resulting camera-side magnitude before
later sensitivity processing is:

`t = r * (r - d) / (1 - d)`

The earlier 0.14 anti-deadzone was therefore always below the game's threshold.
It improved onset only by advancing input toward that threshold, which explains
why the tester continued to feel more vertical travel in both earlier enabled
profiles.

## Candidate response shape

After the 0.11 hardware cutoff, the physical range is rescaled to a desired
post-game magnitude `t`. The fix sends the inverse game-facing magnitude:

`r = (d + sqrt(d*d + 4*(1-d)*t)) / 2`

The output remains radial:

- input at or below the physical cutoff becomes zero;
- at onset, the fix sends exactly `d`, which the game maps back to zero;
- immediately above onset, the composed game output follows `t` continuously;
- response is monotonic and direction is preserved apart from integer
  quantization; and
- target magnitudes at or above one preserve the original square XInput axes.

The initial experiment rescaled every direction to a unit-circle outer radius.
That preserved cardinal full speed but could reduce both axes at the extreme
diagonal corner from 32767 to about 23170. The current candidate instead scales
to the actual edge of XInput's square axis range in the current direction. Full
cardinal, diagonal, and mixed-direction boundary values are therefore preserved.
The comparison profiles retain both earlier anti-deadzone behaviors exactly.

Left-stick values remain untouched. The single recovered XInput path and exact
invertible downstream transform remove the present justification for a deeper
camera hook.

## Objective acceptance checks

The automated suite now verifies:

- disabled mode is bit-transparent;
- the worst captured idle vector is suppressed by the release profile;
- the first surviving physical input clears the game's internal threshold;
- the composed first raw step has no camera-side jump;
- the composed midpoint is linear;
- horizontal and vertical composition are identical;
- an exhaustive 11-by-11 square grid reproduces the desired post-game response
  within integer tolerance;
- cardinal response is monotonic;
- directional response is odd-symmetric;
- full cardinal, diagonal, and mixed-direction square boundaries are preserved;
  and
- both prior experimental behaviors remain exactly reproducible in their
  dedicated comparison modes.

The verified game-response profile passed subjective gameplay comparison with
no drift, no onset jump, good low-speed movement, normal aiming and diagonals,
and normal full-stick speed. Horizontal and vertical response felt nearly
equal, with only an ever-so-slightly larger perceived vertical deadzone.

Disassembly of the post-deadzone conversion also showed identical right-X and
right-Y math and the same output scaling. The small residual difference is
therefore not another axis-specific deadzone in the centralized input path.
Profile 4 is promoted to the controller release candidate without a vertical
boost: introducing unequal API values would distort direction and diagonals to
counter a downstream camera-sensitivity or perception difference.
