[CmdletBinding()]
param(
    [string]$RequestedVersion = '',
    [string]$EventName = '',
    [string]$RefName = '',
    [int]$RunNumber = 0,
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

    $components = [Collections.Generic.List[string]]::new()
    $components.AddRange([string[]]$match.Groups['version'].Value.Split('.'))
    if ($components.Count -eq 3) {
        $components.RemoveAt(2)
    }
    $components.Add($RunNumber.ToString([Globalization.CultureInfo]::InvariantCulture))
    $version = $components -join '.'
}

Assert-Version $version
Write-Output $version
