[CmdletBinding()]
param(
    [string]$RequestedVersion = '',
    [string]$EventName = '',
    [string]$RefName = '',
    [uint64]$RunNumber = 0,
    [string]$HeaderPath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-Version([string]$Version) {
    if ($Version -notmatch '^\d+\.\d{1,2}(?:\.\d+)?$') {
        throw "Invalid version '$Version'. Expected major.minor or major.minor.build; minor must use at most two digits."
    }

    $components = $Version.Split('.')
    foreach ($component in @($components[0]) + $(if ($components.Count -eq 3) { @($components[2]) } else { @() })) {
        $numericComponent = [uint32]::Parse($component, [Globalization.CultureInfo]::InvariantCulture)
        if ($numericComponent -gt 65535) {
            throw "Version component '$component' exceeds the PE version limit of 65535."
        }
    }
}

if ([string]::IsNullOrWhiteSpace($HeaderPath)) {
    $HeaderPath = Join-Path $PSScriptRoot '..\..\TrafficMonitor\stdafx.h'
}
$HeaderPath = [IO.Path]::GetFullPath($HeaderPath)

$version = $RequestedVersion.Trim()
if ([string]::IsNullOrWhiteSpace($version) -and $EventName -eq 'push' -and $RefName -match '^[vV](.+)$') {
    $version = $Matches[1]
}

if ([string]::IsNullOrWhiteSpace($version)) {
    if ($RunNumber -le 0) {
        throw 'RunNumber must be positive when an automatic version is requested.'
    }

    $header = [IO.File]::ReadAllText($HeaderPath, [Text.Encoding]::UTF8)
    $match = [regex]::Match($header, '#define\s+VERSION\s+L"(?<version>\d+\.\d{1,2}(?:\.\d+)?)"')
    if (!$match.Success) {
        throw "Could not read the base version from '$HeaderPath'."
    }

    $baseVersion = $match.Groups['version'].Value
    Assert-Version $baseVersion

    $components = [Collections.Generic.List[string]]::new()
    $components.AddRange([string[]]$baseVersion.Split('.'))
    if ($components.Count -eq 3) {
        $components.RemoveAt(2)
    }

    # Preserve the complete run number across the PE major/build components.
    # Build values are 1..65535; after that range is exhausted, major advances.
    $baseMajor = [uint64]::Parse($components[0], [Globalization.CultureInfo]::InvariantCulture)
    $buildRange = [uint64]65535
    $maximumRunNumber = ([uint64]65536 - $baseMajor) * $buildRange
    if ($RunNumber -gt $maximumRunNumber) {
        throw "RunNumber '$RunNumber' cannot be represented without exceeding the PE version limit."
    }

    $remainder = [long]0
    $majorIncrement = [Math]::DivRem([long]($RunNumber - 1), [long]$buildRange, [ref]$remainder)
    $major = $baseMajor + [uint64]$majorIncrement
    $build = [uint64]$remainder + 1
    $components[0] = $major.ToString([Globalization.CultureInfo]::InvariantCulture)
    $components.Add($build.ToString([Globalization.CultureInfo]::InvariantCulture))
    $version = $components -join '.'
}

Assert-Version $version
Write-Output $version
