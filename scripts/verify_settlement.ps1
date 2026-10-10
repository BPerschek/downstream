. "$PSScriptRoot\project_paths.ps1"
$testRoot = Join-Path $DownstreamRoot 'build\settlement-test-save'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$flowSave = Join-Path $testRoot 'flow'
New-Item -ItemType Directory -Path $flowSave -Force | Out-Null
foreach ($mode in @('flow','reload','unfunded','invalid','write_failure')) {
    $saveDirectory = if ($mode -eq 'reload') { $flowSave } else { Join-Path $testRoot $mode }
    New-Item -ItemType Directory -Path $saveDirectory -Force | Out-Null
    $saveFile = Join-Path $saveDirectory 'prototype-save.json'
    if ($mode -ne 'reload') {
        foreach ($name in @('prototype-save.json','prototype-save.json.bak','prototype-save.json.tmp')) {
            $generated = Join-Path $saveDirectory $name
            if (Test-Path -LiteralPath $generated -PathType Leaf) { Remove-Item -LiteralPath $generated }
        }
        if ($mode -eq 'write_failure') {
            # Original flow snapshot retains two units of in-flight robot cargo.
            Copy-Item -LiteralPath (Join-Path $testRoot 'flow-cargo-snapshot.json') -Destination $saveFile -Force
            New-Item -ItemType Directory -Path (Join-Path $saveDirectory 'prototype-save.json.tmp') -Force | Out-Null
        } elseif ($mode -eq 'invalid') {
            '{"version":2,"banked_salvage":"40","selected_skill":"","structures":[],"hauler":{"built":false,"battery":100,"cargo":0,"hauling_enabled":false}}' | Set-Content -LiteralPath $saveFile -Encoding utf8
        } else {
            $bank = if ($mode -eq 'flow') { 40 } else { 0 }
            [pscustomobject]@{version=1;banked_salvage=$bank;selected_skill='steady_aim'} | ConvertTo-Json | Set-Content -LiteralPath $saveFile -Encoding utf8
        }
    }
    $resultPath = Join-Path $DownstreamRoot "build\settlement-$mode-verification.json"
    if (Test-Path -LiteralPath $resultPath) { Remove-Item -LiteralPath $resultPath }
    $script = (Join-Path $PSScriptRoot 'verify_settlement.py').Replace('\','/')
    $log = Join-Path $DownstreamRoot "build\settlement-$mode-verification.log"
    $process = Start-Process -FilePath $DownstreamEditor -ArgumentList @("`"$DownstreamProject`"","-ExecCmds=`"py $script`"","-DownstreamSaveDir=`"$saveDirectory`"","-DownstreamSettlementMode=$mode",'-unattended','-nop4','-nosplash','-windowed','-ResX=1280','-ResY=720','-NoSound',"-abslog=`"$log`"") -WindowStyle Hidden -PassThru
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while (!$process.WaitForExit(1000)) {
        if ($timer.Elapsed.TotalSeconds -gt 125) { Stop-Process -Id $process.Id; throw "Settlement test $mode timed out. See $log" }
    }
    if (!(Test-Path -LiteralPath $resultPath)) { throw "No settlement $mode report. See $log" }
    $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
    if (!$result.passed) { throw "Settlement $mode failed at $($result.stage): $($result.message). See $log" }
    if ($mode -eq 'flow') { Copy-Item -LiteralPath $saveFile -Destination (Join-Path $testRoot 'flow-cargo-snapshot.json') -Force }
    $result | ConvertTo-Json -Depth 5
}
