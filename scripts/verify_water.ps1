. "$PSScriptRoot\project_paths.ps1"
# Isolated save directories; never touches the owner's prototype-save.json.
$testRoot = Join-Path $DownstreamRoot 'build\water-test-save'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$flowSave = Join-Path $testRoot 'flow'
foreach ($mode in @('legacy','flow','reload','respawn')) {
    $saveDirectory = if ($mode -eq 'reload') { $flowSave } else { Join-Path $testRoot $mode }
    New-Item -ItemType Directory -Path $saveDirectory -Force | Out-Null
    $saveFile = Join-Path $saveDirectory 'prototype-save.json'
    if ($mode -ne 'reload') {
        foreach ($name in @('prototype-save.json','prototype-save.json.bak','prototype-save.json.tmp')) {
            $generated = Join-Path $saveDirectory $name
            if (Test-Path -LiteralPath $generated -PathType Leaf) { Remove-Item -LiteralPath $generated }
        }
        if ($mode -eq 'legacy' -or $mode -eq 'respawn') {
            [pscustomobject]@{version=1;banked_salvage=40;selected_skill='steady_aim'} | ConvertTo-Json | Set-Content -LiteralPath $saveFile -Encoding utf8
        } else {
            '{"version":2,"banked_salvage":30,"selected_skill":"salvage_handling","structures":[{"type":"storage","cell_x":2,"cell_y":0},{"type":"workshop","cell_x":1,"cell_y":1},{"type":"charger","cell_x":2,"cell_y":1}],"hauler":{"built":false,"battery":100,"cargo":0,"hauling_enabled":false}}' | Set-Content -LiteralPath $saveFile -Encoding utf8
        }
    }
    $resultPath = Join-Path $DownstreamRoot "build\water-$mode-verification.json"
    if (Test-Path -LiteralPath $resultPath) { Remove-Item -LiteralPath $resultPath }
    $script = (Join-Path $PSScriptRoot 'verify_water.py').Replace('\','/')
    $log = Join-Path $DownstreamRoot "build\water-$mode-verification.log"
    $process = Start-Process -FilePath $DownstreamEditor -ArgumentList @("`"$DownstreamProject`"","-ExecCmds=`"py $script`"","-DownstreamSaveDir=`"$saveDirectory`"","-DownstreamWaterMode=$mode",'-unattended','-nop4','-nosplash','-windowed','-ResX=1280','-ResY=720','-NoSound',"-abslog=`"$log`"") -WindowStyle Hidden -PassThru
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while (!$process.WaitForExit(1000)) {
        if ($timer.Elapsed.TotalSeconds -gt 320) { Stop-Process -Id $process.Id; throw "Water test $mode timed out. See $log" }
    }
    if (!(Test-Path -LiteralPath $resultPath)) { throw "No water $mode report. See $log" }
    $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
    if (!$result.passed) { throw "Water $mode failed at $($result.stage): $($result.message). See $log" }
    $result | ConvertTo-Json -Depth 5
}
