$ErrorActionPreference = 'Stop'
$DownstreamRoot = Split-Path -Parent $PSScriptRoot
$DownstreamProject = Join-Path $DownstreamRoot 'unreal\Downstream\Downstream.uproject'
$DownstreamEngine = 'C:\Program Files\Epic Games\UE_5.8'
$DownstreamEditor = Join-Path $DownstreamEngine 'Engine\Binaries\Win64\UnrealEditor.exe'
$DownstreamCommandlet = Join-Path $DownstreamEngine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (!(Test-Path -LiteralPath $DownstreamProject)) { throw 'DOWNSTREAM project not found.' }
if (!(Test-Path -LiteralPath $DownstreamCommandlet)) { throw 'Unreal 5.8 not found. Update scripts/project_paths.ps1 for this machine.' }
