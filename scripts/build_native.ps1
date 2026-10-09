param([ValidateSet('Editor','Game')][string]$Target = 'Editor')
. "$PSScriptRoot\project_paths.ps1"
function Remove-NewSource {
    $checkedSource = [IO.Path]::GetFullPath($activeSource)
    $expectedSource = [IO.Path]::GetFullPath((Join-Path $DownstreamRoot 'unreal\Downstream\Source'))
    if ($checkedSource -ne $expectedSource -or !$checkedSource.StartsWith([IO.Path]::GetFullPath($DownstreamRoot) + [IO.Path]::DirectorySeparatorChar)) { throw 'Native source cleanup escaped project' }
    Remove-Item -LiteralPath $checkedSource -Recurse -Force
    $targetCache = Join-Path $DownstreamRoot 'unreal\Downstream\Intermediate\TargetInfo.json'
    if (Test-Path -LiteralPath $targetCache) { Remove-Item -LiteralPath $targetCache }
}
$vswherePath = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswherePath)) {
    throw 'Native build requires Visual Studio 2022 Build Tools (MSVC 14.44.35211+) and Windows SDK 10.0.22621+. The playable Blueprint project remains usable. See docs/TOOLCHAIN.md.'
}
$cppInstall = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$cppInstall) { throw 'Visual Studio is present, but C++ build tools are missing. See docs/TOOLCHAIN.md.' }
$sdkHeader = 'C:\Program Files (x86)\Windows Kits\10\Include'
if (!(Test-Path -LiteralPath $sdkHeader)) { throw 'Windows SDK headers are missing. See docs/TOOLCHAIN.md.' }
$originalDescriptor = Get-Content -LiteralPath $DownstreamProject -Raw
$stagedSource = Join-Path $DownstreamRoot 'native\Source'
$activeSource = Join-Path $DownstreamRoot 'unreal\Downstream\Source'
$createdSource = !(Test-Path -LiteralPath $activeSource)
if ($createdSource) { Copy-Item -LiteralPath $stagedSource -Destination $activeSource -Recurse }
$sourceSnapshot = Join-Path $DownstreamRoot 'build\native-source-snapshot.json'
$previousSources = if (Test-Path -LiteralPath $sourceSnapshot) { Get-Content -LiteralPath $sourceSnapshot -Raw | ConvertFrom-Json } else { $null }
$newSourceHashes = @{}
foreach ($sourceFile in Get-ChildItem -LiteralPath $stagedSource -Recurse -File) {
    $relativeSource = $sourceFile.FullName.Substring($stagedSource.Length + 1)
    $activeFile = Join-Path $activeSource $relativeSource
    $newHash = (Get-FileHash -LiteralPath $sourceFile.FullName -Algorithm SHA256).Hash
    if (Test-Path -LiteralPath $activeFile) {
        $activeHash = (Get-FileHash -LiteralPath $activeFile -Algorithm SHA256).Hash
        $priorProperty = if ($previousSources) { $previousSources.PSObject.Properties[$relativeSource] } else { $null }
        if ($activeHash -ne $newHash -and (!$priorProperty -or $activeHash -ne $priorProperty.Value)) {
            throw "Active source has separate edits: $activeFile. Reconcile them with native/Source before building."
        }
    }
    New-Item -ItemType Directory -Force -Path (Split-Path $activeFile) | Out-Null
    Copy-Item -LiteralPath $sourceFile.FullName -Destination $activeFile -Force
    $newSourceHashes[$relativeSource] = $newHash
}
$newSourceHashes | ConvertTo-Json | Set-Content -LiteralPath $sourceSnapshot -Encoding utf8
$nativeDescriptor = $originalDescriptor | ConvertFrom-Json
$nativeDescriptor | Add-Member -MemberType NoteProperty -Name Modules -Value @(
    [pscustomobject]@{Name='Downstream';Type='Runtime';LoadingPhase='Default'},
    [pscustomobject]@{Name='DownstreamSim';Type='Runtime';LoadingPhase='PreDefault'},
    [pscustomobject]@{Name='DownstreamCombat';Type='Runtime';LoadingPhase='Default'},
    [pscustomobject]@{Name='DownstreamRender';Type='Runtime';LoadingPhase='Default'}
) -Force
$meshPlugin = $nativeDescriptor.Plugins | Where-Object { $_.Name -eq 'ProceduralMeshComponent' }
if (!$meshPlugin) { $nativeDescriptor.Plugins += [pscustomobject]@{Name='ProceduralMeshComponent';Enabled=$true} }
if (!($nativeDescriptor.Plugins | Where-Object { $_.Name -eq 'GameplayAbilities' })) { $nativeDescriptor.Plugins += [pscustomobject]@{Name='GameplayAbilities';Enabled=$true} }
$nativeDescriptor | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $DownstreamProject -Encoding utf8
try {
    $nativeTarget = if ($Target -eq 'Editor') { 'DownstreamEditor' } else { 'Downstream' }
    & (Join-Path $DownstreamEngine 'Engine\Build\BatchFiles\Build.bat') $nativeTarget Win64 Development "-Project=$DownstreamProject" '-WaitMutex' '-NoHotReloadFromIDE'
    if ($LASTEXITCODE -ne 0) { throw "Unreal build failed with exit code $LASTEXITCODE" }
    if ($Target -eq 'Editor') {
        & (Join-Path $DownstreamEngine 'Engine\Build\BatchFiles\Build.bat') 'Downstream' Win64 Development "-Project=$DownstreamProject" '-WaitMutex' '-NoHotReloadFromIDE'
        if ($LASTEXITCODE -ne 0) { throw "Unreal Game build failed with exit code $LASTEXITCODE" }
    }
    if ($Target -eq 'Game') {
        # Game binaries alone cannot be loaded by the Editor executable used by play.ps1.
        Set-Content -LiteralPath $DownstreamProject -Value $originalDescriptor -Encoding utf8
        if ($createdSource) { Remove-NewSource }
    }
} catch {
    # An unsuccessful first build must not strand the working Blueprint project.
    Set-Content -LiteralPath $DownstreamProject -Value $originalDescriptor -Encoding utf8
    if ($createdSource) { Remove-NewSource }
    throw
}
