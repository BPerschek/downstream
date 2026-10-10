. "$PSScriptRoot\project_paths.ps1"
# Imports CC0 assets retained under third_party/ into the project. Close project editors first.
$script = Join-Path $DownstreamRoot 'art\import_third_party.py'
$log = Join-Path $DownstreamRoot 'build\import-third-party.log'
& $DownstreamCommandlet $DownstreamProject '-run=pythonscript' "-script=$script" '-unattended' '-nop4' '-nosplash' '-NullRHI' "-abslog=$log"
if ($LASTEXITCODE -ne 0) { throw "Third-party import failed. See $log" }
Get-Content (Join-Path $DownstreamRoot 'build\third-party-import.json') -Raw
