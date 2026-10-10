# Builds M2_World and MainMenu (scripts/create_frontier_maps.py), then bakes M2 navigation.
. "$PSScriptRoot\project_paths.ps1"
$frontierScript = Join-Path $PSScriptRoot 'create_frontier_maps.py'
$generationLog = Join-Path $DownstreamRoot 'build\generate-frontier.log'
New-Item -ItemType Directory -Path (Split-Path $generationLog) -Force | Out-Null
$frontierResult = Join-Path $DownstreamRoot 'build\frontier-generation.json'
if (Test-Path -LiteralPath $frontierResult) { Remove-Item -LiteralPath $frontierResult }
& $DownstreamCommandlet $DownstreamProject '-run=pythonscript' "-script=$frontierScript" '-unattended' '-nop4' '-nosplash' '-NullRHI' "-abslog=$generationLog"
if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $frontierResult)) { throw "Frontier generation failed. See $generationLog" }
$bakeResult = Join-Path $DownstreamRoot 'build\navigation-bake.json'
if (Test-Path -LiteralPath $bakeResult) { Remove-Item -LiteralPath $bakeResult }
$bakeScript = (Join-Path $PSScriptRoot 'prepare_navigation.py').Replace('\','/')
$bakeLog = Join-Path $DownstreamRoot 'build\prepare-frontier-navigation.log'
$process = Start-Process -FilePath $DownstreamEditor -ArgumentList @("`"$DownstreamProject`"", "-ExecCmds=`"py $bakeScript`"", '-DownstreamMap=/Game/Downstream/Maps/M2_World', '-unattended', '-nop4', '-nosplash', '-NoSound', "-abslog=`"$bakeLog`"") -WindowStyle Hidden -PassThru
$timer = [Diagnostics.Stopwatch]::StartNew()
while (!$process.WaitForExit(1000)) {
    if ($timer.Elapsed.TotalSeconds -gt 180) { Stop-Process -Id $process.Id; throw "Navigation bake timed out. See $bakeLog" }
}
if (!(Test-Path -LiteralPath $bakeResult)) { throw "Navigation bake produced no result. See $bakeLog" }
$result = Get-Content -LiteralPath $bakeResult -Raw | ConvertFrom-Json
if (!$result.passed) { throw $result.message }
Get-Content -LiteralPath $frontierResult -Raw
Write-Output 'Frontier maps and navigation generated.'
