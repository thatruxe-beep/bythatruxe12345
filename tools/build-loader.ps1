param(
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot

$msbuildCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
if (-not $msbuildCommand) {
    throw "MSBuild was not found. Run this script from Developer PowerShell for Visual Studio."
}
$msbuildPath = $msbuildCommand.Source

$outDir = Join-Path $root "build"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exe = Join-Path $outDir "18_32_loader.exe"
Remove-Item -Force -ErrorAction SilentlyContinue $exe

& $msbuildPath (Join-Path $root "18_32_loader\18_32_loader.vcxproj") /m /t:Rebuild `
    /p:Configuration=$Configuration /p:Platform=Win32 "/p:PlatformToolset=v145" `
    "/p:OutDir=$outDir\" /p:TargetName=18_32_loader
if ($LASTEXITCODE -ne 0) {
    throw "18:32 loader build failed with code $LASTEXITCODE"
}

if (-not (Test-Path $exe)) {
    throw "Loader exe was not produced"
}

Write-Host "Built successfully: $exe" -ForegroundColor Green
Write-Host "Put loader.ini next to it (see 18_32_loader\loader.ini.example)." -ForegroundColor Gray
