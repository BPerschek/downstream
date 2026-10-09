. "$PSScriptRoot\project_paths.ps1"
$playResult = Join-Path $DownstreamRoot 'build\play-verification.json'
$playImage = Join-Path $DownstreamRoot 'build\Downstream_Unreal_Preview.png'
foreach ($generatedFile in @($playResult, $playImage)) {
    if (Test-Path -LiteralPath $generatedFile) { Remove-Item -LiteralPath $generatedFile }
}
$playScript = (Join-Path $PSScriptRoot 'verify_play.py').Replace('\','/')
$playLog = Join-Path $DownstreamRoot 'build\verify-play-final.log'
$verificationProcess = Start-Process -FilePath $DownstreamEditor -ArgumentList @("`"$DownstreamProject`"", "-ExecCmds=`"py $playScript`"", '-unattended', '-nop4', '-nosplash', '-windowed', '-ResX=1280', '-ResY=720', '-NoSound', "-abslog=`"$playLog`"") -WindowStyle Hidden -PassThru
$playTimer = [Diagnostics.Stopwatch]::StartNew()
while (!$verificationProcess.WaitForExit(1000)) {
    if ($playTimer.Elapsed.TotalSeconds -gt 120) {
        Stop-Process -Id $verificationProcess.Id
        throw "Play verification timed out. See $playLog"
    }
}
if (!(Test-Path -LiteralPath $playResult)) { throw "Play verification produced no result. See $playLog" }
$result = Get-Content -LiteralPath $playResult -Raw | ConvertFrom-Json
if (!$result.passed) { throw "Play verification failed: $($result.message)" }
$result | ConvertTo-Json -Depth 5
