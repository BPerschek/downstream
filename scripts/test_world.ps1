$ErrorActionPreference = 'Stop'
$DownstreamRoot = Split-Path -Parent $PSScriptRoot
$zigCompiler = Join-Path $DownstreamRoot '.tools\zig-x86_64-windows-0.17.0\zig.exe'
if (!(Test-Path -LiteralPath $zigCompiler)) { throw 'Local portable test compiler missing. See tests/README.md.' }
$testBuild = Join-Path $DownstreamRoot 'build\world'
New-Item -ItemType Directory -Force -Path $testBuild | Out-Null
$testExe = Join-Path $testBuild 'world_tests.exe'
$includePath = Join-Path $DownstreamRoot 'native\Source\DownstreamSim\Public'
$sources = @('tests\world_tests.cpp','native\Source\DownstreamSim\Private\DownstreamWater.cpp','native\Source\DownstreamSim\Private\DownstreamEarthworks.cpp','native\Source\DownstreamSim\Private\DownstreamWorld.cpp') | ForEach-Object { Join-Path $DownstreamRoot $_ }
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $DownstreamRoot '.tools\zig-cache'
& $zigCompiler c++ '-std=c++17' '-O2' '-Wall' '-Wextra' '-Werror' '-pedantic-errors' '-I' $includePath @sources '-o' $testExe
if ($LASTEXITCODE -ne 0) { throw 'World test compilation failed.' }
& $testExe
if ($LASTEXITCODE -ne 0) { throw 'World behavior tests failed.' }
