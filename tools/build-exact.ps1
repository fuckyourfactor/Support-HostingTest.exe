param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$Output = (Join-Path (Split-Path -Parent $PSScriptRoot) 'dist\Release\Exact')
)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $ProjectRoot 'original\HostingTest.exe'
$assets = Join-Path $ProjectRoot 'assets'
$expected = 'dfd183ccccf82d684ed6e4952aa071c251419024c2bbb78755563009f9f62c81'
if (!(Test-Path $exe) -or !(Test-Path $assets)) { throw 'Original binary or resources missing.' }
if ((Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'Original executable differs from the supplied file.' }
New-Item -ItemType Directory -Force -Path $Output | Out-Null
Copy-Item $exe (Join-Path $Output 'HostingTest.exe') -Force
$copiedAssets = Join-Path $Output 'assets'
if (Test-Path $copiedAssets) { Remove-Item $copiedAssets -Recurse -Force }
Copy-Item $assets $copiedAssets -Recurse -Force
Copy-Item (Join-Path $ProjectRoot 'analysis\pe_metadata.json') (Join-Path $Output 'pe_metadata.json') -Force
if ((Get-FileHash (Join-Path $Output 'HostingTest.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'Output binary validation failed.' }
Write-Host 'Exact original executable preserved. SHA256:' $expected
