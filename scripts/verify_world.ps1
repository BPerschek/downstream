. "$PSScriptRoot\project_paths.ps1"
$verifyLog = Join-Path $DownstreamRoot 'build\verify-world.log'
& $DownstreamCommandlet $DownstreamProject '-run=pythonscript' "-script=$PSScriptRoot\verify_world.py" '-unattended' '-nop4' '-nosplash' '-NullRHI' "-abslog=$verifyLog"
if ($LASTEXITCODE -ne 0) { throw "World verification failed. See $verifyLog" }
