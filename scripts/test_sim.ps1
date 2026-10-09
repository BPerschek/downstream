. "$PSScriptRoot\project_paths.ps1"
$zigCompiler = Join-Path $DownstreamRoot '.tools\zig-x86_64-windows-0.17.0\zig.exe'
if (!(Test-Path -LiteralPath $zigCompiler)) { throw 'Local portable test compiler missing. See tests/README.md.' }
$testBuild = Join-Path $DownstreamRoot 'build\sim'
New-Item -ItemType Directory -Force -Path $testBuild | Out-Null
$testExe = Join-Path $testBuild 'sim_tests.exe'
$includePath = Join-Path $DownstreamRoot 'native\Source\DownstreamSim\Public'
$testSource = Join-Path $DownstreamRoot 'tests\sim_tests.cpp'
$simSource = Join-Path $DownstreamRoot 'native\Source\DownstreamSim\Private\DownstreamSimulation.cpp'
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $DownstreamRoot '.tools\zig-cache'
& $zigCompiler c++ '-std=c++17' '-Wall' '-Wextra' '-Werror' '-I' $includePath $testSource $simSource '-o' $testExe
if ($LASTEXITCODE -ne 0) { throw 'Simulation test compilation failed.' }
& $testExe
if ($LASTEXITCODE -ne 0) { throw 'Simulation behavior tests failed.' }
