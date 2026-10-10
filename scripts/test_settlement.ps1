$ErrorActionPreference = 'Stop'
$DownstreamRoot = Split-Path -Parent $PSScriptRoot
$zigCompiler = Join-Path $DownstreamRoot '.tools\zig-x86_64-windows-0.17.0\zig.exe'
if (!(Test-Path -LiteralPath $zigCompiler)) { throw 'Local portable test compiler missing. See tests/README.md.' }
$testBuild = Join-Path $DownstreamRoot 'build\settlement'
New-Item -ItemType Directory -Force -Path $testBuild | Out-Null
$testExe = Join-Path $testBuild 'settlement_tests.exe'
$includePath = Join-Path $DownstreamRoot 'native\Source\DownstreamSim\Public'
$testSource = Join-Path $DownstreamRoot 'tests\settlement_tests.cpp'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $DownstreamRoot '.tools\zig-cache'
& $zigCompiler c++ '-std=c++17' '-Wall' '-Wextra' '-Werror' '-pedantic-errors' '-I' $includePath $testSource '-o' $testExe
if ($LASTEXITCODE -ne 0) { throw 'Settlement test compilation failed.' }
& $testExe
if ($LASTEXITCODE -ne 0) { throw 'Settlement behavior tests failed.' }
