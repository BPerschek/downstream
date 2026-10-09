param([switch]$CapturePreview, [switch]$TechnicalMap)
. "$PSScriptRoot\project_paths.ps1"
# This is the visible game the owner requested, so intentionally show its window.
$gameLog = Join-Path $DownstreamRoot 'build\play.log'
New-Item -ItemType Directory -Force -Path (Split-Path $gameLog) | Out-Null
$startupCommands = 'RebuildNavigation'
if ($CapturePreview) {
    $previewPath = (Join-Path $DownstreamRoot 'build\Downstream_Game_Preview.png').Replace('\','/')
    $startupCommands += ",HighResShot 1280x720 filename=$previewPath"
}
$playMap = if ($TechnicalMap) { '/Game/Downstream/Maps/M0_TestWorld' } else { '/Game/Downstream/Maps/M1_Expedition' }
Start-Process -FilePath $DownstreamEditor -ArgumentList @("`"$DownstreamProject`"", $playMap, '-game', '-windowed', '-ResX=1280', '-ResY=720', '-nosplash', '-NoScreenMessages', "-ExecCmds=`"$startupCommands`"", "-abslog=`"$gameLog`"") -PassThru
