. "$PSScriptRoot\project_paths.ps1"
$isolatedSave = Join-Path $DownstreamRoot 'build\expedition-test-save'
New-Item -ItemType Directory -Path $isolatedSave -Force | Out-Null
foreach ($name in @('prototype-save.json','prototype-save.json.bak','prototype-save.json.tmp')) {
    $generatedPath = Join-Path $isolatedSave $name
    if (Test-Path -LiteralPath $generatedPath) { Remove-Item -LiteralPath $generatedPath }
}
foreach ($reload in @($false,$true)) {
    $resultName = if ($reload) { 'expedition-reload-verification.json' } else { 'expedition-play-verification.json' }
    $resultPath = Join-Path $DownstreamRoot "build\$resultName"
    if (Test-Path -LiteralPath $resultPath) { Remove-Item -LiteralPath $resultPath }
    $verifyScript = (Join-Path $PSScriptRoot 'verify_expedition.py').Replace('\','/')
    $logPath = Join-Path $DownstreamRoot "build\$resultName.log"
    $arguments = @("`"$DownstreamProject`"", "-ExecCmds=`"py $verifyScript`"", "-DownstreamSaveDir=`"$isolatedSave`"", '-unattended', '-nop4', '-nosplash', '-windowed', '-ResX=1280', '-ResY=720', '-NoSound', '-DownstreamNoFieldRespawn', "-abslog=`"$logPath`"")
    if ($reload) { $arguments += '-DownstreamVerifyReload' }
    $process = Start-Process -FilePath $DownstreamEditor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while (!$process.WaitForExit(1000)) {
        if ($timer.Elapsed.TotalSeconds -gt 120) { Stop-Process -Id $process.Id; throw "Expedition verification timed out. See $logPath" }
    }
    if (!(Test-Path -LiteralPath $resultPath)) { throw "No gameplay report. See $logPath" }
    $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
    if (!$result.passed) { throw "Gameplay failed at $($result.stage): $($result.message). See $logPath" }
    $result | ConvertTo-Json -Depth 5
}
