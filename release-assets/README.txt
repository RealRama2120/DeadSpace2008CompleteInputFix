DEAD SPACE COMPLETE INPUT FIX
Release candidate 1 by Rama2120

WHAT IT DOES

- Replaces Dead Space's large right-stick camera deadzone with a measured,
  smooth radial response while preserving full-stick speed.
- Gives the standard mouse camera direct, consistent relative movement without
  changing the menu cursor.
- Allows mouse/keyboard and controller input to be switched normally without a
  launcher, hotkey, or external configuration program.

INSTALLATION

1. Find the Dead Space (2008) installation folder containing Dead Space.exe.
2. Extract all four files from this archive directly into that folder.
3. Launch Dead Space normally through the EA App or Steam.

Do not overwrite an unrelated version.dll from another mod. If one is already
present, stop and check compatibility first. The separately named
DeadSpaceCompleteInputFix.dll must remain beside this mod's version.dll.

The supplied INI contains the recommended defaults. Normal users do not need to
edit it. The same controller and mouse fixes also enable with their recommended
defaults if the INI is missing.

UNINSTALLATION

Delete only these files from the Dead Space folder:

- version.dll
- DeadSpaceCompleteInputFix.dll
- DeadSpaceCompleteInputFix.ini

The mod may also create DeadSpaceCompleteInputFix.log and
DeadSpaceCompleteInputFix.bootstrap.log. They can be deleted after the game is
closed. Do not delete similarly named files belonging to other mods.

COMPATIBILITY AND SAFETY

- Fully runtime-tested on the canonical EA App executable build 1.0.0.222.
- Standalone operation and coexistence with Rama2120's Stability Patch, Texture
  Compatibility, 4GB runtime component, and ReShade stack were tested.
- Steam uses the same normal installation layout but has not yet received a
  separate runtime-validation pass.
- Unsupported or changed mouse signatures fail closed and leave mouse camera
  behavior vanilla. The bootstrap also forwards Windows VERSION functions and
  allows the game to start safely if the payload cannot be loaded.
- Controller and mouse/keyboard coexistence passed gameplay switching tests.

KNOWN LIMITATIONS

- Standard mouse camera behavior is gameplay-verified. Zero-G retains the
  recovered reference scaling but was not separately subjectively tested; it is
  deferred to later gameplay and compatibility feedback.
- Dead Space's occasional incorrect input-prompt edge case remains vanilla. It
  was not reproduced reliably enough to justify a fragile UI hook.
- Controller button remapping is outside this mod's scope.

SUPPORT LOGGING

Normal logging is concise. If support requests periodic controller and mouse
summaries, set DiagnosticsEnabled=1 in DeadSpaceCompleteInputFix.ini, reproduce
the issue briefly, exit the game normally, then restore DiagnosticsEnabled=0.

FILES IN THIS ARCHIVE

- version.dll                         bootstrap and Windows VERSION forwarding
- DeadSpaceCompleteInputFix.dll       input-fix payload
- DeadSpaceCompleteInputFix.ini       recommended zero-configuration defaults
- README.txt                          this document
