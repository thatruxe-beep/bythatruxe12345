$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$sdkDir = $env:PLUGIN_SDK_DIR

if ([string]::IsNullOrWhiteSpace($sdkDir)) {
    $sdkDir = Join-Path $root ".deps\plugin-sdk"
    if (-not (Test-Path $sdkDir)) {
        New-Item -ItemType Directory -Force -Path (Split-Path $sdkDir) | Out-Null
        git clone https://github.com/DK22Pac/plugin-sdk.git $sdkDir
    }
}

$env:PLUGIN_SDK_DIR = $sdkDir
$premake = Join-Path $sdkDir "tools\premake\premake5.exe"
if (-not (Test-Path $premake)) {
    throw "Plugin-SDK premake was not found: $premake"
}

& $premake vs2022 --pluginsdkdir="$sdkDir" --file="$sdkDir\tools\premake\premake5.lua"
if ($LASTEXITCODE -ne 0) { throw "Plugin-SDK project generation failed" }

msbuild (Join-Path $sdkDir "plugin.sln") /m /t:Plugin_SA /p:Configuration=Release /p:Platform="Mixed Platforms"
if ($LASTEXITCODE -ne 0) { throw "Plugin-SDK build failed" }

$pluginLib = Join-Path $sdkDir "output\lib\Plugin.lib"
if (-not (Test-Path $pluginLib)) { throw "Plugin.lib was not produced" }

$outDir = Join-Path $root "build"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
msbuild (Join-Path $root "18_32_cheat\18_32_cheat.vcxproj") /m /t:Build /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:OutDir="$outDir\" /p:TargetName=18_32_cheat
if ($LASTEXITCODE -ne 0) { throw "18:32 cheat build failed" }

$dll = Join-Path $outDir "18_32_cheat.dll"
if (-not (Test-Path $dll)) { throw "DLL was not produced" }
Write-Host "Built successfully: $dll" -ForegroundColor Green
