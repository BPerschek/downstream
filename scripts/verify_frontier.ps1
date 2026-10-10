param([int]$Seed = 3)
. "$PSScriptRoot\project_paths.ps1"
# Isolated save directory; never touches the owner's world-save.json or prototype-save.json.
$saveDirectory = Join-Path $DownstreamRoot 'build\frontier-test-save'
New-Item -ItemType Directory -Path $saveDirectory -Force | Out-Null
foreach ($name in @('world-save.json','world-save.json.bak','world-save.json.tmp')) {
    $generated = Join-Path $saveDirectory $name
    if (Test-Path -LiteralPath $generated -PathType Leaf) { Remove-Item -LiteralPath $generated }
}
foreach ($mode in @('menu','frontier','reload')) {
    $resultPath = Join-Path $DownstreamRoot "build\frontier-$mode-verification.json"
    if (Test-Path -LiteralPath $resultPath) { Remove-Item -LiteralPath $resultPath }
    $script = (Join-Path $PSScriptRoot 'verify_frontier.py').Replace('\','/')
    $log = Join-Path $DownstreamRoot "build\frontier-$mode-verification.log"
    $arguments = @("`"$DownstreamProject`"","-ExecCmds=`"py $script`"","-DownstreamSaveDir=`"$saveDirectory`"","-DownstreamFrontierMode=$mode",'-unattended','-nop4','-nosplash','-windowed','-ResX=1280','-ResY=720','-NoSound',"-abslog=`"$log`"")
    if ($mode -eq 'frontier') { $arguments += "-DownstreamWorldSeed=$Seed" }
    $process = Start-Process -FilePath $DownstreamEditor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while (!$process.WaitForExit(1000)) {
        if ($timer.Elapsed.TotalSeconds -gt 460) { Stop-Process -Id $process.Id; throw "Frontier test $mode timed out. See $log" }
    }
    if (!(Test-Path -LiteralPath $resultPath)) { throw "No frontier $mode report. See $log" }
    $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
    if (!$result.passed) { throw "Frontier $mode failed at $($result.stage): $($result.message). See $log" }
    $result | ConvertTo-Json -Depth 5
}
