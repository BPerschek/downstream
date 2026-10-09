. "$PSScriptRoot\project_paths.ps1"
$worldScript = Join-Path $PSScriptRoot 'create_test_world.py'
$worldLog = Join-Path $DownstreamRoot 'build\generate-world.log'
New-Item -ItemType Directory -Path (Split-Path $worldLog) -Force | Out-Null
& $DownstreamCommandlet $DownstreamProject '-run=pythonscript' "-script=$worldScript" '-unattended' '-nop4' '-nosplash' '-NullRHI' "-abslog=$worldLog"
if ($LASTEXITCODE -ne 0) { throw "World generation failed. See $worldLog" }
$bakeResult = Join-Path $DownstreamRoot 'build\navigation-bake.json'
if (Test-Path -LiteralPath $bakeResult) { Remove-Item -LiteralPath $bakeResult }
$bakeScript = (Join-Path $PSScriptRoot 'prepare_navigation.py').Replace('\','/')
$bakeLog = Join-Path $DownstreamRoot 'build\prepare-navigation.log'
$bakeProcess = Start-Process -FilePath $DownstreamEditor -ArgumentList @("`"$DownstreamProject`"", "-ExecCmds=`"py $bakeScript`"", '-unattended', '-nop4', '-nosplash', '-NoSound', "-abslog=`"$bakeLog`"") -WindowStyle Hidden -PassThru
$bakeTimer = [Diagnostics.Stopwatch]::StartNew()
while (!$bakeProcess.WaitForExit(1000)) {
    if ($bakeTimer.Elapsed.TotalSeconds -gt 120) {
        Stop-Process -Id $bakeProcess.Id
        throw "Navigation bake timed out. See $bakeLog"
    }
}
if (!(Test-Path -LiteralPath $bakeResult)) { throw "Navigation bake produced no result. See $bakeLog" }
$bake = Get-Content -LiteralPath $bakeResult -Raw | ConvertFrom-Json
if (!$bake.passed) { throw "Navigation bake failed: $($bake.message)" }
Write-Output 'Test map and navigation generated successfully.'
