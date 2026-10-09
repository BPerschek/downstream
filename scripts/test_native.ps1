. "$PSScriptRoot\project_paths.ps1"
$automationLog = Join-Path $DownstreamRoot 'build\native-automation.log'
$reportPath = Join-Path $DownstreamRoot 'build\native-automation-report'
& $DownstreamCommandlet $DownstreamProject '-unattended' '-nop4' '-nosplash' '-NullRHI' '-NoSound' '-ExecCmds=Automation RunTests Downstream.' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportPath" "-abslog=$automationLog"
if ($LASTEXITCODE -ne 0) { throw "Native automation failed. See $automationLog" }
$automationReport = Join-Path $reportPath 'index.json'
if (!(Test-Path -LiteralPath $automationReport)) { throw "Native automation report missing. See $automationLog" }
$result = Get-Content -LiteralPath $automationReport -Raw | ConvertFrom-Json
if ($result.succeeded -lt 8 -or $result.failed -gt 0) { throw "Native suite incomplete or failed: $automationReport" }
Write-Output "$($result.succeeded) native Downstream tests passed."
