$ErrorActionPreference = 'Stop'
$compilerRoot = Split-Path -Parent $PSScriptRoot
$toolsFolder = Join-Path $compilerRoot '.tools'
$zigExe = Join-Path $toolsFolder 'zig-x86_64-windows-0.17.0\zig.exe'
if (Test-Path -LiteralPath $zigExe) { Write-Output 'Portable test compiler is already available.'; exit 0 }
New-Item -ItemType Directory -Force -Path $toolsFolder | Out-Null
$zigArchive = Join-Path $toolsFolder 'zig.zip'
$ProgressPreference = 'SilentlyContinue'
Invoke-WebRequest -Uri 'https://ziglang.org/download/0.17.0/zig-x86_64-windows-0.17.0.zip' -OutFile $zigArchive
$expectedHash = 'b5663f69581dcf391293fbf16c06cb80d81d806545ce618b4d0bab7f0eb8c428'
if ((Get-FileHash -LiteralPath $zigArchive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) {
    throw 'Portable compiler archive checksum did not match.'
}
Expand-Archive -LiteralPath $zigArchive -DestinationPath $toolsFolder -Force
Write-Output "Portable compiler ready at $zigExe"
