# Phase 5 mixed-input test

Status: passed on 2026-08-31. I (Rama2120) had already exercised both input paths
and reported that all switching behavior seemed normal. No stuck controller,
corrupted mouse path, or menu-state problem was reported.

The hardened release candidate is already installed in the canonical EA copy.
Leave its files and settings unchanged and launch Dead Space normally through
the EA App with the controller connected.

Use an ordinary gameplay area; zero-G, the store, bench, and other unavailable
interfaces are not required for this pass.

## Test sequence

1. Use the controller for about 15 seconds: walk, turn slowly and quickly, aim,
   and use one normal action.
2. Put the controller down. Use the mouse to turn slowly and quickly, aim, then
   use the keyboard to walk and perform one normal action.
3. Immediately return to the controller. Confirm the right stick responds at
   once, still has no drift or onset jump, and reaches normal full speed.
4. Repeat controller -> mouse/keyboard -> controller five times, including a few
   quick switches rather than waiting between devices.
5. Pause with the controller, move through one or two menu items, resume, move
   the mouse, then return to the controller and pause/resume once more.
6. While safely standing still, disconnect the controller. Confirm mouse and
   keyboard still work. Reconnect the controller, wait up to five seconds, and
   confirm movement, aiming, and the right stick work again without restarting.
7. Exit normally through the game's `Exit to Windows` option.

## Report back

Please report only these observations:

- Did the right stick ever stop responding or change feel after mouse use?
- Did mouse movement ever stop responding or change after controller use?
- Did disconnect/reconnect recover without restarting?
- Did pause/menu input become stuck or confused?
- Did you notice a clearly wrong input prompt while switching? If yes, name the
  exact screen/action and which prompt appeared.
- Did the game exit normally?

If an optional action or interface is inconvenient to reach, skip it and say so.
