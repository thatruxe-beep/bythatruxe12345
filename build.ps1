$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$sdkDir = $env:PLUGIN_SDK_DIR
if ([string]::IsNullOrWhiteSpace($sdkDir)) {
    $sdkDir = Join-Path $root ".deps\plugin-sdk"
}

$msbuildCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
if (-not $msbuildCommand) {
    throw "MSBuild was not found. Run this script from Developer PowerShell for Visual Studio."
}
$msbuildPath = $msbuildCommand.Source
$env:PLUGIN_SDK_DIR = $sdkDir

$platformToolset = "v145"
& (Join-Path $root "tools\prepare-plugin-sdk.ps1") -SdkDir $sdkDir -MsBuildPath $msbuildPath -PlatformToolset $platformToolset
if ($LASTEXITCODE -ne 0) {
    throw "Plugin-SDK preparation failed with code $LASTEXITCODE"
}

$outDir = Join-Path $root "build"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$dll = Join-Path $outDir "18_32_cheat.dll"
Remove-Item -Force -ErrorAction SilentlyContinue $dll

# Always rebuild the DLL so an old menu binary cannot survive UI/source changes.
& $msbuildPath (Join-Path $root "18_32_cheat\18_32_cheat.vcxproj") /m /t:Rebuild /p:Configuration=Release /p:Platform=Win32 "/p:PlatformToolset=$platformToolset" "/p:OutDir=$outDir\" /p:TargetName=18_32_cheat
if ($LASTEXITCODE -ne 0) {
    throw "18:32 cheat build failed with code $LASTEXITCODE"
}

if (-not (Test-Path $dll)) {
    throw "DLL was not produced"
}
Write-Host "Built successfully: $dll" -ForegroundColor Green
