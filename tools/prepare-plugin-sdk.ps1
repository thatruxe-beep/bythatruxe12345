param(
    [Parameter(Mandatory = $true)]
    [string]$SdkDir,

    [Parameter(Mandatory = $true)]
    [string]$MsBuildPath,

    [string]$PlatformToolset = "v145"
)

$ErrorActionPreference = "Stop"
# Generated Plugin-SDK projects reference $(PLUGIN_SDK_DIR). Export it for
# premake and the nested MSBuild process, including builds started from VS.
$env:PLUGIN_SDK_DIR = $SdkDir
$commit = "5da18b6f1956bb20bdfa39dcb07c44863ce26c81"
$pluginLib = Join-Path $SdkDir "output\lib\Plugin.lib"

if (Test-Path $pluginLib) {
    Write-Host "Plugin-SDK is already built: $pluginLib"
    exit 0
}

$sharedHeader = Join-Path $SdkDir "shared\plugin.h"
if (-not (Test-Path $sharedHeader)) {
    $parent = Split-Path -Parent $SdkDir
    $work = Join-Path $parent "plugin-sdk-download"
    $archive = Join-Path $work "plugin-sdk.zip"
    $unpacked = Join-Path $work "unpacked"
    $url = "https://codeload.github.com/DK22Pac/plugin-sdk/zip/$commit"

    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $SdkDir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $work | Out-Null

    Write-Host "Downloading Plugin-SDK..."
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $archive

    Write-Host "Extracting Plugin-SDK..."
    Expand-Archive -Path $archive -DestinationPath $unpacked -Force
    $source = Get-ChildItem -Path $unpacked -Directory | Select-Object -First 1
    if (-not $source) {
        throw "Downloaded Plugin-SDK archive is empty"
    }

    Move-Item -Path $source.FullName -Destination $SdkDir
    Remove-Item $work -Recurse -Force
}

$premake = Join-Path $SdkDir "tools\premake\premake5.exe"
if (-not (Test-Path $premake)) {
    throw "Plugin-SDK premake was not found: $premake"
}
if (-not (Test-Path $MsBuildPath)) {
    throw "MSBuild was not found: $MsBuildPath"
}

Write-Host "Generating Plugin-SDK Visual Studio project..."
& $premake vs2022 "--pluginsdkdir=$SdkDir" "--file=$SdkDir\tools\premake\premake5.lua"
if ($LASTEXITCODE -ne 0) {
    throw "Plugin-SDK project generation failed with code $LASTEXITCODE"
}

Write-Host "Building Plugin-SDK static library..."
& $MsBuildPath (Join-Path $SdkDir "plugin.sln") /m /t:Plugin_SA /p:Configuration=Release '/p:Platform=Mixed Platforms' "/p:PlatformToolset=$PlatformToolset" "/p:PLUGIN_SDK_DIR=$SdkDir"
if ($LASTEXITCODE -ne 0) {
    throw "Plugin-SDK build failed with code $LASTEXITCODE"
}
if (-not (Test-Path $pluginLib)) {
    throw "Plugin-SDK build did not produce Plugin.lib"
}

Write-Host "Plugin-SDK is ready: $pluginLib" -ForegroundColor Green
