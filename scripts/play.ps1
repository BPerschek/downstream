param([switch]$CapturePreview, [switch]$TechnicalMap, [switch]$Classic, [switch]$Frontier, [int]$Seed = 0)
. "$PSScriptRoot\project_paths.ps1"
# This is the visible game the owner requested, so intentionally show its window.
# Default: main menu. -Frontier jumps into the saved frontier (or a new one with -Seed N),
# -Classic opens the training grounds (M1), -TechnicalMap the M0 developer map.
$gameLog = Join-Path $DownstreamRoot 'build\play.log'
New-Item -ItemType Directory -Force -Path (Split-Path $gameLog) | Out-Null
$startupCommands = 'RebuildNavigation'
if ($CapturePreview) {
    $previewPath = (Join-Path $DownstreamRoot 'build\Downstream_Game_Preview.png').Replace('\','/')
    $startupCommands += ",HighResShot 1280x720 filename=$previewPath"
}
$playMap = '/Game/Downstream/Maps/MainMenu'
if ($TechnicalMap) { $playMap = '/Game/Downstream/Maps/M0_TestWorld' }
elseif ($Classic) { $playMap = '/Game/Downstream/Maps/M1_Expedition' }
elseif ($Frontier) { $playMap = '/Game/Downstream/Maps/M2_World' }
$arguments = @("`"$DownstreamProject`"", $playMap, '-game', '-windowed', '-ResX=1600', '-ResY=900', '-nosplash', '-NoScreenMessages', "-ExecCmds=`"$startupCommands`"", "-abslog=`"$gameLog`"")
if ($Frontier -and $Seed -gt 0) { $arguments += "-DownstreamWorldSeed=$Seed" }
Start-Process -FilePath $DownstreamEditor -ArgumentList $arguments -PassThru
