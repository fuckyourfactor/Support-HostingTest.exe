param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$output = Join-Path $projectRoot "dist\$Configuration\Exact"
$exe = Join-Path $projectRoot 'original\HostingTest.exe'
$assets = Join-Path $projectRoot 'assets'
$metadata = Join-Path $projectRoot 'analysis\pe_metadata.json'
$expected = 'dfd183ccccf82d684ed6e4952aa071c251419024c2bbb78755563009f9f62c81'

if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "Original executable not found: $exe"
}
if (-not (Test-Path -LiteralPath $assets -PathType Container)) {
    throw "Extracted assets directory not found: $assets"
}
if (-not (Test-Path -LiteralPath $metadata -PathType Leaf)) {
    throw "PE metadata not found: $metadata"
}

$actual = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actual -ne $expected) {
    throw "Original executable hash mismatch: $actual"
}

New-Item -ItemType Directory -Force -Path $output | Out-Null
$copiedAssets = Join-Path $output 'assets'
if (Test-Path -LiteralPath $copiedAssets) {
    Remove-Item -LiteralPath $copiedAssets -Recurse -Force
}
Copy-Item -LiteralPath $exe -Destination (Join-Path $output 'HostingTest.exe') -Force
Copy-Item -LiteralPath $assets -Destination $copiedAssets -Recurse -Force
Copy-Item -LiteralPath $metadata -Destination (Join-Path $output 'pe_metadata.json') -Force

$outputExe = Join-Path $output 'HostingTest.exe'
$outputHash = (Get-FileHash -LiteralPath $outputExe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($outputHash -ne $expected) {
    throw "Output binary validation failed: $outputHash"
}

Write-Host "Exact original executable preserved at $outputExe"
Write-Host "SHA-256: $outputHash"
