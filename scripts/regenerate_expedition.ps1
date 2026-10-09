. "$PSScriptRoot\project_paths.ps1"
$expeditionScript = Join-Path $PSScriptRoot 'create_expedition_level.py'
$generationLog = Join-Path $DownstreamRoot 'build\generate-expedition.log'
New-Item -ItemType Directory -Path (Split-Path $generationLog) -Force | Out-Null
& $DownstreamCommandlet $DownstreamProject '-run=pythonscript' "-script=$expeditionScript" '-unattended' '-nop4' '-nosplash' '-NullRHI' "-abslog=$generationLog"
if ($LASTEXITCODE -ne 0) { throw "Expedition generation failed. See $generationLog" }
$bakeResult = Join-Path $DownstreamRoot 'build\navigation-bake.json'
if (Test-Path -LiteralPath $bakeResult) { Remove-Item -LiteralPath $bakeResult }
$bakeScript = (Join-Path $PSScriptRoot 'prepare_navigation.py').Replace('\','/')
$bakeLog = Join-Path $DownstreamRoot 'build\prepare-expedition-navigation.log'
$process = Start-Process -FilePath $DownstreamEditor -ArgumentList @("`"$DownstreamProject`"", "-ExecCmds=`"py $bakeScript`"", '-DownstreamMap=/Game/Downstream/Maps/M1_Expedition', '-unattended', '-nop4', '-nosplash', '-NoSound', "-abslog=`"$bakeLog`"") -WindowStyle Hidden -PassThru
$timer = [Diagnostics.Stopwatch]::StartNew()
while (!$process.WaitForExit(1000)) {
    if ($timer.Elapsed.TotalSeconds -gt 120) { Stop-Process -Id $process.Id; throw "Navigation bake timed out. See $bakeLog" }
}
if (!(Test-Path -LiteralPath $bakeResult)) { throw "Navigation bake produced no result. See $bakeLog" }
$result = Get-Content -LiteralPath $bakeResult -Raw | ConvertFrom-Json
if (!$result.passed) { throw $result.message }
Write-Output 'Playable expedition and navigation generated.'
