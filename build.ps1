[CmdletBinding()]
param(
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildRoot = Join-Path $projectRoot 'build'
$releaseRoot = Join-Path $buildRoot 'Release'
$testRoot = Join-Path $buildRoot 'tests'
$fixtureRoot = Join-Path $buildRoot 'fixtures'
$toolRoot = Join-Path $buildRoot 'tools'
$objectRoot = Join-Path $buildRoot 'obj'
$configDefaultsRoot = Join-Path $buildRoot 'tests\config-defaults'
$comparisonRoot = Join-Path $buildRoot 'ControllerComparison'
$mouseComparisonRoot = Join-Path $buildRoot 'MouseComparison'

$vcvarsCandidates = @(
    'C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars32.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat'
)
$vcvars = $vcvarsCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $vcvars) {
    throw 'A Visual C++ x86 build environment was not found.'
}

$environmentLines = & cmd.exe /d /c "call `"$vcvars`" >nul && set"
if ($LASTEXITCODE -ne 0) {
    throw 'vcvars32.bat failed.'
}
foreach ($line in $environmentLines) {
    $separator = $line.IndexOf('=')
    if ($separator -gt 0) {
        [Environment]::SetEnvironmentVariable(
            $line.Substring(0, $separator),
            $line.Substring($separator + 1),
            'Process')
    }
}
$vcPathLine = $environmentLines | Where-Object { $_ -clike 'PATH=*' } | Select-Object -First 1
if ($vcPathLine) {
    [Environment]::SetEnvironmentVariable('Path', $vcPathLine.Substring(5), 'Process')
}

foreach ($directory in @(
    $buildRoot, $releaseRoot, $testRoot, $fixtureRoot, $toolRoot,
    $objectRoot, $configDefaultsRoot, $comparisonRoot, $mouseComparisonRoot)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

function Invoke-Compiler {
    param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string[]]$Arguments
    )
    Write-Host "Building $Name..."
    & cl.exe @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE."
    }
}

function Assert-UniquePattern {
    param(
        [Parameter(Mandatory=$true)][string]$Scanner,
        [Parameter(Mandatory=$true)][string]$Image,
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$Pattern,
        [Parameter(Mandatory=$true)][string]$ExpectedOffset
    )
    $output = & $Scanner $Image $Pattern 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "$Name signature scan failed."
    }
    $text = @($output | ForEach-Object { $_.ToString() })
    if (@($text | Where-Object { $_ -eq $ExpectedOffset }).Count -ne 1 -or
        @($text | Where-Object { $_ -eq '1 match(es)' }).Count -ne 1) {
        throw "$Name signature was not unique at $ExpectedOffset. Output: $($text -join '; ')"
    }
}

$common = @(
    '/nologo', '/std:c++17', '/W4', '/WX', '/O2', '/MT', '/EHsc', '/utf-8',
    '/DWIN32', '/D_WINDOWS', '/DUNICODE', '/D_UNICODE',
    '/DWIN32_LEAN_AND_MEAN', '/DNOMINMAX', '/D_WIN32_WINNT=0x0601',
    ('/Fo' + $objectRoot + '\')
)

Push-Location $projectRoot
try {
    Invoke-Compiler 'version.dll bootstrap' ($common + @(
        '/LD', 'src\bootstrap\version_proxy.cpp',
        "/Fe:$releaseRoot\version.dll",
        "/Fd:$buildRoot\version.pdb",
        '/link', '/DEF:src\bootstrap\version.def', '/SUBSYSTEM:WINDOWS',
        "/PDB:$releaseRoot\version.pdb"
    ))

    Invoke-Compiler 'DeadSpaceCompleteInputFix.dll payload' ($common + @(
        '/LD',
        'src\payload\payload.cpp',
        'src\payload\logging.cpp',
        'src\payload\config.cpp',
        'src\payload\diagnostics.cpp',
        'src\payload\controller_transform.cpp',
        'src\payload\mouse_transform.cpp',
        'src\payload\xinput_hook.cpp',
        'src\payload\mouse_hook.cpp',
        "/Fe:$releaseRoot\DeadSpaceCompleteInputFix.dll",
        "/Fd:$buildRoot\payload.pdb",
        '/link', '/DEF:src\payload\payload.def', '/SUBSYSTEM:WINDOWS',
        "/PDB:$releaseRoot\DeadSpaceCompleteInputFix.pdb"
    ))

    Copy-Item -LiteralPath 'DeadSpaceCompleteInputFix.ini' -Destination $releaseRoot -Force

    foreach ($profile in @(
        @{ Source = 'tests\controller-profiles\1-vanilla.ini'; Directory = '1-Vanilla' },
        @{ Source = 'tests\controller-profiles\2-current-experimental-09-14.ini'; Directory = '2-Current-Experimental-09-14' },
        @{ Source = 'tests\controller-profiles\3-release-candidate-11-14.ini'; Directory = '3-Release-Candidate-11-14' },
        @{ Source = 'tests\controller-profiles\4-verified-game-response.ini'; Directory = '4-Verified-Game-Response' }
    )) {
        $profileRoot = Join-Path $comparisonRoot $profile.Directory
        New-Item -ItemType Directory -Force -Path $profileRoot | Out-Null
        Copy-Item -LiteralPath $profile.Source `
            -Destination (Join-Path $profileRoot 'DeadSpaceCompleteInputFix.ini') -Force
    }

    foreach ($profile in @(
        @{ Source = 'tests\mouse-profiles\1-vanilla-mouse.ini'; Directory = '1-Vanilla-Mouse' },
        @{ Source = 'tests\mouse-profiles\2-raw-camera-candidate.ini'; Directory = '2-Raw-Camera-Candidate' }
    )) {
        $profileRoot = Join-Path $mouseComparisonRoot $profile.Directory
        New-Item -ItemType Directory -Force -Path $profileRoot | Out-Null
        Copy-Item -LiteralPath $profile.Source `
            -Destination (Join-Path $profileRoot 'DeadSpaceCompleteInputFix.ini') -Force
    }

    Invoke-Compiler 'test XInput fixture' ($common + @(
        '/LD', 'tests\xinput_fixture.cpp',
        "/Fe:$fixtureRoot\xinput1_3.dll",
        '/link', '/DEF:tests\xinput_fixture.def', '/SUBSYSTEM:WINDOWS'
    ))

    Invoke-Compiler 'VERSION proxy smoke test' ($common + @(
        'tests\version_proxy_smoke.cpp',
        "/Fe:$testRoot\version_proxy_smoke.exe",
        '/link', '/SUBSYSTEM:CONSOLE'
    ))

    Invoke-Compiler 'XInput chain smoke test' ($common + @(
        'tests\xinput_chain_smoke.cpp',
        "/Fe:$testRoot\xinput_chain_smoke.exe",
        '/link', '/SUBSYSTEM:CONSOLE'
    ))

    Invoke-Compiler 'controller transform tests' ($common + @(
        'tests\controller_transform_tests.cpp',
        'src\payload\controller_transform.cpp',
        "/Fe:$testRoot\controller_transform_tests.exe",
        '/link', '/SUBSYSTEM:CONSOLE'
    ))

    Invoke-Compiler 'mouse transform tests' ($common + @(
        'tests\mouse_transform_tests.cpp',
        'src\payload\mouse_transform.cpp',
        "/Fe:$testRoot\mouse_transform_tests.exe",
        '/link', '/SUBSYSTEM:CONSOLE'
    ))

    Invoke-Compiler 'missing-INI configuration defaults test' ($common + @(
        'tests\config_defaults_tests.cpp',
        'src\payload\config.cpp',
        "/Fe:$configDefaultsRoot\config_defaults_tests.exe",
        '/link', '/SUBSYSTEM:CONSOLE'
    ))

    Invoke-Compiler 'runtime image dump utility' ($common + @(
        'tools\dump_runtime_image.cpp',
        "/Fe:$toolRoot\dump_runtime_image.exe",
        '/link', '/SUBSYSTEM:CONSOLE'
    ))

    Invoke-Compiler 'byte-pattern scan utility' ($common + @(
        'tools\scan_byte_pattern.cpp',
        "/Fe:$toolRoot\scan_byte_pattern.exe",
        '/link', '/SUBSYSTEM:CONSOLE'
    ))

    foreach ($file in @('version.dll', 'DeadSpaceCompleteInputFix.dll')) {
        Copy-Item -LiteralPath (Join-Path $releaseRoot $file) -Destination $testRoot -Force
    }
    Copy-Item -LiteralPath 'tests\transparent-test.ini' `
        -Destination (Join-Path $testRoot 'DeadSpaceCompleteInputFix.ini') -Force
    Copy-Item -LiteralPath (Join-Path $fixtureRoot 'xinput1_3.dll') -Destination $testRoot -Force

    if (-not $SkipTests) {
        $runtimeImage = Join-Path $projectRoot 'test-artifacts\DeadSpace-runtime-unpacked.exe'
        $patternScanner = Join-Path $toolRoot 'scan_byte_pattern.exe'
        if (Test-Path -LiteralPath $runtimeImage) {
            Write-Host 'Verifying unique EA runtime mouse signatures...'
            Assert-UniquePattern $patternScanner $runtimeImage 'mouse capture' `
                '8B 4E 24 8B 54 24 18 89 11 8B 46 24 8B 4C 24 1C 89 48 04 8B 44 24 20 8B 56 24 89 42 08 33 C0 39' `
                '0x004F7BD0'
            Assert-UniquePattern $patternScanner $runtimeImage 'standard camera call' `
                'E8 ?? ?? ?? ?? 8B 97 80 05 00 00 33 C0 8D 74 24 38 C6 44 24 38 00 89 44 24 40 89 44 24 44 89 44' `
                '0x000388E1'
            Assert-UniquePattern $patternScanner $runtimeImage 'zero-G primary call' `
                'E8 ?? ?? ?? ?? 8B 7C 24 30 0F 57 C9 B9 24 00 00 00 8D 74 24 40 F3 A5 F3 0F 10 44 24 40 F3 0F 5C' `
                '0x001447A2'
            Assert-UniquePattern $patternScanner $runtimeImage 'zero-G vertical call' `
                'E8 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? 0F 57 D2 83 EC 08 D9 54 24 04 8D 84 24 D8 00 00 00 D9 1C 24 E8 ?? ?? ?? ??' `
                '0x0014480C'
            Assert-UniquePattern $patternScanner $runtimeImage 'shared camera function' `
                '55 8B EC 83 E4 F0 ?? ?? ?? ?? ?? ?? 83 E8 00 53 8B 5D 08 56 57 ?? ?? ?? ?? ?? ?? 83 E8 01 ?? ??' `
                '0x00121260'
            Assert-UniquePattern $patternScanner $runtimeImage 'mouse sensitivity' `
                'A1 ?? ?? ?? ?? 0F B6 88 60 05 00 00 F3 0F 10 05 ?? ?? ?? ?? F3 0F 59 05 ?? ?? ?? ?? 8B 94 88 E8 04 00 00 F3 0F 58 05 ?? ?? ?? ?? F3 0F 11 82 1C 01 00 00 C3' `
                '0x0022B500'
            Assert-UniquePattern $patternScanner $runtimeImage 'mouse inversion' `
                'A2 ?? ?? ?? ?? 6A 00 B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 04 A2 ?? ?? ?? ?? 6A 00 B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 04 A2 ?? ?? ?? ??' `
                '0x0022AB70'
        }

        Write-Host 'Running controller transform tests...'
        & (Join-Path $testRoot 'controller_transform_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Controller transform tests failed.' }

        Write-Host 'Running mouse transform tests...'
        & (Join-Path $testRoot 'mouse_transform_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Mouse transform tests failed.' }

        $unexpectedDefaultsIni = Join-Path $configDefaultsRoot 'DeadSpaceCompleteInputFix.ini'
        if (Test-Path -LiteralPath $unexpectedDefaultsIni) {
            throw "Refusing an invalid missing-INI test directory: $unexpectedDefaultsIni exists."
        }
        Write-Host 'Running missing-INI configuration defaults test...'
        & (Join-Path $configDefaultsRoot 'config_defaults_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Missing-INI configuration defaults test failed.' }

        Write-Host 'Running VERSION proxy smoke test...'
        & (Join-Path $testRoot 'version_proxy_smoke.exe')
        if ($LASTEXITCODE -ne 0) { throw 'VERSION proxy smoke test failed.' }

        Write-Host 'Running XInput chain smoke test...'
        & (Join-Path $testRoot 'xinput_chain_smoke.exe')
        if ($LASTEXITCODE -ne 0) { throw 'XInput chain smoke test failed.' }

        Write-Host 'Running system XInput chain smoke test...'
        & (Join-Path $testRoot 'xinput_chain_smoke.exe') --system
        if ($LASTEXITCODE -ne 0) { throw 'System XInput chain smoke test failed.' }

        $missingPayloadRoot = Join-Path $testRoot 'missing-payload'
        New-Item -ItemType Directory -Force -Path $missingPayloadRoot | Out-Null
        Copy-Item -LiteralPath (Join-Path $testRoot 'version_proxy_smoke.exe') -Destination $missingPayloadRoot -Force
        Copy-Item -LiteralPath (Join-Path $releaseRoot 'version.dll') -Destination $missingPayloadRoot -Force
        $unexpectedPayload = Join-Path $missingPayloadRoot 'DeadSpaceCompleteInputFix.dll'
        if (Test-Path -LiteralPath $unexpectedPayload) {
            throw "Refusing an invalid missing-payload test directory: $unexpectedPayload exists."
        }
        Write-Host 'Running missing-payload fallback test...'
        & (Join-Path $missingPayloadRoot 'version_proxy_smoke.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Missing-payload fallback test failed.' }

        $affinityHost = Join-Path $missingPayloadRoot 'Dead Space.exe'
        Copy-Item -LiteralPath (Join-Path $testRoot 'version_proxy_smoke.exe') `
            -Destination $affinityHost -Force
        Write-Host 'Running standalone CPU-safety test...'
        & $affinityHost --affinity
        if ($LASTEXITCODE -ne 0) { throw 'Standalone CPU-safety test failed.' }
    }

    Write-Host "Build complete: $releaseRoot"
}
finally {
    Pop-Location
}
