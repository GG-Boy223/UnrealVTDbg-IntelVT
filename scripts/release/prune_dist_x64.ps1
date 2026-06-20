param(
    [string]$DistRoot = "dist\UnrealVTDbg-x64-20260613"
)

$ErrorActionPreference = "Stop"

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

function Test-InDistRoot
{
    param(
        [string]$Root,
        [string]$Path
    )

    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $pathFull = [System.IO.Path]::GetFullPath($Path)
    return $pathFull.StartsWith($rootFull, [System.StringComparison]::OrdinalIgnoreCase)
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
$deleteFiles = New-Object System.Collections.Generic.HashSet[string]

Get-ChildItem -LiteralPath $rootPath -Recurse -File | ForEach-Object {
    $machine = Get-PeMachine -Path $_.FullName
    if ($null -ne $machine -and $machine -ne 0x8664)
    {
        [void]$deleteFiles.Add($_.FullName)
    }

    if ($_.Name -match "(?i)(^|[-.])i386([-.]|$)")
    {
        [void]$deleteFiles.Add($_.FullName)
    }
}

$deleteDirs = Get-ChildItem -LiteralPath $rootPath -Recurse -Directory |
    Where-Object { Test-X86NamedPath -Path $_.FullName } |
    Sort-Object FullName -Descending

foreach ($dir in $deleteDirs)
{
    if (-not (Test-InDistRoot -Root $rootPath -Path $dir.FullName))
    {
        throw "refusing to delete outside dist: $($dir.FullName)"
    }

    Remove-Item -LiteralPath $dir.FullName -Recurse -Force
    Write-Output "deleted directory: $($dir.FullName)"
}

foreach ($file in $deleteFiles)
{
    if (-not (Test-Path -LiteralPath $file))
    {
        continue
    }

    if (-not (Test-InDistRoot -Root $rootPath -Path $file))
    {
        throw "refusing to delete outside dist: $file"
    }

    Remove-Item -LiteralPath $file -Force
    Write-Output "deleted file: $file"
}
