# Mouse release-candidate comparison

Both profiles keep the accepted controller Profile 4 active. Only the mouse
camera experiment changes.

## Profiles

1. `1-Vanilla-Mouse` leaves mouse behavior entirely vanilla.
2. `2-Raw-Camera-Candidate` enables raw standard and zero-G camera deltas while
   leaving DirectInput polling and menus vanilla.

The normal development INI currently matches profile 2.

## Procedure

For each profile, close Dead Space normally, copy that profile's
`DeadSpaceCompleteInputFix.ini` to the game directory, then launch through the
EA App. Do not replace either DLL between profiles.

Use the same in-game sensitivity and test:

- untouched drift;
- the first response to very slow movement;
- equal physical-distance sweeps at slow and fast speeds;
- horizontal, vertical, and diagonal movement;
- aiming;
- switching to the controller while the mouse is still; and
- an explicit zero-G section.

The menu cursor is deliberately excluded as a decision criterion because its
fast/disconnected response is vanilla and neither profile transforms it.
