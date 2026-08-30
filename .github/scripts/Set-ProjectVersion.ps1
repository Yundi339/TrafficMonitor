[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Version,
    [string]$HeaderPath = '',
    [string]$ResourcePath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($Version -notmatch '^\d+\.\d{1,2}(?:\.\d+)?$') {
    throw "Invalid version '$Version'. Expected major.minor or major.minor.build; minor must use at most two digits."
}

$displayComponents = $Version.Split('.')
$major = [uint32]::Parse($displayComponents[0], [Globalization.CultureInfo]::InvariantCulture)
$build = if ($displayComponents.Count -eq 3) {
    [uint32]::Parse($displayComponents[2], [Globalization.CultureInfo]::InvariantCulture)
} else {
    0
}
if ($major -gt 65535 -or $build -gt 65535) {
    throw 'Major and build version components must not exceed 65535.'
}

$minorText = $displayComponents[1].PadLeft(2, '0')
$components = @($major, [uint32]::Parse($minorText.Substring(0, 1)), [uint32]::Parse($minorText.Substring(1, 1)), $build)

$numericVersion = ($components | ForEach-Object { $_.ToString([Globalization.CultureInfo]::InvariantCulture) }) -join ','
$stringVersion = ($components | ForEach-Object { $_.ToString([Globalization.CultureInfo]::InvariantCulture) }) -join '.'

if ([string]::IsNullOrWhiteSpace($HeaderPath)) {
    $HeaderPath = Join-Path $PSScriptRoot '..\..\TrafficMonitor\stdafx.h'
}
if ([string]::IsNullOrWhiteSpace($ResourcePath)) {
    $ResourcePath = Join-Path $PSScriptRoot '..\..\TrafficMonitor\TrafficMonitor.rc'
}
$HeaderPath = [IO.Path]::GetFullPath($HeaderPath)
$ResourcePath = [IO.Path]::GetFullPath($ResourcePath)

function Replace-ExactlyOnce([string]$Text, [string]$Pattern, [string]$Replacement, [string]$Label) {
    $regex = [regex]::new($Pattern, [Text.RegularExpressions.RegexOptions]::Multiline)
    $matches = $regex.Matches($Text)
    if ($matches.Count -ne 1) {
        throw "Expected one $Label field, found $($matches.Count)."
    }
    return $regex.Replace($Text, $Replacement)
}

$headerBytes = [IO.File]::ReadAllBytes($HeaderPath)
$headerHasBom = $headerBytes.Length -ge 3 -and $headerBytes[0] -eq 0xEF -and $headerBytes[1] -eq 0xBB -and $headerBytes[2] -eq 0xBF
$header = [IO.File]::ReadAllText($HeaderPath, [Text.Encoding]::UTF8)
$header = Replace-ExactlyOnce $header '#define\s+VERSION\s+L"[^"]+"' "#define VERSION L`"$Version`"" 'VERSION'
[IO.File]::WriteAllText($HeaderPath, $header, [Text.UTF8Encoding]::new($headerHasBom))

$resourceBytes = [IO.File]::ReadAllBytes($ResourcePath)
if ($resourceBytes.Length -lt 2 -or $resourceBytes[0] -ne 0xFF -or $resourceBytes[1] -ne 0xFE) {
    throw "Resource file '$ResourcePath' is expected to be UTF-16LE."
}
$resource = [IO.File]::ReadAllText($ResourcePath, [Text.Encoding]::Unicode)
$resource = Replace-ExactlyOnce $resource '^ FILEVERSION\s+\d+,\d+,\d+,\d+\s*$' " FILEVERSION $numericVersion" 'FILEVERSION'
$resource = Replace-ExactlyOnce $resource '^ PRODUCTVERSION\s+\d+,\d+,\d+,\d+\s*$' " PRODUCTVERSION $numericVersion" 'PRODUCTVERSION'
$resource = Replace-ExactlyOnce $resource '^\s*VALUE "FileVersion", "[^"]+"\s*$' "            VALUE `"FileVersion`", `"$stringVersion`"" 'FileVersion string'
$resource = Replace-ExactlyOnce $resource '^\s*VALUE "ProductVersion", "[^"]+"\s*$' "            VALUE `"ProductVersion`", `"$stringVersion`"" 'ProductVersion string'
[IO.File]::WriteAllText($ResourcePath, $resource, [Text.Encoding]::Unicode)

if (![string]::IsNullOrWhiteSpace($env:GITHUB_ENV)) {
    "TRAFFICMONITOR_PE_VERSION=$stringVersion" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
}
Write-Output $Version
