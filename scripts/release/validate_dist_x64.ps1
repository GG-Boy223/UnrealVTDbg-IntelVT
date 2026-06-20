param(
    [string]$DistRoot = "dist\UnrealVTDbg-x64-20260613",
    [string]$ManifestName = "PACKAGE-MANIFEST.txt"
)

$ErrorActionPreference = "Stop"

function Get-RelativePath
{
    param(
        [string]$Root,
        [string]$Path
    )

    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\')
    $pathFull = [System.IO.Path]::GetFullPath($Path)
    return $pathFull.Substring($rootFull.Length + 1)
}

function Get-PeMachine
{
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 0x40)
    {
        return $null
    }

    if ($bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A)
    {
        return $null
    }

    $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
    if ($peOffset -lt 0 -or $peOffset + 6 -gt $bytes.Length)
    {
        return $null
    }

    if ($bytes[$peOffset] -ne 0x50 -or
        $bytes[$peOffset + 1] -ne 0x45 -or
        $bytes[$peOffset + 2] -ne 0x00 -or
        $bytes[$peOffset + 3] -ne 0x00)
    {
        return $null
    }

    return [BitConverter]::ToUInt16($bytes, $peOffset + 4)
}

function Test-X86NamedPath
{
    param([string]$Path)

    return (
        $Path -match "(?i)(\\|/)(x32|i386|win32|clibs32|dbk32)(\\|/|$)" -or
        $Path -match "(?i)(\\|/)dlls(\\|/)32(\\|/|$)" -or
        [System.IO.Path]::GetFileName($Path) -match "(?i)(^|[-.])i386([-.]|$)"
    )
}

$rootPath = (Resolve-Path $DistRoot).Path
$manifestPath = Join-Path $rootPath $ManifestName
if (-not (Test-Path -LiteralPath $manifestPath))
{
    throw "manifest missing: $manifestPath"
}

$allFiles = Get-ChildItem -LiteralPath $rootPath -Recurse -File |
    Where-Object {
        $_.Name -ne $ManifestName -and
        $_.Extension -notin @(".log", ".tmp")
    } |
    Sort-Object FullName

$badPe = New-Object System.Collections.Generic.List[string]
foreach ($file in $allFiles)
{
    $machine = Get-PeMachine -Path $file.FullName
    if ($null -ne $machine -and $machine -ne 0x8664)
    {
        $badPe.Add(("{0} machine=0x{1:X4}" -f (Get-RelativePath -Root $rootPath -Path $file.FullName), $machine))
    }
}

$badNames = Get-ChildItem -LiteralPath $rootPath -Recurse |
    Where-Object { Test-X86NamedPath -Path $_.FullName } |
    ForEach-Object {
        Get-RelativePath -Root $rootPath -Path $_.FullName
    }

$manifestEntries = @{}
Get-Content -LiteralPath $manifestPath | ForEach-Object {
    if ($_ -match "^[^#].*`t[0-9]+`tSHA256:")
    {
        $parts = $_ -split "`t"
        $manifestEntries[$parts[0]] = @{
            Size = [Int64]$parts[1]
            Hash = $parts[2].Substring("SHA256:".Length)
        }
    }
}

$manifestErrors = New-Object System.Collections.Generic.List[string]
foreach ($file in $allFiles)
{
    $relativePath = Get-RelativePath -Root $rootPath -Path $file.FullName
    if (-not $manifestEntries.ContainsKey($relativePath))
    {
        $manifestErrors.Add("missing: $relativePath")
        continue
    }

    $entry = $manifestEntries[$relativePath]
    if ($entry.Size -ne $file.Length)
    {
        $manifestErrors.Add("size mismatch: $relativePath")
    }

    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    if ($entry.Hash -ne $hash)
    {
        $manifestErrors.Add("hash mismatch: $relativePath")
    }
}

foreach ($entryPath in $manifestEntries.Keys)
{
    if (-not (Test-Path -LiteralPath (Join-Path $rootPath $entryPath)))
    {
        $manifestErrors.Add("stale: $entryPath")
    }
}

if ($badPe.Count -gt 0 -or $badNames.Count -gt 0 -or $manifestErrors.Count -gt 0)
{
    if ($badPe.Count -gt 0)
    {
        Write-Output "non-x64 PE files:"
        $badPe | ForEach-Object { Write-Output "  $_" }
    }

    if ($badNames.Count -gt 0)
    {
        Write-Output "x86-named leftovers:"
        $badNames | ForEach-Object { Write-Output "  $_" }
    }

    if ($manifestErrors.Count -gt 0)
    {
        Write-Output "manifest errors:"
        $manifestErrors | ForEach-Object { Write-Output "  $_" }
    }

    throw "x64 dist validation failed"
}

Write-Output ("x64 dist validation passed: {0} files" -f $allFiles.Count)
