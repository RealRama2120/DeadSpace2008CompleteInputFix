# Building Dead Space Complete Input Fix

The newest locally evidenced version is **v1.0.0-rc1**. Its browsable C++
source, tests, configuration, and build scripts are at the repository root.

## Requirements

- Windows
- PowerShell
- An x86 Visual C++ build environment from one of the toolchains recognized by
  `build.ps1`:
  - Visual Studio 2019 Build Tools
  - Visual Studio 2022 Community
  - Visual Studio 2022 Build Tools
- The Desktop development with C++ workload and a compatible Windows SDK

No third-party source library is bundled or downloaded by the build.

## Build and test

From a PowerShell prompt in the repository root:

```powershell
.\build.ps1
```

This builds the x86 `version.dll` bootstrap, the uniquely named payload, test
fixtures, smoke tests, transform tests, and diagnostic tools. Output is written
under `build\`, with installable binaries in `build\Release`.

The normal test run performs every self-contained automated test. It also runs
the EA executable signature checks when this local-only file is present:

```text
test-artifacts\DeadSpace-runtime-unpacked.exe
```

That derived game executable is copyrighted game material and is deliberately
excluded from this repository. Its absence causes only those signature checks
to be skipped. Use `-SkipTests` when only a compile is wanted:

```powershell
.\build.ps1 -SkipTests
```

## Create a package

After building, create a new audited package with:

```powershell
.\package.ps1 -Version 1.0.0-rc1
```

The packager accepts only `version.dll`, `DeadSpaceCompleteInputFix.dll`,
`DeadSpaceCompleteInputFix.ini`, and the end-user `README.txt`. It validates the
staged hashes and ZIP entries, then writes the ZIP and its SHA-256 sidecar under
`dist\`. It refuses to overwrite an existing release artifact.

The original locally preserved v1.0.0-rc1 package is in
`versions/v1.0.0-rc1/`. Rebuilding is not expected to reproduce that ZIP's
byte-level hash because compiler and archive metadata may differ.
