[CmdletBinding()]
param(
    [ValidatePattern('^[0-9A-Za-z.-]+$')]
    [string]$Version = '1.0.0-rc1'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$releaseRoot = Join-Path $projectRoot 'build\Release'
$assetRoot = Join-Path $projectRoot 'release-assets'
$distRoot = Join-Path $projectRoot 'dist'
$archiveName = "Dead_Space_Complete_Input_Fix_v${Version}_Rama2120.zip"
$archivePath = Join-Path $distRoot $archiveName
$checksumPath = "$archivePath.sha256"
$expected = @(
    'DeadSpaceCompleteInputFix.dll',
    'DeadSpaceCompleteInputFix.ini',
    'README.txt',
    'version.dll'
)
$sources = @{
    'DeadSpaceCompleteInputFix.dll' = Join-Path $releaseRoot 'DeadSpaceCompleteInputFix.dll'
    'DeadSpaceCompleteInputFix.ini' = Join-Path $releaseRoot 'DeadSpaceCompleteInputFix.ini'
    'README.txt' = Join-Path $assetRoot 'README.txt'
    'version.dll' = Join-Path $releaseRoot 'version.dll'
}

foreach ($name in $expected) {
    if (-not (Test-Path -LiteralPath $sources[$name] -PathType Leaf)) {
        throw "Required release file is missing: $($sources[$name])"
    }
}

New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
if ((Test-Path -LiteralPath $archivePath) -or (Test-Path -LiteralPath $checksumPath)) {
    throw "Refusing to replace an existing release artifact: $archiveName"
}

$temporaryBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$stagingRoot = Join-Path $temporaryBase ("dcif-package-" + [Guid]::NewGuid().ToString('N'))
$resolvedStaging = [System.IO.Path]::GetFullPath($stagingRoot)
if (-not $resolvedStaging.StartsWith($temporaryBase, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe staging path: $resolvedStaging"
}

try {
    New-Item -ItemType Directory -Path $resolvedStaging | Out-Null
    foreach ($name in $expected) {
        $destination = Join-Path $resolvedStaging $name
        Copy-Item -LiteralPath $sources[$name] -Destination $destination
        $sourceHash = (Get-FileHash -LiteralPath $sources[$name] -Algorithm SHA256).Hash
        $stagedHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        if ($sourceHash -ne $stagedHash) {
            throw "Staged hash mismatch for $name"
        }
    }

    Compress-Archive -LiteralPath ($expected | ForEach-Object {
        Join-Path $resolvedStaging $_
    }) -DestinationPath $archivePath -CompressionLevel Optimal

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        $actual = @($zip.Entries | ForEach-Object { $_.FullName } | Sort-Object)
        $wanted = @($expected | Sort-Object)
        if (($actual -join "`n") -cne ($wanted -join "`n")) {
            throw "Archive allowlist mismatch. Actual: $($actual -join ', ')"
        }
        foreach ($entry in $zip.Entries) {
            if ($entry.Length -le 0) {
                throw "Archive contains an empty file: $($entry.FullName)"
            }
        }
    }
    finally {
        $zip.Dispose()
    }

    $archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
    [System.IO.File]::WriteAllText(
        $checksumPath,
        "$archiveHash  $archiveName`r`n",
        [System.Text.UTF8Encoding]::new($false))
    Write-Host "Package: $archivePath"
    Write-Host "SHA-256: $archiveHash"
}
finally {
    if (Test-Path -LiteralPath $resolvedStaging) {
        $checked = [System.IO.Path]::GetFullPath($resolvedStaging)
        if ($checked.StartsWith($temporaryBase, [StringComparison]::OrdinalIgnoreCase) -and
            (Split-Path -Leaf $checked).StartsWith('dcif-package-', [StringComparison]::Ordinal)) {
            Remove-Item -LiteralPath $checked -Recurse -Force
        }
        else {
            throw "Refusing to remove unsafe staging path: $checked"
        }
    }
}
