# Changelog

No original changelog file was present in the local project. This concise
record was created during the private GitHub backup on 2026-09-29 and includes
only versions and validation supported by local evidence.

## v1.0.0-rc1 - 2026-08-31

- Added a standalone x86 `version.dll` bootstrap with transparent Windows
  VERSION forwarding and safe behavior when the payload is unavailable.
- Added the uniquely named `DeadSpaceCompleteInputFix.dll` payload.
- Replaced the large controller right-stick camera dead zone with a measured,
  smooth radial response while preserving full-stick speed.
- Added consistent raw relative movement for the standard mouse camera while
  leaving menu and DirectInput behavior intact.
- Preserved normal switching between mouse/keyboard and controller input.
- Added fail-closed signature handling for unsupported mouse-camera builds.
- Verified normal EA App launch and shutdown on Dead Space 1.0.0.222, standalone
  operation, safe fallback, and coexistence with the other installed runtime
  proxies used during development.
- Post-release gameplay validation on 2026-09-03 covered multiple zero-G levels
  with no reported input or camera issues.

No other release version was found locally during the backup audit.
